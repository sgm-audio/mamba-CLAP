#pragma once

#include <cstdint>
#include <cmath>

/**
 * @file feature_extractor_rt.h
 * @brief Real-time safe audio feature extraction for L4 frame context.
 *
 * All functions are allocation-free (use caller-provided buffers or stack).
 * No heap allocations, no STL containers, no exceptions.
 * Designed to run in the audio callback if needed (though typically L4
 * is assembled on the inference thread from features passed from RT).
 */

/**
 * @brief Audio features for a single buffer/frame.
 *
 * All values are normalized where appropriate.
 */
struct AudioFeatures {
    float rms;                 // [0, 1] approximate (1.0 = 0 dBFS)
    float peak;                // [0, 1]
    float crest_factor;        // peak / rms, typically 1..10
    float spectral_centroid;   // Hz, 0..sample_rate/2
    float spectral_flatness;   // 0..1 (tone-like to noise-like)
    float zero_crossing_rate;  // 0..1 (normalized to sample_rate)
    bool  onset_detected;      // transient flag
    float fundamental_freq;    // Hz, 0 if none
    float pitch_confidence;    // 0..1
    float hnr_db;              // harmonic-to-noise ratio in dB
};

/**
 * @brief Compute RMS of a buffer (mono, interleaved not assumed).
 *
 * @param samples Pointer to audio samples
 * @param num_samples Number of samples
 * @return Floating-point RMS (0..~1.414 for full-scale sine)
 */
static inline float compute_rms(const float* samples, int32_t num_samples) {
    if (num_samples <= 0) return 0.0f;
    double sum_sq = 0.0;
    for (int32_t i = 0; i < num_samples; ++i) {
        float s = samples[i];
        sum_sq += s * s;
    }
    return static_cast<float>(std::sqrt(sum_sq / num_samples));
}

/**
 * @brief Compute peak of a buffer.
 *
 * @param samples Pointer to audio samples
 * @param num_samples Number of samples
 * @return Absolute maximum sample value [0, 1] (assumes normalized audio)
 */
static inline float compute_peak(const float* samples, int32_t num_samples) {
    if (num_samples <= 0) return 0.0f;
    float peak = 0.0f;
    for (int32_t i = 0; i < num_samples; ++i) {
        float abs_val = std::abs(samples[i]);
        if (abs_val > peak) peak = abs_val;
    }
    return peak;
}

/**
 * @brief Compute zero-crossing rate.
 *
 * Counts sign changes between consecutive samples.
 *
 * @param samples Pointer to audio samples
 * @param num_samples Number of samples
 * @return ZCR normalized to [0, 1] (1.0 = every sample crosses)
 */
static inline float compute_zcr(const float* samples, int32_t num_samples) {
    if (num_samples < 2) return 0.0f;
    int32_t crossings = 0;
    for (int32_t i = 1; i < num_samples; ++i) {
        float prev = samples[i-1];
        float curr = samples[i];
        if ((prev <= 0.0f && curr > 0.0f) || (prev >= 0.0f && curr < 0.0f)) {
            crossings++;
        }
    }
    return static_cast<float>(crossings) / static_cast<float>(num_samples - 1);
}

/**
 * @brief Detect onset via simple energy-based transient detection.
 *
 * Uses an envelope follower with attack/release to detect rapid increase.
 *
 * @param samples Input audio buffer
 * @param num_samples Number of samples
 * @return true if significant transient detected
 */
static inline bool detect_onset(const float* samples, int32_t num_samples) {
    if (num_samples < 128) return false;
    // Simple: compare first 16 samples RMS to last 64 samples RMS
    float early_rms = 0.0f, late_rms = 0.0f;
    double sum_early = 0.0, sum_late = 0.0;
    for (int32_t i = 0; i < 16; ++i) sum_early += samples[i] * samples[i];
    for (int32_t i = num_samples - 64; i < num_samples; ++i) sum_late += samples[i] * samples[i];
    early_rms = static_cast<float>(std::sqrt(sum_early / 16.0));
    late_rms = static_cast<float>(std::sqrt(sum_late / 64.0));
    // Onset if late RMS is significantly higher than early
    return late_rms > early_rms * 2.0f && late_rms > 0.01f;
}

/**
 * @brief Estimate fundamental frequency via normalized autocorrelation.
 *
 * Uses peak-picking on the normalized autocorrelation function and provides
 * a confidence metric.
 *
 * @param samples Input buffer (mono)
 * @param num_samples Number of samples (use at least 1024 for accuracy)
 * @param sample_rate Sample rate in Hz
 * @param out_frequency If non-null, receives estimated frequency in Hz
 * @param out_confidence If non-null, receives confidence [0,1]
 * @return true if pitch detected, false if likely unvoiced
 */
