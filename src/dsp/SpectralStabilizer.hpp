#pragma once
#include <cmath>
#include <algorithm>
#include "Biquad.hpp"

namespace Asster {

// Continuous 5-Band Adaptive Spectral Stabilizer (Gullfoss / Ozone Stabilizer style).
// Gently balances sub, mud, presence, harshness, and air in real time toward targetRelDb[5].
class SpectralStabilizer {
public:
    SpectralStabilizer() {
        init(44100.0f);
    }

    void init(float sampleRate) {
        sr = (sampleRate > 8000.0f) ? sampleRate : 44100.0f;
        rmsCoeff = 1.0f - std::exp(-1.0f / (sr * 0.250f)); // 250 ms smooth RMS window
        gainSlew = 1.0f - std::exp(-64.0f / (sr * 0.120f)); // 120 ms smooth EQ slew (per 64 samples)

        bands[0] = { FilterType::LowShelf,  80.0f,    0.71f, -6.0f, 1.0f, 0.01f };
        bands[1] = { FilterType::Peak,      350.0f,   1.10f, -10.0f, 1.0f, 0.01f };
        bands[2] = { FilterType::Peak,      1600.0f,  1.00f, -12.0f, 1.0f, 0.01f };
        bands[3] = { FilterType::Peak,      4500.0f,  1.40f, -14.0f, 1.0f, 0.01f };
        bands[4] = { FilterType::HighShelf, 11000.0f, 0.71f, -18.0f, 1.0f, 0.01f };

        for (int b = 0; b < 5; ++b) {
            float w0 = 2.0f * 3.14159265358979323846f * bands[b].freqHz / sr;
            bands[b].cosW = std::cos(w0);
            bands[b].alpha = std::sin(w0) / (2.0f * bands[b].q);
            FilterType detType = (b == 0) ? FilterType::LowPass
                               : (b == 4 ? FilterType::HighPass : FilterType::BandPass);
            detFilter[b].setParams(detType, sr, bands[b].freqHz, bands[b].q, 0.0f);
            eq[b].setParams(bands[b].type, sr, bands[b].freqHz, bands[b].q, 0.0f);
        }
        reset();
    }

    void reset() {
        broadbandEnergy = 0.0f;
        for (int b = 0; b < 5; ++b) {
            bandEnergy[b] = 0.0f;
            currentGainDb[b] = 0.0f;
            detFilter[b].reset();
            eq[b].reset();
        }
    }

    void setAmount(float pct) {
        amount = std::clamp(pct, 0.0f, 100.0f) / 100.0f;
    }

    void setTargetCurve(const float targets[5]) {
        for (int b = 0; b < 5; ++b) {
            bands[b].targetRelDb = std::clamp(targets[b], -30.0f, 0.0f);
        }
    }

    void process(float* left, float* right, int numSamples) {
        if (!left || numSamples <= 0 || amount <= 0.001f) return;

        for (int i = 0; i < numSamples; ++i) {
            float inL = left[i];
            float inR = right ? right[i] : inL;
            float mono = 0.5f * (inL + inR);

            broadbandEnergy += (mono * mono - broadbandEnergy) * rmsCoeff;
            for (int b = 0; b < 5; ++b) {
                float d = detFilter[b].process(mono);
                bandEnergy[b] += (d * d - bandEnergy[b]) * rmsCoeff;
            }

            if ((i & 63) == 0 && broadbandEnergy > 1e-9f) {
                float broadDb = 10.0f * fastLog10(broadbandEnergy);
                for (int b = 0; b < 5; ++b) {
                    float bDb = 10.0f * fastLog10(std::max(1e-12f, bandEnergy[b]));
                    float relDb = bDb - broadDb;
                    float diff = bands[b].targetRelDb - relDb;
                    float targetGain = std::clamp(diff * 0.35f * amount, -3.5f, 3.5f);
                    float nextGain = currentGainDb[b] + (targetGain - currentGainDb[b]) * gainSlew;
                    if (std::abs(nextGain - currentGainDb[b]) > 0.02f) {
                        currentGainDb[b] = nextGain;
                        if (bands[b].type == FilterType::Peak) {
                            eq[b].setPeakFast(bands[b].cosW, bands[b].alpha, nextGain);
                        } else {
                            bool isHi = (bands[b].type == FilterType::HighShelf);
                            eq[b].setShelfFast(isHi, bands[b].cosW, bands[b].alpha, nextGain);
                        }
                    }
                }
            }

            __m128 v = _mm_set_ps(0.0f, 0.0f, inR, inL);
            for (int b = 0; b < 5; ++b) {
                if (std::abs(currentGainDb[b]) < 0.01f) continue;
                v = eq[b].processVec(v);
            }
            alignas(16) float out[4];
            _mm_store_ps(out, v);
            left[i] = out[0];
            if (right) right[i] = out[1];
        }

        if (broadbandEnergy < 1e-12f) broadbandEnergy = 0.0f;
        for (int b = 0; b < 5; ++b) {
            if (bandEnergy[b] < 1e-12f) bandEnergy[b] = 0.0f;
        }
    }

    float getBandGainDb(int b) const {
        if (b < 0 || b >= 5) return 0.0f;
        return currentGainDb[b];
    }

private:
    struct BandDef {
        FilterType type;
        float freqHz;
        float q;
        float targetRelDb;
        float cosW;
        float alpha;
    };

    float sr = 44100.0f;
    float amount = 0.15f;
    float rmsCoeff = 0.0001f;
    float gainSlew = 0.006f;

    BandDef bands[5];
    float broadbandEnergy = 0.0f;
    float bandEnergy[5] = {0.0f};
    float currentGainDb[5] = {0.0f};

    Biquad detFilter[5];
    StereoBiquad eq[5];
};

} // namespace Asster
