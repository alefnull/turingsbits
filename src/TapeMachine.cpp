#include "plugin.hpp"
#include <bit>
#include <random>
#include <ctime>
#include "inc/cvRange.hpp"

struct TapeMachineModule : Module
{
  enum Params
  {
    PROBABILITY_PARAM,
    CLEAR_PARAM,
    SET_PARAM,
    SHIFT_PARAM,
    DIR_PARAM,
    DUAL_PARAM,
    ENUMS(GRID_LOGIC_PARAM, 8),
    NUM_PARAMS
  };
  enum Inputs
  {
    CLOCK_INPUT,
    CLEAR_INPUT,
    SET_INPUT,
    SHIFT_INPUT,
    DIR_INPUT,
    DUAL_INPUT,
    NUM_INPUTS
  };
  enum Outputs
  {
    VOLTAGE_OUTPUT,
    FLIPPED_OUTPUT,
    ENUMS(PULSE_OUTPUT, 16),
    MIN_OUTPUT,
    MAX_OUTPUT,
    RANDOM_PULSE_OUTPUT,
    ENUMS(GRID_LOGIC_OUTPUT, 8),
    NUM_OUTPUTS
  };
  enum Lights
  {
    ENUMS(BIT_LIGHT, 16),
    CLEAR_LIGHT,
    SET_LIGHT,
    ENUMS(GRID_LOGIC_LIGHT, 8),
    RANDOM_PULSE_LIGHT,
    NUM_LIGHTS
  };
  enum LogicMode
  {
    AND,
    OR,
    XOR
  };

  bool dual = false;
  uint16_t tape = 0b0;
  uint8_t tapeA = 0, tapeB = 0; 
  bool last_dual = false;       
  bool bit_toggled = false;
  dsp::PulseGenerator random_pulse;
  uint16_t mask[16] = {
      0b0000000000000001,
      0b0000000000000010,
      0b0000000000000100,
      0b0000000000001000,
      0b0000000000010000,
      0b0000000000100000,
      0b0000000001000000,
      0b0000000010000000,
      0b0000000100000000,
      0b0000001000000000,
      0b0000010000000000,
      0b0000100000000000,
      0b0001000000000000,
      0b0010000000000000,
      0b0100000000000000,
      0b1000000000000000};
  float prob = 0.5;
  float noise_a = 0.f;
  float noise_b = 0.f;
  bool clear = false;
  bool set = false;
  int shift_amt = 1;
  CVRange voltage_range;
  CVRange flipped_voltage_range;
  CVRange min_voltage_range;
  CVRange max_voltage_range;

  dsp::SchmittTrigger clock;
  dsp::SchmittTrigger dir_trigger;

  size_t bit_pulse_mode = 1;
  size_t random_pulse_mode = 1;
  std::vector<std::string> mode_labels = {"trigger", "clock", "hold"};
  std::vector<dsp::PulseGenerator> bit_pulses;
  std::vector<dsp::PulseGenerator> light_pulses;
  LogicMode grid_logic_modes[8] = {LogicMode::AND};

  bool rtl = false;