static inline bool estimate_pitch(const float* samples, int32_t num_samples,
                                   double sample_rate,
                                   float* out_frequency, float* out_confidence) {
    if (num_samples < 1024) {
        if (out_frequency) *out_frequency = 0.0f;
        if (out_confidence) *out_confidence = 0.0f;
        return false;
    }

    const int32_t min_period = static_cast<int32_t>(sample_rate / 500.0); // 500 Hz max
    const int32_t max_period = static_cast<int32_t>(sample_rate / 80.0);  // 80 Hz min

    double best_corr = -1.0;
    int32_t best_period = 0;

    for (int32_t period = min_period; period <= max_period; ++period) {
        double corr = 0.0;
        double energy1 = 0.0;
        double energy2 = 0.0;
        for (int32_t i = 0; i < num_samples - period; ++i) {
            corr += samples[i] * samples[i + period];
            energy1 += samples[i] * samples[i];
            energy2 += samples[i + period] * samples[i + period];
        }
        double norm = std::sqrt(energy1 * energy2);
        double c = (norm > 1e-10) ? (corr / norm) : 0.0;
        if (c > best_corr) {
            best_corr = c;
            best_period = period;
        }
    }

    if (best_corr < 0.3 || best_period == 0) {
        if (out_frequency) *out_frequency = 0.0f;
        if (out_confidence) *out_confidence = 0.0f;
        return false;
    }

    if (out_frequency)
        *out_frequency = static_cast<float>(sample_rate / best_period);
    if (out_confidence)
        *out_confidence = static_cast<float>(best_corr);
    return true;
}

/**
 * @brief Compute harmonic-to-noise ratio (HNR) in dB.
 *
 * Very basic: ratio of energy in harmonic bands vs. total energy.
 * Real implementation would use harmonic sieve or cepstrum.
 *
 * @param rms Overall RMS
 * @param peak Peak amplitude
 * @return HNR in dB (higher = more tonal)
 */
static inline float compute_hnr(float rms, float peak) {
    if (rms < 1e-6f) return 0.0f;
    // Simple proxy: if crest factor is high, likely tonal
    float crest = peak / rms;
    // Map crest to dB: crest 1 -> 0dB, crest 10 -> ~20dB
    float hnr_db = 20.0f * std::log10(std::max(1.0f, crest));
    return hnr_db;
}

/**
 * @brief Extract all features from an audio buffer (RT-safe).
 *
 * @param samples Input audio buffer (assumed mono or summed to mono)
 * @param num_samples Number of samples
 * @param sample_rate Sample rate in Hz
 * @return Populated AudioFeatures struct
 */
static inline AudioFeatures extract_features(const float* samples, int32_t num_samples, double sample_rate) {
    AudioFeatures feats = {};
    feats.rms = compute_rms(samples, num_samples);
    feats.peak = compute_peak(samples, num_samples);
    feats.crest_factor = (feats.rms > 1e-6f) ? (feats.peak / feats.rms) : 1.0f;
    feats.spectral_centroid = sample_rate * 0.25f; // Placeholder: proper implementation requires FFT
    feats.spectral_flatness = 0.5f; // Placeholder
    feats.zero_crossing_rate = compute_zcr(samples, num_samples);
    feats.onset_detected = detect_onset(samples, num_samples);
    feats.hnr_db = compute_hnr(feats.rms, feats.peak);

    // Pitch estimation (only if signal strong enough)
    if (feats.rms > 0.01f) {
        float pitch_hz, confidence;
        if (estimate_pitch(samples, num_samples, sample_rate, &pitch_hz, &confidence)) {
            feats.fundamental_freq = pitch_hz;
            feats.pitch_confidence = confidence;
        }
    }

    return feats;
}

/**
 * @brief Format features as L4 prompt string.
 *
 * NOT RT-safe (allocates string). Call from inference thread.
 *
 * @param feats Audio features
 * @return Formatted L4 prompt
 */
static inline std::string build_frame_prompt(const AudioFeatures& feats) {
    char buf[512];
    std::snprintf(buf, sizeof(buf),
        "Audio frame context:\n"
        "- RMS: %.4f (%.1f dBFS)\n"
        "- Peak: %.4f (%.1f dBFS)\n"
        "- Crest factor: %.2f\n"
        "- Spectral centroid: %.1f Hz\n"
        "- Spectral flatness: %.4f\n"
        "- Zero crossing rate: %.4f\n"
        "- Onset detected: %s\n"
        "- Estimated pitch: %.1f Hz (confidence: %.2f)\n"
        "- Harmonic-to-noise ratio: %.2f dB\n\n"
        "Analyze this frame relative to session context and parameter intent.",
        feats.rms,
        20.0f * std::log10(std::max(feats.rms, 1e-6f)),
        feats.peak,
        20.0f * std::log10(std::max(feats.peak, 1e-6f)),
        feats.crest_factor,
        feats.spectral_centroid,
        feats.spectral_flatness,
        feats.zero_crossing_rate,
        feats.onset_detected ? "yes" : "no",
        feats.fundamental_freq,
        feats.pitch_confidence,
        feats.hnr_db
    );
    return std::string(buf);
}
