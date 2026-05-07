#ifndef JUCE_PLUGIN_PROCESSOR_H_INCLUDED
#define JUCE_PLUGIN_PROCESSOR_H_INCLUDED

#include <JuceHeader.h>
#include "../include/clap-plugin.hpp"

// Forward declaration to avoid circular dependency
namespace mamba_audio {
    class MambaClapPlugin;
}

class MambaAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    MambaAudioProcessor();
    ~MambaAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const juce::AudioProcessor::BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::AudioBuffer<midi>&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Parameters
    std::atomic<float> thresholdParam { -24.0f };   // dB
    std::atomic<float> ratioParam { 4.0f };         // Ratio:1
    std::atomic<float> attackParam { 10.0f };       // ms
    std::atomic<float> releaseParam { 100.0f };     // ms
    std::atomic<float> makeupParam { 0.0f };        // dB
    
    // Visualization parameters
    std::atomic<float> inputLevel { -60.0f };
    std::atomic<float> outputLevel { -60.0f };
    std::atomic<float> gainReduction { 0.0f };
    
    // Reference to our CLAP plugin for actual processing
    std::unique_ptr<mamba_audio::MambaClapPlugin> clapPlugin;

private:
    // For level metering
    juce::dsp::AudioLevelMeter<float> inputMeter;
    juce::dsp::AudioLevelMeter<float> outputMeter;
    
    // For gain reduction metering
    juce::dsp::AudioLevelMeter<float> grMeter;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MambaAudioProcessor)
};

#endif  // JUCE_PLUGIN_PROCESSOR_H_INCLUDED
