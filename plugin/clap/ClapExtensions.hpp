#pragma once
// Static extension vtables for params, state, latency, tail, audio-ports.
// Each vtable lives in static storage; no heap involved.

#include <clap/ext/params.h>
#include <clap/ext/state.h>
#include <clap/ext/latency.h>
#include <clap/ext/tail.h>
#include <clap/ext/audio-ports.h>

namespace mamba::clap_adapter {

// ── clap_plugin_params_t ──────────────────────────────────────────────────────
extern const clap_plugin_params_t kParamsExt;

// ── clap_plugin_state_t ───────────────────────────────────────────────────────
extern const clap_plugin_state_t  kStateExt;

// ── clap_plugin_latency_t ─────────────────────────────────────────────────────
extern const clap_plugin_latency_t kLatencyExt;

// ── clap_plugin_tail_t ────────────────────────────────────────────────────────
extern const clap_plugin_tail_t    kTailExt;

// ── clap_plugin_audio_ports_t ─────────────────────────────────────────────────
extern const clap_plugin_audio_ports_t kAudioPortsExt;

#ifdef MAMBA_ENABLE_ARA
// Returns the ARA bridge extension interface (opaque to the non-ARA build).
const void* get_ara_extension() noexcept;
#endif

} // namespace mamba::clap_adapter
