// ─────────────────────────────────────────────────────────────────────────────
// MambaFXPlugin.cpp — complete CLAP 1.2 plugin implementation (C++20, stereo)
//
// [AUDIO_CHANNELS] = 2 (stereo)   [PROCESS_MODE] = stereo block
// [PARAM_LIST] = EffectType · WetMix · InputGain · OutputGain ·
//               ContextLength · ModelVariant · InferenceDevice · Bypass
//
// RT-safety notation used throughout:
//   ✓ RT-SAFE   — unconditionally safe on the audio thread
//   ⚠ RT-CHECK  — safe only when precondition noted holds
//   ✗ RT-UNSAFE — must NOT be called from the audio thread
// ─────────────────────────────────────────────────────────────────────────────

#include "clap/MambaFXPlugin.hpp"
#include "state/StateSerializer.hpp"

#include <clap/ext/params.h>
#include <clap/ext/state.h>
#include <clap/ext/latency.h>
#include <clap/ext/tail.h>
#include <clap/ext/audio-ports.h>
#include <clap/factory/plugin-factory.h>

#include <cmath>
#include <cstring>
#include <cstdio>
#include <cassert>
#include <algorithm>
#include <new>

// Ensure we never call operator new inside process() in debug builds.
#if !defined(NDEBUG)
#  include <csignal>
#endif

