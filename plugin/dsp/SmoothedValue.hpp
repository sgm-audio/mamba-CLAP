#pragma once
// SmoothedValue<T> — lock-free, allocation-free parameter smoother.
//
// Used inside process() to ramp parameter changes sample-accurately so
// automation does not cause audible zipper noise.
//
// Algorithm: one-pole IIR low-pass (exponential smoothing).
//   y[n] = α·target + (1-α)·y[n-1]
//   α = 1 - exp(-1 / (τ · sr))   where τ = time constant in seconds.
//
// Thread contract:
//   set_target() — called from the audio thread only, inside process(),
//                  after draining the input event queue.  Writes a single
//                  float with relaxed ordering (same thread reads it).
//   next()       — called per-sample from the audio thread.  Returns the
//                  smoothed value; updates internal state.
//   skip(n)      — advance n samples without computing output (silence gate).
//
// Real-time safety: no allocation, no branch on hot path (except is_at_target).

#include <atomic>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace mamba::dsp {

template<typename T = float>
class SmoothedValue {
public:
    SmoothedValue() = default;

    // ── Initialisation (main thread, before processing) ───────────────────────
    void prepare(double sample_rate, double ramp_time_seconds) noexcept {
        sample_rate_ = static_cast<T>(sample_rate);
        // α recomputed whenever sample rate or ramp time changes.
        alpha_ = static_cast<T>(
            1.0 - std::exp(-1.0 / std::max(ramp_time_seconds * sample_rate, 1.0)));
        current_  = target_;
    }

    void reset(T value) noexcept {
        current_ = target_ = value;
    }

    // ── Audio thread ─────────────────────────────────────────────────────────

    // Call once per process() block, after draining input events.
    void set_target(T value) noexcept { target_ = value; }

    // Call once per sample inside the inner loop.
    T next() noexcept {
        current_ += alpha_ * (target_ - current_);
        return current_;
    }

    // Skip n samples (used when channel is silent to avoid unnecessary work).
    void skip(uint32_t n_samples) noexcept {
        // Closed-form advance: current + (target-current)*(1-(1-α)^n)
        const T coef = static_cast<T>(
            std::pow(static_cast<double>(1.f - alpha_), static_cast<double>(n_samples)));
        current_ = target_ + (current_ - target_) * coef;
    }

    T get_current() const noexcept { return current_; }
    T get_target()  const noexcept { return target_;  }

    bool is_at_target(T epsilon = static_cast<T>(1e-5)) const noexcept {
        const T diff = target_ - current_;
        return (diff < epsilon) && (diff > -epsilon);
    }

private:
    T alpha_{0.01f};
    T current_{0.f};
    T target_{0.f};
    T sample_rate_{44100.f};
};

// ── AutomationBlock — batch-apply a param change at a specific sample offset ─
// Used inside process() to honour the sample-accurate timing of CLAP events.
struct AutomationEvent {
    uint32_t sample_offset{0};
    uint32_t param_id{0};
    float    value{0.f};
};

} // namespace mamba::dsp
