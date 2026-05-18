#include "ManualRevPanel.h"
#include "MambaColors.h"

using namespace juce;

ManualRevPanel::ManualRevPanel(ParamCallback cb) : onParam_(std::move(cb)) {
    toggleBtn_.setClickingTogglesState(false);
    toggleBtn_.onClick = [this] {
        enabled_ = !enabled_;
        if (onParam_) onParam_(17, enabled_ ? 1.0 : 0.0);
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
    setupLabel(decayLabel_, "Decay");
    setupLabel(preLabel_,   "Pre ms");
    setupLabel(diffLabel_,  "Diffuse");

    decaySlider_.setRange(0.1, 8.0, 0.01);
    decaySlider_.setValue(1.5, dontSendNotification);
    decaySlider_.setTextBoxStyle(Slider::TextBoxRight, false, 36, 18);
    decaySlider_.setTextValueSuffix(" s");
    decaySlider_.onValueChange = [this] {
        if (onParam_) onParam_(11, decaySlider_.getValue());
    };
    addAndMakeVisible(decaySlider_);

    preSlider_.setRange(0.0, 100.0, 0.1);
    preSlider_.setValue(20.0, dontSendNotification);
    preSlider_.setTextBoxStyle(Slider::TextBoxRight, false, 36, 18);
    preSlider_.onValueChange = [this] {
        if (onParam_) onParam_(31, preSlider_.getValue());
    };
    addAndMakeVisible(preSlider_);

    diffSlider_.setRange(0.0, 1.0, 0.0);
    diffSlider_.setValue(0.5, dontSendNotification);
    diffSlider_.setTextBoxStyle(Slider::TextBoxRight, false, 36, 18);
    diffSlider_.onValueChange = [this] {
        if (onParam_) onParam_(32, diffSlider_.getValue());
    };
    addAndMakeVisible(diffSlider_);

    updateToggleLabel();
}

void ManualRevPanel::updateToggleLabel() {
    toggleBtn_.setButtonText(enabled_ ? "REV: MAN" : "REV: AI");
    toggleBtn_.setColour(TextButton::buttonColourId,
        enabled_ ? MambaColors::amber.withAlpha(0.25f) : MambaColors::surface);
    toggleBtn_.setColour(TextButton::textColourOffId,
        enabled_ ? MambaColors::amber : MambaColors::textSecondary);
}

void ManualRevPanel::syncFromProcessor(bool enabled, double decay, double predelay, double diffusion) {
    if (enabled != enabled_) {
        enabled_ = enabled;
        updateToggleLabel();
    }
    decaySlider_.setValue(decay,     dontSendNotification);
    preSlider_  .setValue(predelay,  dontSendNotification);
    diffSlider_ .setValue(diffusion, dontSendNotification);
}

void ManualRevPanel::paint(Graphics& g) {
    g.fillAll(MambaColors::surface.darker(0.15f));
    g.setColour(MambaColors::border);
    g.fillRect(0, 0, 1, getHeight());
}

void ManualRevPanel::resized() {
    const int w       = getWidth();
    const int padX    = 3;
    const int toggleH = 22;
    const int lblH    = 13;
    const int slH     = 24;
    const int rowH    = lblH + slH + 2;

    toggleBtn_.setBounds(1, 2, w - 2, toggleH - 2);

    int y = toggleH + 4;
    decayLabel_ .setBounds(padX, y, w - padX * 2, lblH);
    decaySlider_.setBounds(padX, y + lblH, w - padX * 2, slH);
    y += rowH + 2;
    preLabel_   .setBounds(padX, y, w - padX * 2, lblH);
    preSlider_  .setBounds(padX, y + lblH, w - padX * 2, slH);
    y += rowH + 2;
    diffLabel_  .setBounds(padX, y, w - padX * 2, lblH);
    diffSlider_ .setBounds(padX, y + lblH, w - padX * 2, slH);
}
