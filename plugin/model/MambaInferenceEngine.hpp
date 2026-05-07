#pragma once
// MambaInferenceEngine — manages the real-time inference worker thread
// and the model lifecycle.
//
// Architecture:
//   One engine per plugin instance.
//   Engine owns:
//     • ModelLoader (async load, hot-swap)
//     • MambaSession (active real-time session — swapped atomically on reload)
//     • std::thread  (inference worker — reads AudioRingBuffer, runs session)
//     • InferenceResultQueue (writes results back to audio thread)
//
// Worker thread behaviour:
//   while running:
//     wait (with timeout) for audio data in ring buffer
//     run MambaSession::step() for each buffered mel frame
//     push InferenceResult to result_queue_
//     if seek_generation changed: session->reset()
//
// Real-time guarantee:
//   The audio thread NEVER calls libtorch.  It only:
//     • pushes raw samples to AudioRingBuffer (lock-free)
//     • reads InferenceResult from result_queue_ (lock-free)
//   Both are wait-free for the producer/consumer respectively.

#include <memory>
#include <thread>
#include <atomic>
#include <string>
#include <filesystem>

namespace mamba::core  { class ParameterBank; class InferenceResultQueue; }
namespace mamba::dsp   { class AudioRingBuffer; class STFTEngine; }

namespace mamba::model {

class ModelLoader;
class MambaSession;
class InferenceQueue;

class MambaInferenceEngine {
public:
    MambaInferenceEngine(core::ParameterBank&        params,
                          core::InferenceResultQueue& result_queue);
    ~MambaInferenceEngine();

    // Called from PluginCore::activate() — starts the worker thread.
    // Also triggers async model load if not yet loaded.
    void start(double sample_rate, int n_fft, int hop_length, int n_mels);

    // Called from PluginCore::deactivate() — suspends worker but keeps state.
    void suspend();

    // Called from ~PluginCore — joins worker thread; must be called before DSP
    // objects are destroyed.
    void shutdown();

    // Audio thread calls this to make the ring buffer available to the engine.
    // Ownership is NOT transferred; pointer must remain valid until suspend().
    void set_audio_ring(dsp::AudioRingBuffer* ring) noexcept {
        audio_ring_.store(ring, std::memory_order_release);
    }
    void set_stft_engine(dsp::STFTEngine* stft) noexcept {
        stft_engine_.store(stft, std::memory_order_release);
    }

    // Called by ModelLoader callback (loader thread) on successful load.
    // Atomically replaces active_session_ and posts AudioCommand::ModelReloaded.
    void on_model_ready(std::shared_ptr<torch::jit::Module> module) noexcept;

    // Active session pointer — read by ARAPlaybackRenderer to clone a session.
    MambaSession* active_session() const noexcept {
        return active_session_.load(std::memory_order_acquire);
    }

    InferenceQueue& ara_queue() noexcept { return *ara_queue_; }

private:
    void worker_loop();
    void process_mel_frames(MambaSession& session, int n_mels, int frames_in_ring) noexcept;

    core::ParameterBank&         params_;
    core::InferenceResultQueue&  result_queue_;

    std::unique_ptr<ModelLoader>       loader_;
    std::unique_ptr<InferenceQueue>    ara_queue_;  // for ARA offline jobs

    // Active session — written by loader thread, read by worker thread.
    // Swap is sequentially consistent to ensure the old session is not
    // used after the new one is installed.
    std::atomic<MambaSession*> active_session_{nullptr};

    // Owning storage for sessions; swapped together with active_session_.
    std::unique_ptr<MambaSession> current_session_;
    std::unique_ptr<MambaSession> pending_session_; // being installed

    // Non-owning pointers to DSP objects (owned by PluginCore).
    std::atomic<dsp::AudioRingBuffer*> audio_ring_{nullptr};
    std::atomic<dsp::STFTEngine*>      stft_engine_{nullptr};

    std::thread       worker_;
    std::atomic<bool> running_{false};
    std::atomic<bool> suspended_{false};

    double   sample_rate_{44100.0};
    int      n_fft_{1024};
    int      hop_length_{256};
    int      n_mels_{80};

    // Mirror of transport seek generation; worker resets session on change.
    std::atomic<uint64_t> last_seek_gen_{0};
};

} // namespace mamba::model