  TapeMachineModule()
  {
    config(Params::NUM_PARAMS, Inputs::NUM_INPUTS, Outputs::NUM_OUTPUTS, Lights::NUM_LIGHTS);
    configParam(Params::PROBABILITY_PARAM, 0, 1, 0.5, "probability", "%", 0, 100);
    getParamQuantity(Params::PROBABILITY_PARAM)->description = "probability of a bit being toggled on each clock pulse.";
    configParam(Params::CLEAR_PARAM, 0, 1, 0, "clear");
    getParamQuantity(Params::CLEAR_PARAM)->description = "clears first bit on each clock pulse while held.";
    configParam(Params::SET_PARAM, 0, 1, 0, "set");
    getParamQuantity(Params::SET_PARAM)->description = "sets first bit on each clock pulse while held.";
    configParam(Params::SHIFT_PARAM, 1, 15, 1, "shift", " bit(s)");
    getParamQuantity(Params::SHIFT_PARAM)->description = "how many bits to shift with each clock pulse. (1-15 bits)";
    getParamQuantity(Params::SHIFT_PARAM)->snapEnabled = true;
    configSwitch(Params::DUAL_PARAM, 0, 1, 0, "dual mode", {"single", "dual"});
    configInput(Inputs::DUAL_INPUT, "dual mode");
    getInputInfo(Inputs::DUAL_INPUT)->description = "toggle between single and dual mode. expects 0-10V gate signal.";
    configInput(Inputs::CLOCK_INPUT, "clock");
    configInput(Inputs::CLEAR_INPUT, "clear");
    getInputInfo(Inputs::CLEAR_INPUT)->description = "clears first bit on each clock pulse while input gate is high. expects 0-10V.";
    configInput(Inputs::SET_INPUT, "set");
    getInputInfo(Inputs::SET_INPUT)->description = "sets first bit on each clock pulse while input gate is high. expects 0-10V.";
    configInput(Inputs::SHIFT_INPUT, "shift");
    getInputInfo(Inputs::SHIFT_INPUT)->description = "how many bits to shift with each clock pulse. expects 0-10V (1-15 bits).";
    configOutput(Outputs::VOLTAGE_OUTPUT, "voltage");
    getOutputInfo(Outputs::VOLTAGE_OUTPUT)->description = "default range +/- 1V. adjust in context menu.";
    configOutput(Outputs::FLIPPED_OUTPUT, "flipped");
    getOutputInfo(Outputs::FLIPPED_OUTPUT)->description = "default range +/- 1V. adjust in context menu.";
    configOutput(Outputs::MIN_OUTPUT, "minimum");
    getOutputInfo(Outputs::MIN_OUTPUT)->description = "default range +/- 1V. adjust in context menu.";
    configOutput(Outputs::MAX_OUTPUT, "maximum");
    getOutputInfo(Outputs::MAX_OUTPUT)->description = "default range +/- 1V. adjust in context menu.";
    configOutput(Outputs::RANDOM_PULSE_OUTPUT, "random pulse");
    getOutputInfo(Outputs::RANDOM_PULSE_OUTPUT)->description = "outputs pulse signal (set mode in context menu) when a bit is toggled.";
    configSwitch(Params::DIR_PARAM, 0, 1, 0, "direction", {"left-to-right", "right-to-left"});
    getParamQuantity(Params::DIR_PARAM)->description = "direction to shift bits.";
    configInput(Inputs::DIR_INPUT, "direction");
    getInputInfo(Inputs::DIR_INPUT)->description = "toggle direction to shift bits between left-to-right and right-to-left. expects 0-10V gate signal.";
    for (int i = 0; i < 8; i++)
    {
      configSwitch(Params::GRID_LOGIC_PARAM + i, 0, 2, 0, "column " + std::to_string(i + 1) + " logic", {"AND", "OR", "XOR"});
      configOutput(Outputs::GRID_LOGIC_OUTPUT + i, "column " + std::to_string(i + 1) + " logic");
    }
    for (int i = 0; i < 16; i++)
    {
      configOutput(Outputs::PULSE_OUTPUT + i, "bit 2^" + std::to_string(i));
      bit_pulses.push_back(dsp::PulseGenerator());
      light_pulses.push_back(dsp::PulseGenerator());
    }
  }

  void onReset() override
  {
    tape = 0;
    tapeA = 0;
    tapeB = 0;
    last_dual = dual;

    voltage_range.cv_a = -1;
    voltage_range.cv_b = 1;
    voltage_range.updateInternal();

    flipped_voltage_range.cv_a = -1;
    flipped_voltage_range.cv_b = 1;
    flipped_voltage_range.updateInternal();

    min_voltage_range.cv_a = -1;
    min_voltage_range.cv_b = 1;
    min_voltage_range.updateInternal();

    max_voltage_range.cv_a = -1;
    max_voltage_range.cv_b = 1;
    max_voltage_range.updateInternal();

    for (int i = 0; i < 16; i++)
    {
      bit_pulses[i].reset();
      light_pulses[i].reset();
      if (i < 8)
      {
        params[Params::GRID_LOGIC_PARAM + i].setValue(LogicMode::AND);
        outputs[Outputs::GRID_LOGIC_OUTPUT + i].setVoltage(0.f);
        lights[Lights::GRID_LOGIC_LIGHT + i].setBrightness(0.f);
      }
    }
  }

  json_t *dataToJson() override
  {
    json_t *rootJ = json_object();
    json_object_set_new(rootJ, "bit_pulse_mode", json_integer(bit_pulse_mode));
    json_object_set_new(rootJ, "random_pulse_mode", json_integer(random_pulse_mode));
    json_object_set_new(rootJ, "voltage_range", voltage_range.dataToJson());
    json_object_set_new(rootJ, "flipped_voltage_range", flipped_voltage_range.dataToJson());
    json_object_set_new(rootJ, "min_voltage_range", min_voltage_range.dataToJson());
    json_object_set_new(rootJ, "max_voltage_range", max_voltage_range.dataToJson());
    json_object_set_new(rootJ, "dual_mode", json_boolean(dual));
    json_object_set_new(rootJ, "tape", json_integer(tape));
    json_object_set_new(rootJ, "tapeA", json_integer(tapeA));
    json_object_set_new(rootJ, "tapeB", json_integer(tapeB));
    json_object_set_new(rootJ, "rtl", json_boolean(rtl));
    json_object_set_new(rootJ, "last_dual", json_boolean(last_dual));
    json_object_set_new(rootJ, "bit_toggled", json_boolean(bit_toggled));
    json_object_set_new(rootJ, "prob", json_real(prob));
    json_object_set_new(rootJ, "noise_a", json_real(noise_a));
    json_object_set_new(rootJ, "noise_b", json_real(noise_b));
    json_object_set_new(rootJ, "shift_amt", json_integer(shift_amt));
    for (int i = 0; i < 8; i++)
    {
      std::string logicModeXJ = "grid_logic_mode_" + std::to_string(i);
      json_object_set_new(rootJ, logicModeXJ.c_str(), json_integer(static_cast<int>(params[Params::GRID_LOGIC_PARAM + i].getValue())));
    }
    return rootJ;
  }

