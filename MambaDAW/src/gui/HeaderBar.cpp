#include "HeaderBar.h"
#include "MambaColors.h"

using namespace juce;

HeaderBar::HeaderBar() {
    setOpaque(false);
}

void HeaderBar::setInferenceActive(bool active) noexcept { inferenceActive_ = active; repaint(); }
void HeaderBar::setModelLoaded(bool loaded) noexcept { modelLoaded_ = loaded; repaint(); }
void HeaderBar::setDegraded(bool degraded) noexcept { degraded_ = degraded; repaint(); }
void HeaderBar::setCpuOverloaded(bool overloaded) noexcept { cpuOverloaded_ = overloaded; repaint(); }

void HeaderBar::paint(Graphics& g) {
    g.fillAll(MambaColors::surface);

    const int h = getHeight();

    g.setFont(Font(FontOptions(15.f).withStyle("Bold")));
    g.setColour(MambaColors::textPrimary);
    g.drawText(CharPointer_UTF8("\xe2\x89\x8b MambaDAW"),
               8, 0, 200, h, Justification::centredLeft);

    if (cpuOverloaded_) {
        g.setFont(Font(FontOptions(12.f)));
        g.setColour(MambaColors::red);
        g.drawText(CharPointer_UTF8("CPU OVERLOAD"), 200, 0, 110, h, Justification::centredLeft);
    }

    const Colour dotColour = inferenceActive_ ? MambaColors::green : MambaColors::textSecondary;
    g.setFont(Font(FontOptions(13.f)));
    g.setColour(dotColour);
    const int centreX = getWidth() / 2;
    g.drawText(CharPointer_UTF8("\xe2\x97\x8f AI ACTIVE"),
               centreX - 60, 0, 120, h, Justification::centred);

    juce::String statusText;
    Colour statusColour;
    if (degraded_) {
        statusText = "STATUS: DEGRADED";
        statusColour = MambaColors::red;
    } else if (modelLoaded_) {
        statusText = "STATUS: MODEL LOADED";
        statusColour = MambaColors::green;
    } else {
        statusText = "STATUS: NO MODEL";
        statusColour = MambaColors::amber;
    }
    g.setFont(Font(FontOptions(12.f)));
    g.setColour(statusColour);
    g.drawText(statusText, 0, 0, getWidth() - 8, h, Justification::centredRight);

    g.setColour(MambaColors::border);
    g.fillRect(0, h - 1, getWidth(), 1);
}
