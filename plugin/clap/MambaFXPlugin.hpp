#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// MambaFXPlugin — production-ready CLAP 1.2 plugin skeleton (C++20)
//
// Audio configuration: STEREO (2 channels in / 2 channels out), in-place capable
// Process mode:        block-based, variable frame count (min=1, max=MAX_BLOCK)
// Parameters (8):      EffectType · WetMix · InputGain · OutputGain ·
//                      ContextLength · ModelVariant · InferenceDevice · Bypass
//
// RT-safety contract (enforced throughout process()):
//   ✗ No heap allocation  — all memory pre-allocated in activate()
//   ✗ No std::mutex       — SPSC queues + atomics only
//   ✗ No system calls     — no mmap, read, write, gettimeofday, etc.
//   ✗ No libtorch         — inference runs on dedicated worker thread
//   ✗ No exceptions       — all paths noexcept; errors silently bypass
//   ✓ Bounded loops only  — no unbounded waits or retries
//   ✓ Wait-free reads     — atomic<float> params, SPSC queue pop
//
// Thread boundaries are marked with:
//   [MAIN]      — main/UI thread (init, destroy, activate, deactivate, save/load)
//   [AUDIO-RT]  — real-time audio thread (process, params_flush, start/stop)
//   [ANY]       — safe to call from any thread (atomics, SPSC push/pop)
// ─────────────────────────────────────────────────────────────────────────────

#include <clap/clap.h>
#include <clap/ext/params.h>
#include <clap/ext/state.h>
#include <clap/ext/latency.h>
#include <clap/ext/tail.h>
#include <clap/ext/audio-ports.h>
#include <clap/ext/note-ports.h>
#include <clap/factory/plugin-factory.h>

#include "core/ParameterBank.hpp"
#include "core/EventQueue.hpp"
#include "core/TransportState.hpp"
#include "dsp/SmoothedValue.hpp"
#include "dsp/AudioRingBuffer.hpp"
#include "dsp/EffectApplicator.hpp"

#include <array>
#include <atomic>
#include <memory>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cassert>
#include <algorithm>
#include <cmath>

namespace mamba::clap_adapter {

// ─── Constants ───────────────────────────────────────────────────────────────
inline constexpr uint32_t kChannels    = 2;        // stereo
inline constexpr uint32_t kMaxBlock    = 8192;     // max frames per process()
inline constexpr uint32_t kLatency     = 256;      // hop_length: reported to host
inline constexpr uint32_t kTailSamples = 1024;     // STFT window tail
inline constexpr double   kSmoothMs    = 20.0;     // param ramp time in ms
inline constexpr uint32_t kEventCapacity = 256;    // max events drained per block

// ─── clap-validator compliance assertions ────────────────────────────────────
// These fire at compile time to catch spec violations early.
static_assert(kChannels >= 1 && kChannels <= 64,
    "CLAP: channel count must be 1..64");
static_assert(kLatency < kMaxBlock * 16,
    "CLAP: reported latency must be less than 16× max block size (practical rule)");

// ─────────────────────────────────────────────────────────────────────────────
// ParamSmootherBank — one SmoothedValue per automatable parameter.
// Owned by MambaFXPlugin; only accessed from [AUDIO-RT].
// ─────────────────────────────────────────────────────────────────────────────
class ParamSmootherBank {
public:
    using core::kParamCount;
    using core::kParamMeta;
    using core::ParamId;

    // [MAIN] Call after sample rate is known.
    void prepare(double sample_rate, double ramp_ms) noexcept {
        const double ramp_s = ramp_ms * 0.001;
        for (uint32_t i = 0; i < kParamCount; ++i) {
            smoothers_[i].prepare(sample_rate, ramp_s);
            smoothers_[i].reset(static_cast<float>(kParamMeta[i].default_value));
        }
    }

    // [AUDIO-RT] Set the target for a parameter (called after draining events).
    void set_target(ParamId id, float value) noexcept {
        smoothers_[static_cast<uint32_t>(id)].set_target(value);
    }

