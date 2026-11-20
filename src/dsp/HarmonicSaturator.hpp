#pragma once
#include <cmath>
#include <algorithm>
#include "Biquad.hpp"

namespace Asster {

enum SaturationType {
    SAT_TAPE = 0,
    SAT_TUBE = 1,
    SAT_CONSOLE = 2
};

class HarmonicSaturator {
public:
    HarmonicSaturator() {
        init(44100.0f);
    }

    void init(float sampleRate) {
        sr = (sampleRate > 8000.0f) ? sampleRate : 44100.0f;
        toneFilterL.setParams(FilterType::HighShelf, sr, 4000.0f, 0.7071f, 0.0f);
        toneFilterR.setParams(FilterType::HighShelf, sr, 4000.0f, 0.7071f, 0.0f);
        reset();
    }

    void reset() {
        toneFilterL.reset();
        toneFilterR.reset();
    }

    void setDrive(float percent) {
        drive = std::clamp(percent, 0.0f, 100.0f);
        // Map 0..100% to drive factor 1.0 .. 6.0
        driveGain = 1.0f + (drive / 100.0f) * 3.5f;
    }

    void setMix(float percent) {
        mix = std::clamp(percent, 0.0f, 100.0f) / 100.0f;
    }

    void setType(SaturationType type) {
        this->type = type;
    }

    void setTone(float toneDb) {
        toneFilterL.setParams(FilterType::HighShelf, sr, 4000.0f, 0.7071f, toneDb);
        toneFilterR.setParams(FilterType::HighShelf, sr, 4000.0f, 0.7071f, toneDb);
    }

    void process(float* left, float* right, int numSamples) {
        if (!left || numSamples <= 0 || drive <= 0.01f || mix <= 0.001f) return;

        float invDrive = 1.0f / std::sqrt(driveGain); // Auto-leveling makeup

        for (int i = 0; i < numSamples; ++i) {
            float inL = left[i];
            float inR = right ? right[i] : inL;

            float satL = applySaturation(inL * driveGain) * invDrive;
            float satR = applySaturation(inR * driveGain) * invDrive;

            satL = toneFilterL.process(satL);
            satR = toneFilterR.process(satR);

            // Dry/Wet blend
            left[i] = inL * (1.0f - mix) + satL * mix;
            if (right) right[i] = inR * (1.0f - mix) + satR * mix;
        }
    }

private:
    inline float applySaturation(float x) {
        switch (type) {
            case SAT_TAPE: {
                // Smooth tape compression curve (Arctan / tanh blend)
                float xClamped = std::clamp(x, -3.0f, 3.0f);
                return (1.25f * xClamped - 0.25f * xClamped * xClamped * xClamped) / (1.0f + 0.35f * std::abs(xClamped));
            }
            case SAT_TUBE: {
                // Asymmetric triode tube saturation: adds warm 2nd order harmonic
                float asym = x + 0.18f * x * x;
                return asym / (1.0f + std::abs(asym));
            }
            case SAT_CONSOLE:
            default: {
                // Soft console clipping: gentle knee
                if (x > 1.0f) return 1.0f - std::exp(-x);
                if (x < -1.0f) return -1.0f + std::exp(x);
                return x - (x * x * x) * 0.15f;
            }
        }
    }

    float sr = 44100.0f;
    float drive = 25.0f;
    float driveGain = 1.8f;
    float mix = 0.5f;
    SaturationType type = SAT_TAPE;

    Biquad toneFilterL, toneFilterR;
};

} // namespace Asster
