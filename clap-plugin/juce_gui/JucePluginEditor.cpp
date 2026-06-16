#include "JucePluginEditor.h"
#include "JucePluginProcessor.h"
#include <cmath>

//==============================================================================
MambaAudioProcessorEditor::MambaAudioProcessorEditor (MambaAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Set the size of the plugin editor
    setSize (400, 300);

    // Initialize sliders
    // Threshold slider: -60 dB to 0 dB
    thresholdSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    thresholdSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, true, 50, 20);
    thresholdSlider.setRange (-60.0, 0.0);
    thresholdSlider.setValue (*audioProcessor.thresholdParam.load());
    thresholdSlider.onValueChange = [this] { *audioProcessor.thresholdParam = thresholdSlider.getValue(); };
    addAndMakeVisible (thresholdSlider);

    thresholdLabel.attachToComponent (&thresholdSlider, false);
    thresholdLabel.setText ("Threshold (dB)", juce::dontSendNotification);
    thresholdLabel.setJustificationType (juce::Justification::centred);

    // Ratio slider: 1:1 to 20:1
    ratioSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    ratioSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, true, 50, 20);
    ratioSlider.setRange (1.0, 20.0);
    ratioSlider.setValue (*audioProcessor.ratioParam.load());
    ratioSlider.onValueChange = [this] { *audioProcessor.ratioParam = ratioSlider.getValue(); };
    addAndMakeVisible (ratioSlider);

    ratioLabel.attachToComponent (&ratioSlider, false);
    ratioLabel.setText ("Ratio", juce::dontSendNotification);
    ratioLabel.setJustificationType (juce::Justification::centred);

    // Attack slider: 0.1ms to 200ms
    attackSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    attackSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, true, 50, 20);
    attackSlider.setRange (0.1, 200.0);
    attackSlider.setValue (*audioProcessor.attackParam.load());
    attackSlider.onValueChange = [this] { *audioProcessor.attackParam = attackSlider.getValue(); };
    addAndMakeVisible (attackSlider);

    attackLabel.attachToComponent (&attackSlider, false);
    attackLabel.setText ("Attack (ms)", juce::dontSendNotification);
    attackLabel.setJustificationType (juce::Justification::centred);

    // Release slider: 10ms to 2000ms
    releaseSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    releaseSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, true, 50, 20);
    releaseSlider.setRange (10.0, 2000.0);
    releaseSlider.setValue (*audioProcessor.releaseParam.load());
    releaseSlider.onValueChange = [this] { *audioProcessor.releaseParam = releaseSlider.getValue(); };
    addAndMakeVisible (releaseSlider);

    releaseLabel.attachToComponent (&releaseSlider, false);
    releaseLabel.setText ("Release (ms)", juce::dontSendNotification);
    releaseLabel.setJustificationType (juce::Justification::centred);

    // Makeup gain slider: -24 dB to +24 dB
    makeupSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    makeupSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, true, 50, 20);
    makeupSlider.setRange (-24.0, 24.0);
    makeupSlider.setValue (*audioProcessor.makeupParam.load());
    makeupSlider.onValueChange = [this] { *audioProcessor.makeupParam = makeupSlider.getValue(); };
    addAndMakeVisible (makeupSlider);

    makeupLabel.attachToComponent (&makeupSlider, false);
    makeupLabel.setText ("Makeup (dB)", juce::dontSendNotification);
    makeupLabel.setJustificationType (juce::Justification::centred);

    // Start the timer to update the GUI at 30 FPS
    startTimerHz (30);
}

MambaAudioProcessorEditor::~MambaAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void MambaAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Fill the background with a dark color
    g.fillAll (juce::Colours::darkgrey);

    // Draw a title
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (18.0f).bolded);
    g.drawText ("Mamba Spectrogram Enhancer", 0, 10, getWidth(), 20, juce::Justification::centred);

    // Draw the meters
    drawMeter (g, inputMeterBounds, audioProcessor.inputLevel.load(), "Input", juce::Colours::green);
    drawMeter (g, outputMeterBounds, audioProcessor.outputLevel.load(), "Output", juce::Colours::blue);
    drawMeter (g, gainReductionMeterBounds, audioProcessor.gainReduction.load(), "GR", juce::Colours::orange);

    // Draw some decorative elements
    g.setColour (juce::Colours::lightgrey);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (5.0f), 5.0f, 1.0f);
}

void MambaAudioProcessorEditor::resized()
{
    // Calculate bounds for components
    auto bounds = getLocalBounds().reduced (10);

    // Calculate meter bounds (top area)
    int meterHeight = 20;
    int meterWidth = (bounds.getWidth() - 40) / 3; // 3 meters with spacing
    int meterY = 40;

    inputMeterBounds = juce::Rectangle<int> (bounds.getX(), meterY, meterWidth, meterHeight);
    outputMeterBounds = juce::Rectangle<int> (bounds.getX() + meterWidth + 10, meterY, meterWidth, meterHeight);
    gainReductionMeterBounds = juce::Rectangle<int> (bounds.getX() + 2 * (meterWidth + 10), meterY, meterWidth, meterHeight);

    // Calculate slider bounds (bottom area)
    int sliderSize = 80;
    int sliderStartX = bounds.getX() + (bounds.getWidth() - 3 * sliderSize - 2 * 20) / 2;
    int sliderStartY = bounds.getY() + bounds.getHeight() - sliderSize - 40;

    thresholdSlider.setBounds (sliderStartX, sliderStartY, sliderSize, sliderSize);
    ratioSlider.setBounds (sliderStartX + sliderSize + 20, sliderStartY, sliderSize, sliderSize);
    attackSlider.setBounds (sliderStartX + 2 * (sliderSize + 20), sliderStartY, sliderSize, sliderSize);
    releaseSlider.setBounds (sliderStartX, sliderStartY + sliderSize + 20, sliderSize, sliderSize);
    makeupSlider.setBounds (sliderStartX + sliderSize + 20, sliderStartY + sliderSize + 20, sliderSize, sliderSize);
}

//==============================================================================
void MambaAudioProcessorEditor::timerCallback()
{
    // Call repaint to update the meters and any other changing visuals
    repaint();
}

void MambaAudioProcessorEditor::drawMeter (juce::Graphics& g, const juce::Rectangle<int>& bounds, float levelDB, const juce::String& label, juce::Colour colour)
{
    // Draw the meter background
    g.setColour (juce::Colours::grey);
    g.fillRect (bounds);

    // Calculate the fill level based on dB
    // We'll map from -60 dB to 0 dB to 0% to 100% fill
    float normalizedLevel = juce::jlimit (0.0f, 1.0f, (levelDB + 60.0f) / 60.0f);
    int fillHeight = static_cast<int> (bounds.getHeight() * normalizedLevel);

    // Draw the filled portion
    g.setColour (colour);
    g.fillRect (bounds.getX(), bounds.getBottom() - fillHeight, bounds.getWidth(), fillHeight);

    // Draw the border
    g.setColour (juce::Colours::white);
    g.drawRect (bounds);

    // Draw the label
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (12.0f));
    g.drawText (label, bounds, juce::Justification::centred);

    // Draw the dB value
    juce::String dbString;
    dbString << levelDB;
    g.drawText (dbString + " dB", bounds.translated (0, bounds.getHeight()), juce::Justification::centred);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MambaAudioProcessor();
}
