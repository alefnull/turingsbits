#pragma once
#include <rack.hpp>
using namespace ::rack;

extern Plugin *pluginInstance;

extern Model *modelNala;

struct BitKnob : RoundBlackKnob
{
    BitKnob()
    {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/bitknob_fg.svg")));
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/bitknob_bg.svg")));
    }
};

struct LargeBitKnob : RoundLargeBlackKnob
{
    LargeBitKnob()
    {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/largebitknob_fg.svg")));
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/largebitknob_bg.svg")));
    }
};

struct HugeBitKnob : RoundHugeBlackKnob
{
    HugeBitKnob()
    {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/hugebitknob_fg.svg")));
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/hugebitknob_bg.svg")));
    }
};

struct SmallBitKnob : RoundSmallBlackKnob
{
    SmallBitKnob()
    {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/smallbitknob_fg.svg")));
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/smallbitknob_bg.svg")));
    }
};

struct BitTrimpot : Trimpot
{
    BitTrimpot()
    {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/bittrimpot_fg.svg")));
        bg->setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/bittrimpot_bg.svg")));
    }
};

struct BitPort : SvgPort
{
    BitPort()
    {
        setSvg(APP->window->loadSvg(rack::asset::plugin(pluginInstance, "res/components/bitport.svg")));
        this->shadow->opacity = 0.f;
    }
};

struct EmptyPort : SvgPort
{
    EmptyPort()
    {
        setSvg(APP->window->loadSvg(rack::asset::plugin(pluginInstance, "res/components/empty.svg")));
        this->shadow->opacity = 0.f;
    }
    void onHover(const event::Hover &e) override
    {
        this->destroyTooltip();
    }
};
