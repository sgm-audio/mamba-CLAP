#include "model/MambaSession.hpp"
#include <stdexcept>
#include <cstring>

namespace mamba::model {

MambaSession::MambaSession(std::shared_ptr<torch::jit::Module> module,
                            int n_mels, torch::Device device)
    : module_(std::move(module))
    , n_mels_(n_mels)
    , device_(device)
{
    if (!module_) return;

    // Pre-allocate input scratch.
    input_scratch_ = torch::zeros({1, n_mels_}, torch::dtype(torch::kFloat32).device(device_));

    // Query number of layers from the module and pre-allocate state tensors.
    // Convention: the TorchScript module exposes `get_initial_state(batch) -> List[Tensor]`.
    try {
        auto init_state = module_->run_method("get_initial_state",
                                               torch::jit::IValue(1));
        auto state_list = init_state.toTensorList();
        state_.reserve(state_list.size());
        for (const auto& t : state_list)
            state_.push_back(t.clone().to(device_));
    }
    catch (...) {
        // Module doesn't expose initialisation method — use empty state.
        state_.clear();
    }
}

void MambaSession::reset() noexcept {
    try {
        for (auto& s : state_)
            s.zero_();
    } catch (...) {}
}

StepOutput MambaSession::step(const float* mel_frame,
                               EffectType   effect,
                               float        context_weight) noexcept
{
    StepOutput out;
    if (!module_) { out.valid = false; return out; }

    try {
        // Copy mel frame into pre-allocated scratch — no heap alloc.
        std::memcpy(input_scratch_.data_ptr<float>(), mel_frame,
                    static_cast<std::size_t>(n_mels_) * sizeof(float));

        // Pack state as IValue list.
        std::vector<torch::jit::IValue> args;
        args.reserve(3 + state_.size());
        args.emplace_back(input_scratch_);
        args.emplace_back(static_cast<int64_t>(effect));
        args.emplace_back(context_weight);
        for (const auto& s : state_)
            args.emplace_back(s);

        // Forward: returns tuple (effect_params_tensor, new_state_list).
        auto result = module_->run_method("step", args);
        auto tuple  = result.toTuple();

        const auto& params_tensor = tuple->elements()[0].toTensor();
        const auto& new_states    = tuple->elements()[1].toTensorList();

        // Update recurrent state in-place (no alloc — copy_ uses pre-allocated buffer).
        for (std::size_t l = 0; l < state_.size() && l < new_states.size(); ++l)
            state_[l].copy_(new_states[l]);

        // Decode effect parameters (up to 8 floats).
        const float* p = params_tensor.data_ptr<float>();
        const int count = std::min<int>(8, static_cast<int>(params_tensor.numel()));
        for (int i = 0; i < count; ++i)
            out.effect_params[i] = p[i];

        // First decoded param interpreted as the overall wet gain [0,1].
        if (count > 0) out.wet_gain = std::clamp(p[0], 0.f, 1.f);
    }
    catch (...) {
        out.valid = false;
        reset();   // reset state after error to avoid propagating corrupted tensors
    }
    return out;
}

torch::Tensor MambaSession::process_segment(const torch::Tensor& mel_spec,
                                             EffectType           effect)
{
    if (!module_) return torch::zeros_like(mel_spec);

    // Full sequence forward — allocates; only used in offline contexts.
    auto result = module_->run_method(
        "forward",
        mel_spec.to(device_),
        static_cast<int64_t>(effect));
    return result.toTensor().cpu();
}

} // namespace mamba::model
