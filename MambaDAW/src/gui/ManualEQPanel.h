#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdint>
#include <functional>

class ManualEQPanel : public juce::Component {
public:
    using ParamCallback = std::function<void(uint32_t, double)>;

    explicit ManualEQPanel(ParamCallback cb);

    void syncFromProcessor(bool enabled,
                           double loGain, double loHz,
                           double midGain, double midHz, double midQ,
                           double hiGain, double hiHz);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    ParamCallback onParam_;
    juce::TextButton toggleBtn_;

    struct Row {
        juce::Label  label;
        juce::Slider slider{ juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    };
    Row rows_[7];  // lo_gain, lo_hz, mid_gain, mid_hz, mid_q, hi_gain, hi_hz

    bool enabled_ = false;
    void updateToggleLabel();
};
