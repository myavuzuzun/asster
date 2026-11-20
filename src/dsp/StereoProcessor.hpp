#pragma once
#include <cmath>
#include <algorithm>
#include "Biquad.hpp"

namespace Asster {

class StereoProcessor {
public:
    StereoProcessor() {
        init(44100.0f);
    }

    void init(float sampleRate) {
        sr = (sampleRate > 8000.0f) ? sampleRate : 44100.0f;
        sideAirFilter.setParams(FilterType::HighShelf, sr, 6000.0f, 0.7071f, 1.5f);
        reset();
    }

    void reset() {
        sideAirFilter.reset();
    }

    void setWidth(float percent) {
        width = std::clamp(percent, 0.0f, 200.0f) / 100.0f;
    }

    void setSideAirDb(float db) {
        sideAirDb = std::clamp(db, -6.0f, 6.0f);
        sideAirFilter.setParams(FilterType::HighShelf, sr, 6000.0f, 0.7071f, sideAirDb);
    }

    void process(float* left, float* right, int numSamples) {
        if (!left || !right || numSamples <= 0) return;

        for (int i = 0; i < numSamples; ++i) {
            float inL = left[i];
            float inR = right[i];

            // Mid/Side encoding
            float mid = 0.5f * (inL + inR);
            float side = 0.5f * (inL - inR);

            // Enhance high frequencies in the side channel for dimensional width
            if (std::abs(sideAirDb) > 0.05f) {
                side = sideAirFilter.process(side);
            }

            // Apply width multiplier
            side *= width;

            // Mid/Side decoding
            left[i] = mid + side;
            right[i] = mid - side;
        }
    }

private:
    float sr = 44100.0f;
    float width = 1.15f; // 115% default subtle wide punch
    float sideAirDb = 1.0f;
    Biquad sideAirFilter;
};

} // namespace Asster
