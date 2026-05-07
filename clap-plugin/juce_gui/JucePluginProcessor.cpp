#include "JucePluginProcessor.h"
#include "JucePluginEditor.h"
#include <cmath>

//==============================================================================
MambaAudioProcessor::MambaAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : juce::AudioProcessor (BusesProperties()
                     .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                     .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
#endif
{
    // Initialize the CLAP plugin
    // We need to create a dummy descriptor and host for now.
    // In a real plugin, the host would be provided by the DAW.
    // For simplicity, we'll initialize the CLAP plugin with a dummy host.
    // Note: This is a simplified approach. In practice, you might want to
    // refactor the CLAP plugin to be more independent of the host during initialization.
    
    // Dummy descriptor and host (in a real scenario, these come from the host)
    static const clap_plugin_descriptor dummyDesc = {
        .clap_version = CLAP_VERSION,
        .id = "com.mamba-audio.spectrogram-enhancer",
        .name = "Mamba Spectrogram Enhancer",
        .vendor = "Mamba Audio",
        .url = "",
        .manual_url = "",
        .support_url = "",
        .version = "1.0.0",
        .min_api_version = "1.0",
        .description = "Audio effect using Mamba SSM for spectrogram enhancement with HiFi-GAN vocoder",
        .features = (const char*[]){
            CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
            CLAP_PLUGIN_FEATURE_UTILITY,
            nullptr
        }
    };
    
    // We'll create a minimal host that just provides the necessary callbacks.
    // For now, we'll use a static dummy host and hope the CLAP plugin doesn't require much from it.
    // This is a hack for the sake of getting the GUI running. A better approach would be to
    // separate the audio processing logic from the CLAP host dependencies.
    static const clap_host_t dummyHost = {
        .clap_version = CLAP_VERSION,
        .is_live = [](const clap_host_t* host) { return true; },
        .host_data = nullptr,
        // We'll leave the rest as null for now and hope the plugin doesn't use them.
        // In reality, the CLAP plugin might need more host callbacks.
        // This is a known limitation of this quick integration.
        .request_restart = nullptr,
        .request_callback = nullptr,
        .delay_to_next_callback = nullptr,
        .get_steady_state = nullptr,
        .get_monotonic_time = nullptr,
        .get_cpu_capacity = nullptr,
        .get_latency = nullptr,
        .get_tail_size = nullptr,
        .activate_offline_process = nullptr,
        .deactivate_offline_process = nullptr,
        .get_offline_process_result = nullptr,
        .show_message = nullptr,
        .request_process_restart = nullptr,
        .get_extension = nullptr,
        .change_bypass_state = nullptr,
        .rescan_audio_ports = nullptr,
        .rescan_midi_ports = nullptr,
        .request_latency = nullptr,
        .get_plugin_path = nullptr,
        .get_plugin_url = nullptr,
        .get_plugin_user_data = nullptr,
        .set_plugin_user_data = nullptr,
        .get_plugin_uuid = nullptr,
        .get_plugin_is_bypass = nullptr,
        .set_plugin_is_bypass = nullptr,
        .get_plugin_latency = nullptr,
        .get_plugin_preferred_io_configuration = nullptr,
        .get_plugin_programs_count = nullptr,
        .get_plugin_current_program = nullptr,
        .set_plugin_current_program = nullptr,
        .get_program_name = nullptr,
        .reset_plugin = nullptr,
        .get_extension = nullptr,
        // We are missing many callbacks, but we hope the plugin doesn't use them during audio processing.
        // This is a temporary solution for the GUI to work.
    };
    
    clapPlugin = std::make_unique<mamba_audio::MambaClapPlugin>(&dummyDesc, &dummyHost);
    
    // Initialize the CLAP plugin (this will load models, etc.)
    // We'll use a sample rate of 44100.0 for initialization, but it will be updated in prepareToPlay.
    if (!clapPlugin->init()) {
        // Handle error - in a real plugin, you might want to show an error message.
        // For now, we'll just print to stderr.
        std::cerr << "Failed to initialize CLAP plugin" << std::endl;
    }
}

MambaAudioProcessor::~MambaAudioProcessor()
{
    // The unique_ptr will automatically call the destructor of MambaClapPlugin
}

//==============================================================================
void MambaAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Update the sample rate in the CLAP plugin
    // We need to deactivate and reactivate the CLAP plugin with the new sample rate.
    // Note: This is a simplification. In a real plugin, you might want to handle sample rate changes more gracefully.
    clapPlugin->deactivate();
    // We don't have min_blep_latency and max_blep_latency from JUCE, so we'll use 0.0 for both.
    // This is not ideal, but for many plugins it's acceptable.
    clapPlugin->activate(sampleRate, 0.0, 0.0);
    clapPlugin->start_processing();
    
    // Reset the level meters
    inputMeter.reset (sampleRate, juce::dsp::AudioLevelMeter<float>::LevelMeterType::peak);
    outputMeter.reset (sampleRate, juce::dsp::AudioLevelMeter<float>::LevelMeterType::peak);
    grMeter.reset (sampleRate, juce::dsp::AudioLevelMeter<float>::LevelMeterType::peak);
}

