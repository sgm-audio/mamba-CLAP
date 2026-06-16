#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MambaKnob.h"
#include <functional>
#include <initializer_list>
#include <memory>
#include <vector>

class SectionPanel : public juce::Component {
public:
    using ParamCallback = std::function<void(uint32_t, double)>;

 SectionPanel(const juce::String& title,
 std::initializer_list<juce::String> paramNames,
 std::initializer_list<juce::String> unitSuffixes,
 uint32_t wetParamId,
 ParamCallback cb);

    // Called from timer; normalizedValues length must be >= knob count.
    void updateAIValues(const float* normalizedValues, int count, bool manualMode);

    juce::Slider& wetSlider() { return wetSlider_; }

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::String title_;
    std::vector<std::unique_ptr<MambaKnob>> knobs_;
    juce::Slider wetSlider_;
    juce::Label  wetLabel_;

 const uint32_t wetParamId_;
 ParamCallback onParam_;
};

