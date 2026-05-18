#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "LevelMeter.h"
#include <cstdint>
#include <functional>

class ControlBar : public juce::Component {
public:
    using ParamCallback = std::function<void(uint32_t paramId, double value)>;

    explicit ControlBar(ParamCallback cb);

    void setPeakL(float v) { meter_.setPeakL(v); }
    void setPeakR(float v) { meter_.setPeakR(v); }
    void tickMeter(float deltaSeconds) { meter_.tick(deltaSeconds); }

    void setModeValue(int idx)       { modeCombo_.setSelectedItemIndex(idx, juce::dontSendNotification); }
    void setEffectTypeValue(int idx) { typeCombo_.setSelectedItemIndex(idx, juce::dontSendNotification); }
    void setDryWet(double v)         { dryWetSlider_.setValue(v, juce::dontSendNotification); }
    void setBypass(bool b)           { bypassButton_.setToggleState(b, juce::dontSendNotification); }

    void resized() override;

private:
    ParamCallback    onParam_;
    LevelMeter       meter_;
    juce::ComboBox   modeCombo_;
    juce::ComboBox   typeCombo_;
    juce::Slider     dryWetSlider_;
    juce::TextButton bypassButton_{ "BYP" };
    juce::Label      modeLabel_;
    juce::Label      typeLabel_;
    juce::Label      dryWetLabel_;
};
