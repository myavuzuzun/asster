#pragma once
#include <xmmintrin.h>
#include <cmath>
#include <algorithm>
#include "FastMath.hpp"

namespace Asster {

inline float flushDenormal(float val) {
    return (std::abs(val) < 1e-15f) ? 0.0f : val;
}

enum class FilterType {
    LowPass,
    HighPass,
    BandPass,
    Notch,
    Peak,
    LowShelf,
    HighShelf
};

class Biquad {
public:
    Biquad() {
        reset();
    }

    void reset() {
        z1 = 0.0f;
        z2 = 0.0f;
    }

    void setParams(FilterType type, float sampleRate, float frequency, float q, float gainDb = 0.0f) {
        if (sampleRate <= 0.0f) sampleRate = 44100.0f;
        float nyquist = sampleRate * 0.495f;
        frequency = std::clamp(frequency, 10.0f, nyquist);
        q = std::max(0.01f, q);

        float w0 = 2.0f * 3.14159265358979323846f * frequency / sampleRate;
        float cosW = std::cos(w0);
        float sinW = std::sin(w0);
        float alpha = sinW / (2.0f * q);
        float A = std::pow(10.0f, gainDb / 40.0f);

        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a0 = 1.0f, a1 = 0.0f, a2 = 0.0f;

        switch (type) {
            case FilterType::LowPass:
                b0 = (1.0f - cosW) * 0.5f;
                b1 = 1.0f - cosW;
                b2 = (1.0f - cosW) * 0.5f;
                a0 = 1.0f + alpha;
                a1 = -2.0f * cosW;
                a2 = 1.0f - alpha;
                break;

            case FilterType::HighPass:
                b0 = (1.0f + cosW) * 0.5f;
                b1 = -(1.0f + cosW);
                b2 = (1.0f + cosW) * 0.5f;
                a0 = 1.0f + alpha;
                a1 = -2.0f * cosW;
                a2 = 1.0f - alpha;
                break;

            case FilterType::BandPass:
                b0 = alpha;
                b1 = 0.0f;
                b2 = -alpha;
                a0 = 1.0f + alpha;
                a1 = -2.0f * cosW;
                a2 = 1.0f - alpha;
                break;

            case FilterType::Notch:
                b0 = 1.0f;
                b1 = -2.0f * cosW;
                b2 = 1.0f;
                a0 = 1.0f + alpha;
                a1 = -2.0f * cosW;
                a2 = 1.0f - alpha;
                break;

            case FilterType::Peak: {
                b0 = 1.0f + alpha * A;
                b1 = -2.0f * cosW;
                b2 = 1.0f - alpha * A;
                a0 = 1.0f + alpha / A;
                a1 = -2.0f * cosW;
                a2 = 1.0f - alpha / A;
                break;
            }

            case FilterType::LowShelf: {
                float sqrtA = std::sqrt(A);
                b0 = A * ((A + 1.0f) - (A - 1.0f) * cosW + 2.0f * sqrtA * alpha);
                b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosW);
                b2 = A * ((A + 1.0f) - (A - 1.0f) * cosW - 2.0f * sqrtA * alpha);
                a0 = (A + 1.0f) + (A - 1.0f) * cosW + 2.0f * sqrtA * alpha;
                a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosW);
                a2 = (A + 1.0f) + (A - 1.0f) * cosW - 2.0f * sqrtA * alpha;
                break;
            }