  void dataFromJson(json_t *rootJ) override
  {
    json_t *bitModeJ = json_object_get(rootJ, "bit_pulse_mode");
    if (bitModeJ)
    {
      bit_pulse_mode = json_integer_value(bitModeJ);
    }
    json_t *randomModeJ = json_object_get(rootJ, "random_pulse_mode");
    if (randomModeJ)
    {
      random_pulse_mode = json_integer_value(randomModeJ);
    }
    json_t *vRangeJ = json_object_get(rootJ, "voltage_range");
    if (vRangeJ)
    {
      voltage_range.dataFromJson(vRangeJ);
    }
    json_t *fRangeJ = json_object_get(rootJ, "flipped_voltage_range");
    if (fRangeJ)
    {
      flipped_voltage_range.dataFromJson(fRangeJ);
    }
    json_t *minRangeJ = json_object_get(rootJ, "min_voltage_range");
    if (minRangeJ)
    {
      min_voltage_range.dataFromJson(minRangeJ);
    }
    json_t *maxRangeJ = json_object_get(rootJ, "max_voltage_range");
    if (maxRangeJ)
    {
      max_voltage_range.dataFromJson(maxRangeJ);
    }
    json_t *dualModeJ = json_object_get(rootJ, "dual_mode");
    if (dualModeJ)
    {
      dual = json_is_true(dualModeJ);
    }
    json_t *tapeJ = json_object_get(rootJ, "tape");
    if (tapeJ)
    {
      tape = json_integer_value(tapeJ);
    }
    json_t *tapeAJ = json_object_get(rootJ, "tapeA");
    if (tapeAJ)
    {
      tapeA = json_integer_value(tapeAJ);
    }
    json_t *tapeBJ = json_object_get(rootJ, "tapeB");
    if (tapeBJ)
    {
      tapeB = json_integer_value(tapeBJ);
    }
    json_t *rtlJ = json_object_get(rootJ, "rtl");
    if (rtlJ)
    {
      rtl = json_is_true(rtlJ);
    }
    json_t *lastDualJ = json_object_get(rootJ, "last_dual");
    if (lastDualJ)
    {
      last_dual = json_is_true(lastDualJ);
    }
    json_t *bitToggledJ = json_object_get(rootJ, "bit_toggled");
    if (bitToggledJ)
    {
      bit_toggled = json_is_true(bitToggledJ);
    }
    json_t *probJ = json_object_get(rootJ, "prob");
    if (probJ)
    {
      prob = json_real_value(probJ);
    }
    json_t *noiseAJ = json_object_get(rootJ, "noise_a");
    if (noiseAJ)
    {
      noise_a = json_real_value(noiseAJ);
    }
    json_t *noiseBJ = json_object_get(rootJ, "noise_b");
    if (noiseBJ)
    {
      noise_b = json_real_value(noiseBJ);
    }
    json_t *shiftAmtJ = json_object_get(rootJ, "shift_amt");
    if (shiftAmtJ)
    {
      shift_amt = json_integer_value(shiftAmtJ);
      if (shift_amt < 1) shift_amt = 1;
      if (shift_amt > 15) shift_amt = 15;
    }
    for (int i = 0; i < 8; i++)
    {
      std::string logicModeX = "grid_logic_mode_" + std::to_string(i);
      json_t *logicModeJ = json_object_get(rootJ, logicModeX.c_str());
      if (logicModeJ)
      {
        int modeValue = json_integer_value(logicModeJ);
        if (modeValue >= 0 && modeValue <= 2) {
          params[Params::GRID_LOGIC_PARAM + i].setValue(static_cast<float>(modeValue));
        }
      }
    }
  }

  size_t getBitMode()
  {
    return bit_pulse_mode;
  }

  void setBitMode(size_t mode)
  {
    bit_pulse_mode = mode;
  }

  size_t getRandomMode()
  {
    return random_pulse_mode;
  }

  void setRandomMode(size_t mode)
  {
    random_pulse_mode = mode;
  }

