#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class HeaderBar : public juce::Component {
public:
    HeaderBar();

    void setInferenceActive(bool active) noexcept;
    void setModelLoaded(bool loaded) noexcept;
    void setDegraded(bool degraded) noexcept;
    void setCpuOverloaded(bool overloaded) noexcept;

    void paint(juce::Graphics&) override;

private:
    bool inferenceActive_{ false };
    bool modelLoaded_{ false };
    bool degraded_{ false };
    bool cpuOverloaded_{ false };
};
