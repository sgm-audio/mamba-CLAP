#pragma once
#ifdef MAMBA_ENABLE_ARA
// ARADocumentController — implements the ARA 2 DocumentController role
// using the clap-ara bridge (header-only shim over ARA_API).
//
// Thread model for ARA:
//   • Document thread  = main thread in all target hosts (Reaper, Bitwig, Logic).
//   • doUpdateAudioSourceContent / doUpdatePlaybackRegionProperties are called
//     on the document thread; they post work to the InferenceQueue (thread pool).
//   • Analysis results are written back to ARARegion::analysisGrade via an
//     async callback which also touches the ARA document — still document thread.
//
// Ownership:
//   PluginCore owns ARADocumentController.
//   ARADocumentController owns ARARegionAnalyzer (via unique_ptr).
//   The ARA host owns the document/region/source objects; we never delete them.
//
// Serialisation:
//   ARA document state = host-managed XML/binary blob per project.
//   We store our per-source analysis cache inside the ARA Persistent IDs via
//   doStoreObjectsToArchive / doRestoreObjectsFromArchive.

#include <ARA_API/ARAInterface.h>
#include <clap-ara/clap_ara_bridge.h>
#include <memory>
#include <unordered_map>
#include <string>
#include <functional>
#include <mutex>

namespace mamba::model { class InferenceQueue; }
namespace mamba::core  { class ParameterBank; }

namespace mamba::ara {

// ── Per-audio-source analysis cache ──────────────────────────────────────────
struct SourceAnalysis {
    std::string persistent_id;
    float       avg_rms{0.f};
    float       spectral_flux{0.f};
    float       crest_factor{0.f};
    bool        completed{false};
};

// ── Per-region metadata (fed from SourceAnalysis + region geometry) ───────────
struct RegionMeta {
    ARA::ARAPlaybackRegionRef ref{nullptr};
    std::string               name;
    double                    start_sec{0.0};
    double                    duration_sec{0.0};
    uint32_t                  bar_offset{0};
    uint32_t                  bar_length{1};
    float                     energy_rms{0.f};
    float                     spectral_flux{0.f};
};

// ─────────────────────────────────────────────────────────────────────────────
class ARADocumentController final : public ClapARADocumentController {
public:
    using AnalysisReadyCallback = std::function<void(std::string json_result)>;

    explicit ARADocumentController(const ARA::ARADocumentControllerHostInstance* instance,
                                   model::InferenceQueue&  queue,
                                   core::ParameterBank&    params);
    ~ARADocumentController() override;

    // Called by PluginCore when the host requests ARA extension pointer.
    static const ARA::ARADocumentControllerInterface* get_interface() noexcept;

    // Optional callback invoked after each arrangement analysis completes
    // (used to update UI via a separate notification path).
    void set_analysis_ready_callback(AnalysisReadyCallback cb) {
        std::lock_guard lock(cb_mutex_);
        on_analysis_ready_ = std::move(cb);
    }

protected:
    // ── ARA document lifecycle ───────────────────────────────────────────────
    void willBeginEditing() override;
    void didEndEditing()    override;

    // ── Audio source callbacks ───────────────────────────────────────────────
    void doUpdateAudioSourceContent(
        ARA::ARAAudioSourceRef          source_ref,
        const ARA::ARAContentTimeRange* range,
        ARA::ARAContentUpdateFlags      flags) override;

    void doEnableAudioSourceSamplesAccess(
        ARA::ARAAudioSourceRef source_ref,
        bool                   enable) override;

    // ── Playback region callbacks ────────────────────────────────────────────
    void doUpdatePlaybackRegionProperties(
        ARA::ARAPlaybackRegionRef region_ref) override;

    // ── Persistence ─────────────────────────────────────────────────────────
    bool doStoreObjectsToArchive(ARA::ARAArchiveWriterHostRef writer,
                                 const ARA::ARAStoreObjectsFilter* filter) override;

    bool doRestoreObjectsFromArchive(ARA::ARAArchiveReaderHostRef reader,
                                     const ARA::ARARestoreObjectsFilter* filter) override;

private:
    void schedule_analysis() noexcept;
    void run_arrangement_analysis();

    // Collect all regions in document order.
    std::vector<RegionMeta> collect_regions() const;

    // Dispatched to InferenceQueue; runs on worker thread.
    void analyze_audio_source(ARA::ARAAudioSourceRef source_ref) noexcept;

    model::InferenceQueue&  queue_;
    core::ParameterBank&    params_;

    bool editing_in_progress_{false};
    bool analysis_pending_{false};

    // Source analyses keyed by ARA persistent ID.
    mutable std::mutex                                 sources_mutex_;
    std::unordered_map<std::string, SourceAnalysis>    sources_;

    mutable std::mutex     cb_mutex_;
    AnalysisReadyCallback  on_analysis_ready_;

    // Debounce: analysis fires 300ms after last edit burst.
    // In the clap-ara bridge we use a simple generation counter instead of timers.
    std::atomic<uint64_t>  edit_generation_{0};
};

} // namespace mamba::ara
#endif // MAMBA_ENABLE_ARA