  const int PARAM_INTERVAL = 64;
  int check_params = 0;
  void processParams()
  {
    prob = params[PROBABILITY_PARAM].getValue();
    clear = params[CLEAR_PARAM].getValue();
    set = params[SET_PARAM].getValue();
    shift_amt = params[SHIFT_PARAM].getValue();
    rtl = params[DIR_PARAM].getValue();
    dual = params[DUAL_PARAM].getValue() > 0.f;

    for (int i = 0; i < 8; i++)
    {
      int mode = (int)params[GRID_LOGIC_PARAM + i].getValue();
      if (mode < 0 || mode > 2) mode = LogicMode::AND; 
      switch (mode)
      {
      case LogicMode::AND:
        grid_logic_modes[i] = LogicMode::AND;
        break;
      case LogicMode::OR:
        grid_logic_modes[i] = LogicMode::OR;
        break;
      case LogicMode::XOR:
        grid_logic_modes[i] = LogicMode::XOR;
        break;
      }
    }

    if (!dual)
    {
      for (int i = 0; i < 16; i++)
      {
        configOutput(Outputs::PULSE_OUTPUT + i, "bit 2^" + std::to_string(i));
      }
    }
    else
    {
      for (int i = 0; i < 8; i++)
      {
        configOutput(Outputs::PULSE_OUTPUT + i, "bit 2^" + std::to_string(i) + " (A)");
      }
      for (int i = 8; i < 16; i++)
      {
        configOutput(Outputs::PULSE_OUTPUT + i, "bit 2^" + std::to_string(i - 8) + " (B)");
      }
    }
  }

  uint16_t rotl(uint16_t value, int shift, int bits = 16)
  {
    shift = shift % bits;
    return ((value << shift) | (value >> (bits - shift))) & ((1u << bits) - 1);
  }

  uint16_t rotr(uint16_t value, int shift, int bits = 16)
  {
    shift = shift % bits;
    return ((value >> shift) | (value << (bits - shift))) & ((1u << bits) - 1);
  }

  void updateDualFromSingle() {
    tapeA = (tape >> 8) & 0xFF;
    tapeB = tape & 0xFF;
  }
  void updateSingleFromDual() {
    tape = ((uint16_t)tapeA << 8) | tapeB;
  }

