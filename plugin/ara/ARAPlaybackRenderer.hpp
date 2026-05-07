#pragma once
#ifdef MAMBA_ENABLE_ARA
// ARAPlaybackRenderer — renders audio from ARA audio sources for a set of
// playback regions.  In MambaFX this is the offline processing path: when the
// host requests offline rendering (e.g. export), the renderer pulls audio via
// ARA sample access, runs full Mamba inference (no real-time budget), and
// writes the processed output back through the ARA render callback.
//
// Ownership: DocumentController creates one PlaybackRenderer per render
// context (some hosts create several for parallel region rendering).
// PluginCore does not directly own this; the ARA host manages its lifetime
// through the DocumentController.
//
// Thread: the host calls renderPlaybackRegions on whichever thread it chooses
// (often a dedicated render thread). All calls for a given renderer are
// serialised by the host. We must NOT touch the document thread state.

#include <ARA_API/ARAInterface.h>
#include <clap-ara/clap_ara_bridge.h>
#include <memory>

namespace mamba::model { class MambaSession; class MambaInferenceEngine; }

namespace mamba::ara {

class ARAPlaybackRenderer final : public ClapARAPlaybackRenderer {
public:
    explicit ARAPlaybackRenderer(const ARA::ARAPlaybackRendererHostInstance* instance,
                                 model::MambaInferenceEngine& engine);
    ~ARAPlaybackRenderer() override;

protected:
    // ── ARA PlaybackRenderer interface ──────────────────────────────────────
    bool startRendering(ARA::ARASampleCount max_samples_per_slice,
                        double              sample_rate) override;

    void stopRendering() override;

    bool renderPlaybackRegions(float* const*    buffers,
                               ARA::ARASamplePosition sample_position,
                               ARA::ARASampleCount    sample_count,
                               bool                   realtime) override;

private:
    model::MambaInferenceEngine& engine_;

    // Dedicated per-renderer session; holds independent Mamba recurrent state
    // so parallel offline renders don't share state with real-time processing.
    std::unique_ptr<model::MambaSession> session_;

    double   sample_rate_{44100.0};
    uint32_t max_slice_{0};
};

} // namespace mamba::ara
#endif // MAMBA_ENABLE_ARA
