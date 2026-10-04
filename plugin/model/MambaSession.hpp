#pragma once
// MambaSession — experimental TorchScript recurrent-session interface.
//
// STATUS: the worker/plug-in lifecycle is incomplete and this path is not
// validated as real-time inference. Although state tensors and input scratch are
// initialized at construction, step() builds a temporary IValue vector and calls
// TorchScript; allocation behavior and latency are therefore unverified. Keep
// this work off the audio callback and do not claim hard-real-time safety.
//
// A session is intended for single-thread-at-a-time use. The current incomplete
// inference-engine path does not establish end-to-end ownership/thread safety.

#include <torch/script.h>
#include <vector>
#include <memory>
#include <cstdint>

namespace mamba::model {

enum class EffectType : uint8_t {
    Reverb      = 0,
    Compression = 1,
    Saturation  = 2,
    EQ          = 3,
};

// Output of one inference step — plain data, no torch dependency outside model/.
struct StepOutput {
    float wet_gain{1.f};      // per-sample gain applied in EffectApplicator
    float effect_params[8]{};  // effect-specific decoded parameters
    bool  valid{true};
};

class MambaSession {
public:
    MambaSession(std::shared_ptr<torch::jit::Module> module,
                 int n_mels,
                 torch::Device device);

    // Resets all recurrent state to zeros (call after transport seek).
    void reset() noexcept;

    // Process one mel-spectrogram frame: input [1, n_mels].
    // Updates internal state in-place; returns decoded effect parameters.
    // noexcept: any torch exception is caught internally and sets valid=false.
    StepOutput step(const float* mel_frame,
                    EffectType   effect,
                    float        context_weight) noexcept;

    // Process a full offline segment: input [T, n_mels] → output [T, n_mels].
    // May allocate; only called from non-RT contexts (ARA offline renderer).
    torch::Tensor process_segment(const torch::Tensor& mel_spectrogram,
                                  EffectType           effect);

    int n_mels() const noexcept { return n_mels_; }
    bool is_valid() const noexcept { return module_ != nullptr; }

private:
    std::shared_ptr<torch::jit::Module> module_;
    int           n_mels_;
    torch::Device device_;

    // Recurrent state: one tensor per Mamba layer.
    // Shape: [1, d_model, d_state] per layer; pre-allocated in constructor.
    std::vector<torch::Tensor> state_;

    // Scratch input tensor [1, n_mels] — reused each step; no realloc.
    torch::Tensor input_scratch_;
};

} // namespace mamba::model