  void process(const ProcessArgs &args) override
  {
    if (++check_params > PARAM_INTERVAL)
    {
      check_params = 0;
      processParams();
    }
    
    if (dual != last_dual) {
      if (dual) {
        updateDualFromSingle();
      } else {
        updateSingleFromDual();
      }
      last_dual = dual;
    }

    if (inputs[SHIFT_INPUT].isConnected())
    {
      shift_amt = (int)((inputs[SHIFT_INPUT].getVoltage() / 10.f) * 15.f);
    }

    if (inputs[DIR_INPUT].isConnected())
    {
      if (dir_trigger.process(inputs[DIR_INPUT].getVoltage()))
      {
        rtl = !rtl;
        getParamQuantity(DIR_PARAM)->setValue(rtl);
      }
    }

    if (inputs[CLEAR_INPUT].getVoltage() > 5.f)
    {
      clear = true;
    }

    if (inputs[SET_INPUT].getVoltage() > 5.f)
    {
      set = true;
    }

    noise_a = random::uniform();
    noise_b = random::uniform();

    float clock_input = inputs[CLOCK_INPUT].getVoltage();
    bool new_clock = clock.process(clock_input);

    if (new_clock)
    {
      if (dual && !last_dual)
      {
        
        tapeA = tape & 0xFF;
        tapeB = (tape >> 8) & 0xFF;
      }
      else if (!dual && last_dual)
      {
        
        tape = (tapeA & 0xFF) | ((tapeB & 0xFF) << 8);
      }

      last_dual = dual; 

      if (dual) {
        
        if (rtl) {
          tapeA = (tapeA << shift_amt) | (tapeA >> (8 - shift_amt));
        } else {
          tapeA = (tapeA >> shift_amt) | (tapeA << (8 - shift_amt));
        }
        
        if (rtl) {
          tapeB = (tapeB << shift_amt) | (tapeB >> (8 - shift_amt));
        } else {
          tapeB = (tapeB >> shift_amt) | (tapeB << (8 - shift_amt));
        }
        
        if (dual) {
          bool toggledA = false, toggledB = false;
          if (noise_a <= prob) {
            if (rtl)
              tapeA ^= 0x01; 
            else
              tapeA ^= 0x80; 
            toggledA = true;
          }
          if (noise_b <= prob) {
            if (rtl)
              tapeB ^= 0x01; 
            else
              tapeB ^= 0x80; 
            toggledB = true;
          }
          bit_toggled = toggledA || toggledB;
          if (bit_toggled && random_pulse_mode == 0)
            random_pulse.trigger(0.01f);
        } else {
          if (noise_a <= prob) {
            if (rtl)
              tapeA ^= 0x01, tapeB ^= 0x01;
            else
              tapeA ^= 0x80, tapeB ^= 0x80;
            bit_toggled = true;
            if (random_pulse_mode == 0)
              random_pulse.trigger(0.01f);
          } else {
            bit_toggled = false;
          }
        }
        
        if (clear) {
          for (int i = 0; i < shift_amt; i++) {
            tapeA &= ~(rtl ? (1 << i) : (0x80 >> i));
            tapeB &= ~(rtl ? (1 << i) : (0x80 >> i));
          }
        }
        if (set) {
          for (int i = 0; i < shift_amt; i++) {
            tapeA |= (rtl ? (1 << i) : (0x80 >> i));
            tapeB |= (rtl ? (1 << i) : (0x80 >> i));
          }
        }
        
        updateSingleFromDual();
      } else {
        if (rtl)
        {
          tape = rotl(tape, shift_amt);
        }
        else
        {
          tape = rotr(tape, shift_amt);
        }
        if (noise_a <= prob)
        {
          if (rtl)
          {
            tape ^= mask[0];
          }
          else
          {
            tape ^= mask[15];
          }
          bit_toggled = true;
          if (random_pulse_mode == 0)
          {
            random_pulse.trigger(0.01f);
          }
        }
        else
        {
          bit_toggled = false;
        }

        if (clear)
        {
          for (int i = 0; i < shift_amt; i++)
          {
            tape &= (~mask[15 << i]);
          }
        }

        if (set)
        {
          for (int i = 0; i < shift_amt; i++)
          {
            tape |= mask[15 << i];
          }
        }
      }
    }

    lights[CLEAR_LIGHT].setBrightness(clear ? 1.0f : 0.0f);
    lights[SET_LIGHT].setBrightness(set ? 1.0f : 0.0f);

    uint16_t flipped_tape = (~tape);
    float voltage = voltage_range.map(tape / 65535.f);
    float flipped_voltage = flipped_voltage_range.map(flipped_tape / 65535.f);

    outputs[VOLTAGE_OUTPUT].setVoltage(voltage);
    outputs[FLIPPED_OUTPUT].setVoltage(flipped_voltage);

    float min_voltage = flipped_tape ^ ((tape ^ flipped_tape) & -(tape < flipped_tape));
    min_voltage = min_voltage_range.map(min_voltage / 65535.f);
    float max_voltage = tape ^ ((tape ^ flipped_tape) & -(tape < flipped_tape));
    max_voltage = max_voltage_range.map(max_voltage / 65535.f);

    outputs[MIN_OUTPUT].setVoltage(min_voltage);
    outputs[MAX_OUTPUT].setVoltage(max_voltage);

    switch (bit_pulse_mode)
    {
    case 0: 
      for (int i = 0; i < 16; i++)
      {
        if (new_clock && (tape & mask[i]))
        {
          bit_pulses[i].trigger(0.01f);
          light_pulses[i].trigger(0.05f);
        }
        bool bp = bit_pulses[i].process(args.sampleTime);
        bool lp = light_pulses[i].process(args.sampleTime);
        outputs[PULSE_OUTPUT + i].setVoltage(bp ? 10.f : 0.f);
        lights[BIT_LIGHT + i].setBrightness(((tape & mask[i]) && lp) ? 1.f : 0.f);
      }
      break;
    case 1: 
      for (int i = 0; i < 16; i++)
      {
        outputs[PULSE_OUTPUT + i].setVoltage((tape & mask[i]) ? clock_input : 0.f);
        lights[BIT_LIGHT + i].setBrightness(((tape & mask[i]) && clock_input > 0.5f) ? 1.f : 0.f);
      }
      break;
    case 2: 
      for (int i = 0; i < 16; i++)
      {
        outputs[PULSE_OUTPUT + i].setVoltage((tape & mask[i]) ? 10.f : 0.f);
        lights[BIT_LIGHT + i].setBrightness((tape & mask[i]) ? 1.f : 0.f);
      }
      break;
    default: 
      for (int i = 0; i < 16; i++)
      {
        outputs[PULSE_OUTPUT + i].setVoltage((tape & mask[i]) ? clock_input : 0.f);
        lights[BIT_LIGHT + i].setBrightness(((tape & mask[i]) && clock_input > 0.5f) ? 1.f : 0.f);
      }
      break;
    }

    switch (random_pulse_mode)
    {
    case 0: 
      outputs[RANDOM_PULSE_OUTPUT].setVoltage(random_pulse.process(args.sampleTime) ? 10.f : 0.f);
      lights[RANDOM_PULSE_LIGHT].setBrightness(random_pulse.process(args.sampleTime) ? 1.f : 0.f);
      break;
    case 1: 
      outputs[RANDOM_PULSE_OUTPUT].setVoltage(bit_toggled ? clock_input : 0.f);
      lights[RANDOM_PULSE_LIGHT].setBrightness((bit_toggled && clock_input > 0.5f) ? 1.f : 0.f);
      break;
    case 2: 
      outputs[RANDOM_PULSE_OUTPUT].setVoltage(bit_toggled ? 10.f : 0.f);
      lights[RANDOM_PULSE_LIGHT].setBrightness(bit_toggled ? 1.f : 0.f);
      break;
    default: 
      outputs[RANDOM_PULSE_OUTPUT].setVoltage(bit_toggled ? clock_input : 0.f);
      lights[RANDOM_PULSE_LIGHT].setBrightness((bit_toggled && clock_input > 0.5f) ? 1.f : 0.f);
      break;
    }

    
    for (int i = 0; i < 8; i++) {
        
        bool bitA, bitB;
        if (dual) {
            bitA = (tapeA >> (7 - i)) & 0x1; 
            bitB = (tapeB >> (7 - i)) & 0x1; 
        } else {
            bitA = (tape >> (15 - i)) & 0x1; 
            bitB = (tape >> (7 - i)) & 0x1;  
        }

        bool logic_result = false;
        switch (grid_logic_modes[i]) {
            case LogicMode::AND: logic_result = bitA && bitB; break;
            case LogicMode::OR:  logic_result = bitA || bitB; break;
            case LogicMode::XOR: logic_result = bitA != bitB; break;
        }

        
        float out = 0.f;
        switch (bit_pulse_mode) {
            case 0: 
                
                if (logic_result && new_clock) {
                    bit_pulses[16 + i].trigger(0.01f); 
                }
                out = bit_pulses[16 + i].process(args.sampleTime) ? 10.f : 0.f;
                break;
            case 1: 
                out = logic_result ? clock_input : 0.f;
                break;
            case 2: 
                out = logic_result ? 10.f : 0.f;
                break;
            default:
                out = logic_result ? clock_input : 0.f;
                break;
        }
        outputs[GRID_LOGIC_OUTPUT + i].setVoltage(out);
        lights[GRID_LOGIC_LIGHT + i].setBrightness(out);
    }
  }
};

