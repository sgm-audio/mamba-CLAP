# Mamba-SSM Streaming Inference Contract

Based on the custom, bare-metal C++ architecture we have established for the `MambaDAW` plugin, here is the explicit streaming inference contract.

## 1. Tensor & State Lifecycle Table

Because we use a custom flat-binary loader (`MambaWeightLoader`) instead of TorchScript or ONNX, we avoid tensor library overhead entirely. All state is maintained as pre-allocated, SIMD-aligned `std::vector<float>` arrays inside the `MambaLayer` instances.

| Tensor / State | Shape | Data Type | Lifecycle & Persistence |
| :--- | :--- | :--- | :--- |
| **Input Audio** | `[1]` (Scalar) | `FP32` | Transient. Provided per sample by the host audio block. |
| **Output Audio** | `[1]` (Scalar) | `FP32` | Transient. Returned per sample to the host audio block. |
| **SSM Hidden State** (`state_h_`) | `[n_layers, d_inner * d_state]` | `FP32` | **Persistent.** Pre-allocated on plugin instantiation. Carried over across all audio blocks. Flushed to zero only on explicit host reset or playback stop. |
| **Conv Ring Buffer** (`conv_buffer_`) | `[n_layers, d_inner * d_conv]` | `FP32` | **Persistent.** Pre-allocated on instantiation. Acts as causal history for the 1D convolution. Carried across blocks. |
| **Conv Pointer** (`conv_pos_`) | `[n_layers]` | `int64` | **Persistent.** Tracks the circular write head for the convolution buffer. |
| **Static Weights** (`dA, dB, C, D`, etc.) | Various | `FP32` | **Read-Only.** Loaded on the main thread via `MambaWeightLoader` during initialization. |

---

## 2. Streaming Inference Contract (Pseudocode)

The plugin operates on a strictly per-sample basis, making it entirely agnostic to the host's block size. It natively handles any block size from 1 to N without internal buffering or block-boundary state resets.

```cpp
// 1. Initialization (Main Thread / NRT)
void MambaPlugin::activate(double sample_rate) {
    // If the model was trained at 44.1kHz and the host provides 96kHz,
    // we must reject, disable processing, or enforce an offline resampler wrapper.
    // (Mamba's learned 'dt' assumes a fixed temporal resolution).
    assert(sample_rate == EXPECTED_TRAINING_SR);

    // Allocate all layers. No allocations will happen after this point.
    mamba_model_->initializeArchitecture();
    mamba_model_->resetState(); // Zeroes out state_h_ and conv_buffer_
}

// 2. Real-Time Audio Callback (Audio Thread)
void MambaPlugin::processBlock(const float* input, float* output, int num_samples) noexcept {
    // Variable host block sizes are natively supported by the sample-by-sample loop.
    for (int t = 0; t < num_samples; ++t) {

        // 3. Step execution (Inlined, SIMD accelerated)
        float current_sample = input[t];

        for (auto& layer : mamba_model_->layers) {
            // A. Update Convolution Ring Buffer (Causal)
            layer.conv_buffer_[layer.conv_pos_] = current_sample;
            float conv_out = calculate_depthwise_conv(layer.conv_buffer_, layer.conv_pos_);
            layer.conv_pos_ = (layer.conv_pos_ + 1) % d_conv;

            // B. Selective Scan (State Update) - Carries over from previous step
            // h_t = dA * h_{t-1} + dB * conv_out
            // y_t = C * h_t + D * conv_out
            float ssm_out = 0.0f;
            update_state_simd(layer.state_h_, layer.dA_, layer.dB_, conv_out);
            ssm_out = compute_output_simd(layer.state_h_, layer.C_, layer.D_, conv_out);

            // C. Apply gating and feed to next layer
            current_sample = apply_gating(ssm_out);
        }

        output[t] = current_sample; // Mix logic omitted for brevity
    }
}
```

## 3. Declared Plugin Latency

**Reported Latency: `0` samples.**
Because the architecture relies on a strictly causal 1D convolution (`causal-conv1d`) and a recurrent SSM state update (`selective_scan`), the prediction for sample `t` depends *only* on samples `0` to `t`. There is zero lookahead, zero overlap-add (STFT) buffering, and therefore zero algorithmic latency.

## 4. Runtime Constraints & Assertions

To guarantee real-time safety and mathematical correctness, the following invariants **must** hold and should be validated via assertions (`assert()`) in debug builds:

1. **No NaN/Inf Propagation**:
   - `assert(!std::isnan(output[t]) && !std::isinf(output[t]));`
   - *Requirement*: The model must have anti-denormal protections (flush-to-zero at `1e-30`) in the recurrent loop to prevent CPU stalls, and output bounds checking to prevent blowing out the DAW master bus.
2. **Buffer Alignments**:
   - `assert(reinterpret_cast<uintptr_t>(state_h_.data()) % 32 == 0);`
   - *Requirement*: Because the recurrent step uses `_mm256_load_ps` (AVX2), state vectors must be 32-byte aligned to prevent segmentation faults.
3. **No Malloc in RT**:
   - *Requirement*: Running the plugin under Valgrind Massif or overriding `operator new` globally in debug mode must yield exactly `0` allocations during `processBlock`.
4. **Valid Sample Rate**:
   - `assert(host_sample_rate == model_sample_rate);`
   - *Requirement*: Mamba's discretized state transition matrices (`A_bar`, `B_bar`) are tied to the step size (`dt`). If the host sample rate changes, the model must either be swapped for one trained at the new rate, or bypass processing entirely.
