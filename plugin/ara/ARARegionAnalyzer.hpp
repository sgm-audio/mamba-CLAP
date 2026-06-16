#pragma once
#ifdef MAMBA_ENABLE_ARA
// ARARegionAnalyzer — stateless utility that reads raw audio samples from
// an ARA audio source and computes the feature vector used by the Mamba
// inference engine to condition arrangement-level decisions.
//
// Design: pure functions operating on pre-allocated scratch buffers;
// no dynamic allocation inside compute_features().  The caller (typically
// ARADocumentController::analyze_audio_source on a worker thread) supplies
// the scratch memory.
//
// Features computed:
//   • RMS energy per 100 ms window
//   • Spectral flux (frame-to-frame magnitude difference via mini FFT)
//   • Crest factor (peak / RMS)
//   • Zero-crossing rate (rough timbral roughness proxy)

#include <ARA_API/ARAInterface.h>
#include <span>
#include <vector>
#include <cstdint>

namespace mamba::ara {

struct AudioFeatures {
    float avg_rms{0.f};
    float peak_rms{0.f};
    float spectral_flux{0.f};
    float crest_factor{0.f};
    float zcr{0.f};             // zero-crossing rate
};

// Scratch buffers pre-allocated by the caller to avoid any alloc on the
// worker thread once analysis is underway.
struct AnalysisScratch {
    std::vector<float> mono_buf;    // resized to window_samples
    std::vector<float> prev_mag;    // FFT magnitude frame (n_fft/2+1)
    std::vector<float> curr_mag;
    std::vector<float> fft_buf;     // complex FFT scratch (n_fft * 2)

    void reserve(uint32_t window_samples, uint32_t n_fft) {
        mono_buf.resize(window_samples);
        prev_mag.resize(n_fft / 2 + 1, 0.f);
        curr_mag.resize(n_fft / 2 + 1, 0.f);
        fft_buf.resize(n_fft * 2, 0.f);
    }
};

class ARARegionAnalyzer {
public:
    // n_fft must be a power of two; window_sec is the analysis hop in seconds.
    ARARegionAnalyzer(uint32_t n_fft = 512, float window_sec = 0.1f);

    // Synchronously reads samples via ARA host APIs and returns feature vector.
    // Called from a worker thread; the ARA host must have sample access enabled.
    AudioFeatures compute_features(
        const ARA::ARAAudioSourceHostRef source_host_ref,
        const ARA::ARAHostAudioReader&   reader,
        double                           source_sample_rate,
        int64_t                          num_source_samples,
        int32_t                          num_channels,
        AnalysisScratch&                 scratch) const;

private:
    uint32_t n_fft_;
    float    window_sec_;

    // Real FFT via split-radix DIF; no external dependency.
    // Writes magnitude spectrum to |out_mag| (size n_fft/2+1).
    void compute_fft_magnitude(std::span<const float> in,
                               std::span<float>       out_mag,
                               std::span<float>       scratch_complex) const;
};

} // namespace mamba::ara
#endif // MAMBA_ENABLE_ARA
