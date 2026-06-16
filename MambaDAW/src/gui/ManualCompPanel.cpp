#include "ManualCompPanel.h"
#include "MambaColors.h"

using namespace juce;

static const char* kCompNames[5] = { "Thresh", "Ratio", "Attack", "Release", "Makeup" };

struct CompRowCfg {
    double lo, hi, def;
    const char* suffix;
    uint32_t paramId;
};
static const CompRowCfg kCfg[5] = {
    { -40.0, 0.0, -20.0, " dB", 10 },  // thresh (param 10)
    { 1.0, 10.0, 2.0, ":1", 25 },      // ratio (param 25)
    { 0.5, 50.0, 10.0, " ms", 26 },    // attack (param 26)
    { 20.0, 500.0, 100.0, " ms", 27 }, // release (param 27)
    { -2.0, 12.0, 0.0, " dB", 28 },    // makeup (param 28)
};

ManualCompPanel::ManualCompPanel(ParamCallback cb) : onParam_(std::move(cb)) {
    toggleBtn_.setClickingTogglesState(false);
    toggleBtn_.onClick = [this] {
        enabled_ = !enabled_;
        if (onParam_) onParam_(15, enabled_ ? 1.0 : 0.0);
        updateToggleLabel();
    };
    addAndMakeVisible(toggleBtn_);

    for (int i = 0; i < 5; ++i) {
        rows_[i].label.setText(kCompNames[i], dontSendNotification);
        rows_[i].label.setColour(Label::textColourId, MambaColors::textSecondary);
        rows_[i].label.setFont(Font(FontOptions(9.5f)));
        rows_[i].label.setJustificationType(Justification::centredRight);
        addAndMakeVisible(rows_[i].label);

        rows_[i].slider.setRange(kCfg[i].lo, kCfg[i].hi, 0.0);
        rows_[i].slider.setValue(kCfg[i].def, dontSendNotification);
        rows_[i].slider.setTextBoxStyle(Slider::TextBoxRight, false, 50, 18);
        rows_[i].slider.setTextValueSuffix(kCfg[i].suffix);
        const uint32_t pid = kCfg[i].paramId;
        rows_[i].slider.onValueChange = [this, pid, i] {
            if (onParam_) onParam_(pid, rows_[i].slider.getValue());
        };
        addAndMakeVisible(rows_[i].slider);
    }

    updateToggleLabel();
}

void ManualCompPanel::updateToggleLabel() {
    toggleBtn_.setButtonText(enabled_ ? "COMP: MANUAL" : "COMP: AI");
    toggleBtn_.setColour(TextButton::buttonColourId,
        enabled_ ? MambaColors::amber.withAlpha(0.25f) : MambaColors::surface);
    toggleBtn_.setColour(TextButton::textColourOffId,
        enabled_ ? MambaColors::amber : MambaColors::textSecondary);
}

void ManualCompPanel::syncFromProcessor(bool enabled, double thresh, double ratio,
  double attack, double release, double makeup,
  const int* paramSuppress) {
  if (enabled != enabled_) {
    enabled_ = enabled;
    updateToggleLabel();
  }
  const double vals[5] = { thresh, ratio, attack, release, makeup };
  for (int i = 0; i < 5; ++i) {
    const uint32_t pid = kCfg[i].paramId;
    if (paramSuppress && pid < 33 && paramSuppress[pid] > 0) continue;
    if (!rows_[i].slider.isMouseButtonDown())
      rows_[i].slider.setValue(vals[i], dontSendNotification);
  }
}

void ManualCompPanel::paint(Graphics& g) {
    g.fillAll(MambaColors::surface.darker(0.15f));
    g.setColour(MambaColors::border);
    g.fillRect(0, 0, 1, getHeight());
}

void ManualCompPanel::resized() {
    const int w    = getWidth();
    const int padX = 4;
    const int lblW = 52;
    const int boxW = 50;
    const int slW  = w - padX * 2 - lblW - boxW;

    const int toggleH = 22;
    toggleBtn_.setBounds(2, 2, w - 4, toggleH - 2);

    const int remaining = getHeight() - toggleH - 2;
    const int rowH = remaining / 5;
    int y = toggleH + 2;
    for (int i = 0; i < 5; ++i) {
        rows_[i].label .setBounds(padX, y + 1, lblW, rowH - 2);
        rows_[i].slider.setBounds(padX + lblW, y, slW + boxW, rowH);
        y += rowH;
    }
}
