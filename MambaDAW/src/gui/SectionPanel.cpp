#include "SectionPanel.h"
#include "MambaColors.h"

using namespace juce;

SectionPanel::SectionPanel(const String& title,
                           std::initializer_list<String> paramNames,
                           std::initializer_list<String> unitSuffixes)
    : title_(title)
    , wetSlider_(Slider::LinearHorizontal, Slider::NoTextBox)
{
    auto unitIt = unitSuffixes.begin();
    for (const auto& name : paramNames) {
        const String unit = (unitIt != unitSuffixes.end()) ? *unitIt++ : String{};
        auto knob = std::make_unique<MambaKnob>(name, unit);
        knob->setRange(0.0, 1.0, 0.0);
        addAndMakeVisible(*knob);
        knobs_.push_back(std::move(knob));
    }

    wetSlider_.setRange(0.0, 1.0, 0.0);
    wetSlider_.getProperties().set("manualOverride", false);
    addAndMakeVisible(wetSlider_);

    wetLabel_.setText("Wet", dontSendNotification);
    wetLabel_.setJustificationType(Justification::centredLeft);
    wetLabel_.setColour(Label::textColourId, MambaColors::textSecondary);
    wetLabel_.setFont(Font(FontOptions(10.f)));
    addAndMakeVisible(wetLabel_);
}

void SectionPanel::updateAIValues(const float* vals, int count, bool manualMode) {
    for (int i = 0; i < static_cast<int>(knobs_.size()) && i < count; ++i) {
        knobs_[i]->setAIValue(vals[i]);
        knobs_[i]->setManualOverride(manualMode);
    }
    wetSlider_.getProperties().set("manualOverride", manualMode);
    wetSlider_.repaint();
}

void SectionPanel::paint(Graphics& g) {
    const auto bounds = getLocalBounds().toFloat().reduced(2.f);
    g.setColour(MambaColors::surface);
    g.fillRoundedRectangle(bounds, 6.f);
    g.setColour(MambaColors::border);
    g.drawRoundedRectangle(bounds, 6.f, 1.f);

    g.setFont(Font(FontOptions(10.f).withStyle("Bold")));
    g.setColour(MambaColors::textSecondary);
    g.drawText(title_, 8, 4, getWidth() - 16, 13, Justification::centredLeft);
}

void SectionPanel::resized() {
    const int titleH  = 17;
    const int wetH    = 20;
    const int wetLblW = 22;
    const int pad     = 4;

    const int knobAreaH = getHeight() - titleH - wetH - pad * 2;
    const int knobAreaY = titleH;

    const int n = static_cast<int>(knobs_.size());
    if (n > 0) {
        const int knobW = (getWidth() - pad * 2) / n;
        for (int i = 0; i < n; ++i)
            knobs_[i]->setBounds(pad + i * knobW, knobAreaY, knobW, knobAreaH);
    }

    const int wetY = getHeight() - wetH - pad;
    wetLabel_.setBounds(pad, wetY + 2, wetLblW, wetH - 4);
    wetSlider_.setBounds(pad + wetLblW, wetY, getWidth() - pad * 2 - wetLblW, wetH);
}
