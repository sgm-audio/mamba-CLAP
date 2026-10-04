#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Prototype UI component: displays a caller-supplied buffer-fill ratio and a
// configured target. It does not prove effective model memory or audio quality.
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
