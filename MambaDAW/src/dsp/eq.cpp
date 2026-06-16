#include "eq.h"
#include <cmath>

Eq::Eq() {
    for (int b = 0; b < NUM_BANDS; ++b) {
        coeffs_[b] = {1.f, 0.f, 0.f, 0.f, 0.f};
        for (int ch = 0; ch < 2; ++ch) {
            states_[b][ch] = {0.f, 0.f, 0.f, 0.f};
        }
    }
}

void Eq::setSampleRate(float sr) {
    sample_rate_ = sr;
    dirty_ = true;
}

void Eq::computePeakingCoeffs(BiquadCoeffs& c, float freq_hz, float gain_db, float Q) {
    float A = std::pow(10.f, gain_db / 40.f);
    float w0 = 2.f * M_PI * freq_hz / sample_rate_;
    float cos_w0 = std::cos(w0);
    float sin_w0 = std::sin(w0);
    float alpha = sin_w0 / (2.f * Q);

    float a0 = 1.f + alpha / A;
    c.b0 = (1.f + alpha * A) / a0;
    c.b1 = (-2.f * cos_w0) / a0;
    c.b2 = (1.f - alpha * A) / a0;
    c.a1 = (-2.f * cos_w0) / a0;
    c.a2 = (1.f - alpha / A) / a0;
}

void Eq::setParams(const EffectParamBlock& params) {
    float w = params.eq_wet;
    if (w < 0.f) w = 0.f;
    if (w > 1.f) w = 1.f;

    if (params.eq_low_db != last_low_db_ ||
        params.eq_mid_db != last_mid_db_ ||
        params.eq_high_db != last_high_db_ ||
        params.eq_low_hz != last_low_hz_ ||
        params.eq_high_hz != last_high_hz_ ||
        params.eq_mid_hz != last_mid_hz_ ||
        params.eq_mid_q  != last_mid_q_  ||
        w != last_wet_) {
        last_low_db_  = params.eq_low_db;
        last_mid_db_  = params.eq_mid_db;
        last_high_db_ = params.eq_high_db;
        last_low_hz_  = params.eq_low_hz;
        last_high_hz_ = params.eq_high_hz;
        last_mid_hz_  = params.eq_mid_hz;
        last_mid_q_   = params.eq_mid_q;
        last_wet_     = w;
        dirty_ = true;
    }

    if (!dirty_) return;
    dirty_ = false;

    wet_ = last_wet_;

    computePeakingCoeffs(coeffs_[0], last_low_hz_,  last_low_db_,  1.0f);
    computePeakingCoeffs(coeffs_[1], last_mid_hz_,  last_mid_db_,  last_mid_q_);
    computePeakingCoeffs(coeffs_[2], last_high_hz_, last_high_db_, 1.0f);
}

void Eq::processStereo(float* L, float* R, int n) {
    const float w = wet_;
    for (int b = 0; b < NUM_BANDS; ++b) {
        const BiquadCoeffs& c = coeffs_[b];
        BiquadState& sL = states_[b][0];
        BiquadState& sR = states_[b][1];

        for (int i = 0; i < n; ++i) {
            float xL = L[i];
            float yL = c.b0 * xL + c.b1 * sL.x1 + c.b2 * sL.x2 - c.a1 * sL.y1 - c.a2 * sL.y2;
            if (!std::isfinite(yL)) { yL = 0.f; sL.x1 = 0.f; sL.x2 = 0.f; sL.y1 = 0.f; sL.y2 = 0.f; }
            sL.x2 = sL.x1; sL.x1 = xL;
            sL.y2 = sL.y1; sL.y1 = yL;
            L[i] = xL * (1.f - w) + yL * w;

            float xR = R[i];
            float yR = c.b0 * xR + c.b1 * sR.x1 + c.b2 * sR.x2 - c.a1 * sR.y1 - c.a2 * sR.y2;
            if (!std::isfinite(yR)) { yR = 0.f; sR.x1 = 0.f; sR.x2 = 0.f; sR.y1 = 0.f; sR.y2 = 0.f; }
            sR.x2 = sR.x1; sR.x1 = xR;
            sR.y2 = sR.y1; sR.y1 = yR;
            R[i] = xR * (1.f - w) + yR * w;
        }
    }
}