            case FilterType::HighShelf: {
                float sqrtA = std::sqrt(A);
                b0 = A * ((A + 1.0f) + (A - 1.0f) * cosW + 2.0f * sqrtA * alpha);
                b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosW);
                b2 = A * ((A + 1.0f) + (A - 1.0f) * cosW - 2.0f * sqrtA * alpha);
                a0 = (A + 1.0f) - (A - 1.0f) * cosW + 2.0f * sqrtA * alpha;
                a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosW);
                a2 = (A + 1.0f) - (A - 1.0f) * cosW - 2.0f * sqrtA * alpha;
                break;
            }
        }

        float invA0 = 1.0f / a0;
        this->a1 = a1 * invA0;
        this->a2 = a2 * invA0;
        this->b0 = b0 * invA0;
        this->b1 = b1 * invA0;
        this->b2 = b2 * invA0;
    }

    // Fast coefficient update without sin/cos/pow for dynamic EQ & stabilizer
    void setPeakFast(float cosW, float alpha, float gainDb) {
        float A = fastExp2(gainDb * 0.08304820237218406f); // 10^(gainDb/40)
        float b0 = 1.0f + alpha * A;
        float b1 = -2.0f * cosW;
        float b2 = 1.0f - alpha * A;
        float a0 = 1.0f + alpha / A;
        float a1 = b1;
        float a2 = 1.0f - alpha / A;
        float invA0 = 1.0f / a0;
        this->a1 = a1 * invA0;
        this->a2 = a2 * invA0;
        this->b0 = b0 * invA0;
        this->b1 = b1 * invA0;
        this->b2 = b2 * invA0;
    }

    void setShelfFast(bool isHighShelf, float cosW, float alpha, float gainDb) {
        float A = fastExp2(gainDb * 0.08304820237218406f);
        float sqrtA = std::sqrt(A);
        float twoSqrtAAlpha = 2.0f * sqrtA * alpha;
        float b0, b1, b2, a0, a1, a2;
        if (!isHighShelf) {
            b0 = A * ((A + 1.0f) - (A - 1.0f) * cosW + twoSqrtAAlpha);
            b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosW);
            b2 = A * ((A + 1.0f) - (A - 1.0f) * cosW - twoSqrtAAlpha);
            a0 = (A + 1.0f) + (A - 1.0f) * cosW + twoSqrtAAlpha;
            a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosW);
            a2 = (A + 1.0f) + (A - 1.0f) * cosW - twoSqrtAAlpha;
        } else {
            b0 = A * ((A + 1.0f) + (A - 1.0f) * cosW + twoSqrtAAlpha);
            b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosW);
            b2 = A * ((A + 1.0f) + (A - 1.0f) * cosW - twoSqrtAAlpha);
            a0 = (A + 1.0f) - (A - 1.0f) * cosW + twoSqrtAAlpha;
            a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosW);
            a2 = (A + 1.0f) - (A - 1.0f) * cosW - twoSqrtAAlpha;
        }
        float invA0 = 1.0f / a0;
        this->a1 = a1 * invA0;
        this->a2 = a2 * invA0;
        this->b0 = b0 * invA0;
        this->b1 = b1 * invA0;
        this->b2 = b2 * invA0;
    }

    inline float process(float in) {
        // Direct Form II Transposed (audio thread runs with SSE FTZ/DAZ enabled)
        float out = b0 * in + z1;
        z1 = b1 * in - a1 * out + z2;
        z2 = b2 * in - a2 * out;
        return out;
    }

    void flushState() {
        z1 = flushDenormal(z1);
        z2 = flushDenormal(z2);
    }

    float getMagnitudeDb(float freqHz, float sampleRate) const {
        if (sampleRate <= 0.0f) sampleRate = 44100.0f;
        float w = 2.0f * 3.14159265358979323846f * freqHz / sampleRate;
        float cosW = std::cos(w);
        float sinW = std::sin(w);
        float cos2W = std::cos(2.0f * w);
        float sin2W = std::sin(2.0f * w);

        float numRe = b0 + b1 * cosW + b2 * cos2W;
        float numIm = -b1 * sinW - b2 * sin2W;
        float denRe = 1.0f + a1 * cosW + a2 * cos2W;
        float denIm = -a1 * sinW - a2 * sin2W;

        float magSq = (numRe * numRe + numIm * numIm) / std::max(1e-20f, denRe * denRe + denIm * denIm);
        return 10.0f * std::log10(std::max(1e-12f, magSq));
    }

private:
    friend class StereoBiquad;
    float a1 = 0.0f;
    float a2 = 0.0f;
    float b0 = 1.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;
    float z1 = 0.0f;
    float z2 = 0.0f;
};

class StereoBiquad {
public:
    StereoBiquad() { reset(); }

    void reset() {
        z1 = _mm_setzero_ps();
        z2 = _mm_setzero_ps();
    }

    void flushState() {
        alignas(16) float v1[4], v2[4];
        _mm_store_ps(v1, z1);
        _mm_store_ps(v2, z2);
        for (int i = 0; i < 2; ++i) {
            if (std::abs(v1[i]) < 1e-20f) v1[i] = 0.0f;
            if (std::abs(v2[i]) < 1e-20f) v2[i] = 0.0f;
        }
        z1 = _mm_load_ps(v1);
        z2 = _mm_load_ps(v2);
    }

    void setParams(FilterType type, float sampleRate, float freq, float q, float gainDb = 0.0f) {
        Biquad tmp;
        tmp.setParams(type, sampleRate, freq, q, gainDb);
        copyCoeffs(tmp);
    }

    void setPeakFast(float cosW, float alpha, float gainDb) {
        Biquad tmp;
        tmp.setPeakFast(cosW, alpha, gainDb);
        copyCoeffs(tmp);
    }

    void setShelfFast(bool isHighShelf, float cosW, float alpha, float gainDb) {
        Biquad tmp;
        tmp.setShelfFast(isHighShelf, cosW, alpha, gainDb);
        copyCoeffs(tmp);
    }

    inline __m128 processVec(__m128 in) {
        __m128 vb0 = _mm_set1_ps(b0);
        __m128 vb1 = _mm_set1_ps(b1);
        __m128 vb2 = _mm_set1_ps(b2);
        __m128 va1 = _mm_set1_ps(a1);
        __m128 va2 = _mm_set1_ps(a2);
        __m128 out = _mm_add_ps(_mm_mul_ps(vb0, in), z1);
        z1 = _mm_add_ps(_mm_sub_ps(_mm_mul_ps(vb1, in), _mm_mul_ps(va1, out)), z2);
        z2 = _mm_sub_ps(_mm_mul_ps(vb2, in), _mm_mul_ps(va2, out));
        return out;
    }

    inline void processSample(float& l, float& r) {
        __m128 in = _mm_set_ps(0.0f, 0.0f, r, l);
        __m128 out = processVec(in);
        alignas(16) float res[4];
        _mm_store_ps(res, out);
        l = res[0];
        r = res[1];
    }

    float getMagnitudeDb(float freqHz, float sampleRate) const {
        Biquad tmp;
        tmp.b0 = b0; tmp.b1 = b1; tmp.b2 = b2;
        tmp.a1 = a1; tmp.a2 = a2;
        return tmp.getMagnitudeDb(freqHz, sampleRate);
    }

private:
    inline void copyCoeffs(const Biquad& src) {
        b0 = src.b0; b1 = src.b1; b2 = src.b2;
        a1 = src.a1; a2 = src.a2;
    }

    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;
    __m128 z1 = _mm_setzero_ps();
    __m128 z2 = _mm_setzero_ps();
};

} // namespace Asster
