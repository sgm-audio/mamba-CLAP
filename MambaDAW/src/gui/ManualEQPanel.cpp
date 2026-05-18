#include "ManualEQPanel.h"
#include "MambaColors.h"

using namespace juce;

static const char* kRowNames[7] = { "Lo Gain", "Lo Hz", "Mid Gain", "Mid Hz", "Mid Q", "Hi Gain", "Hi Hz" };

struct EQRowCfg {
    double lo, hi, def;
    const char* suffix;
};
static const EQRowCfg kCfg[7] = {
    { -12.0, 12.0,  0.0, " dB" },   // lo_gain   param 18
    {  60.0, 500.0, 200.0, " Hz" },  // lo_hz     param 19
    { -12.0, 12.0,  0.0, " dB" },   // mid_gain   param 20
    { 250.0, 4000.0, 1000.0, " Hz" },// mid_hz    param 21
    {  0.3,  8.0,   1.0, ""    },    // mid_q      param 22
    { -12.0, 12.0,  0.0, " dB" },   // hi_gain    param 23
    { 2000.0,16000.0,8000.0," Hz"},  // hi_hz      param 24
};
static constexpr uint32_t kParamBase = 18; // lo_gain is param 18

ManualEQPanel::ManualEQPanel(ParamCallback cb) : onParam_(std::move(cb)) {
    toggleBtn_.setClickingTogglesState(false);
    toggleBtn_.onClick = [this] {
        enabled_ = !enabled_;
        if (onParam_) onParam_(14, enabled_ ? 1.0 : 0.0);
        updateToggleLabel();
    };
    addAndMakeVisible(toggleBtn_);

    for (int i = 0; i < 7; ++i) {
        rows_[i].label.setText(kRowNames[i], dontSendNotification);
        rows_[i].label.setColour(Label::textColourId, MambaColors::textSecondary);
        rows_[i].label.setFont(Font(FontOptions(9.5f)));
        rows_[i].label.setJustificationType(Justification::centredRight);
        addAndMakeVisible(rows_[i].label);

        rows_[i].slider.setRange(kCfg[i].lo, kCfg[i].hi, 0.0);
        rows_[i].slider.setValue(kCfg[i].def, dontSendNotification);
        rows_[i].slider.setTextBoxStyle(Slider::TextBoxRight, false, 46, 18);
        if (kCfg[i].suffix[0] != '\0')
            rows_[i].slider.setTextValueSuffix(kCfg[i].suffix);
        const uint32_t paramId = kParamBase + static_cast<uint32_t>(i);
        rows_[i].slider.onValueChange = [this, paramId, i] {
            if (onParam_) onParam_(paramId, rows_[i].slider.getValue());
        };
        addAndMakeVisible(rows_[i].slider);
    }

    updateToggleLabel();
}

void ManualEQPanel::updateToggleLabel() {
    toggleBtn_.setButtonText(enabled_ ? "EQ: MANUAL" : "EQ: AI");
    toggleBtn_.setColour(TextButton::buttonColourId,
        enabled_ ? MambaColors::amber.withAlpha(0.25f) : MambaColors::surface);
    toggleBtn_.setColour(TextButton::textColourOffId,
        enabled_ ? MambaColors::amber : MambaColors::textSecondary);
}

void ManualEQPanel::syncFromProcessor(bool enabled,
                                      double loGain, double loHz,
                                      double midGain, double midHz, double midQ,
                                      double hiGain, double hiHz) {
    if (enabled != enabled_) {
        enabled_ = enabled;
        updateToggleLabel();
    }
    const double vals[7] = { loGain, loHz, midGain, midHz, midQ, hiGain, hiHz };
    for (int i = 0; i < 7; ++i)
        rows_[i].slider.setValue(vals[i], dontSendNotification);
}

void ManualEQPanel::paint(Graphics& g) {
    g.fillAll(MambaColors::surface.darker(0.15f));
    g.setColour(MambaColors::border);
    g.fillRect(0, 0, 1, getHeight());
    g.fillRect(getWidth() - 1, 0, 1, getHeight());
}

void ManualEQPanel::resized() {
    const int w   = getWidth();
    const int padX = 4;
    const int lblW = 48;
    const int boxW = 46;
    const int slW  = w - padX * 2 - lblW - boxW;

    const int toggleH = 22;
    toggleBtn_.setBounds(2, 2, w - 4, toggleH - 2);

    const int remaining = getHeight() - toggleH - 2;
    const int rowH = remaining / 7;
    int y = toggleH + 2;
    for (int i = 0; i < 7; ++i) {
        rows_[i].label .setBounds(padX, y + 1, lblW, rowH - 2);
        rows_[i].slider.setBounds(padX + lblW, y, slW + boxW, rowH);
        y += rowH;
    }
}
