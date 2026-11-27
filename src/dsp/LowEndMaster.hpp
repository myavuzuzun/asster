#pragma once
#include <cmath>
#include <algorithm>
#include "Biquad.hpp"

namespace Asster {

class LowEndMaster {
public:
    LowEndMaster() {
        init(44100.0f);
    }

    void init(float sampleRate) {
        sr = (sampleRate > 8000.0f) ? sampleRate : 44100.0f;
        updateFilters();
        reset();
    }

    void reset() {
        subHp1.reset();
        subHp2.reset();
        crossoverLp.reset();
        crossoverHp.reset();
    }

    void setSubCutFreq(float freqHz) {
        subCutHz = std::clamp(freqHz, 15.0f, 60.0f);
        updateFilters();
    }

    void setMonoCrossoverFreq(float freqHz) {
        crossoverHz = std::clamp(freqHz, 40.0f, 200.0f);
        updateFilters();
    }

    void setMonoEnabled(bool enabled) {
        monoEnabled = enabled;
    }

    void setSubCutEnabled(bool enabled) {
        subCutEnabled = enabled;
    }

    void process(float* left, float* right, int numSamples) {
        if (!left || numSamples <= 0) return;

        for (int i = 0; i < numSamples; ++i) {
            float inL = left[i];
            float inR = right ? right[i] : inL;
            __m128 v = _mm_set_ps(0.0f, 0.0f, inR, inL);

            // 1. Subsonic High-Pass (18-24 dB/oct steep slope via cascaded biquads)
            if (subCutEnabled) {
                v = subHp2.processVec(subHp1.processVec(v));
            }

            // 2. Mono-Bass Crossover
            if (monoEnabled && right) {
                __m128 lowV = crossoverLp.processVec(v);
                __m128 highV = crossoverHp.processVec(v);

                alignas(16) float lowArr[4], highArr[4];
                _mm_store_ps(lowArr, lowV);
                _mm_store_ps(highArr, highV);

                // Sum low frequencies to mono (0.5 * (L + R))
                float monoLow = 0.5f * (lowArr[0] + lowArr[1]);
                left[i] = monoLow + highArr[0];
                right[i] = monoLow + highArr[1];
            } else {
                alignas(16) float outArr[4];
                _mm_store_ps(outArr, v);
                left[i] = outArr[0];
                if (right) right[i] = outArr[1];
            }
        }
    }

    float getMagnitudeDb(float freqHz) const {
        if (!subCutEnabled) return 0.0f;
        return subHp1.getMagnitudeDb(freqHz, sr) + subHp2.getMagnitudeDb(freqHz, sr);
    }

private:
    void updateFilters() {
        // Cascaded Butterworth HP filters for steep subsonic roll-off
        subHp1.setParams(FilterType::HighPass, sr, subCutHz, 0.7071f, 0.0f);
        subHp2.setParams(FilterType::HighPass, sr, subCutHz, 0.7071f, 0.0f);

        // Linkwitz-Riley crossover (Q = 0.5 for flat amplitude recombination)
        crossoverLp.setParams(FilterType::LowPass, sr, crossoverHz, 0.5f, 0.0f);
        crossoverHp.setParams(FilterType::HighPass, sr, crossoverHz, 0.5f, 0.0f);
    }

    float sr = 44100.0f;
    float subCutHz = 28.0f;
    float crossoverHz = 90.0f;
    bool subCutEnabled = true;
    bool monoEnabled = true;

    StereoBiquad subHp1, subHp2;
    StereoBiquad crossoverLp, crossoverHp;
};

} // namespace Asster
