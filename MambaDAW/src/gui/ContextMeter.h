#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class ContextMeter : public juce::Component {
public:
    ContextMeter();

    void setFillRatio(float ratio) noexcept;
    void setContextLengthSeconds(float s) noexcept;
    void setInferenceActive(bool active) noexcept;

    // Advance the 2 Hz pulse animation; call once per timer tick.
    void tick(float deltaSeconds) noexcept;

    void paint(juce::Graphics&) override;

private:
    float fillRatio_{ 0.f };
    float maxSeconds_{ 60.f };
    bool  inferenceActive_{ false };
    float pulsePhase_{ 0.f };
};
