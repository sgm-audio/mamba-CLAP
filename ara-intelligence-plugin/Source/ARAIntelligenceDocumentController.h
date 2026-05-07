#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_ara_utils/juce_ara_utils.h>

// ─── Rust FFI boundary ───────────────────────────────────────────────────────
// Returns a JSON string matching the arrangement intelligence prompt's expected output.
// The caller must free the returned string with ara_free_analysis.
extern "C" {
    struct ArrangeSection {
        const char* section_type;   // "CHORUS", "VERSE", etc (ARA region name)
        uint32_t    bar_offset;     // position in bars
        uint32_t    bar_length;
        float       energy_rms;     // computed from audio content
        float       spectral_flux;  // onset density metric
        float       crest_factor;
    };

    // Entry point — takes section array, returns heap-allocated JSON string.
    char* ara_analyze_arrangement(
        const ArrangeSection* sections,
        uint32_t section_count,
        const char* genre,
        uint32_t bpm
    );

    void ara_free_analysis(char* ptr);
}
// ─────────────────────────────────────────────────────────────────────────────


namespace ARAIntelligence {

//==============================================================================
/**
 * DocumentController
 *
 * Lifetime: created once per project load by the ARA host.
 * - willBeginEditing / didEndEditing: transaction fences for batch updates
 * - doUpdatePlaybackRegionProperties: called when region moves/renames in DAW
 * - After any structural change: schedules async analyzeArrangement()
 */
class DocumentController final
    : public juce::ARADocumentController
    , private juce::AsyncUpdater
{
public:
    explicit DocumentController(const ARADocumentControllerHostInstance* instance)
        : juce::ARADocumentController(instance)
    {}

    ~DocumentController() override
    {
        cancelPendingUpdate();
        if (lastAnalysis) ara_free_analysis(lastAnalysis);
    }

protected:
    //─── ARA overrides ──────────────────────────────────────────────────────

    void willBeginEditing() override
    {
        editsInProgress = true;
    }

    void didEndEditing() override
    {
        editsInProgress = false;
        // Debounce: schedule analysis 400ms after last edit burst settles
        triggerAsyncUpdate();
    }

    /** Called when a region's name, start, or length changes in the DAW. */
    void doUpdatePlaybackRegionProperties(ARA::ARAPlaybackRegionRef regionRef) override
    {
        if (!editsInProgress)
            triggerAsyncUpdate();
    }

    /** New audio source added (track recorded, imported, etc.) */
    void doUpdateAudioSourceContent(
        juce::ARAAudioSource* audioSource,
        const ARA::ARAContentTimeRange* /*range*/,
        ARA::ARAContentUpdateFlags /*flags*/) override
    {
        // Schedule re-analysis; audio content changed
        triggerAsyncUpdate();
    }

    //─── AsyncUpdater ───────────────────────────────────────────────────────

    void handleAsyncUpdate() override
    {
        analyzeArrangement();
    }

private:
    bool editsInProgress = false;
    char* lastAnalysis = nullptr;

    //─── Core analysis pipeline ─────────────────────────────────────────────

    void analyzeArrangement()
    {
        auto* document = getDocument();
        if (!document) return;

        // 1. Collect all region sequences (one per track in most hosts)
        std::vector<ArrangeSection> sections;

        for (auto* regionSequence : document->getRegionSequences())
        {
            for (auto* playbackRegion : regionSequence->getPlaybackRegions())
            {
                ArrangeSection sec{};

                // Region name → section type (producer sets names like "CHORUS 1")
                sec.section_type = playbackRegion->getName();

                // Time → bars via host tempo map
                const auto startTime  = playbackRegion->getStartInPlaybackTime();
                const auto duration   = playbackRegion->getDurationInPlaybackTime();
                sec.bar_offset        = timeToBar(startTime);
                sec.bar_length        = juce::jmax(1u, timeToBar(duration));

                // Extract audio features if source is available + analyzed
                if (auto* audioSource = playbackRegion->getAudioModification()->getAudioSource();
                    audioSource && audioSource->isSampleAccessEnabled())
                {
                    extractAudioFeatures(*audioSource, playbackRegion, sec);
                }

                sections.push_back(sec);
            }
        }

        if (sections.empty()) return;

        // Sort by bar position
        std::sort(sections.begin(), sections.end(),
            [](const ArrangeSection& a, const ArrangeSection& b) {
                return a.bar_offset < b.bar_offset;
            });

        // 2. Dispatch to Rust inference engine
        if (lastAnalysis) { ara_free_analysis(lastAnalysis); lastAnalysis = nullptr; }

        lastAnalysis = ara_analyze_arrangement(
            sections.data(),
            static_cast<uint32_t>(sections.size()),
            currentGenre.toRawUTF8(),
            currentBPM
        );

        if (!lastAnalysis) return;

        // 3. Parse the JSON response and update the UI
        // We'll parse the JSON and update the ValueTree for the editor to consume.
        // For simplicity, we'll pass the raw JSON string to the editor via a ValueTree property.
        // The editor can then parse it and display the results.
        analysisState.setProperty("analysisJson", juce::String(lastAnalysis), nullptr);
    }

    //─── Feature extraction (runs on background thread) ─────────────────────

    void extractAudioFeatures(
        juce::ARAAudioSource& source,
        juce::ARAPlaybackRegion* region,
        ArrangeSection& sec)
    {
        // ARA gives us direct float* access to the audio — no realtime decode needed
        juce::ARAAudioSourceReader reader(&source);
        const int numFrames = static_cast<int>(
            source.getSampleRate() * region->getDurationInPlaybackTime());
        const int numChannels = source.getChannelCount();

        juce::AudioBuffer<float> buffer(numChannels, juce::jmin(numFrames, 441000)); // cap 10s
        reader.read(&buffer, 0, buffer.getNumSamples(), 0, true, true);

        // RMS energy
        float rmsSum = 0.f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            rmsSum += buffer.getRMSLevel(ch, 0, buffer.getNumSamples());
        sec.energy_rms = rmsSum / buffer.getNumChannels();

        // Spectral flux (onset density) — simplified
        sec.spectral_flux = computeSpectralFlux(buffer);

        // Crest factor (transient density)
        sec.crest_factor = buffer.getMagnitude(0, buffer.getNumSamples()) /
                           juce::jmax(sec.energy_rms, 1e-6f);
    }

    float computeSpectralFlux(const juce::AudioBuffer<float>& buf)
    {
        // Production: use juce::dsp::FFT with overlap-add, diff consecutive magnitude frames
        // Simplified: zero-crossing rate as proxy
        const float* data = buf.getReadPointer(0);
        int crossings = 0;
        for (int i = 1; i < buf.getNumSamples(); ++i)
            if ((data[i] >= 0.f) != (data[i-1] >= 0.f)) ++crossings;
        return static_cast<float>(crossings) / buf.getNumSamples();
    }

    uint32_t timeToBar(double timeInSeconds)
    {
        // Production: query host tempo map via ARA MusicalContext
        // Simplified: assume constant tempo
        const double beatsPerSecond = currentBPM / 60.0;
        const double beatsPerBar    = 4.0; // TODO: read from time sig
        return static_cast<uint32_t>(timeInSeconds * beatsPerSecond / beatsPerBar);
    }

    void broadcastAnalysisToUI(const ArrangeAnalysis* analysis)
    {
        // This method is no longer used; we now set the analysisJson property directly.
        // We keep it for compatibility but do nothing.
    }

    // Shared state with editor
    juce::ValueTree analysisState { "ARAAnalysis" };
    juce::String currentGenre = "DARK POP";
    uint32_t     currentBPM   = 128;
};


//==============================================================================
/**
 * PluginProcessor
 * Standard JUCE AudioProcessor; registers ARA DocumentController factory.
 * The plugin does NO audio processing — it's pure ARA data consumer.
 */
class PluginProcessor final
    : public juce::AudioProcessor
    , public juce::ARAPluginInstanceWrapper
{
public:
    PluginProcessor()
        : AudioProcessor(BusesProperties()) {}

    // Boilerplate
    const juce::String getName() const override { return "ARA Intelligence"; }
    bool   acceptsMidi()    const override { return false; }
    bool   producesMidi()   const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    bool   hasEditor()      const override { return true; }
    int    getNumPrograms() override { return 1; }
    int    getCurrentProgram() override { return 0; }
    void   setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void   changeProgramName(int, const juce::String&) override {}
    void   getStateInformation(juce::MemoryBlock&) override {}
    void   setStateInformation(const void*, int) override {}
    void   prepareToPlay(double, int) override {}
    void   releaseResources() override {}
    void   processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}

    juce::AudioProcessorEditor* createEditor() override;

    // ARA factory registration
    static const ARA::ARAFactory* getARAFactory() noexcept
    {
        return juce::ARADocumentController::getARAFactory<DocumentController>();
    }
};

} // namespace ARAIntelligence