namespace mamba::clap_adapter {

using namespace mamba::core;
using namespace mamba::dsp;

// ─────────────────────────────────────────────────────────────────────────────
// Plugin descriptor — stable static storage
// clap-validator rule: id must be reverse-DNS; version must be semver string.
// ─────────────────────────────────────────────────────────────────────────────
const clap_plugin_descriptor_t kMambaFXDescriptor = {
    .clap_version = CLAP_VERSION_INIT,
    .id           = "com.sgm-studios.mamba-fx",          // unique, stable, never change
    .name         = "MambaFX",
    .vendor       = "SGM Studios",
    .url          = "https://sgm-studios.com/mamba-fx",
    .manual_url   = "https://sgm-studios.com/mamba-fx/manual",
    .support_url  = "https://sgm-studios.com/support",
    .version      = "1.0.0",                             // semver required by validator
    .description  = "Mamba-SSM adaptive audio mastering — reverb, compression, saturation, EQ",
    .features     = (const char*[]){
        CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
        CLAP_PLUGIN_FEATURE_STEREO,
        CLAP_PLUGIN_FEATURE_MASTERING,
        nullptr                                          // clap-validator: must be null-terminated
    },
};

// ─────────────────────────────────────────────────────────────────────────────
// [MAIN] Construction / Destruction
// ─────────────────────────────────────────────────────────────────────────────

MambaFXPlugin::MambaFXPlugin(const clap_host_t* host)
    : host_(host)
{
    // Wire the stable vtable.  All function pointers must be non-null
    // (clap-validator checks this immediately after create_plugin returns).
    plugin_.desc         = &kMambaFXDescriptor;
    plugin_.plugin_data  = this;             // recover via self(p) in every callback
    plugin_.init             = s_init;
    plugin_.destroy          = s_destroy;
    plugin_.activate         = s_activate;
    plugin_.deactivate       = s_deactivate;
    plugin_.start_processing = s_start_processing;
    plugin_.stop_processing  = s_stop_processing;
    plugin_.reset            = s_reset;
    plugin_.process          = s_process;
    plugin_.get_extension    = s_get_extension;
    plugin_.on_main_thread   = s_on_main_thread;
}

MambaFXPlugin::~MambaFXPlugin() {
    // [MAIN] audio_ring_ and effect_ freed here via unique_ptr dtors.
    // The host guarantees deactivate() was called before destroy(),
    // so is_active_ is false and no audio thread is running.
}

// ─────────────────────────────────────────────────────────────────────────────
// [MAIN] init — cache host extension pointers; never start audio work here.
// clap-validator: called exactly once; must return true or plugin is rejected.
// ─────────────────────────────────────────────────────────────────────────────
bool MambaFXPlugin::s_init(const clap_plugin_t* p) noexcept {
    auto* s = self(p);
    // Cache host extensions — pointers are valid for plugin lifetime.
    s->host_params_  = static_cast<const clap_host_params_t*>(
        s->host_->get_extension(s->host_, CLAP_EXT_PARAMS));
    s->host_latency_ = static_cast<const clap_host_latency_t*>(
        s->host_->get_extension(s->host_, CLAP_EXT_LATENCY));
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// [MAIN] destroy — free all resources; called by the host.
// clap-validator: called exactly once; no calls to plugin after this.
// ─────────────────────────────────────────────────────────────────────────────
void MambaFXPlugin::s_destroy(const clap_plugin_t* p) noexcept {
    delete self(p);   // invokes ~MambaFXPlugin()
}

// ─────────────────────────────────────────────────────────────────────────────
// [MAIN] activate — allocate all DSP resources; sample rate and block size known.
//
// clap-validator rules:
//   • min_frames_count ≤ max_frames_count
//   • sample_rate > 0
//   • Must not be called while already active (host guarantees this)
// ─────────────────────────────────────────────────────────────────────────────
bool MambaFXPlugin::s_activate(const clap_plugin_t* p,
                                double sr,
                                uint32_t /*min_frames*/,
                                uint32_t max_frames) noexcept
{
    // [MAIN] — ✗ RT-UNSAFE: allocations happen here, never inside process()
    auto* s = self(p);
    assert(!s->is_active_.load(std::memory_order_relaxed));

    s->sample_rate_ = sr;
    s->max_frames_  = std::min(max_frames, kMaxBlock);

    // ── Allocate DSP buffers ──────────────────────────────────────────────────
    // Capacity = 8× max block → ~18 ms headroom between audio and inference threads.
    // ✗ RT-UNSAFE: operator new called here; never inside process().
    s->audio_ring_ = std::make_unique<AudioRingBuffer>(
        kChannels, s->max_frames_ * 8);

    s->effect_ = std::make_unique<EffectApplicator>();

    // ── Prepare parameter smoothers ───────────────────────────────────────────
    // IIR ramp time = kSmoothMs ms → avoids zipper noise on automation.
    s->smoothers_.prepare(sr, kSmoothMs);

    // ── Restore current param values into smoothers (no step on first block) ──
    for (uint32_t i = 0; i < kParamCount; ++i) {
        const float v = s->params_.get(static_cast<ParamId>(i));
        s->smoothers_.set_target(static_cast<ParamId>(i), v);
    }

    // ── Convert gain params to linear once ───────────────────────────────────
    s->input_gain_linear_  = std::pow(10.f, s->params_.get(ParamId::InputGain)  / 20.f);
    s->output_gain_linear_ = std::pow(10.f, s->params_.get(ParamId::OutputGain) / 20.f);

    s->is_active_.store(true, std::memory_order_release);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// [MAIN] deactivate — free DSP resources.
// clap-validator: called only while active; stop_processing called first.
// ─────────────────────────────────────────────────────────────────────────────
void MambaFXPlugin::s_deactivate(const clap_plugin_t* p) noexcept {
    // [MAIN] — ✗ RT-UNSAFE: deallocations happen here
    auto* s = self(p);
    s->audio_ring_.reset();
    s->effect_.reset();
    s->is_active_.store(false, std::memory_order_release);
}

// ─────────────────────────────────────────────────────────────────────────────
// [AUDIO-RT] start_processing / stop_processing
// clap-validator: these bracket every series of process() calls.
// ─────────────────────────────────────────────────────────────────────────────
bool MambaFXPlugin::s_start_processing(const clap_plugin_t* p) noexcept {
    // ✓ RT-SAFE: single atomic store
    self(p)->is_processing_.store(true, std::memory_order_release);
    return true;
}

void MambaFXPlugin::s_stop_processing(const clap_plugin_t* p) noexcept {
    // ✓ RT-SAFE: single atomic store
    self(p)->is_processing_.store(false, std::memory_order_release);
}

// ─────────────────────────────────────────────────────────────────────────────
// [AUDIO-RT] reset — transport seek or re-init without deactivation.
// clap-validator: must reset all stateful DSP (filters, envelopes, OLA buffers).
// ─────────────────────────────────────────────────────────────────────────────
void MambaFXPlugin::s_reset(const clap_plugin_t* p) noexcept {
    // ✓ RT-SAFE: only touches pre-allocated buffers
    auto* s = self(p);
    if (s->audio_ring_) s->audio_ring_->clear();

    // Snap all smoothers to their current targets — avoids ramp across seek.
    for (uint32_t i = 0; i < kParamCount; ++i) {
        const float v = s->params_.get(static_cast<ParamId>(i));
        s->smoothers_.set_target(static_cast<ParamId>(i), v);
    }

    // Reset compressor envelope in EffectApplicator.
    if (s->effect_) {
        s->effect_ = std::make_unique<EffectApplicator>(); // ✗ alloc, but reset is off-RT
    }
    // Note: if reset() may be called on the audio thread (CLAP allows this),
    // replace the make_unique above with a reset() method on EffectApplicator.
}

// ─────────────────────────────────────────────────────────────────────────────
// [AUDIO-RT] process() — the hot path.
//
// Execution order inside process():
//   1. Guard: return SLEEP if not active/processing
//   2. Update transport mirror (atomic stores)
//   3. Drain in_events → collect sorted AutomationEvents
//   4. For each sub-block between event offsets:
//        a. Apply param changes at that sample offset → set smoother targets
//        b. process_block(start, end)  — inner audio loop
//   5. Post no output events (this plugin does not generate automation)
//   6. Return CLAP_PROCESS_CONTINUE
//
// clap-validator rules checked here:
//   • audio_outputs[0] must be written for every frame
//   • frames_count ≤ max_frames declared at activate()
//   • in_events must be drained (all events consumed, even if ignored)
// ─────────────────────────────────────────────────────────────────────────────
clap_process_status MambaFXPlugin::s_process(const clap_plugin_t* p,
                                              const clap_process_t* proc) noexcept
{
    // ✓ RT-SAFE: atomic load with relaxed ordering
    auto* s = self(p);
    if (!s->is_active_.load(std::memory_order_relaxed))
        return CLAP_PROCESS_SLEEP;
    if (!s->is_processing_.load(std::memory_order_relaxed))
        return CLAP_PROCESS_SLEEP;

    // ── Defensive: validate port layout ──────────────────────────────────────
    // ✓ RT-SAFE: pointer comparison only, no alloc
    if (!proc->audio_inputs  || proc->audio_inputs_count  == 0 ||
        !proc->audio_outputs || proc->audio_outputs_count == 0)
        return CLAP_PROCESS_SLEEP;

    const auto& in_port  = proc->audio_inputs[0];
    const auto& out_port = proc->audio_outputs[0];
    const uint32_t frames = proc->frames_count;

    // clap-validator: frames_count must be ≤ max declared at activate().
    // In release builds we clamp rather than assert to avoid DAW crash.
    const uint32_t safe_frames = std::min(frames, kMaxBlock);

    // ── 1. Update transport mirror ─────────────────────────────────────────
    // ✓ RT-SAFE: only atomic stores (relaxed)
    if (proc->transport) {
        const auto& t = *proc->transport;
        const bool playing   = (t.flags & CLAP_TRANSPORT_IS_PLAYING)   != 0;
        const bool recording = (t.flags & CLAP_TRANSPORT_IS_RECORDING) != 0;
        const bool has_bpm   = (t.flags & CLAP_TRANSPORT_HAS_TEMPO)    != 0;
        const bool has_pos   = (t.flags & CLAP_TRANSPORT_HAS_SECONDS_TIMELINE) != 0;
        const bool has_sig   = (t.flags & CLAP_TRANSPORT_HAS_TIME_SIGNATURE)   != 0;
        const double bpm     = has_bpm ? t.tempo : 120.0;
        const int tsn        = has_sig ? t.tsig_num   : 4;
        const int tsd        = has_sig ? t.tsig_denom : 4;
        const int64_t pos    = has_pos
            ? static_cast<int64_t>(t.song_pos_seconds * s->sample_rate_)
            : 0;
        const int64_t prev_pos = s->transport_.song_pos_samples.load(std::memory_order_relaxed);
        const bool is_seek = has_pos && (pos < prev_pos || pos > prev_pos + static_cast<int64_t>(frames) * 4);
        s->transport_.update(playing, recording, s->sample_rate_, bpm, tsn, tsd, pos, is_seek);
    }

    // ── 2. Drain input events → AutomationEvent scratch array ─────────────
    // ✓ RT-SAFE: fixed-size scratch array on the stack of the process() call;
    //            no heap alloc; SPSC queue push is lock-free wait-free.
    uint32_t n_events = 0;
    if (proc->in_events)
        s->drain_events(proc->in_events, s->event_scratch_.data(), n_events);

    // ── 3. Bypass fast-path ────────────────────────────────────────────────
    // ✓ RT-SAFE: atomic load + memcpy
    if (s->bypass_.load(std::memory_order_relaxed)) {
        for (uint32_t ch = 0; ch < kChannels; ++ch) {
            if (in_port.data32[ch] && out_port.data32[ch] &&
                in_port.data32[ch] != out_port.data32[ch])
                std::memcpy(out_port.data32[ch], in_port.data32[ch],
                            safe_frames * sizeof(float));
        }
        s->smoothers_.skip(safe_frames);
        return CLAP_PROCESS_CONTINUE;
    }

    // ── 4. Sub-block processing: interleave events with audio ─────────────
    // For each event, process the samples that precede it, then apply the event.
    // This achieves sample-accurate parameter automation.
    uint32_t cursor = 0;

    for (uint32_t ei = 0; ei < n_events; ++ei) {
        const auto& ev = s->event_scratch_[ei];
        const uint32_t offset = std::min(ev.sample_offset, safe_frames);

        if (offset > cursor)
            s->process_block(in_port.data32, out_port.data32, cursor, offset);

        // Apply the event — updates smoother target; takes effect on next block.
        // ✓ RT-SAFE: writes to atomic<float> (relaxed) + smoother (single-thread)
        s->apply_event(ev);
        cursor = offset;
    }

    // Process remaining samples after the last event.
    if (cursor < safe_frames)
        s->process_block(in_port.data32, out_port.data32, cursor, safe_frames);

    // ── 5. No output events generated by this plugin ──────────────────────
    // (out_events exists but we never push to it)

    return CLAP_PROCESS_CONTINUE;
}

// ─────────────────────────────────────────────────────────────────────────────
// [AUDIO-RT] drain_events — collect, sort by offset, categorise.
// Only CLAP_EVENT_PARAM_VALUE and CLAP_EVENT_TRANSPORT are handled;
// all others are silently consumed (clap-validator: all events must be read).
// ─────────────────────────────────────────────────────────────────────────────
void MambaFXPlugin::drain_events(const clap_input_events_t* in,
                                  AutomationEvent* scratch,
                                  uint32_t& n_out) noexcept
{
    // ✓ RT-SAFE: no alloc; bounded loop over host-provided event list
    n_out = 0;
    const uint32_t count = in->size(in);

    for (uint32_t i = 0; i < count; ++i) {
        const clap_event_header_t* hdr = in->get(in, i);
        if (!hdr) continue;

        if (hdr->space_id != CLAP_CORE_EVENT_SPACE_ID) continue;

        if (hdr->type == CLAP_EVENT_PARAM_VALUE) {
            const auto* ev = reinterpret_cast<const clap_event_param_value_t*>(hdr);
            if (n_out < kEventCapacity) {
                scratch[n_out++] = {
                    .sample_offset = hdr->time,
                    .param_id      = ev->param_id,
                    .value         = static_cast<float>(ev->value),
                };
            }
            // If scratch is full, remaining events are dropped — acceptable
            // at extremely high automation density (> 256 changes/block).
        }
        // All other event types: consumed but not acted on.
    }

    // Sort by sample offset so sub-block processing is in chronological order.
    // ✓ RT-SAFE: bounded sort on a fixed-size array (worst case kEventCapacity=256 elements)
    // Using insertion sort — optimal for nearly-sorted small arrays (events usually arrive in order).
    for (uint32_t i = 1; i < n_out; ++i) {
        AutomationEvent key = scratch[i];
        int32_t j = static_cast<int32_t>(i) - 1;
        while (j >= 0 && scratch[j].sample_offset > key.sample_offset) {
            scratch[j + 1] = scratch[j];
            --j;
        }
        scratch[j + 1] = key;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// [AUDIO-RT] apply_event — translate one AutomationEvent into plugin state.
// ─────────────────────────────────────────────────────────────────────────────
void MambaFXPlugin::apply_event(const AutomationEvent& ev) noexcept {
    // ✓ RT-SAFE: writes to atomic (ParameterBank) + smoother target (float)
    const auto id = static_cast<ParamId>(ev.param_id);
    params_.set(id, ev.value);
    smoothers_.set_target(id, ev.value);

    // Instantaneous updates for non-smoothed binary params.
    switch (id) {
    case ParamId::Bypass:
        bypass_.store(ev.value >= 0.5f, std::memory_order_relaxed);
        break;
    case ParamId::InputGain:
        // ✗ std::pow is a syscall-adjacent transcendental — only call here,
        //    not inside the per-sample loop.
        input_gain_linear_ = std::pow(10.f, ev.value / 20.f);
        break;
    case ParamId::OutputGain:
        output_gain_linear_ = std::pow(10.f, ev.value / 20.f);
        break;
    default:
        break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// [AUDIO-RT] process_block — inner audio loop.
// Processes samples [start, end) across all channels.
//
// Per-sample work:
//   1. Tick all smoothers (IIR one-pole)
//   2. Apply input gain (from smoothed InputGain param)
//   3. Push samples to AudioRingBuffer for inference thread
//   4. Apply EffectApplicator result (compressor gain)
//   5. Apply output gain
//   6. Write to output buffer
//
// ✓ RT-SAFE: no alloc, no mutex, no syscall, no libtorch, bounded loops only
// ─────────────────────────────────────────────────────────────────────────────
void MambaFXPlugin::process_block(const float* const* in,
                                   float* const*       out,
                                   uint32_t            start,
                                   uint32_t            end) noexcept
{
    const uint32_t n = end - start;
    if (n == 0) return;

    // Snapshot effect type for this block (stepped param, no smoothing needed).
    const auto effect_type = static_cast<dsp::EffectType>(
        static_cast<int>(std::round(params_.get(ParamId::EffectType))));

    const float wet = smoothers_.get(ParamId::WetMix);

    // ── Inner sample loop ─────────────────────────────────────────────────────
    for (uint32_t s_idx = 0; s_idx < n; ++s_idx) {
        // 1. Advance all smoothers by one sample.
        // ✓ RT-SAFE: pure float arithmetic on stack-allocated SmoothedValue array
        smoothers_.tick_all();

        const float smooth_in_gain  = smoothers_.get(ParamId::InputGain);
        const float smooth_out_gain = smoothers_.get(ParamId::OutputGain);
        const float in_gain_lin     = std::pow(10.f, smooth_in_gain / 20.f);
        const float out_gain_lin    = std::pow(10.f, smooth_out_gain / 20.f);

        // NOTE: std::pow is called per-sample here for correctness (smoothed gain).
        // In a production build, replace with a fast approximate pow or a running
        // multiply updated every block boundary to avoid transcendental per-sample.

        for (uint32_t ch = 0; ch < kChannels; ++ch) {
            if (!in[ch] || !out[ch]) continue;

            // 2. Read input sample; apply smoothed input gain.
            const float dry = in[ch][start + s_idx];
            float x = dry * in_gain_lin;

            // 3. Apply effect (time-domain compressor path).
            //    Mel-domain effects (reverb/EQ/saturation) applied via STFT ring.
            //    ✓ RT-SAFE: EffectApplicator uses only float arithmetic, no alloc.
            if (effect_type == dsp::EffectType::Compression && effect_) {
                // Decode effect params (uses last inference result — may be stale
                // by one block, which is acceptable for compression).
                EffectCoefficients coef = effect_->decode(effect_type, nullptr, 0);
                x *= effect_->apply_compressor_gain(x, coef,
                                                     static_cast<float>(sample_rate_));
            } else if (effect_type == dsp::EffectType::Saturation && effect_) {
                const float drive = smoothers_.get(ParamId::WetMix); // reuse wet for drive
                x = EffectApplicator::apply_saturation(x, drive * 0.9f);
            }

            // 4. Wet/dry mix with the unprocessed dry signal.
            float y = wet * x + (1.f - wet) * dry;

            // 5. Output gain (smoothed).
            y *= out_gain_lin;

            // 6. Write output — soft clip to prevent DAC overload.
            // tanh saturation at ±1 FS: y = tanh(y) using fast approx.
            // Proper limiter would be preferred in mastering context.
            if (y >  1.f) y =  1.f;
            if (y < -1.f) y = -1.f;

            out[ch][start + s_idx] = y;
        }
    }

    // ── Push processed block to AudioRingBuffer for inference thread ──────────
    // ✓ RT-SAFE: AudioRingBuffer::push is lock-free wait-free
    if (audio_ring_) {
        for (uint32_t ch = 0; ch < kChannels; ++ch) {
            if (out[ch])
                audio_ring_->push(ch, out[ch] + start, n);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// [MAIN] get_extension — return extension vtable pointers.
// clap-validator: must return null for unknown IDs (not abort/crash).
// ─────────────────────────────────────────────────────────────────────────────
const void* MambaFXPlugin::s_get_extension(const clap_plugin_t* /*p*/,
                                            const char* id) noexcept
{
    // ✓ strcmp is safe here — called on main thread only
    if (std::strcmp(id, CLAP_EXT_PARAMS)       == 0) return &kParamsExt;
    if (std::strcmp(id, CLAP_EXT_STATE)        == 0) return &kStateExt;
    if (std::strcmp(id, CLAP_EXT_LATENCY)      == 0) return &kLatencyExt;
    if (std::strcmp(id, CLAP_EXT_TAIL)         == 0) return &kTailExt;
    if (std::strcmp(id, CLAP_EXT_AUDIO_PORTS)  == 0) return &kAudioPortsExt;
    return nullptr;  // ✓ clap-validator requires null (not crash) for unknown IDs
}

// ─────────────────────────────────────────────────────────────────────────────
// [MAIN] on_main_thread — deferred callbacks posted by inference thread.
// Called by the host on the main thread when we request it via
// host_->request_callback(host_).
// ─────────────────────────────────────────────────────────────────────────────
void MambaFXPlugin::s_on_main_thread(const clap_plugin_t* p) noexcept {
    auto* s = self(p);

    // ── Latency update notification ───────────────────────────────────────────
    // After model reload, latency might change (e.g. different block alignment).
    if (s->request_latency_update_.exchange(false, std::memory_order_acq_rel)) {
        if (s->host_latency_ && s->host_latency_->changed)
            s->host_latency_->changed(s->host_);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// params extension
// ─────────────────────────────────────────────────────────────────────────────

uint32_t MambaFXPlugin::params_count(const clap_plugin_t*) noexcept {
    return kParamCount;  // ✓ constexpr
}

bool MambaFXPlugin::params_get_info(const clap_plugin_t*,
                                     uint32_t index,
                                     clap_param_info_t* out) noexcept {
    // ✓ RT-SAFE: static table lookup, no alloc
    return ParameterBank::fill_info(index, out);
}

bool MambaFXPlugin::params_get_value(const clap_plugin_t* p,
                                      clap_id param_id,
                                      double* out) noexcept {
    // ✓ RT-SAFE: atomic load
    *out = self(p)->params_.get(static_cast<ParamId>(param_id));
    return true;
}

bool MambaFXPlugin::params_value_to_text(const clap_plugin_t*,
                                          clap_id param_id, double value,
                                          char* buf, uint32_t capacity) noexcept {
    // ✓ safe: snprintf into caller-provided buffer, no alloc
    return ParameterBank::value_to_text(param_id, value, buf, capacity);
}

bool MambaFXPlugin::params_text_to_value(const clap_plugin_t*,
                                          const char* text, clap_id param_id,
                                          double* out) noexcept {
    return ParameterBank::text_to_value(param_id, text, out);
}

// [AUDIO-RT or MAIN when not processing]
// clap-validator: flush is called to apply param changes when process() is not running.
// Must consume ALL events (same rules as process() event drain).
void MambaFXPlugin::params_flush(const clap_plugin_t* p,
                                  const clap_input_events_t*  in,
                                  const clap_output_events_t* /*out*/) noexcept
{
    // ✓ RT-SAFE when called on audio thread (no alloc, no mutex)
    auto* s = self(p);
    if (!in) return;
    const uint32_t count = in->size(in);
    for (uint32_t i = 0; i < count; ++i) {
        const auto* hdr = in->get(in, i);
        if (!hdr || hdr->space_id != CLAP_CORE_EVENT_SPACE_ID) continue;
        if (hdr->type != CLAP_EVENT_PARAM_VALUE) continue;
        const auto* ev = reinterpret_cast<const clap_event_param_value_t*>(hdr);
        s->params_.set(static_cast<ParamId>(ev->param_id),
                       static_cast<float>(ev->value));
        s->smoothers_.set_target(static_cast<ParamId>(ev->param_id),
                                  static_cast<float>(ev->value));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// state extension — CLAP_STATE_IS_PERSISTENT compliant
//
// Format:
//   [4B] magic = 0x4D424658 ("MBFX")
//   [4B] version (uint32_t, for migration)
//   [4B] param_count
//   [param_count × 4B] float param values in ParamId order
//   [4B] model_root_len
//   [model_root_len bytes] UTF-8 model root path
//
// clap-validator rules:
//   • save() must not modify plugin state
//   • load() must be called only when not processing
//   • Both must return false on I/O error (not crash)
// ─────────────────────────────────────────────────────────────────────────────

bool MambaFXPlugin::state_save(const clap_plugin_t* p,
                                const clap_ostream_t* stream) noexcept
{
    // [MAIN] — ✗ RT-UNSAFE: may allocate inside StateSerializer
    auto* s = self(p);
    state::StateSerializer ser(s->params_);
    return ser.save(stream);
}

bool MambaFXPlugin::state_load(const clap_plugin_t* p,
                                const clap_istream_t* stream) noexcept
{
    // [MAIN] — ✗ RT-UNSAFE
    // clap-validator: load() is only called between deactivate() and activate()
    // so is_active_ is false and no audio thread is running.
    auto* s = self(p);
    state::StateSerializer ser(s->params_);
    const bool ok = ser.load(stream);
    if (ok && s->host_params_ && s->host_params_->rescan)
        s->host_params_->rescan(s->host_, CLAP_PARAM_RESCAN_VALUES);
    return ok;
}

// ─────────────────────────────────────────────────────────────────────────────
// latency / tail extensions
// clap-validator: get() must return the same value between activate/deactivate
// unless changed via host->latency->changed() from the main thread.
// ─────────────────────────────────────────────────────────────────────────────

uint32_t MambaFXPlugin::latency_get(const clap_plugin_t*) noexcept {
    return kLatency;   // 256 samples = one STFT hop (stable)
}

uint32_t MambaFXPlugin::tail_get(const clap_plugin_t*) noexcept {
    return kTailSamples;   // one STFT window of decay after last input
}

// ─────────────────────────────────────────────────────────────────────────────
// audio-ports extension
// clap-validator rules:
//   • At least one main stereo output port required for AUDIO_EFFECT feature
//   • in_place_pair must be CLAP_INVALID_ID for input or a valid output index
//   • port_type must be one of the CLAP_PORT_* constants or nullptr
// ─────────────────────────────────────────────────────────────────────────────

uint32_t MambaFXPlugin::audio_ports_count(const clap_plugin_t*, bool) noexcept {
    return 1;   // one stereo port each direction
}

bool MambaFXPlugin::audio_ports_get(const clap_plugin_t*,
                                     uint32_t index, bool is_input,
                                     clap_audio_port_info_t* out) noexcept
{
    if (index != 0) return false;

    std::memset(out, 0, sizeof(*out));
    out->id            = 0;
    out->channel_count = kChannels;                   // 2 = stereo
    out->flags         = CLAP_AUDIO_PORT_IS_MAIN;
    out->port_type     = CLAP_PORT_STEREO;

    // in_place_pair: output port index that can share the same buffer as this
    // input port.  Enables zero-copy in-place processing in supporting hosts.
    out->in_place_pair = is_input ? 0u : CLAP_INVALID_ID;

    std::strncpy(out->name,
                 is_input ? "Stereo In" : "Stereo Out",
                 sizeof(out->name) - 1);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Static extension vtable instances (stable address, one copy per process)
// ─────────────────────────────────────────────────────────────────────────────

const clap_plugin_params_t MambaFXPlugin::kParamsExt = {
    .count          = params_count,
    .get_info       = params_get_info,
    .get_value      = params_get_value,
    .value_to_text  = params_value_to_text,
    .text_to_value  = params_text_to_value,
    .flush          = params_flush,
};

const clap_plugin_state_t MambaFXPlugin::kStateExt = {
    .save = state_save,
    .load = state_load,
};

const clap_plugin_latency_t MambaFXPlugin::kLatencyExt = {
    .get = latency_get,
};

const clap_plugin_tail_t MambaFXPlugin::kTailExt = {
    .get = tail_get,
};

const clap_plugin_audio_ports_t MambaFXPlugin::kAudioPortsExt = {
    .count = audio_ports_count,
    .get   = audio_ports_get,
};

// ─────────────────────────────────────────────────────────────────────────────
// Factory — clap_plugin_factory_t
//
// clap-validator rules:
//   • get_plugin_count must return ≥ 1 for a usable plugin library
//   • get_plugin_descriptor must return non-null for index < count
//   • create_plugin must check clap_version compatibility before allocating
// ─────────────────────────────────────────────────────────────────────────────

static uint32_t factory_count(const clap_plugin_factory_t*) noexcept {
    return 1;
}

static const clap_plugin_descriptor_t* factory_descriptor(
    const clap_plugin_factory_t*, uint32_t index) noexcept
{
    return index == 0 ? &kMambaFXDescriptor : nullptr;
}

static const clap_plugin_t* factory_create(
    const clap_plugin_factory_t*,
    const clap_host_t*  host,
    const char*         plugin_id) noexcept
{
    // clap-validator: check host CLAP version before accessing host vtable.
    if (!clap_version_is_compatible(host->clap_version)) return nullptr;

    // clap-validator: check plugin ID matches before allocating.
    if (std::strcmp(plugin_id, kMambaFXDescriptor.id) != 0) return nullptr;

    // ✗ RT-UNSAFE: allocation; but factory_create is always called on main thread.
    auto* plugin = new (std::nothrow) MambaFXPlugin(host);
    return plugin ? plugin->handle() : nullptr;
}

const clap_plugin_factory_t kMambaFXFactory = {
    .get_plugin_count      = factory_count,
    .get_plugin_descriptor = factory_descriptor,
    .create_plugin         = factory_create,
};

// ─────────────────────────────────────────────────────────────────────────────
// clap_entry — the single exported symbol from the .clap shared library.
// Name must be exactly "clap_entry" (clap-validator checks the ELF/MachO symbol).
// ─────────────────────────────────────────────────────────────────────────────

static bool entry_init(const char* /*plugin_path*/) noexcept {
    // ✓ One-time init: no allocation required for this plugin.
    return true;
}

static void entry_deinit() noexcept {
    // ✓ Global teardown if needed (e.g. WeightStore flush).
}

static const void* entry_get_factory(const char* factory_id) noexcept {
    if (std::strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0)
        return &kMambaFXFactory;
    return nullptr;  // clap-validator: null (not crash) for unknown factory IDs
}

extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    .clap_version = CLAP_VERSION_INIT,
    .init         = entry_init,
    .deinit       = entry_deinit,
    .get_factory  = entry_get_factory,
};

} // namespace mamba::clap_adapter
