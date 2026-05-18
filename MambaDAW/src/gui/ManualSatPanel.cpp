#include "ManualSatPanel.h"
#include "MambaColors.h"

using namespace juce;

ManualSatPanel::ManualSatPanel(ParamCallback cb) : onParam_(std::move(cb)) {
    toggleBtn_.setClickingTogglesState(false);
    toggleBtn_.onClick = [this] {
        enabled_ = !enabled_;
        if (onParam_) onParam_(16, enabled_ ? 1.0 : 0.0);
        updateToggleLabel();
    };
    addAndMakeVisible(toggleBtn_);

    auto setupLabel = [this](Label& lbl, const char* text) {
        lbl.setText(text, dontSendNotification);
        lbl.setColour(Label::textColourId, MambaColors::textSecondary);
        lbl.setFont(Font(FontOptions(9.5f)));
        lbl.setJustificationType(Justification::centredLeft);
        addAndMakeVisible(lbl);
    };
    setupLabel(driveLabel_, "Drive");
    setupLabel(toneLabel_,  "Tone");

    driveSlider_.setRange(0.0, 1.0, 0.0);
    driveSlider_.setValue(0.3, dontSendNotification);
    driveSlider_.setTextBoxStyle(Slider::TextBoxRight, false, 40, 18);
    driveSlider_.onValueChange = [this] {
        if (onParam_) onParam_(29, driveSlider_.getValue());
    };
    addAndMakeVisible(driveSlider_);

    toneSlider_.setRange(-1.0, 1.0, 0.0);
    toneSlider_.setValue(0.0, dontSendNotification);
    toneSlider_.setTextBoxStyle(Slider::TextBoxRight, false, 40, 18);
    toneSlider_.onValueChange = [this] {
        if (onParam_) onParam_(30, toneSlider_.getValue());
    };
    addAndMakeVisible(toneSlider_);

    updateToggleLabel();
}

void ManualSatPanel::updateToggleLabel() {
    toggleBtn_.setButtonText(enabled_ ? "SAT: MANUAL" : "SAT: AI");
    toggleBtn_.setColour(TextButton::buttonColourId,
        enabled_ ? MambaColors::amber.withAlpha(0.25f) : MambaColors::surface);
    toggleBtn_.setColour(TextButton::textColourOffId,
        enabled_ ? MambaColors::amber : MambaColors::textSecondary);
}

void ManualSatPanel::syncFromProcessor(bool enabled, double drive, double tone) {
    if (enabled != enabled_) {
        enabled_ = enabled;
        updateToggleLabel();
    }
    driveSlider_.setValue(drive, dontSendNotification);
    toneSlider_ .setValue(tone,  dontSendNotification);
}

void ManualSatPanel::paint(Graphics& g) {
    g.fillAll(MambaColors::surface.darker(0.15f));
    g.setColour(MambaColors::border);
    g.fillRect(0, 0, 1, getHeight());
}

void ManualSatPanel::resized() {
    const int w       = getWidth();
    const int padX    = 4;
    const int toggleH = 22;
    const int lblH    = 14;
    const int slH     = 26;
    const int rowH    = lblH + slH + 2;

    toggleBtn_.setBounds(2, 2, w - 4, toggleH - 2);

    int y = toggleH + 6;
    driveLabel_ .setBounds(padX, y, w - padX * 2, lblH);
    driveSlider_.setBounds(padX, y + lblH, w - padX * 2, slH);
    y += rowH + 4;
    toneLabel_  .setBounds(padX, y, w - padX * 2, lblH);
    toneSlider_ .setBounds(padX, y + lblH, w - padX * 2, slH);
}
