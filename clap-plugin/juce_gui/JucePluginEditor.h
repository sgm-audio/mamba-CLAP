#ifndef JUCE_PLUGIN_EDITOR_H_INCLUDED
#define JUCE_PLUGIN_EDITOR_H_INCLUDED

#include <JuceHeader.h>
#include "JucePluginProcessor.h"

// Forward declaration
class MambaAudioProcessor;

class MambaAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                   private juce::Timer
{
public:
    MambaAudioProcessorEditor (MambaAudioProcessor&);
    ~MambaAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // Reference to the processor
    MambaAudioProcessor& audioProcessor;

    // GUI components
    juce::Slider thresholdSlider;
    juce::Slider ratioSlider;
    juce::Slider attackSlider;
    juce::Slider releaseSlider;
    juce::Slider makeupSlider;

    juce::Label thresholdLabel;
    juce::Label ratioLabel;
    juce::Label attackLabel;
    juce::Label releaseLabel;
    juce::Label makeupLabel;

    // Meter displays
    juce::Rectangle<int> inputMeterBounds;
    juce::Rectangle<int> outputMeterBounds;
    juce::Rectangle<int> gainReductionMeterBounds;

    // For visual feedback
    juce::Image inputWaveformImage;
    juce::Image outputWaveformImage;

    // Timer callback for updating the GUI
    void timerCallback() override;

    // Helper methods for drawing meters
    void drawMeter (juce::Graphics& g, const juce::Rectangle<int>& bounds, float levelDB, const juce::String& label, juce::Colour colour);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MambaAudioProcessorEditor)
};

#endif  // JUCE_PLUGIN_EDITOR_H_INCLUDED
