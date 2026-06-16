#include "ManualControlsBar.h"
#include "MambaColors.h"

using namespace juce;

ManualControlsBar::ManualControlsBar() {
    titleLabel_.setText("MANUAL OVERRIDES", dontSendNotification);
    titleLabel_.setColour(Label::textColourId, MambaColors::amber);
    titleLabel_.setFont(Font(FontOptions(10.f).withStyle("Bold")));
    titleLabel_.setJustificationType(Justification::centredLeft);
    addAndMakeVisible(titleLabel_);

    hintLabel_.setText("Toggle per-section AI\xE2\x86\x94Manual below", dontSendNotification);
    hintLabel_.setColour(Label::textColourId, MambaColors::textSecondary);
    hintLabel_.setFont(Font(FontOptions(9.f)));
    hintLabel_.setJustificationType(Justification::centredRight);
    addAndMakeVisible(hintLabel_);
}

void ManualControlsBar::paint(Graphics& g) {
    g.fillAll(MambaColors::surface.darker(0.3f));
    g.setColour(MambaColors::border);
    g.fillRect(0, 0, getWidth(), 1);
    g.fillRect(0, getHeight() - 1, getWidth(), 1);
}

void ManualControlsBar::resized() {
    const int w   = getWidth();
    const int h   = getHeight();
    const int pad = 8;
    titleLabel_.setBounds(pad, 0, 180, h);
    hintLabel_ .setBounds(w - 280 - pad, 0, 280, h);
}
