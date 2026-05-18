#ifndef PLUGIN_PROCESSOR_H
#define PLUGIN_PROCESSOR_H

#include "mamba_model.h"
#include "dsp/dsp_chain.h"
#include "dsp/effect_params.h"
#include "feature_extractor.h"
#include "feature_extractor_rt.h"
#include "inference_thread.h"
#include "effect_decoder.h"
#include <memory>
#include <cstdint>
#include <string>
#include <atomic>
#include <array>
#include <vector>
#include "parameter_gate.h"
#include "param_smoother.h"
#include "state_serialization.h"
#include "cpu_budget_monitor.h"

// Thread-domain tags per ADR-002:
//   [Audio]  — hard RT audio thread only. No allocations, locks, I/O, or logging.
//   [Main]   — main/host thread only. May allocate, do I/O, start/stop threads.
//   [Any]    — safe from any thread (atomic read or const-only access).
//   [Audio|Main] — callable from either audio or main thread (uses atomics for data exchange).

static constexpr uint32_t kNumClapParams = 33;

enum class EffectMode : int { Auto = 0, Manual = 1 };

class PluginProcessor {
public:
    explicit PluginProcessor(int sample_rate = 44100);   // [Main]
    ~PluginProcessor();                                    // [Main]

    void activate(double sample_rate, uint32_t min_frame_count, uint32_t max_frame_count); // [Main]
    void deactivate();                                     // [Main]
    void resetState();                                     // [Main]

    void processBlock(float** inputs, float** outputs,    // [Audio]
        int32_t num_frames,
        int32_t num_input_channels,
        int32_t num_output_channels);

    // param_id 0: Dry/Wet [0, 1]
    // param_id 1: Effect Mode [0=auto, 1=manual] stepped
    // param_id 2: Bypass [0, 1] stepped
    // param_id 3: Input Gain [0, 2]
    // param_id 4: Output Gain [0, 2]
    // param_id 5: Context Length [10, 120] seconds
    // param_id 6: EQ Wet [0, 1]
    // param_id 7: Comp Wet [0, 1]
    // param_id 8: Sat Wet [0, 1]
    // param_id 9: Rev Wet [0, 1]
    // param_id 10: Manual Comp Thresh [-60, 0] dB
    // param_id 11: Manual Rev Decay [0.1, 8.0] s
    // param_id 12: Effect Type [0=All,1=EQ,2=Comp,3=Sat,4=Rev] stepped
    // param_id 13: Reset Context [0, 1] trigger — fires flush, auto-resets to 0
    void setParamValue(uint32_t param_id, double value);   // [Audio|Main] — writes atomics
    double getParamValue(uint32_t param_id) const;         // [Any] — reads atomics
    bool isBypassed() const;                               // [Any]
    void setBypass(bool bypass);                           // [Audio|Main] — writes atomic

    void processParamUpdates(const std::vector<ParamSnapshot>& updates); // [Main]
    void processParamUpdates(const ParamSnapshot* updates, uint32_t count); // [Audio] — RT-safe overload

    void updateSessionFromTransport(double bpm, double tsig_num, // [Audio]
        double tsig_denom, double pos,
        bool is_playing, bool is_looping, bool has_seeked);

    bool loadModel(const std::string& path);               // [Main] — file I/O, weight loading
    bool isModelLoaded() const;                            // [Any]
    bool isDegraded() const { return degraded_.load(std::memory_order_relaxed); } // [Any]
    bool isCpuOverloaded() const { return cpu_overload_.load(std::memory_order_relaxed); } // [Any]
    float cpuLoad() const { return cpu_budget_.currentLoad(); } // [Any]
    bool checksumMismatchDetected() { return checksum_mismatch_.exchange(false, std::memory_order_acq_rel); } // [Main] — consume-once
    double sampleRate() const { return static_cast<double>(sample_rate_); } // [Any]

    bool clipDetected() const { return dsp_chain_.clipDetected(); } // [Any]
    void clearClipFlag() { dsp_chain_.clearClipFlag(); }  // [Main]

    float getContextFillRatio() const noexcept;           // [Any] — atomic read
    bool isInferenceActive() const noexcept;              // [Any] — atomic read
    float getAIParamValue(int index) const noexcept;      // [Any] — atomic read
    float getPeakL() const noexcept { return level_peak_l_.load(std::memory_order_relaxed); } // [Any]
    float getPeakR() const noexcept { return level_peak_r_.load(std::memory_order_relaxed); } // [Any]

    void handleDeferredReinit();                          // [Main] — restarts inference thread
    bool hasDeferredReinit() const;                       // [Any] — atomic read

