#include "clap/ClapExtensions.hpp"
#include "clap/ClapPlugin.hpp"
#include "core/ParameterBank.hpp"
#include "core/PluginCore.hpp"

#include <clap/ext/params.h>
#include <clap/ext/state.h>
#include <clap/ext/latency.h>
#include <clap/ext/tail.h>
#include <clap/ext/audio-ports.h>
#include <cassert>
#include <cstring>

using namespace mamba::core;
using namespace mamba::clap_adapter;

// ─── Helper: recover PluginCore from the plugin pointer ──────────────────────
static PluginCore& core_of(const clap_plugin_t* p) noexcept {
    return static_cast<ClapPlugin*>(p->plugin_data)->core();
}

// ─── clap_plugin_params_t ─────────────────────────────────────────────────────
static uint32_t params_count(const clap_plugin_t*) noexcept {
    return kParamCount;
}

static bool params_get_info(const clap_plugin_t*,
                             uint32_t index,
                             clap_param_info_t* out) noexcept {
    return ParameterBank::fill_info(index, out);
}

static bool params_get_value(const clap_plugin_t* p,
                              clap_id param_id,
                              double* out_value) noexcept {
    *out_value = core_of(p).params().get(static_cast<ParamId>(param_id));
    return true;
}

static bool params_value_to_text(const clap_plugin_t*,
                                  clap_id param_id,
                                  double value,
                                  char* out_buf,
                                  uint32_t capacity) noexcept {
    return ParameterBank::value_to_text(param_id, value, out_buf, capacity);
}

static bool params_text_to_value(const clap_plugin_t*,
                                  clap_id param_id,
                                  const char* text,
                                  double* out_value) noexcept {
    return ParameterBank::text_to_value(param_id, text, out_value);
}

static void params_flush(const clap_plugin_t* p,
                          const clap_input_events_t* in,
                          const clap_output_events_t*) noexcept {
    if (!in) return;
    const uint32_t count = in->size(in);
    for (uint32_t i = 0; i < count; ++i) {
        const auto* hdr = in->get(in, i);
        if (!hdr || hdr->type != CLAP_EVENT_PARAM_VALUE) continue;
        const auto* ev = reinterpret_cast<const clap_event_param_value_t*>(hdr);
        core_of(p).params().set(static_cast<ParamId>(ev->param_id),
                                 static_cast<float>(ev->value));
    }
}

const clap_plugin_params_t mamba::clap_adapter::kParamsExt = {
    .count          = params_count,
    .get_info       = params_get_info,
    .get_value      = params_get_value,
    .value_to_text  = params_value_to_text,
    .text_to_value  = params_text_to_value,
    .flush          = params_flush,
};

// ─── clap_plugin_state_t ──────────────────────────────────────────────────────
static bool state_save(const clap_plugin_t* p, const clap_ostream_t* stream) noexcept {
    return core_of(p).save_state(stream);
}

static bool state_load(const clap_plugin_t* p, const clap_istream_t* stream) noexcept {
    return core_of(p).load_state(stream);
}

const clap_plugin_state_t mamba::clap_adapter::kStateExt = {
    .save = state_save,
    .load = state_load,
};

// ─── clap_plugin_latency_t ────────────────────────────────────────────────────
// Latency = hop_length samples (STFT analysis window advance).
// 256 samples at 44.1 kHz ≈ 5.8 ms — reported once; stable after activate().
static uint32_t latency_get(const clap_plugin_t*) noexcept {
    return 256u;  // kHopLength — must match STFTEngine
}

const clap_plugin_latency_t mamba::clap_adapter::kLatencyExt = {
    .get = latency_get,
};

// ─── clap_plugin_tail_t ───────────────────────────────────────────────────────
// Return the STFT synthesis tail: one full window after last input sample.
static uint32_t tail_get(const clap_plugin_t*) noexcept {
    return 1024u;  // kNFFT — STFT analysis window
}

const clap_plugin_tail_t mamba::clap_adapter::kTailExt = {
    .get = tail_get,
};

// ─── clap_plugin_audio_ports_t ────────────────────────────────────────────────
static uint32_t audio_ports_count(const clap_plugin_t*, bool) noexcept {
    return 1u;  // One stereo in, one stereo out
}

static bool audio_ports_get(const clap_plugin_t*,
                             uint32_t index,
                             bool is_input,
                             clap_audio_port_info_t* out) noexcept {
    if (index != 0) return false;
    out->id                = 0;
    out->channel_count     = 2;
    out->flags             = CLAP_AUDIO_PORT_IS_MAIN;
    out->port_type         = CLAP_PORT_STEREO;
    out->in_place_pair     = is_input ? CLAP_INVALID_ID : 0;
    std::strncpy(out->name, is_input ? "Audio In" : "Audio Out",
                 sizeof(out->name) - 1);
    return true;
}

const clap_plugin_audio_ports_t mamba::clap_adapter::kAudioPortsExt = {
    .count = audio_ports_count,
    .get   = audio_ports_get,
};