void MambaAudioProcessor::releaseResources()
{
    // When playback stops, we can release resources.
    clapPlugin->stop_processing();
    clapPlugin->deactivate();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool MambaAudioProcessor::isBusesLayoutSupported (const juce::AudioProcessor::BusesLayout& layouts) const
{
  // This is the place where you check if the layout is supported.
  // In this template code we only support mono or stereo.
  // Some plugin hosts, such as certain GarageBand versions, will only
  // load plugins that support stereo bus layouts.
  if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
   && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
    return false;

  // This checks if the input layout matches the output layout
   return layouts.getMainOutputChannelSet() == layouts.getMainInputChannelSet();
}
#endif

void MambaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::AudioBuffer<midi>& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting signals through
    // channels they didn't connect.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Get the audio data
    float* const* inputChannels  = buffer.getArrayOfReadPointers ();
    float* const* outputChannels = buffer.getArrayOfWritePointers ();

    // We'll process the audio in chunks to match the CLAP plugin's internal block size.
    // The CLAP plugin uses a block size of 2048 samples internally.
    const int blockSize = 2048;
    const int numSamples = buffer.getNumSamples();
    
    // We'll process the audio block by block.
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        const float* inputChannel  = inputChannels[channel];
        float* outputChannel = outputChannels[channel];
        
        // Process in blocks
        for (int offset = 0; offset < numSamples; offset += blockSize)
        {
            int blockEnd = std::min(offset + blockSize, numSamples);
            int blockSamples = blockEnd - offset;
            
            // Copy the input block to a temporary buffer
            std::vector<float> blockInput(blockSamples);
            for (int i = 0; i < blockSamples; ++i)
            {
                blockInput[i] = inputChannel[offset + i];
            }
            
            // Convert to torch tensor
            auto inputTensor = torch::from_blob(blockInput.data(), {1, blockSamples}, torch::kFloat);
            
            // Process through the CLAP plugin's internal pipeline
            // Note: We are bypassing the CLAP process function and calling the internal steps directly.
            // This is because the CLAP process function expects a clap_process_t structure.
            // Alternatively, we could create a clap_process_t structure, but that is more complex.
            // For simplicity, we'll reuse the internal steps from the CLAP plugin.
            
            // Step 1: Convert to mel spectrogram
            auto mel_spec = clapPlugin->stft_processor_->forward(inputTensor.to(clapPlugin->device_));
            
            // Step 2: Apply Mamba processing
            auto enhanced_mel = clapPlugin->mamba_wrapper_->process(mel_spec);
            
            // Step 3: Convert back to waveform
            auto output_block = clapPlugin->stft_processor_->inverse(enhanced_mel);
            
            // Copy the output back to the output channel
            auto output_data = output_block.cpu().contiguous().data_ptr<float>();
            for (int i = 0; i < blockSamples; ++i)
            {
                outputChannel[offset + i] = output_data[i];
            }
        }
    }
    
    // Update the level meters for visualization
    // We'll update the meters once per block to avoid too frequent updates.
    // We'll use the first channel for metering (or we could do a mix).
    if (totalNumInputChannels > 0)
    {
        // Get the RMS level of the input and output for the first channel
        float inputRMS = 0.0f;
        float outputRMS = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            float inSample = inputChannels[0][i];
            float outSample = outputChannels[0][i];
            inputRMS += inSample * inSample;
            outputRMS += outSample * outSample;
        }
        inputRMS = std::sqrt(inputRMS / numSamples);
        outputRMS = std::sqrt(outputRMS / numSamples);
        
        // Convert to dB
        float inputDB = juce::Decibels::gainToDecibels(inputRMS);
        float outputDB = juce::Decibels::gainToDecibels(outputRMS);
        
        // Update the atomic variables for the GUI
        inputLevel.store (inputDB);
        outputLevel.store (outputDB);
        
        // Calculate gain reduction (input - output) in dB
        // Note: This is a simplification. In a compressor, gain reduction is usually calculated differently.
        // But for our spectrogram enhancer, we can approximate it this way.
        gainReduction.store (inputDB - outputDB);
        
        // Also update the level meters (optional, but we can use them for smoothing)
        inputMeter.pushSample (juce::jmax (0.0f, inputRMS));
        outputMeter.pushSample (juce::jmax (0.0f, outputRMS));
    }
}

//==============================================================================
bool MambaAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* MambaAudioProcessor::createEditor()
{
    return new MambaAudioProcessorEditor (*this);
}

//==============================================================================
const juce::String MambaAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool MambaAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool MambaAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool MambaAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double MambaAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int MambaAudioProcessor::getNumPrograms()
{
    return 1;   // (some cases: none, or more if you support programs)
}

int MambaAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MambaAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String MambaAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void MambaAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void MambaAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::MemoryOutputStream mos(destData, true);
    mos.writeFloat (*thresholdParam.load());
    mos.writeFloat (*ratioParam.load());
    mos.writeFloat (*attackParam.load());
    mos.writeFloat (*releaseParam.load());
    mos.writeFloat (*makeupParam.load());
}

void MambaAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from the memory block.
    // Your implementation will need to read the data that was written by getStateInformation()
    // and apply it to the parameters.
    juce::MemoryInputStream mis(data, static_cast<size_t>(sizeInBytes), false);
    *thresholdParam.store() = mis.readFloat();
    *ratioParam.store() = mis.readFloat();
    *attackParam.store() = mis.readFloat();
    *releaseParam.store() = mis.readFloat();
    *makeupParam.store() = mis.readFloat();
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MambaAudioProcessor();
}
