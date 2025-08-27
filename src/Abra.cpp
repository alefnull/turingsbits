#include "plugin.hpp"
#include "inc/cvRange.hpp"

struct AbraModule : Module
{
  enum ParamId
  {
    PARAMS_LEN
  };
  enum InputId
  {
    ANALOG_INPUT,
    CLOCK_INPUT,
    INPUTS_LEN
  };
  enum OutputId
  {
    ENUMS(BIT_0_OUTPUT, 8),
    OUTPUTS_LEN
  };
  enum LightId
  {
    ENUMS(BIT_0_LIGHT, 8),
    LIGHTS_LEN
  };
  enum PulseMode
  {
    CLOCK,
    TRIGGER,
    HOLD
  };

  dsp::SchmittTrigger clock_trigger;
  dsp::PulseGenerator bit_pulses[8];
  dsp::PulseGenerator light_pulses[8];
  CVRange input_range;
  PulseMode pulse_mode = CLOCK;
  int bit_value = 0;

  AbraModule() {
    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configInput(ANALOG_INPUT, "Analog signal");
    configInput(CLOCK_INPUT, "Clock");
    for (int i = 0; i < 8; i++) {
      configOutput(BIT_0_OUTPUT + i, "Bit 2^" + std::to_string(i));
    }
    input_range = CVRange(0.f, 10.f);
  }

  void process(const ProcessArgs& args) override {
    float clock_in = inputs[CLOCK_INPUT].getVoltage();
    bool new_clock = clock_trigger.process(clock_in);
    if (new_clock) {
      float analog_in = inputs[ANALOG_INPUT].getVoltage();
      int new_bit_value = (int)std::round((analog_in - input_range.min) / input_range.range * 255.f);
      bit_value = std::max(0, std::min(255, new_bit_value));
      if (pulse_mode == TRIGGER) {
        for (int i = 0; i < 8; i++) {
          if (bit_value & (1 << i)) {
            bit_pulses[i].trigger(1e-3f);
            light_pulses[i].trigger(0.1f);
          }
        }
      }
    }
    for (int i = 0; i < 8; i++) {
      switch (pulse_mode) {
        case CLOCK:
          outputs[BIT_0_OUTPUT + i].setVoltage((bit_value & (1 << i)) && (clock_in > 0.5f) ? 10.f : 0.f);
          lights[BIT_0_LIGHT + i].setBrightness((bit_value & (1 << i)) && (clock_in > 0.5f) ? 1.f : 0.f);
          break;
        case TRIGGER:
          outputs[BIT_0_OUTPUT + i].setVoltage((bit_value & (1 << i)) && (bit_pulses[i].process(args.sampleTime)) ? 10.f : 0.f);
          lights[BIT_0_LIGHT + i].setBrightness((bit_value & (1 << i)) && (light_pulses[i].process(args.sampleTime)) ? 1.f : 0.f);
          break;
        case HOLD:
          outputs[BIT_0_OUTPUT + i].setVoltage((bit_value & (1 << i)) ? 10.f : 0.f);
          lights[BIT_0_LIGHT + i].setBrightness((bit_value & (1 << i)) ? 1.f : 0.f);
          break;
      }
    }
  }

  json_t* dataToJson() override {
    json_t* rootJ = json_object();
    json_object_set_new(rootJ, "input_range", input_range.dataToJson());
    json_object_set_new(rootJ, "pulse_mode", json_integer(pulse_mode));
    return rootJ;
  }
  void dataFromJson(json_t* rootJ) override {
    json_t* rangeJ = json_object_get(rootJ, "input_range");
    if (rangeJ) {
      input_range.dataFromJson(rangeJ);
    } else {
      input_range = CVRange(0.f, 10.f);
    }
    json_t* pulseModeJ = json_object_get(rootJ, "pulse_mode");
    if (pulseModeJ) {
      pulse_mode = (PulseMode)json_integer_value(pulseModeJ);
    } else {
      pulse_mode = CLOCK;
    }
  }
};

struct AbraWidget : ModuleWidget
{
  AbraWidget(AbraModule* module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, "res/abra.svg"), asset::plugin(pluginInstance, "res/abra-dark.svg")));

    addInput(createInputCentered<BitPort>(mm2px(Vec(10.16, 18.014)), module, AbraModule::ANALOG_INPUT));
    addInput(createInputCentered<BitPort>(mm2px(Vec(10.16, 28.691)), module, AbraModule::CLOCK_INPUT));

    addOutput(createOutputCentered<BitPort>(mm2px(Vec(10.16, 43.147)), module, AbraModule::BIT_0_OUTPUT));
    addChild(createLightCentered<MediumSimpleLight<GreenLight>>(mm2px(Vec(10.16, 43.147)), module, AbraModule::BIT_0_LIGHT));
    addOutput(createOutputCentered<BitPort>(mm2px(Vec(10.16, 52.701)), module, AbraModule::BIT_0_OUTPUT + 1));
    addChild(createLightCentered<MediumSimpleLight<GreenLight>>(mm2px(Vec(10.16, 52.701)), module, AbraModule::BIT_0_LIGHT + 1));
    addOutput(createOutputCentered<BitPort>(mm2px(Vec(10.16, 62.256)), module, AbraModule::BIT_0_OUTPUT + 2));
    addChild(createLightCentered<MediumSimpleLight<GreenLight>>(mm2px(Vec(10.16, 62.256)), module, AbraModule::BIT_0_LIGHT + 2));
    addOutput(createOutputCentered<BitPort>(mm2px(Vec(10.16, 71.811)), module, AbraModule::BIT_0_OUTPUT + 3));
    addChild(createLightCentered<MediumSimpleLight<GreenLight>>(mm2px(Vec(10.16, 71.811)), module, AbraModule::BIT_0_LIGHT + 3));
    addOutput(createOutputCentered<BitPort>(mm2px(Vec(10.16, 81.365)), module, AbraModule::BIT_0_OUTPUT + 4));
    addChild(createLightCentered<MediumSimpleLight<GreenLight>>(mm2px(Vec(10.16, 81.365)), module, AbraModule::BIT_0_LIGHT + 4));
    addOutput(createOutputCentered<BitPort>(mm2px(Vec(10.16, 90.92)), module, AbraModule::BIT_0_OUTPUT + 5));
    addChild(createLightCentered<MediumSimpleLight<GreenLight>>(mm2px(Vec(10.16, 90.92)), module, AbraModule::BIT_0_LIGHT + 5));
    addOutput(createOutputCentered<BitPort>(mm2px(Vec(10.16, 100.475)), module, AbraModule::BIT_0_OUTPUT + 6));
    addChild(createLightCentered<MediumSimpleLight<GreenLight>>(mm2px(Vec(10.16, 100.475)), module, AbraModule::BIT_0_LIGHT + 6));
    addOutput(createOutputCentered<BitPort>(mm2px(Vec(10.16, 110.029)), module, AbraModule::BIT_0_OUTPUT + 7));
    addChild(createLightCentered<MediumSimpleLight<GreenLight>>(mm2px(Vec(10.16, 110.029)), module, AbraModule::BIT_0_LIGHT + 7));
  }

  void appendContextMenu(Menu* menu) override {
    auto module = dynamic_cast<AbraModule*>(this->module);
    if (module) {
      menu->addChild(new ui::MenuSeparator());
      module->input_range.addMenu(module, menu, "Input range");
      menu->addChild(createIndexPtrSubmenuItem("Pulse mode", { "Clock", "Trigger", "Hold" }, &module->pulse_mode));
    }
  }
};

Model* modelAbra = createModel<AbraModule, AbraWidget>("abra");