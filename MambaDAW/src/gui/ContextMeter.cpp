#include "ContextMeter.h"
#include "MambaColors.h"
#include <cmath>

using namespace juce;

ContextMeter::ContextMeter() {
    setOpaque(false);
}

void ContextMeter::setFillRatio(float ratio) noexcept {
    fillRatio_ = jlimit(0.f, 1.f, ratio);
}

void ContextMeter::setContextLengthSeconds(float s) noexcept {
    maxSeconds_ = s;
}

void ContextMeter::setInferenceActive(bool active) noexcept {
    inferenceActive_ = active;
}

void ContextMeter::tick(float deltaSeconds) noexcept {
    if (inferenceActive_) {
        pulsePhase_ += deltaSeconds * MathConstants<float>::twoPi * 2.f;  // 2 Hz
        if (pulsePhase_ > MathConstants<float>::twoPi)
            pulsePhase_ -= MathConstants<float>::twoPi;
    } else {
        pulsePhase_ = 0.f;
    }
}

void ContextMeter::paint(Graphics& g) {
    const auto bounds = getLocalBounds().toFloat().reduced(4.f, 4.f);
    const float corner = 3.f;

    g.setColour(MambaColors::border);
    g.fillRoundedRectangle(bounds, corner);

    if (fillRatio_ > 0.f) {
        const float fillW = bounds.getWidth() * fillRatio_;
        const auto  fillRect = bounds.withWidth(fillW);

        // 2 Hz pulse: alpha oscillates 80%–100% when inference is active.
        const float alpha = inferenceActive_
            ? 0.8f + 0.2f * (0.5f + 0.5f * std::sin(pulsePhase_))
            : 1.f;

        const ColourGradient grad(MambaColors::aiBlue.withAlpha(alpha),    fillRect.getX(),     0.f,
                                  MambaColors::aiBlueGlow.withAlpha(alpha), fillRect.getRight(), 0.f, false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(fillRect, corner);
    }

    const float filledSeconds = fillRatio_ * maxSeconds_;
    const String label = String("Context: ")
                       + String(filledSeconds, 1)
                       + String("s / ")
                       + String(static_cast<int>(maxSeconds_))
                       + String("s");
    g.setColour(MambaColors::textPrimary);
    g.setFont(Font(FontOptions(12.f)));
    g.drawFittedText(label, bounds.reduced(6.f, 0.f).toNearestInt(),
                     Justification::centredRight, 1);
}
