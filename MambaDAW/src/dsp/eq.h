#ifndef EQ_H
#define EQ_H

#include "effect_params.h"
#include <cmath>
#include <array>

class Eq {
public:
    Eq();

    void setSampleRate(float sr);
    void setParams(const EffectParamBlock& params);
    void processStereo(float* L, float* R, int n);

private:
    struct BiquadCoeffs {
        float b0 = 1.f, b1 = 0.f, b2 = 0.f;
        float a1 = 0.f, a2 = 0.f;
    };

    struct BiquadState {
        float x1 = 0.f, x2 = 0.f;
        float y1 = 0.f, y2 = 0.f;
    };

    void computePeakingCoeffs(BiquadCoeffs& c, float freq_hz, float gain_db, float Q);

    static constexpr int NUM_BANDS = 3;
    std::array<BiquadCoeffs, NUM_BANDS> coeffs_;
    std::array<BiquadState, 2> states_[NUM_BANDS];
    float sample_rate_ = 44100.f;
    float wet_ = 1.f;
    bool dirty_ = true;

    float last_low_db_ = 0.f, last_mid_db_ = 0.f, last_high_db_ = 0.f;
    float last_low_hz_ = 200.f, last_high_hz_ = 5000.f;
    float last_mid_hz_ = 1000.f, last_mid_q_ = 1.0f;
    float last_wet_ = -1.f;
};

#endif
