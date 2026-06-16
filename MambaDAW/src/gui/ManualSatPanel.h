#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdint>
#include <functional>

class ManualSatPanel : public juce::Component {
public:
    using ParamCallback = std::function<void(uint32_t, double)>;

    explicit ManualSatPanel(ParamCallback cb);

    void syncFromProcessor(bool enabled, double drive, double tone,
  const int* paramSuppress = nullptr);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    ParamCallback onParam_;
    juce::TextButton toggleBtn_;

    juce::Label  driveLabel_, toneLabel_;
    juce::Slider driveSlider_{ juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider toneSlider_ { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };

    bool enabled_ = false;
    void updateToggleLabel();
};
