#include "plugin.hpp"
#include "inc/cvRange.hpp"

struct CadabraModule : Module
{
  enum ParamId
  {
    PARAMS_LEN
  };
  enum InputId
  {
    ENUMS(BIT_0_INPUT, 8),
    CLOCK_INPUT,
    INPUTS_LEN
  };
  enum OutputId
  {
    ANALOG_OUTPUT,
    OUTPUTS_LEN
  };
  enum LightId
  {
    LIGHTS_LEN
  };
  enum TriggerMode
  {
    TOGGLE,
    HOLD
  };

  dsp::SchmittTrigger clock_trigger;
  dsp::SchmittTrigger bit_triggers[8];
  CVRange output_range;
  int bit_value = 0;
  int trigger_mode = TOGGLE;

  CadabraModule() {
    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    for (int i = 0; i < 8; i++) {
      configInput(BIT_0_INPUT + i, "Bit 2^" + std::to_string(i));
    }
    configInput(CLOCK_INPUT, "Clock");
    configOutput(ANALOG_OUTPUT, "Analog signal");
    output_range = CVRange(0.f, 10.f);
  }

  void process(const ProcessArgs& args) override {
    const bool clock_connected = inputs[CLOCK_INPUT].isConnected();
    const float clock_in = inputs[CLOCK_INPUT].getVoltage();
    bool new_clock = false;
    if (clock_connected)
      new_clock = clock_trigger.process(clock_in);

    int new_bit_value = bit_value;

    for (int i = 0; i < 8; i++) {
      const float bit_in = inputs[BIT_0_INPUT + i].getVoltage();
      if ((clock_connected && new_clock) || (!clock_connected)) {
        switch (trigger_mode) {
          case TOGGLE:
          default:
            if (bit_triggers[i].process(bit_in, 0.1f, 1.f))
              new_bit_value ^= (1 << i);
            break;
          case HOLD:
            if (bit_in >= 1.f)
              new_bit_value |= (1 << i);
            else
              new_bit_value &= ~(1 << i);
            break;
        }
      }
    }

    bit_value = new_bit_value;
    outputs[ANALOG_OUTPUT].setVoltage(output_range.map((float)bit_value / 255.f));
  }

  json_t* dataToJson() override {
    json_t* rootJ = json_object();
    json_object_set_new(rootJ, "output_range", output_range.dataToJson());
    json_object_set_new(rootJ, "trigger_mode", json_integer(trigger_mode));
    return rootJ;
  }

  void dataFromJson(json_t* rootJ) override {
    json_t* output_rangeJ = json_object_get(rootJ, "output_range");
    if (output_rangeJ) {
      output_range.dataFromJson(output_rangeJ);
    } else {
      output_range = CVRange(0.f, 10.f);
    }
    if (json_t* triggerModeJ = json_object_get(rootJ, "trigger_mode")) {
      trigger_mode = json_integer_value(triggerModeJ);
    } else {
      trigger_mode = TOGGLE;
    }
  }
};


struct CadabraWidget : ModuleWidget
{
  CadabraWidget(CadabraModule* module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, "res/cadabra.svg"), asset::plugin(pluginInstance, "res/cadabra-dark.svg")));

    addInput(createInputCentered<BitPort>(mm2px(Vec(10.16, 18.014)), module, CadabraModule::BIT_0_INPUT));
    addInput(createInputCentered<BitPort>(mm2px(Vec(10.16, 27.568)), module, CadabraModule::BIT_0_INPUT + 1));
    addInput(createInputCentered<BitPort>(mm2px(Vec(10.16, 37.123)), module, CadabraModule::BIT_0_INPUT + 2));
    addInput(createInputCentered<BitPort>(mm2px(Vec(10.16, 46.678)), module, CadabraModule::BIT_0_INPUT + 3));
    addInput(createInputCentered<BitPort>(mm2px(Vec(10.16, 56.232)), module, CadabraModule::BIT_0_INPUT + 4));
    addInput(createInputCentered<BitPort>(mm2px(Vec(10.16, 65.787)), module, CadabraModule::BIT_0_INPUT + 5));
    addInput(createInputCentered<BitPort>(mm2px(Vec(10.16, 75.341)), module, CadabraModule::BIT_0_INPUT + 6));
    addInput(createInputCentered<BitPort>(mm2px(Vec(10.16, 84.896)), module, CadabraModule::BIT_0_INPUT + 7));
    addInput(createInputCentered<BitPort>(mm2px(Vec(10.16, 99.516)), module, CadabraModule::CLOCK_INPUT));

    addOutput(createOutputCentered<BitPort>(mm2px(Vec(10.16, 110.193)), module, CadabraModule::ANALOG_OUTPUT));
  }

  void appendContextMenu(ui::Menu* menu) override {
    auto module = dynamic_cast<CadabraModule*>(this->module);
    if (module) {
      menu->addChild(new ui::MenuSeparator());
      module->output_range.addMenu(module, menu, "Output Range");
      menu->addChild(createIndexPtrSubmenuItem("Trigger Mode", { "Trigger", "Hold" }, &module->trigger_mode));
    }
  }
};


Model* modelCadabra = createModel<CadabraModule, CadabraWidget>("cadabra");