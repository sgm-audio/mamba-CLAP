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

    void setModeValue(int idx) { modeCombo_.setSelectedItemIndex(idx, juce::dontSendNotification); }
    void setEffectTypeValue(int idx) { typeCombo_.setSelectedItemIndex(idx, juce::dontSendNotification); }
    void setDryWet(double v) { if (!dryWetSlider_.isMouseButtonDown()) dryWetSlider_.setValue(v, juce::dontSendNotification); }
    void setBypass(bool b) { bypassButton_.setToggleState(b, juce::dontSendNotification); }
 void setInputGain(double v) { if (!inputGainSlider_.isMouseButtonDown()) inputGainSlider_.setValue(v, juce::dontSendNotification); }
 void setOutputGain(double v) { if (!outputGainSlider_.isMouseButtonDown()) outputGainSlider_.setValue(v, juce::dontSendNotification); }

 bool isDraggingDryWet() const { return dryWetSlider_.isMouseButtonDown(); }
 bool isDraggingInputGain() const { return inputGainSlider_.isMouseButtonDown(); }
 bool isDraggingOutputGain() const { return outputGainSlider_.isMouseButtonDown(); }

    void resized() override;

private:
    ParamCallback onParam_;
    LevelMeter meter_;
    juce::ComboBox modeCombo_;
    juce::ComboBox typeCombo_;
    juce::Slider dryWetSlider_;
    juce::Slider inputGainSlider_;
    juce::Slider outputGainSlider_;
    juce::TextButton bypassButton_{ "BYP" };
    juce::Label modeLabel_;
    juce::Label typeLabel_;
    juce::Label dryWetLabel_;
    juce::Label inputGainLabel_;
    juce::Label outputGainLabel_;
};
