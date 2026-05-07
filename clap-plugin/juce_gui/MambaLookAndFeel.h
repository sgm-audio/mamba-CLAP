#ifndef MAMBA_LOOK_AND_FEEL_H_INCLUDED
#define MAMBA_LOOK_AND_FEEL_H_INCLUDED

#include <JuceHeader.h>

class MambaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MambaLookAndFeel();
    ~MambaLookAndFeel() override;

    // Custom colors
    static const juce::Colour voidBlack;
    static const juce::Colour neonGreen;
    static const juce::Colour alertAmber;
    static const juce::Colour darkGrey;

    // Override drawing methods
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawLabel (juce::Graphics&, juce::Label&) override;

    juce::Font getLabelFont (juce::Label&) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MambaLookAndFeel)
};

#endif // MAMBA_LOOK_AND_FEEL_H_INCLUDED
