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
      NUM_PARAMS
   };
   enum Inputs
   {
      CLOCK_INPUT,
      CLEAR_INPUT,
      SET_INPUT,
      SHIFT_INPUT,
      NUM_INPUTS
   };
   enum Outputs
   {
      VOLTAGE_OUTPUT,
      FLIPPED_OUTPUT,
      ENUMS(PULSE_OUTPUT, 16),
      MIN_OUTPUT,
      MAX_OUTPUT,
      NUM_OUTPUTS
   };
   enum Lights
   {
      ENUMS(BIT_LIGHT, 16),
      CLEAR_LIGHT,
      SET_LIGHT,
      NUM_LIGHTS
   };

   uint16_t tape = 0b0;
   uint16_t masks[16] = {
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
   float noise = 0.f;
   bool clear = false;
   bool set = false;
   int shift_amt = 1;
   CVRange voltage_range;
   CVRange flipped_voltage_range;
   CVRange min_voltage_range;
   CVRange max_voltage_range;

   // float voltage = 0.f;
   // float flipped_voltage = 0.f;
   // float min_voltage = 0.f;
   // float max_voltage = 0.f;

   dsp::SchmittTrigger clock;

   size_t gate_mode = 1;
   std::vector<std::string> mode_labels = {"trigger", "clock", "hold"};
   std::vector<dsp::PulseGenerator> bit_pulses;
   std::vector<dsp::PulseGenerator> light_pulses;

   bool rtl = false;

   TapeMachineModule()
   {
      config(Params::NUM_PARAMS, Inputs::NUM_INPUTS, Outputs::NUM_OUTPUTS, Lights::NUM_LIGHTS);
      configParam(Params::PROBABILITY_PARAM, 0, 1, 0.5, "probability", "%", 0, 100);
      configParam(Params::CLEAR_PARAM, 0, 1, 0, "clear");
      configParam(Params::SET_PARAM, 0, 1, 0, "set");
      configParam(Params::SHIFT_PARAM, 1, 15, 1, "shift", " bit(s)");
      getParamQuantity(Params::SHIFT_PARAM)->snapEnabled = true;
      configInput(Inputs::CLOCK_INPUT, "clock");
      configInput(Inputs::CLEAR_INPUT, "clear");
      getInputInfo(Inputs::CLEAR_INPUT)->description = "clears first bit while input gate is high. expects 0-10V.";
      configInput(Inputs::SET_INPUT, "set");
      getInputInfo(Inputs::SET_INPUT)->description = "sets first bit while input gate is high. expects 0-10V.";
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
      configSwitch(Params::DIR_PARAM, 0, 1, 0, "direction", {"left-to-right", "right-to-left"});
      for (int i = 0; i < 16; i++)
      {
         configOutput(Outputs::PULSE_OUTPUT + i, "bit 2^" + std::to_string(i));
         bit_pulses.push_back(dsp::PulseGenerator());
         light_pulses.push_back(dsp::PulseGenerator());
      }
   }

   void onReset() override
   {
      tape = 0b0;
      gate_mode = 1;

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
      }
   }

   json_t *dataToJson() override
   {
      json_t *rootJ = json_object();
      json_object_set_new(rootJ, "mode", json_integer(gate_mode));
      json_object_set_new(rootJ, "voltage_range", voltage_range.dataToJson());
      json_object_set_new(rootJ, "flipped_voltage_range", flipped_voltage_range.dataToJson());
      json_object_set_new(rootJ, "min_voltage_range", min_voltage_range.dataToJson());
      json_object_set_new(rootJ, "max_voltage_range", max_voltage_range.dataToJson());
      return rootJ;
   }

   void dataFromJson(json_t *rootJ) override
   {
      json_t *modeJ = json_object_get(rootJ, "mode");
      if (modeJ)
      {
         gate_mode = json_integer_value(modeJ);
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
   }

   size_t getMode()
   {
      return gate_mode;
   }

   void setMode(size_t mode)
   {
      gate_mode = mode;
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
   }

   void process(const ProcessArgs &args) override
   {
      if (++check_params > PARAM_INTERVAL)
      {
         check_params = 0;
         processParams();
      }

      if (inputs[SHIFT_INPUT].isConnected())
      {
         shift_amt = (int)((inputs[SHIFT_INPUT].getVoltage() / 10.f) * 15.f);
      }

      if (params[CLEAR_PARAM].getValue() > 0.f || inputs[CLEAR_INPUT].getVoltage() > 5.f)
      {
         clear = true;
      }
      else
      {
         clear = false;
      }
      if (params[SET_PARAM].getValue() > 0.f || inputs[SET_INPUT].getVoltage() > 5.f)
      {
         set = true;
      }
      else
      {
         set = false;
      }

      noise = random::uniform();

      float clock_input = inputs[CLOCK_INPUT].getVoltage();
      bool new_clock = clock.process(clock_input);

      if (new_clock)
      {
         // tape = (tape >> shift_amt) | (tape << (16 - shift_amt));
         // tape = std::rotr(tape, shift_amt);
         if (rtl)
         {
            tape = std::rotl(tape, shift_amt);
         }
         else
         {
            tape = std::rotr(tape, shift_amt);
         }

         if (noise >= prob)
         {
            if (rtl)
            {
               tape ^= masks[0];
            }
            else
         {
            tape ^= masks[15];
         }
         if (clear)
         {
            // tape &= (~masks[15]);
            for (int i = 0; i < shift_amt; i++)
            {
               tape &= (~masks[15 << i]);
            }
         }
         if (set)
         {
            // tape |= masks[15];
            for (int i = 0; i < shift_amt; i++)
            {
               tape |= masks[15 << i];
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

      // for each individual bit output, on each clock trigger (rising edge), if the bit is set:
      // trigger: output a default pulse from the associated PulseGenerator
      // clock/default: pass through the incoming clock signal
      // hold: hold the outgoing gate state at 10.0f as long as the bit is still set
      switch (gate_mode)
      {
      case 0: // trigger
         for (int i = 0; i < 16; i++)
         {
            if (new_clock && (tape & masks[i]))
            {
               bit_pulses[i].trigger(0.01f);
               light_pulses[i].trigger(0.05f);
            }
            bool bp = bit_pulses[i].process(args.sampleTime);
            bool lp = light_pulses[i].process(args.sampleTime);
            outputs[PULSE_OUTPUT + i].setVoltage(bp ? 10.f : 0.f);
            lights[BIT_LIGHT + i].setBrightness(((tape & masks[i]) && lp) ? 1.f : 0.f);
         }
         break;
      case 1: // clock
         for (int i = 0; i < 16; i++)
         {
            outputs[PULSE_OUTPUT + i].setVoltage((tape & masks[i]) ? clock_input : 0.f);
            lights[BIT_LIGHT + i].setBrightness(((tape & masks[i]) && clock_input > 0.5f) ? 1.f : 0.f);
         }
         break;
      case 2: // hold
         for (int i = 0; i < 16; i++)
         {
            outputs[PULSE_OUTPUT + i].setVoltage((tape & masks[i]) ? 10.f : 0.f);
            lights[BIT_LIGHT + i].setBrightness((tape & masks[i]) ? 1.f : 0.f);
         }
         break;
      default: // clock (1, default)
         for (int i = 0; i < 16; i++)
         {
            outputs[PULSE_OUTPUT + i].setVoltage((tape & masks[i]) ? clock_input : 0.f);
            lights[BIT_LIGHT + i].setBrightness(((tape & masks[i]) && clock_input > 0.5f) ? 1.f : 0.f);
         }
         break;
      }
   }
};

struct TapeMachineModuleWidget : ModuleWidget
{
   TapeMachineModuleWidget(TapeMachineModule *module)
   {
      setModule(module);
      setPanel(createPanel(asset::plugin(pluginInstance, "res/tape-machine.svg")));

      float dx = RACK_GRID_WIDTH;
      float dy = RACK_GRID_WIDTH;
      float x_start = dx * 3;
      float y_start = dy * 5;
      float x = x_start;
      float y = y_start;

      addParam(createParamCentered<LargeBitKnob>(Vec(x, y), module, TapeMachineModule::PROBABILITY_PARAM));
      x += dx * 4;
      addParam(createParamCentered<SmallBitKnob>(Vec(x, y), module, TapeMachineModule::SHIFT_PARAM));
      x += dx * 2;
      addInput(createInputCentered<BitPort>(Vec(x, y), module, TapeMachineModule::SHIFT_INPUT));
      x += dx * 4;
      addParam(createParamCentered<LEDButton>(Vec(x, y), module, TapeMachineModule::CLEAR_PARAM));
      x += dx * 2;
      addInput(createInputCentered<BitPort>(Vec(x, y), module, TapeMachineModule::CLEAR_INPUT));
      x += dx * 2;
      addChild(createLightCentered<MediumLight<GreenLight>>(Vec(x, y), module, TapeMachineModule::CLEAR_LIGHT));
      x -= dx * 4;
      y += dy * 2;
      addParam(createParamCentered<LEDButton>(Vec(x, y), module, TapeMachineModule::SET_PARAM));
      x += dx * 2;
      addInput(createInputCentered<BitPort>(Vec(x, y), module, TapeMachineModule::SET_INPUT));
      x += dx * 2;
      addChild(createLightCentered<MediumLight<BlueLight>>(Vec(x, y), module, TapeMachineModule::SET_LIGHT));
      x -= dx * 10;
      x -= dx * 4;
      y += dy * 2;
      addInput(createInputCentered<BitPort>(Vec(x, y), module, TapeMachineModule::CLOCK_INPUT));
      x += dx * 2;
      addParam(createParamCentered<CKSS>(Vec(x, y), module, TapeMachineModule::DIR_PARAM));
      x -= dx * 2;
      y += dy * 2;
      addOutput(createOutputCentered<BitPort>(Vec(x, y), module, TapeMachineModule::VOLTAGE_OUTPUT));
      x += dx * 2;
      addOutput(createOutputCentered<BitPort>(Vec(x, y), module, TapeMachineModule::FLIPPED_OUTPUT));
      x += dx * 2;
      addOutput(createOutputCentered<BitPort>(Vec(x, y), module, TapeMachineModule::MIN_OUTPUT));
      x += dx * 2;
      addOutput(createOutputCentered<BitPort>(Vec(x, y), module, TapeMachineModule::MAX_OUTPUT));
      x -= dx * 7;
      y += dy * 4;
      addChild(createLightCentered<MediumLight<RedLight>>(Vec(x, y), module, TapeMachineModule::BIT_LIGHT + 15));
      x += dx * 3;
      addChild(createLightCentered<MediumLight<RedLight>>(Vec(x, y), module, TapeMachineModule::BIT_LIGHT + 14));
      x += dx * 3;
      addChild(createLightCentered<MediumLight<RedLight>>(Vec(x, y), module, TapeMachineModule::BIT_LIGHT + 13));
      x += dx * 3;
      addChild(createLightCentered<MediumLight<RedLight>>(Vec(x, y), module, TapeMachineModule::BIT_LIGHT + 12));
      x += dx * 3;
      addChild(createLightCentered<MediumLight<RedLight>>(Vec(x, y), module, TapeMachineModule::BIT_LIGHT + 11));
      x += dx * 3;
      addChild(createLightCentered<MediumLight<RedLight>>(Vec(x, y), module, TapeMachineModule::BIT_LIGHT + 10));
      x += dx * 3;
      addChild(createLightCentered<MediumLight<RedLight>>(Vec(x, y), module, TapeMachineModule::BIT_LIGHT + 9));
      x += dx * 3;
      addChild(createLightCentered<MediumLight<RedLight>>(Vec(x, y), module, TapeMachineModule::BIT_LIGHT + 8));
      x -= dx * 21;
      y += dy * 1.5;
      addOutput(createOutputCentered<BitPort>(Vec(x, y), module, TapeMachineModule::PULSE_OUTPUT + 15));
      x += dx * 3;
      addOutput(createOutputCentered<BitPort>(Vec(x, y), module, TapeMachineModule::PULSE_OUTPUT + 14));
      x += dx * 3;
      addOutput(createOutputCentered<BitPort>(Vec(x, y), module, TapeMachineModule::PULSE_OUTPUT + 13));
      x += dx * 3;
      addOutput(createOutputCentered<BitPort>(Vec(x, y), module, TapeMachineModule::PULSE_OUTPUT + 12));
      x += dx * 3;
      addOutput(createOutputCentered<BitPort>(Vec(x, y), module, TapeMachineModule::PULSE_OUTPUT + 11));
      x += dx * 3;
      addOutput(createOutputCentered<BitPort>(Vec(x, y), module, TapeMachineModule::PULSE_OUTPUT + 10));
      x += dx * 3;
      addOutput(createOutputCentered<BitPort>(Vec(x, y), module, TapeMachineModule::PULSE_OUTPUT + 9));
      x += dx * 3;
      addOutput(createOutputCentered<BitPort>(Vec(x, y), module, TapeMachineModule::PULSE_OUTPUT + 8));
      x -= dx * 21;
      y += dy * 2;
      addChild(createLightCentered<MediumLight<RedLight>>(Vec(x, y), module, TapeMachineModule::BIT_LIGHT + 7));
      x += dx * 3;
      addChild(createLightCentered<MediumLight<RedLight>>(Vec(x, y), module, TapeMachineModule::BIT_LIGHT + 6));
      x += dx * 3;
      addChild(createLightCentered<MediumLight<RedLight>>(Vec(x, y), module, TapeMachineModule::BIT_LIGHT + 5));
      x += dx * 3;
      addChild(createLightCentered<MediumLight<RedLight>>(Vec(x, y), module, TapeMachineModule::BIT_LIGHT + 4));
      x += dx * 3;
      addChild(createLightCentered<MediumLight<RedLight>>(Vec(x, y), module, TapeMachineModule::BIT_LIGHT + 3));
      x += dx * 3;
      addChild(createLightCentered<MediumLight<RedLight>>(Vec(x, y), module, TapeMachineModule::BIT_LIGHT + 2));
      x += dx * 3;
      addChild(createLightCentered<MediumLight<RedLight>>(Vec(x, y), module, TapeMachineModule::BIT_LIGHT + 1));
      x += dx * 3;
      addChild(createLightCentered<MediumLight<RedLight>>(Vec(x, y), module, TapeMachineModule::BIT_LIGHT + 0));
      x -= dx * 21;
      y += dy * 1.5;
      addOutput(createOutputCentered<BitPort>(Vec(x, y), module, TapeMachineModule::PULSE_OUTPUT + 7));
      x += dx * 3;
      addOutput(createOutputCentered<BitPort>(Vec(x, y), module, TapeMachineModule::PULSE_OUTPUT + 6));
      x += dx * 3;
      addOutput(createOutputCentered<BitPort>(Vec(x, y), module, TapeMachineModule::PULSE_OUTPUT + 5));
      x += dx * 3;
      addOutput(createOutputCentered<BitPort>(Vec(x, y), module, TapeMachineModule::PULSE_OUTPUT + 4));
      x += dx * 3;
      addOutput(createOutputCentered<BitPort>(Vec(x, y), module, TapeMachineModule::PULSE_OUTPUT + 3));
      x += dx * 3;
      addOutput(createOutputCentered<BitPort>(Vec(x, y), module, TapeMachineModule::PULSE_OUTPUT + 2));
      x += dx * 3;
      addOutput(createOutputCentered<BitPort>(Vec(x, y), module, TapeMachineModule::PULSE_OUTPUT + 1));
      x += dx * 3;
      addOutput(createOutputCentered<BitPort>(Vec(x, y), module, TapeMachineModule::PULSE_OUTPUT + 0));
   }

   void appendContextMenu(Menu *menu) override
   {
      TapeMachineModule *module = dynamic_cast<TapeMachineModule *>(this->module);
      assert(module);

      menu->addChild(new MenuSeparator());
      menu->addChild(createIndexSubmenuItem("pulse mode", module->mode_labels, [=]
                                            { return module->getMode(); }, [=](size_t mode)
                                            { module->setMode(mode); }));
      menu->addChild(new MenuSeparator());
      module->voltage_range.addMenu(module, menu, "voltage range");
      module->flipped_voltage_range.addMenu(module, menu, "flipped voltage range");
      module->min_voltage_range.addMenu(module, menu, "min voltage range");
      module->max_voltage_range.addMenu(module, menu, "max voltage range");
   }
};

Model *modelTapemachine = createModel<TapeMachineModule, TapeMachineModuleWidget>("tape-machine");