struct TapeMachineModuleWidget : ModuleWidget
{
  TapeMachineModuleWidget(TapeMachineModule *module)
  {
    setModule(module);
    SvgPanel *panel = createPanel(asset::plugin(pluginInstance, "res/tape-machine-v2.svg"));
    setPanel(panel);

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<TL1105>(mm2px(Vec(25.625, 25.412)), module, TapeMachineModule::SET_PARAM));
		addParam(createParamCentered<TL1105>(mm2px(Vec(35.904, 25.412)), module, TapeMachineModule::CLEAR_PARAM));
		addParam(createParamCentered<SmallBitKnob>(mm2px(Vec(75.148, 25.412)), module, TapeMachineModule::SHIFT_PARAM));
		addParam(createParamCentered<HugeBitKnob>(mm2px(Vec(55.88, 28.625)), module, TapeMachineModule::PROBABILITY_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(68.148, 45.858)), module, TapeMachineModule::DIR_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(79.101, 45.858)), module, TapeMachineModule::DUAL_PARAM));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(13.436, 105.151)), module, TapeMachineModule::GRID_LOGIC_PARAM + 0));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(25.483, 105.151)), module, TapeMachineModule::GRID_LOGIC_PARAM + 1));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(37.529, 105.151)), module, TapeMachineModule::GRID_LOGIC_PARAM + 2));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(49.576, 105.151)), module, TapeMachineModule::GRID_LOGIC_PARAM + 3));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(61.622, 105.151)), module, TapeMachineModule::GRID_LOGIC_PARAM + 4));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(73.669, 105.151)), module, TapeMachineModule::GRID_LOGIC_PARAM + 5));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(85.715, 105.151)), module, TapeMachineModule::GRID_LOGIC_PARAM + 6));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(97.762, 105.151)), module, TapeMachineModule::GRID_LOGIC_PARAM + 7));

		addInput(createInputCentered<BitPort>(mm2px(Vec(25.625, 32.838)), module, TapeMachineModule::SET_INPUT));
		addInput(createInputCentered<BitPort>(mm2px(Vec(35.904, 32.838)), module, TapeMachineModule::CLEAR_INPUT));
		addInput(createInputCentered<BitPort>(mm2px(Vec(75.148, 32.838)), module, TapeMachineModule::SHIFT_INPUT));
		addInput(createInputCentered<BitPort>(mm2px(Vec(37.625, 48.925)), module, TapeMachineModule::CLOCK_INPUT));
		addInput(createInputCentered<BitPort>(mm2px(Vec(68.148, 54.62)), module, TapeMachineModule::DIR_INPUT));
		addInput(createInputCentered<BitPort>(mm2px(Vec(79.101, 54.62)), module, TapeMachineModule::DUAL_INPUT));

		addOutput(createOutputCentered<BitPort>(mm2px(Vec(31.253, 70.539)), module, TapeMachineModule::VOLTAGE_OUTPUT));
		addOutput(createOutputCentered<BitPort>(mm2px(Vec(43.3, 70.539)), module, TapeMachineModule::FLIPPED_OUTPUT));
		addOutput(createOutputCentered<BitPort>(mm2px(Vec(55.346, 70.539)), module, TapeMachineModule::MIN_OUTPUT));
		addOutput(createOutputCentered<BitPort>(mm2px(Vec(67.393, 70.539)), module, TapeMachineModule::MAX_OUTPUT));
		addOutput(createOutputCentered<BitPort>(mm2px(Vec(79.439, 70.539)), module, TapeMachineModule::RANDOM_PULSE_OUTPUT));
		addOutput(createOutputCentered<BitPort>(mm2px(Vec(13.436, 82.807)), module, TapeMachineModule::PULSE_OUTPUT + 15));
		addOutput(createOutputCentered<BitPort>(mm2px(Vec(25.483, 82.807)), module, TapeMachineModule::PULSE_OUTPUT + 14));
		addOutput(createOutputCentered<BitPort>(mm2px(Vec(37.529, 82.807)), module, TapeMachineModule::PULSE_OUTPUT + 13));
		addOutput(createOutputCentered<BitPort>(mm2px(Vec(49.576, 82.807)), module, TapeMachineModule::PULSE_OUTPUT + 12));
		addOutput(createOutputCentered<BitPort>(mm2px(Vec(61.622, 82.807)), module, TapeMachineModule::PULSE_OUTPUT + 11));
		addOutput(createOutputCentered<BitPort>(mm2px(Vec(73.669, 82.807)), module, TapeMachineModule::PULSE_OUTPUT + 10));
		addOutput(createOutputCentered<BitPort>(mm2px(Vec(85.715, 82.807)), module, TapeMachineModule::PULSE_OUTPUT + 9));
		addOutput(createOutputCentered<BitPort>(mm2px(Vec(97.762, 82.807)), module, TapeMachineModule::PULSE_OUTPUT + 8));
		addOutput(createOutputCentered<BitPort>(mm2px(Vec(13.436, 94.198)), module, TapeMachineModule::PULSE_OUTPUT + 7));
		addOutput(createOutputCentered<BitPort>(mm2px(Vec(25.483, 94.198)), module, TapeMachineModule::PULSE_OUTPUT + 6));
		addOutput(createOutputCentered<BitPort>(mm2px(Vec(37.529, 94.198)), module, TapeMachineModule::PULSE_OUTPUT + 5));
		addOutput(createOutputCentered<BitPort>(mm2px(Vec(49.576, 94.198)), module, TapeMachineModule::PULSE_OUTPUT + 4));
		addOutput(createOutputCentered<BitPort>(mm2px(Vec(61.622, 94.198)), module, TapeMachineModule::PULSE_OUTPUT + 3));
		addOutput(createOutputCentered<BitPort>(mm2px(Vec(73.669, 94.198)), module, TapeMachineModule::PULSE_OUTPUT + 2));
		addOutput(createOutputCentered<BitPort>(mm2px(Vec(85.715, 94.198)), module, TapeMachineModule::PULSE_OUTPUT + 1));
		addOutput(createOutputCentered<BitPort>(mm2px(Vec(97.762, 94.198)), module, TapeMachineModule::PULSE_OUTPUT + 0));
		addOutput(createOutputCentered<BitPort>(mm2px(Vec(13.436, 116.543)), module, TapeMachineModule::GRID_LOGIC_OUTPUT + 0));
		addOutput(createOutputCentered<BitPort>(mm2px(Vec(25.483, 116.543)), module, TapeMachineModule::GRID_LOGIC_OUTPUT + 1));
		addOutput(createOutputCentered<BitPort>(mm2px(Vec(37.529, 116.543)), module, TapeMachineModule::GRID_LOGIC_OUTPUT + 2));
		addOutput(createOutputCentered<BitPort>(mm2px(Vec(49.576, 116.543)), module, TapeMachineModule::GRID_LOGIC_OUTPUT + 3));
		addOutput(createOutputCentered<BitPort>(mm2px(Vec(61.622, 116.543)), module, TapeMachineModule::GRID_LOGIC_OUTPUT + 4));
		addOutput(createOutputCentered<BitPort>(mm2px(Vec(73.669, 116.543)), module, TapeMachineModule::GRID_LOGIC_OUTPUT + 5));
		addOutput(createOutputCentered<BitPort>(mm2px(Vec(85.715, 116.543)), module, TapeMachineModule::GRID_LOGIC_OUTPUT + 6));
		addOutput(createOutputCentered<BitPort>(mm2px(Vec(97.762, 116.543)), module, TapeMachineModule::GRID_LOGIC_OUTPUT + 7));

		addChild(createLightCentered<MediumSimpleLight<RedLight>>(mm2px(Vec(25.625, 32.838)), module, TapeMachineModule::SET_LIGHT));
		addChild(createLightCentered<MediumSimpleLight<RedLight>>(mm2px(Vec(35.904, 32.838)), module, TapeMachineModule::CLEAR_LIGHT));
		addChild(createLightCentered<MediumSimpleLight<RedLight>>(mm2px(Vec(79.439, 70.539)), module, TapeMachineModule::RANDOM_PULSE_LIGHT));
		addChild(createLightCentered<MediumSimpleLight<RedLight>>(mm2px(Vec(13.436, 82.807)), module, TapeMachineModule::BIT_LIGHT + 15));
		addChild(createLightCentered<MediumSimpleLight<RedLight>>(mm2px(Vec(25.483, 82.807)), module, TapeMachineModule::BIT_LIGHT + 14));
		addChild(createLightCentered<MediumSimpleLight<RedLight>>(mm2px(Vec(37.529, 82.807)), module, TapeMachineModule::BIT_LIGHT + 13));
		addChild(createLightCentered<MediumSimpleLight<RedLight>>(mm2px(Vec(49.576, 82.807)), module, TapeMachineModule::BIT_LIGHT + 12));
		addChild(createLightCentered<MediumSimpleLight<RedLight>>(mm2px(Vec(61.622, 82.807)), module, TapeMachineModule::BIT_LIGHT + 11));
		addChild(createLightCentered<MediumSimpleLight<RedLight>>(mm2px(Vec(73.669, 82.807)), module, TapeMachineModule::BIT_LIGHT + 10));
		addChild(createLightCentered<MediumSimpleLight<RedLight>>(mm2px(Vec(85.715, 82.807)), module, TapeMachineModule::BIT_LIGHT + 9));
		addChild(createLightCentered<MediumSimpleLight<RedLight>>(mm2px(Vec(97.762, 82.807)), module, TapeMachineModule::BIT_LIGHT + 8));
		addChild(createLightCentered<MediumSimpleLight<RedLight>>(mm2px(Vec(13.436, 94.198)), module, TapeMachineModule::BIT_LIGHT + 7));
		addChild(createLightCentered<MediumSimpleLight<RedLight>>(mm2px(Vec(25.483, 94.198)), module, TapeMachineModule::BIT_LIGHT + 6));
		addChild(createLightCentered<MediumSimpleLight<RedLight>>(mm2px(Vec(37.529, 94.198)), module, TapeMachineModule::BIT_LIGHT + 5));
		addChild(createLightCentered<MediumSimpleLight<RedLight>>(mm2px(Vec(49.576, 94.198)), module, TapeMachineModule::BIT_LIGHT + 4));
		addChild(createLightCentered<MediumSimpleLight<RedLight>>(mm2px(Vec(61.622, 94.198)), module, TapeMachineModule::BIT_LIGHT + 3));
		addChild(createLightCentered<MediumSimpleLight<RedLight>>(mm2px(Vec(73.669, 94.198)), module, TapeMachineModule::BIT_LIGHT + 2));
		addChild(createLightCentered<MediumSimpleLight<RedLight>>(mm2px(Vec(85.715, 94.198)), module, TapeMachineModule::BIT_LIGHT + 1));
		addChild(createLightCentered<MediumSimpleLight<RedLight>>(mm2px(Vec(97.762, 94.198)), module, TapeMachineModule::BIT_LIGHT + 0));
		addChild(createLightCentered<MediumSimpleLight<RedLight>>(mm2px(Vec(13.436, 116.543)), module, TapeMachineModule::GRID_LOGIC_LIGHT + 0));
		addChild(createLightCentered<MediumSimpleLight<RedLight>>(mm2px(Vec(25.483, 116.543)), module, TapeMachineModule::GRID_LOGIC_LIGHT + 1));
		addChild(createLightCentered<MediumSimpleLight<RedLight>>(mm2px(Vec(37.529, 116.543)), module, TapeMachineModule::GRID_LOGIC_LIGHT + 2));
		addChild(createLightCentered<MediumSimpleLight<RedLight>>(mm2px(Vec(49.576, 116.543)), module, TapeMachineModule::GRID_LOGIC_LIGHT + 3));
		addChild(createLightCentered<MediumSimpleLight<RedLight>>(mm2px(Vec(61.622, 116.543)), module, TapeMachineModule::GRID_LOGIC_LIGHT + 4));
		addChild(createLightCentered<MediumSimpleLight<RedLight>>(mm2px(Vec(73.669, 116.543)), module, TapeMachineModule::GRID_LOGIC_LIGHT + 5));
		addChild(createLightCentered<MediumSimpleLight<RedLight>>(mm2px(Vec(85.715, 116.543)), module, TapeMachineModule::GRID_LOGIC_LIGHT + 6));
		addChild(createLightCentered<MediumSimpleLight<RedLight>>(mm2px(Vec(97.762, 116.543)), module, TapeMachineModule::GRID_LOGIC_LIGHT + 7));
  }

  void appendContextMenu(Menu *menu) override
  {
    TapeMachineModule *module = dynamic_cast<TapeMachineModule *>(this->module);
    assert(module);

    menu->addChild(new MenuSeparator());
    menu->addChild(createIndexSubmenuItem("bit pulse mode", module->mode_labels, [=]
                                          { return module->getBitMode(); }, [=](size_t mode)
                                          { module->setBitMode(mode); }));
    menu->addChild(createIndexSubmenuItem("random pulse mode", module->mode_labels, [=]
                                          { return module->getRandomMode(); }, [=](size_t mode)
                                          { module->setRandomMode(mode); }));
    menu->addChild(new MenuSeparator());
    module->voltage_range.addMenu(module, menu, "voltage range");
    module->flipped_voltage_range.addMenu(module, menu, "flipped voltage range");
    module->min_voltage_range.addMenu(module, menu, "min voltage range");
    module->max_voltage_range.addMenu(module, menu, "max voltage range");
  }
};

Model *modelTapemachine = createModel<TapeMachineModule, TapeMachineModuleWidget>("tape-machine");