    std::vector<uint8_t> saveState();                     // [Main] — stops inference thread
    bool loadState(const uint8_t* data, uint32_t size);   // [Main] — may load weights, reset state

private:
    void buildManualEffectBlock(EffectParamBlock& out) const; // [Audio]
    void readInferenceParams(EffectParamBlock& out);          // [Audio]

    std::unique_ptr<MambaModel> mamba_model_;
    int sample_rate_;
    std::string weight_path_;

    ParameterGate param_gate_;
    ParamSmoother param_smoother_;

    static constexpr int kDspMaxBlockFrames = 8192;

    static constexpr float kContextLenMin = 10.f;
    static constexpr float kContextLenMax = 120.f;

    DspChain dsp_chain_;
    FeatureExtractor feature_extractor_;
    std::unique_ptr<InferenceThread> inference_thread_;
    MambaInferenceEngine* mamba_engine_{nullptr}; // non-owning; owned by inference_thread_
    EffectDecoder effect_decoder_;

    std::array<std::atomic<float>, N_EFFECT_PARAMS> ai_params_;

    std::array<float, kDspMaxBlockFrames> dsp_dry_l_{};
    std::array<float, kDspMaxBlockFrames> dsp_dry_r_{};
    std::array<float, kDspMaxBlockFrames> dsp_wet_l_{};
    std::array<float, kDspMaxBlockFrames> dsp_wet_r_{};
    std::array<float, kDspMaxBlockFrames> mel_mono_scratch_{};
    std::array<float, N_MEL * 4> mel_scratch_{};

    int mel_hop_counter_{0};

    std::atomic<bool> activated_{false};
    std::atomic<bool> bypass_{false};
    std::atomic<bool> model_loaded_{false};
    std::atomic<float> dry_wet_mix_{0.5f};
    std::atomic<int> effect_mode_{0};
    std::atomic<float> input_gain_{1.0f};
    std::atomic<float> output_gain_{1.0f};
    std::atomic<float> context_length_s_{60.0f};
    std::atomic<float> eq_wet_{1.0f};
    std::atomic<float> comp_wet_{1.0f};
    std::atomic<float> sat_wet_{1.0f};
    std::atomic<float> rev_wet_{1.0f};
    std::atomic<float> manual_comp_thresh_db_{-20.0f};
    std::atomic<float> manual_rev_decay_s_{1.5f};
    std::atomic<int> effect_type_{0};
    std::atomic<bool> reset_context_{false};

    // Per-section manual enable flags (params 14–17)
    std::atomic<bool>  eq_manual_enable_{false};
    std::atomic<bool>  comp_manual_enable_{false};
    std::atomic<bool>  sat_manual_enable_{false};
    std::atomic<bool>  rev_manual_enable_{false};

    // Manual EQ (params 18–24, physical units)
    std::atomic<float> manual_eq_lo_gain_{0.f};
    std::atomic<float> manual_eq_lo_freq_{200.f};
    std::atomic<float> manual_eq_mid_gain_{0.f};
    std::atomic<float> manual_eq_mid_freq_{1000.f};
    std::atomic<float> manual_eq_mid_q_{1.0f};
    std::atomic<float> manual_eq_hi_gain_{0.f};
    std::atomic<float> manual_eq_hi_freq_{8000.f};

    // Manual compressor extra params (params 25–28; thresh reuses param 10)
    std::atomic<float> manual_comp_ratio_{2.f};
    std::atomic<float> manual_comp_attack_ms_{10.f};
    std::atomic<float> manual_comp_release_ms_{100.f};
    std::atomic<float> manual_comp_makeup_db_{0.f};

    // Manual saturator (params 29–30)
    std::atomic<float> manual_sat_drive_{0.3f};
    std::atomic<float> manual_sat_tone_{0.f};  // stored as -1..1, converted to 0..1 in DSP

    // Manual reverb extra params (params 31–32; decay reuses param 11)
    std::atomic<float> manual_rev_predelay_ms_{20.f};
    std::atomic<float> manual_rev_diffusion_{0.5f};

    std::atomic<bool> flush_pending_{false};
    std::atomic<bool> reinit_pending_{false};
    std::atomic<bool> nan_reset_pending_{false};
    std::atomic<bool> degraded_{false};

    static constexpr int kNanThreshold = 4;
    int nan_count_{0};

    CpuBudgetMonitor cpu_budget_;
    std::atomic<bool> cpu_overload_{false};
    std::atomic<bool> checksum_mismatch_{false};

    std::atomic<float> level_peak_l_{0.f};
    std::atomic<float> level_peak_r_{0.f};
};

#endif // PLUGIN_PROCESSOR_H
