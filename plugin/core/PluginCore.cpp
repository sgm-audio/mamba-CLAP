#include "core/PluginCore.hpp"
#include "core/ParameterBank.hpp"
#include "core/TransportState.hpp"
#include "core/EventQueue.hpp"
#include "dsp/AudioRingBuffer.hpp"
#include "dsp/STFTEngine.hpp"
#include "dsp/OverlapAddSynthesizer.hpp"
#include "dsp/EffectApplicator.hpp"
#include "model/MambaInferenceEngine.hpp"
#include "model/MambaSession.hpp"
#include "state/StateSerializer.hpp"
#ifdef MAMBA_ENABLE_ARA
#include "ara/ARADocumentController.hpp"
#endif

#include <clap/ext/params.h>
#include <clap/ext/log.h>
#include <cassert>
#include <cstring>

namespace mamba::core {

PluginCore::PluginCore(const clap_host_t* host)
    : host_(host)
    , params_(std::make_unique<ParameterBank>())
    , transport_(std::make_unique<TransportState>())
    , cmd_queue_(std::make_unique<AudioCommandQueue>())
    , result_queue_(std::make_unique<InferenceResultQueue>())
    , inference_engine_(std::make_unique<model::MambaInferenceEngine>(
          *params_, *result_queue_))
    , serialiser_(std::make_unique<state::StateSerializer>(*params_))
{}

PluginCore::~PluginCore() {
    // Stop inference before DSP objects are torn down.
    if (inference_engine_) inference_engine_->shutdown();
    // active_session_ is owned by inference_engine_; no separate delete needed.
}

// ─── Activation ───────────────────────────────────────────────────────────────

bool PluginCore::activate(double sr, uint32_t min_frames, uint32_t max_frames) {
    assert(!is_active_.load());
    sample_rate_ = sr;
    max_frames_  = max_frames;

    transport_->sample_rate.store(sr, std::memory_order_relaxed);

    // Pre-allocate all DSP memory; never allocates again at RT.
    constexpr int kNFFT      = 1024;
    constexpr int kHopLength = 256;
    constexpr int kNMels     = 80;

    stft_  = std::make_unique<dsp::STFTEngine>(kNFFT, kHopLength, kNMels, sr);
    ola_   = std::make_unique<dsp::OverlapAddSynthesizer>(kNFFT, kHopLength, max_frames);
    effect_= std::make_unique<dsp::EffectApplicator>();
    audio_ring_ = std::make_unique<dsp::AudioRingBuffer>(
        /*channels=*/2, /*capacity_frames=*/max_frames * 8);

    // Start inference worker; loads model on background thread.
    inference_engine_->start(sr, kNFFT, kHopLength, kNMels);

    is_active_.store(true, std::memory_order_release);
    return true;
}

void PluginCore::deactivate() {
    assert(is_active_.load());
    inference_engine_->suspend();
    stft_.reset();
    ola_.reset();
    effect_.reset();
    audio_ring_.reset();
    is_active_.store(false, std::memory_order_release);
}

bool PluginCore::start_processing() {
    is_processing_.store(true, std::memory_order_release);
    return true;
}

void PluginCore::stop_processing() {
    is_processing_.store(false, std::memory_order_release);
}

// ─── Real-time process() ──────────────────────────────────────────────────────

clap_process_status PluginCore::process(const clap_process_t* proc) noexcept {
    if (!is_active_.load(std::memory_order_relaxed))
        return CLAP_PROCESS_SLEEP;

    // 1. Drain any pending commands from UI/main thread.
    drain_command_queue();

    // 2. Update transport mirror.
    if (proc->transport) {
        const auto& t = *proc->transport;
        const bool playing   = (t.flags & CLAP_TRANSPORT_IS_PLAYING)   != 0;
        const bool recording = (t.flags & CLAP_TRANSPORT_IS_RECORDING) != 0;
        const bool looping   = (t.flags & CLAP_TRANSPORT_IS_LOOP_ACTIVE) != 0;

        const bool has_bpm  = (t.flags & CLAP_TRANSPORT_HAS_TEMPO) != 0;
        const bool has_sig  = (t.flags & CLAP_TRANSPORT_HAS_TIME_SIGNATURE) != 0;
        const bool has_pos  = (t.flags & CLAP_TRANSPORT_HAS_SECONDS_TIMELINE) != 0;

        const double bpm     = has_bpm ? t.tempo : 120.0;
        const int num        = has_sig ? t.tsig_num : 4;
        const int den        = has_sig ? t.tsig_denom : 4;
        const int64_t pos_s  = has_pos ? static_cast<int64_t>(t.song_pos_seconds * sample_rate_) : 0;

        // Detect seek: position jumped backwards or non-monotonically.
        const int64_t prev_pos = transport_->song_pos_samples.load(std::memory_order_relaxed);
        const bool is_seek = has_pos && (pos_s < prev_pos || pos_s > prev_pos + max_frames_ * 4);

        transport_->update(playing, recording, sample_rate_, bpm, num, den, pos_s, is_seek);

        if (is_seek) {
            // Signal inference thread to reset Mamba recurrent state.
            AudioEvent ev;
            ev.command = AudioCommand::TransportReset;
            cmd_queue_->push(ev);
        }
        (void)looping;
    }

    // 3. Handle CLAP input events (note/param changes).
    if (proc->in_events)
        handle_input_events(proc->in_events);

    // 4. Run DSP.
    if (!bypass_.load(std::memory_order_relaxed))
        run_dsp(proc);
    else {
        // Passthrough: copy input to output.
        for (uint32_t ch = 0; ch < proc->audio_outputs[0].channel_count; ++ch) {
            if (proc->audio_inputs[0].data32[ch] && proc->audio_outputs[0].data32[ch]) {
                std::memcpy(proc->audio_outputs[0].data32[ch],
                            proc->audio_inputs[0].data32[ch],
                            proc->frames_count * sizeof(float));
            }
        }
    }

    // 5. Post automation output events if needed.
    if (proc->out_events)
        post_output_events(proc->out_events);

    return CLAP_PROCESS_CONTINUE;
}

void PluginCore::drain_command_queue() noexcept {
    while (auto ev = cmd_queue_->pop()) {
        switch (ev->command) {
        case AudioCommand::ParamChanged:
            params_->set(static_cast<ParamId>(ev->param_id), ev->param_value);
            break;
        case AudioCommand::Bypass:
            bypass_.store(true, std::memory_order_relaxed);
            break;
        case AudioCommand::Unbypass:
            bypass_.store(false, std::memory_order_relaxed);
            break;
        case AudioCommand::ModelReloaded:
            // inference_engine_ atomically updated active_session_ already.
            break;
        default:
            break;
        }
    }
}

void PluginCore::handle_input_events(const clap_input_events_t* in) noexcept {
    const uint32_t count = in->size(in);
    for (uint32_t i = 0; i < count; ++i) {
        const auto* hdr = in->get(in, i);
        if (!hdr) continue;
        if (hdr->type == CLAP_EVENT_PARAM_VALUE) {
            const auto* ev = reinterpret_cast<const clap_event_param_value_t*>(hdr);
            // Direct write: audio thread is the only writer during process().
            params_->set(static_cast<ParamId>(ev->param_id),
                         static_cast<float>(ev->value));
        }
    }
}

void PluginCore::run_dsp(const clap_process_t* proc) noexcept {
    // Defensive: check audio port arrays are valid.
    if (!proc->audio_inputs || !proc->audio_outputs) return;
    if (proc->audio_inputs_count == 0 || proc->audio_outputs_count == 0) return;

    const auto& in_port  = proc->audio_inputs[0];
    const auto& out_port = proc->audio_outputs[0];
    const uint32_t frames = proc->frames_count;
    const uint32_t channels = std::min(in_port.channel_count, out_port.channel_count);

    // Push raw audio into the ring for the inference thread.
    for (uint32_t ch = 0; ch < channels; ++ch)
        audio_ring_->push(ch, in_port.data32[ch], frames);

    // Pull the latest inference result (may be slightly stale; that is acceptable).
    const float wet = params_->get(ParamId::WetMix);

    // Let STFTEngine + OLA synthesiser produce the processed output.
    // All allocations happened in activate(); these calls are alloc-free.
    stft_->process(*audio_ring_, *ola_, frames);
    ola_->synthesise(out_port.data32, channels, frames, wet);
}

void PluginCore::post_output_events(const clap_output_events_t*) noexcept {
    // Currently no plugin-generated parameter changes; placeholder for
    // future features (e.g. AI auto-parameter suggestion).
}

// ─── Model hot-swap ───────────────────────────────────────────────────────────

void PluginCore::notify_model_ready(void* new_session_ptr) noexcept {
    active_session_.store(static_cast<model::MambaSession*>(new_session_ptr),
                          std::memory_order_release);
    AudioEvent ev;
    ev.command = AudioCommand::ModelReloaded;
    ev.payload = new_session_ptr;
    cmd_queue_->push(ev);
}

// ─── State serialisation ──────────────────────────────────────────────────────

bool PluginCore::save_state(const clap_ostream_t* stream) const {
    return serialiser_->save(stream);
}

bool PluginCore::load_state(const clap_istream_t* stream) {
    return serialiser_->load(stream);
}

} // namespace mamba::core
