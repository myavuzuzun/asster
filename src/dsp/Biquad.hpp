#pragma once
#include <cmath>
#include <algorithm>

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

    inline float process(float in) {
        // Direct Form II Transposed for numerical precision and lowest phase noise
        float out = b0 * in + z1;
        z1 = b1 * in - a1 * out + z2;
        z2 = b2 * in - a2 * out;

        // Flush persistent states to zero to eliminate subnormal/denormal CPU microcode traps
        if (std::abs(z1) < 1e-15f) z1 = 0.0f;
        if (std::abs(z2) < 1e-15f) z2 = 0.0f;
        if (std::abs(out) < 1e-15f) out = 0.0f;

        return out;
    }

private:
    float a1 = 0.0f;
    float a2 = 0.0f;
    float b0 = 1.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;
    float z1 = 0.0f;
    float z2 = 0.0f;
};

} // namespace Asster
