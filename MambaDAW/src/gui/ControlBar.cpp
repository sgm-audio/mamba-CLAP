#include "ControlBar.h"
#include "MambaColors.h"

using namespace juce;

ControlBar::ControlBar(ParamCallback cb)
    : onParam_(std::move(cb))
    , dryWetSlider_(Slider::LinearHorizontal, Slider::NoTextBox)
{
    addAndMakeVisible(meter_);

    modeCombo_.addItem("Auto",   1);
    modeCombo_.addItem("Manual", 2);
    modeCombo_.setSelectedItemIndex(0, dontSendNotification);
    modeCombo_.onChange = [this] {
        if (onParam_) onParam_(1, static_cast<double>(modeCombo_.getSelectedItemIndex()));
    };
    addAndMakeVisible(modeCombo_);

    typeCombo_.addItem("All FX",    1);
    typeCombo_.addItem("EQ Only",   2);
    typeCombo_.addItem("Comp Only", 3);
    typeCombo_.addItem("Sat Only",  4);
    typeCombo_.addItem("Rev Only",  5);
    typeCombo_.setSelectedItemIndex(0, dontSendNotification);
    typeCombo_.onChange = [this] {
        if (onParam_) onParam_(12, static_cast<double>(typeCombo_.getSelectedItemIndex()));
    };
    addAndMakeVisible(typeCombo_);

    dryWetSlider_.setRange(0.0, 1.0, 0.0);
    dryWetSlider_.onValueChange = [this] {
        if (onParam_) onParam_(0, dryWetSlider_.getValue());
    };
    addAndMakeVisible(dryWetSlider_);

    bypassButton_.setClickingTogglesState(true);
    bypassButton_.onClick = [this] {
        if (onParam_) onParam_(2, bypassButton_.getToggleState() ? 1.0 : 0.0);
    };
    addAndMakeVisible(bypassButton_);

    auto setupLabel = [this](Label& lbl, const String& text) {
        lbl.setText(text, dontSendNotification);
        lbl.setColour(Label::textColourId, MambaColors::textSecondary);
        lbl.setFont(Font(FontOptions(11.f)));
        addAndMakeVisible(lbl);
    };
    setupLabel(modeLabel_,   "Mode");
    setupLabel(typeLabel_,   "Type");
    setupLabel(dryWetLabel_, "Dry/Wet");
}

void ControlBar::resized() {
    const int h        = getHeight();
    const int labelH   = 14;
    const int ctrlH    = h - labelH - 4;
    const int ctrlY    = labelH + 2;
    const int pad      = 4;

    meter_.setBounds(pad, 4, 20, h - 8);
    int x = pad + 20 + pad;

    const int modeW    = 80;
    modeLabel_.setBounds(x, 2, modeW, labelH);
    modeCombo_.setBounds(x, ctrlY, modeW, ctrlH);
    x += modeW + pad;

    const int typeW    = 100;
    typeLabel_.setBounds(x, 2, typeW, labelH);
    typeCombo_.setBounds(x, ctrlY, typeW, ctrlH);
    x += typeW + pad;

    const int bypassW  = 44;
    const int dryWetW  = getWidth() - x - bypassW - pad * 2;
    dryWetLabel_.setBounds(x, 2, dryWetW, labelH);
    dryWetSlider_.setBounds(x, ctrlY, dryWetW, ctrlH);
    x += dryWetW + pad;

    bypassButton_.setBounds(x, (h - ctrlH) / 2, bypassW, ctrlH);
}
