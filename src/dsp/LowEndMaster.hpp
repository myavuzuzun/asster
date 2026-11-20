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
        subHpL1.reset();
        subHpL2.reset();
        subHpR1.reset();
        subHpR2.reset();
        crossoverLpL.reset();
        crossoverLpR.reset();
        crossoverHpL.reset();
        crossoverHpR.reset();
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

            // 1. Subsonic High-Pass (18-24 dB/oct steep slope via cascaded biquads)
            if (subCutEnabled) {
                inL = subHpL2.process(subHpL1.process(inL));
                inR = subHpR2.process(subHpR1.process(inR));
            }

            // 2. Mono-Bass Crossover
            if (monoEnabled && right) {
                float lowL = crossoverLpL.process(inL);
                float lowR = crossoverLpR.process(inR);
                float highL = crossoverHpL.process(inL);
                float highR = crossoverHpR.process(inR);

                // Sum low frequencies to mono (0.5 * (L + R))
                float monoLow = 0.5f * (lowL + lowR);

                inL = monoLow + highL;
                inR = monoLow + highR;
            }

            left[i] = inL;
            if (right) right[i] = inR;
        }
    }

private:
    void updateFilters() {
        // Cascaded Butterworth HP filters for steep subsonic roll-off
        subHpL1.setParams(FilterType::HighPass, sr, subCutHz, 0.7071f, 0.0f);
        subHpL2.setParams(FilterType::HighPass, sr, subCutHz, 0.7071f, 0.0f);
        subHpR1.setParams(FilterType::HighPass, sr, subCutHz, 0.7071f, 0.0f);
        subHpR2.setParams(FilterType::HighPass, sr, subCutHz, 0.7071f, 0.0f);

        // Linkwitz-Riley crossover (Q = 0.5 for flat amplitude recombination)
        crossoverLpL.setParams(FilterType::LowPass, sr, crossoverHz, 0.5f, 0.0f);
        crossoverLpR.setParams(FilterType::LowPass, sr, crossoverHz, 0.5f, 0.0f);
        crossoverHpL.setParams(FilterType::HighPass, sr, crossoverHz, 0.5f, 0.0f);
        crossoverHpR.setParams(FilterType::HighPass, sr, crossoverHz, 0.5f, 0.0f);
    }

    float sr = 44100.0f;
    float subCutHz = 28.0f;
    float crossoverHz = 90.0f;
    bool subCutEnabled = true;
    bool monoEnabled = true;

    Biquad subHpL1, subHpL2;
    Biquad subHpR1, subHpR2;
    Biquad crossoverLpL, crossoverLpR;
    Biquad crossoverHpL, crossoverHpR;
};

} // namespace Asster
