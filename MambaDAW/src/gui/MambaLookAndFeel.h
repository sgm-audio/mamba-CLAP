#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MambaColors.h"

class MambaLookAndFeel : public juce::LookAndFeel_V4 {
public:
    MambaLookAndFeel();

    // Rotary knob — dark ring + aiBlue arc + brushed body + position dot.
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPosProportional,
                          float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override;

    // Linear slider — thin track, filled with aiBlue, rounded thumb.
    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle, juce::Slider&) override;

    // Buttons — pill shape; aiBlue tint + border when toggled.
    void drawButtonBackground(juce::Graphics&, juce::Button&,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    void drawButtonText(juce::Graphics&, juce::TextButton&,
                        bool isHighlighted, bool isDown) override;

    // ComboBox — dark rounded rect with arrow.
    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox&) override;

    juce::Font getComboBoxFont(juce::ComboBox&) override;

    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;

    // Popup menu — dark background, aiBlue highlight.
    void drawPopupMenuBackground(juce::Graphics&, int width, int height) override;

    void drawPopupMenuItem(juce::Graphics&, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted,
                           bool isTicked, bool hasSubMenu,
                           const juce::String& text,
                           const juce::String& shortcutKeyText,
                           const juce::Drawable* icon,
                           const juce::Colour* textColour) override;

    // Labels — consistent secondary-colour units text.
    void drawLabel(juce::Graphics&, juce::Label&) override;

    juce::Font getLabelFont(juce::Label&) override;

private:
    juce::Font mainFont_;
    juce::Font smallFont_;
};
