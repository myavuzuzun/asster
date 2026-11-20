#pragma once
#include <cmath>
#include <algorithm>
#include "Biquad.hpp"

namespace Asster {

struct EQBandSettings {
    FilterType type;
    float freqHz;
    float q;
    float gainDb;
};

class MasteringEQ {
public:
    MasteringEQ() {
        init(44100.0f);
    }

    void init(float sampleRate) {
        sr = (sampleRate > 8000.0f) ? sampleRate : 44100.0f;
        updateFilters();
        reset();
    }

    void reset() {
        for (int i = 0; i < 4; ++i) {
            bandL[i].reset();
            bandR[i].reset();
        }
    }

    void setBand(int bandIndex, FilterType type, float freqHz, float q, float gainDb) {
        if (bandIndex < 0 || bandIndex >= 4) return;
        bands[bandIndex] = { type, freqHz, q, gainDb };
        bandL[bandIndex].setParams(type, sr, freqHz, q, gainDb);
        bandR[bandIndex].setParams(type, sr, freqHz, q, gainDb);
    }

    void setBandGain(int bandIndex, float gainDb) {
        if (bandIndex < 0 || bandIndex >= 4) return;
        bands[bandIndex].gainDb = gainDb;
        bandL[bandIndex].setParams(bands[bandIndex].type, sr, bands[bandIndex].freqHz, bands[bandIndex].q, gainDb);
        bandR[bandIndex].setParams(bands[bandIndex].type, sr, bands[bandIndex].freqHz, bands[bandIndex].q, gainDb);
    }

    void process(float* left, float* right, int numSamples) {
        if (!left || numSamples <= 0) return;

        for (int i = 0; i < numSamples; ++i) {
            float inL = left[i];
            float inR = right ? right[i] : inL;

            // Cascade through 4 mastering EQ bands
            for (int b = 0; b < 4; ++b) {
                inL = bandL[b].process(inL);
                inR = bandR[b].process(inR);
            }

            left[i] = inL;
            if (right) right[i] = inR;
        }
    }

private:
    void updateFilters() {
        // Defaults: Gentle mastering profile
        bands[0] = { FilterType::LowShelf,  80.0f, 0.71f, 0.0f };
        bands[1] = { FilterType::Peak,     450.0f, 1.2f,  0.0f };
        bands[2] = { FilterType::Peak,    3500.0f, 1.4f,  0.0f };
        bands[3] = { FilterType::HighShelf, 10000.0f, 0.71f, 0.0f };

        for (int b = 0; b < 4; ++b) {
            bandL[b].setParams(bands[b].type, sr, bands[b].freqHz, bands[b].q, bands[b].gainDb);
            bandR[b].setParams(bands[b].type, sr, bands[b].freqHz, bands[b].q, bands[b].gainDb);
        }
    }

    float sr = 44100.0f;
    EQBandSettings bands[4];
    Biquad bandL[4];
    Biquad bandR[4];
};

} // namespace Asster