    // [AUDIO-RT] Advance all smoothers by one sample.
    void tick_all() noexcept {
        for (auto& s : smoothers_) s.next();
    }

    // [AUDIO-RT] Current smoothed value for a param.
    float get(ParamId id) const noexcept {
        return smoothers_[static_cast<uint32_t>(id)].get_current();
    }

    // [AUDIO-RT] Skip n samples for all params (silence gate).
    void skip(uint32_t n) noexcept {
        for (auto& s : smoothers_) s.skip(n);
    }

    bool all_at_target() const noexcept {
        for (const auto& s : smoothers_)
            if (!s.is_at_target()) return false;
        return true;
    }

private:
    std::array<dsp::SmoothedValue<float>, kParamCount> smoothers_{};
};

// ─────────────────────────────────────────────────────────────────────────────
// MambaFXPlugin — the one class the host interacts with.
//
// Ownership:
//   Created by factory_create_plugin(); destroyed by clap_plugin_t::destroy().
//   Owns PluginCore (full subsystem tree) via unique_ptr.
//   The clap_plugin_t handle is a value member; its address is stable because
//   MambaFXPlugin itself is heap-allocated and never moved.
// ─────────────────────────────────────────────────────────────────────────────
class MambaFXPlugin {
public:
    // [MAIN] Constructor — called from factory_create_plugin().
    explicit MambaFXPlugin(const clap_host_t* host);

    // [MAIN] Destructor — called by s_destroy() on the main thread.
    ~MambaFXPlugin();

    // Non-copyable, non-movable.
    MambaFXPlugin(const MambaFXPlugin&)            = delete;
    MambaFXPlugin& operator=(const MambaFXPlugin&) = delete;

    // Returns the stable clap_plugin_t* the host stores.
    const clap_plugin_t* handle() const noexcept { return &plugin_; }

    // ── Extension pointers (static storage, returned by s_get_extension) ─────
    static const clap_plugin_params_t     kParamsExt;
    static const clap_plugin_state_t      kStateExt;
    static const clap_plugin_latency_t    kLatencyExt;
    static const clap_plugin_tail_t       kTailExt;
    static const clap_plugin_audio_ports_t kAudioPortsExt;

private:
    // ── clap_plugin_t vtable — all static, all noexcept ──────────────────────
    // [MAIN]
    static bool s_init            (const clap_plugin_t*) noexcept;
    // [MAIN]
    static void s_destroy         (const clap_plugin_t*) noexcept;
    // [MAIN]
    static bool s_activate        (const clap_plugin_t*, double sr,
                                   uint32_t min_f, uint32_t max_f) noexcept;
    // [MAIN]
    static void s_deactivate      (const clap_plugin_t*) noexcept;
    // [AUDIO-RT]
    static bool s_start_processing(const clap_plugin_t*) noexcept;
    // [AUDIO-RT]
    static void s_stop_processing (const clap_plugin_t*) noexcept;
    // [AUDIO-RT] — resets all DSP state; called after host seek or re-init
    static void s_reset           (const clap_plugin_t*) noexcept;
    // [AUDIO-RT] — hot path; must satisfy RT-safety contract
    static clap_process_status
                s_process         (const clap_plugin_t*,
                                   const clap_process_t*) noexcept;
    // [MAIN]
    static const void*
                s_get_extension   (const clap_plugin_t*, const char*) noexcept;
    // [MAIN] — deferred work posted by inference thread
    static void s_on_main_thread  (const clap_plugin_t*) noexcept;

    // ── params extension callbacks ────────────────────────────────────────────
    // [MAIN or AUDIO-RT]
    static uint32_t  params_count      (const clap_plugin_t*) noexcept;
    static bool      params_get_info   (const clap_plugin_t*, uint32_t,
                                        clap_param_info_t*) noexcept;
    static bool      params_get_value  (const clap_plugin_t*, clap_id,
                                        double*) noexcept;
    static bool      params_value_to_text(const clap_plugin_t*, clap_id, double,
                                          char*, uint32_t) noexcept;
    static bool      params_text_to_value(const clap_plugin_t*, const char*,
                                          clap_id, double*) noexcept;
    // [AUDIO-RT or MAIN — called when not processing]
    static void      params_flush      (const clap_plugin_t*,
                                        const clap_input_events_t*,
                                        const clap_output_events_t*) noexcept;

