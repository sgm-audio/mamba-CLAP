#ifndef PLUGIN_PROCESSOR_H
#define PLUGIN_PROCESSOR_H

#include "mamba_model.h"
#include <memory>
#include <cstdint>
#include <string>
#include <atomic>
#include <vector>
#include "parameter_gate.h"

/**
 * @brief Real-Time Audio Processor — State-Centric Mamba SSM Design.
 *
 * Rules enforced by this class:
 *   - processBlock() has ZERO heap allocations.
 *   - processBlock() performs NO logging (no cout/cerr/printf).
 *   - All host-written parameters are std::atomic to prevent data races.
 *   - MambaModel state persists across processBlock calls (it IS the memory).
 *   - Weight loading only happens on non-RT threads via loadModel().
 */
class PluginProcessor {
public:
    explicit PluginProcessor(int sample_rate = 44100);
    ~PluginProcessor();

    // -------------------------------------------------------------------------
    // Lifecycle — called on non-RT thread by CLAP host
    // -------------------------------------------------------------------------

    /** Called before processing starts. Resets internal state. */
    void activate(double sample_rate, uint32_t min_frame_count, uint32_t max_frame_count);

    /** Called after processing stops. */
    void deactivate();

    /** Called by host reset event. Zeroes all Mamba hidden states. */
    void resetState();

    // -------------------------------------------------------------------------
    // Audio processing — called on RT thread
    // -------------------------------------------------------------------------

    /**
     * @brief Process a block of audio samples.
     *
     * INVARIANTS (must hold for all code paths):
     *   - Zero heap allocations.
     *   - No blocking I/O (no cout, cerr, file access).
     *   - No mutex locks.
     *   - Mamba state update happens strictly sample-by-sample.
     */
    void processBlock(float** inputs, float** outputs,
                      int32_t num_frames,
                      int32_t num_input_channels,
                      int32_t num_output_channels);

    // -------------------------------------------------------------------------
    // Parameters — written by host (non-RT), read by RT thread
    // -------------------------------------------------------------------------

    /** param_id 0: Dry/Wet [0.0, 1.0]
     *  param_id 1: Model Type (reserved, always 0)
     *  param_id 2: Bypass [0=off, 1=on]
     *  param_id 3: Context Length Scale [0.0, 1.0] */
    void   setParamValue(uint32_t param_id, double value);
    double getParamValue(uint32_t param_id) const;
    bool   isBypassed() const;
    void   setBypass(bool bypass);

    // Parameter batch processing (called from CLAP params_flush)
    void processParamUpdates(const std::vector<ParamSnapshot>& updates);

    // -------------------------------------------------------------------------
    // Session — called when host transport changes
    // -------------------------------------------------------------------------
    void updateSessionFromTransport(double bpm, double tsig_num, double tsig_denom, double pos);

    // -------------------------------------------------------------------------
    // Model loading — non-RT thread only
    // -------------------------------------------------------------------------

    /** Load a .mamba_weights binary file. Thread-safe. Never call from RT. */
    bool loadModel(const std::string& path);
    bool isModelLoaded() const;

private:
    std::unique_ptr<MambaModel> mamba_model_;
    std::unique_ptr<MambaModel> snapshot_model_;
    int sample_rate_;

    static constexpr int CROSSFADE_LEN = 128;
    std::atomic<int> crossfade_samples_{0};

    // Atomic state — written by non-RT thread, read by RT thread
    std::atomic<bool>  activated_{false};
    std::atomic<bool>  bypass_{false};
    std::atomic<bool>  model_loaded_{false};
    std::atomic<float> dry_wet_mix_{0.5f};
    std::atomic<float> context_scale_{1.0f};
    std::atomic<int>   model_type_{0};

    // Parameter gating and flush control
    ParameterGate param_gate_;
    std::atomic<bool> flush_pending_{false};
};

#endif // PLUGIN_PROCESSOR_H
