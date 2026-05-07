#include "MambaLookAndFeel.h"

const juce::Colour MambaLookAndFeel::voidBlack  = juce::Colour (0xff050505);
const juce::Colour MambaLookAndFeel::neonGreen  = juce::Colour (0xff00ff41);
const juce::Colour MambaLookAndFeel::alertAmber = juce::Colour (0xffffb600);
const juce::Colour MambaLookAndFeel::darkGrey   = juce::Colour (0xff1a1a1a);

MambaLookAndFeel::MambaLookAndFeel()
{
    // Set global component colors
    setColour (juce::Slider::thumbColourId, neonGreen);
    setColour (juce::Slider::rotarySliderFillColourId, neonGreen.withAlpha (0.8f));
    setColour (juce::Slider::rotarySliderOutlineColourId, darkGrey);
    
    setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.9f));
    setColour (juce::TextButton::buttonColourId, darkGrey);
    setColour (juce::TextButton::textColourOffId, juce::Colours::white);
}

MambaLookAndFeel::~MambaLookAndFeel() = default;

void MambaLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                       float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                       juce::Slider& slider)
{
    auto outline = slider.findColour (juce::Slider::rotarySliderOutlineColourId);
    auto fill    = slider.findColour (juce::Slider::rotarySliderFillColourId);

    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (10);

    auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    auto lineW = 4.0f;
    auto arcRadius = radius - lineW * 0.5f;

    // Draw background arc
    juce::Path backgroundArc;
    backgroundArc.addCentredArc (bounds.getCentreX(),
                                 bounds.getCentreY(),
                                 arcRadius,
                                 arcRadius,
                                 0.0f,
                                 rotaryStartAngle,
                                 rotaryEndAngle,
                                 true);

    g.setColour (outline);
    g.strokePath (backgroundArc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (slider.isEnabled())
    {
        // Draw value arc
        juce::Path valueArc;
        valueArc.addCentredArc (bounds.getCentreX(),
                                bounds.getCentreY(),
                                arcRadius,
                                arcRadius,
                                0.0f,
                                rotaryStartAngle,
                                toAngle,
                                true);

        // Add a subtle glow to the fill
        g.setColour (fill);
        g.strokePath (valueArc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        
        // Draw "Thumb" / Pointer
        auto thumbWidth = lineW * 2.0f;
        juce::Path thumb;
        thumb.addRectangle (-thumbWidth * 0.5f, -radius, thumbWidth, lineW * 2.0f);
        
        g.setColour (juce::Colours::white);
        g.fillPath (thumb, juce::AffineTransform::rotation (toAngle).translated (bounds.getCentre()));
    }
}

void MambaLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    g.setColour (label.findColour (juce::Label::textColourId));
    
    auto font = getLabelFont (label);
    g.setFont (font);
    
    auto textArea = getLabelBorderSize (label).subtractedFrom (label.getLocalBounds());
    
    g.drawFittedText (label.getText(), textArea, label.getJustificationType(),
                      juce::jmax (1, (int) (textArea.getHeight() / font.getHeight())),
                      label.getMinimumHorizontalScale());
}

juce::Font MambaLookAndFeel::getLabelFont (juce::Label& label)
{
    // Prefer monospaced font for HUD feel
    return juce::Font ("JetBrains Mono", 12.0f, juce::Font::plain);
}
