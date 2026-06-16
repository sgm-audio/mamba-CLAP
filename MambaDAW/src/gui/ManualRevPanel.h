#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdint>
#include <functional>

class ManualRevPanel : public juce::Component {
public:
    using ParamCallback = std::function<void(uint32_t, double)>;

    explicit ManualRevPanel(ParamCallback cb);

    void syncFromProcessor(bool enabled, double decay, double predelay, double diffusion,
  const int* paramSuppress = nullptr);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    ParamCallback onParam_;
    juce::TextButton toggleBtn_;

    juce::Label  decayLabel_, preLabel_, diffLabel_;
    juce::Slider decaySlider_  { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider preSlider_    { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider diffSlider_   { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };

    bool enabled_ = false;
    void updateToggleLabel();
};