    // ── state extension callbacks ─────────────────────────────────────────────
    // [MAIN]
    static bool state_save(const clap_plugin_t*, const clap_ostream_t*) noexcept;
    // [MAIN]
    static bool state_load(const clap_plugin_t*, const clap_istream_t*) noexcept;

    // ── latency / tail ────────────────────────────────────────────────────────
    static uint32_t latency_get(const clap_plugin_t*) noexcept;
    static uint32_t tail_get   (const clap_plugin_t*) noexcept;

    // ── audio-ports ───────────────────────────────────────────────────────────
    static uint32_t audio_ports_count(const clap_plugin_t*, bool) noexcept;
    static bool     audio_ports_get  (const clap_plugin_t*, uint32_t,
                                       bool, clap_audio_port_info_t*) noexcept;

    // ── Internal process() helpers — all [AUDIO-RT], all noexcept ────────────
    void drain_events       (const clap_input_events_t*,
                             dsp::AutomationEvent* scratch,
                             uint32_t& n_events_out) noexcept;
    void apply_event        (const dsp::AutomationEvent&) noexcept;
    void process_block      (const float* const* in, float* const* out,
                             uint32_t start, uint32_t end) noexcept;
    void apply_param_smoothing(uint32_t sample) noexcept;

    // ── Recover self from opaque plugin_data ──────────────────────────────────
    static MambaFXPlugin* self(const clap_plugin_t* p) noexcept {
        return static_cast<MambaFXPlugin*>(p->plugin_data);
    }

    // ── Members — grouped by access thread ───────────────────────────────────

    // [MAIN] — stable C handle given to host
    clap_plugin_t        plugin_{};
    const clap_host_t*   host_{nullptr};

    // [MAIN] — host extension pointers cached at init()
    const clap_host_params_t*   host_params_{nullptr};
    const clap_host_latency_t*  host_latency_{nullptr};

    // [ANY] — atomic parameter values (written by host automation, read at RT)
    core::ParameterBank  params_;

    // [ANY] — transport mirror written by audio thread, read by inference thread
    core::TransportState transport_;

    // [ANY] — command queue: UI/main → audio thread
    core::AudioCommandQueue cmd_queue_;

    // [AUDIO-RT] — parameter smoothers; one per param
    ParamSmootherBank    smoothers_;

    // [AUDIO-RT] — per-sample smoothed gain staging
    float                input_gain_linear_{1.f};
    float                output_gain_linear_{1.f};

    // [AUDIO-RT] — pre-allocated DSP buffers (set in activate, freed in deactivate)
    std::unique_ptr<dsp::AudioRingBuffer> audio_ring_;
    std::unique_ptr<dsp::EffectApplicator> effect_;

    // [AUDIO-RT] — scratch buffer for in-order automation events (stack-like)
    // Fixed capacity avoids any allocation inside process().
    std::array<dsp::AutomationEvent, kEventCapacity> event_scratch_{};

    // [ATOMIC] — operational flags
    std::atomic<bool> is_active_{false};
    std::atomic<bool> is_processing_{false};
    std::atomic<bool> bypass_{false};
    std::atomic<bool> request_latency_update_{false};

    double   sample_rate_{44100.0};
    uint32_t max_frames_{kMaxBlock};
};

// ─────────────────────────────────────────────────────────────────────────────
// Factory descriptor — static storage; address is stable for the process lifetime.
// ─────────────────────────────────────────────────────────────────────────────
extern const clap_plugin_descriptor_t kMambaFXDescriptor;
extern const clap_plugin_factory_t    kMambaFXFactory;

} // namespace mamba::clap_adapter
