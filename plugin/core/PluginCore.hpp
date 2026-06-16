#pragma once
// PluginCore — the single ownership root of every plugin subsystem.
//
// Lifetime:
//   Created on the main thread in clap_plugin_t::init().
//   Destroyed on the main thread in clap_plugin_t::destroy().
//
// Thread model (no shared mutable state crosses these boundaries without
// the explicit mechanisms listed):
//
//   ┌─────────────┐  AudioCommandQueue (SPSC, lock-free)   ┌──────────────┐
//   │  Main/UI    │ ────────────────────────────────────►  │  Audio RT    │
//   │  Thread     │ ◄────────────────────────────────────  │  Thread      │
//   └─────────────┘  InferenceResultQueue (SPSC,lock-free) └──────┬───────┘
//          │                                                       │
//          │  std::mutex (non-RT, model-load only)                 │ AudioRingBuffer
//          ▼                                                       ▼
//   ┌──────────────┐                                       ┌──────────────┐
//   │  Inference   │ ◄───── MambaSession::step() ──────── │  Inference   │
//   │  Thread Pool │ ──── InferenceResult → queue ───────►│  Thread      │
//   └──────────────┘                                       └──────────────┘
//          │
//          │  ARA document callbacks (document thread ≡ main in most hosts)
//          ▼
//   ┌──────────────┐
//   │  ARA Doc     │  ARARegionAnalyzer runs per-region on thread pool
//   │  Controller  │
//   └──────────────┘

#include <memory>
#include <atomic>
#include <clap/clap.h>

namespace mamba::core   { class ParameterBank; class TransportState; }
namespace mamba::core   { class AudioCommandQueue; class InferenceResultQueue; }
namespace mamba::dsp    { class STFTEngine; class OverlapAddSynthesizer;
                          class AudioRingBuffer; class EffectApplicator; }
namespace mamba::model  { class MambaInferenceEngine; class MambaSession; }
namespace mamba::state  { class StateSerializer; }
#ifdef MAMBA_ENABLE_ARA
namespace mamba::ara    { class ARADocumentController; }
#endif

namespace mamba::core {

class PluginCore {
public:
    // Constructed from the CLAP factory on main thread.
    explicit PluginCore(const clap_host_t* host);

    // RAII: all sub-objects destroyed in a well-defined order.
    ~PluginCore();

    // Non-copyable, non-movable; identity matters (raw ptr shared with C API).
    PluginCore(const PluginCore&)            = delete;
    PluginCore& operator=(const PluginCore&) = delete;
    PluginCore(PluginCore&&)                 = delete;
    PluginCore& operator=(PluginCore&&)      = delete;

    // ── Activation lifecycle (main thread) ──────────────────────────────────
    bool activate(double sample_rate, uint32_t min_frames, uint32_t max_frames);
    void deactivate();
    bool start_processing();
    void stop_processing();

    // ── Real-time audio callback (audio thread) ──────────────────────────────
    // Returns CLAP_PROCESS_CONTINUE or CLAP_PROCESS_SLEEP.
    clap_process_status process(const clap_process_t* proc) noexcept;

    // ── Parameter access (any thread) ───────────────────────────────────────
    ParameterBank&        params()          noexcept { return *params_; }
    const ParameterBank&  params()    const noexcept { return *params_; }
    TransportState&       transport()       noexcept { return *transport_; }

    // ── State serialisation (main thread) ───────────────────────────────────
    bool save_state(const clap_ostream_t* stream) const;
    bool load_state(const clap_istream_t* stream);

    // ── Model hot-swap (inference thread posts; audio thread applies) ────────
    void notify_model_ready(void* new_session_ptr) noexcept;

    const clap_host_t* host() const noexcept { return host_; }

#ifdef MAMBA_ENABLE_ARA
    mamba::ara::ARADocumentController& ara_controller() noexcept {
        return *ara_controller_;
    }
#endif

private:
    // Processing helpers called from process() — all noexcept, no alloc.
    void drain_command_queue() noexcept;
    void handle_input_events(const clap_input_events_t* in) noexcept;
    void run_dsp(const clap_process_t* proc) noexcept;
    void post_output_events(const clap_output_events_t* out) noexcept;

    // ── Owned subsystems — destruction order is declaration order (reverse) ──
    // Destroyed last:
    const clap_host_t*  host_;                        // non-owning, lifetime: plugin

    std::unique_ptr<ParameterBank>           params_;
    std::unique_ptr<TransportState>          transport_;
    std::unique_ptr<AudioCommandQueue>       cmd_queue_;       // UI→Audio
    std::unique_ptr<InferenceResultQueue>    result_queue_;    // Inference→Audio

    // DSP objects pre-allocate all memory during activate(); no alloc at RT.
    std::unique_ptr<dsp::AudioRingBuffer>       audio_ring_;
    std::unique_ptr<dsp::STFTEngine>            stft_;
    std::unique_ptr<dsp::OverlapAddSynthesizer> ola_;
    std::unique_ptr<dsp::EffectApplicator>      effect_;

    // Model objects live on background threads; session swapped atomically.
    std::unique_ptr<model::MambaInferenceEngine> inference_engine_;
    std::atomic<model::MambaSession*>            active_session_{nullptr}; // owned by engine

    // State serialiser bridges CLAP params ↔ ARA doc model.
    std::unique_ptr<state::StateSerializer>   serialiser_;

#ifdef MAMBA_ENABLE_ARA
    std::unique_ptr<ara::ARADocumentController> ara_controller_;
#endif

    // Operational flags (atomic so audio thread can read without locking).
    std::atomic<bool>  is_active_{false};
    std::atomic<bool>  is_processing_{false};
    std::atomic<bool>  bypass_{false};

    double   sample_rate_{44100.0};
    uint32_t max_frames_{0};
};

} // namespace mamba::core
