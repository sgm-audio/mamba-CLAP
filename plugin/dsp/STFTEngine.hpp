#pragma once
// STFTEngine — real-time STFT analysis + ISTFT synthesis.
//
// All memory is pre-allocated during construction.
// process() and synthesise() must not allocate or block.
//
// Signal flow (analysis):
//   AudioRingBuffer → overlap-add input frames → FFT → magnitude → mel filter → log mel
//
// Signal flow (synthesis):
//   Inference output → inverse mel → ISTFT magnitude → Griffin-Lim or phase-vocoder → OLA
//
// Window: Hann, 50% overlap.
// FFT: kissFFT (MIT licence, header-only, no dynamic alloc).
//
// Note: only the magnitude mel spectrogram is passed to Mamba.
// Phase reconstruction uses the modified signal's magnitude + original phase
// (phase-vocoder approach) to minimise artefacts without Griffin-Lim iterations.

#include <vector>
#include <span>
#include <cstdint>

namespace mamba::dsp {

class AudioRingBuffer;
class OverlapAddSynthesizer;

struct MelFrame {
    // One frame of log-mel spectrogram in host-memory float.
    // Size = n_mels.  Layout: mel_bin[0..n_mels-1].
    float* data{nullptr};
    int    n_mels{0};
    bool   valid{false};
};

class STFTEngine {
public:
    // n_fft, hop_length, n_mels must match the trained model.
    STFTEngine(int n_fft, int hop_length, int n_mels, double sample_rate);
    ~STFTEngine();

    // Called from the real-time audio process() on the inference buffer
    // after the inference worker has filled it.
    // Returns the number of complete mel frames produced.
    int process(AudioRingBuffer& ring, OverlapAddSynthesizer& ola,
                uint32_t frames_available) noexcept;

    // Inference worker calls this to retrieve the next mel frame.
    // Returns false if no frame is ready.
    bool next_mel_frame(MelFrame& out) noexcept;

    // Inference worker pushes back processed mel frames.
    void push_processed_mel(const float* mel_frame) noexcept;

    int n_fft()      const noexcept { return n_fft_; }
    int hop_length() const noexcept { return hop_length_; }
    int n_mels()     const noexcept { return n_mels_; }

private:
    void apply_hann_window(float* buf, int n) noexcept;
    void compute_fft      (float* complex_buf) noexcept;
    void compute_ifft     (float* complex_buf) noexcept;
    void mag_to_mel       (const float* mag, float* mel) noexcept;
    void mel_to_mag       (const float* mel, float* mag) noexcept;

    int    n_fft_;
    int    hop_length_;
    int    n_mels_;
    double sample_rate_;

    // Pre-allocated analysis / synthesis buffers.
    std::vector<float> window_;           // Hann window [n_fft]
    std::vector<float> analysis_buf_;     // input overlap buffer [n_fft]
    std::vector<float> fft_complex_;      // interleaved complex [n_fft*2]
    std::vector<float> mag_buf_;          // magnitude spectrum [n_fft/2+1]
    std::vector<float> phase_buf_;        // original phase [n_fft/2+1]
    std::vector<float> mel_buf_;          // mel spectrogram frame [n_mels]
    std::vector<float> mel_basis_;        // mel filterbank [n_mels × (n_fft/2+1)]
    std::vector<float> inv_mel_basis_;    // pseudo-inverse [n_fft/2+1 × n_mels]

    // Mel frame ring buffer between analysis (audio thread) and inference (worker).
    // Capacity = 32 frames (≈ 32 × hop_length samples of lookahead).
    static constexpr int kMelRingCap = 32;
    std::vector<float>   mel_ring_;       // [kMelRingCap × n_mels]
    int                  mel_write_{0};
    int                  mel_read_{0};

    int analysis_buf_fill_{0};  // samples currently in analysis_buf_
};

} // namespace mamba::dsp
