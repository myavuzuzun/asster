#pragma once
#include <cmath>
#include <algorithm>
#include "Biquad.hpp"

namespace Asster {

// 2-Band Dynamic Mastering EQ:
//   Band 0 (Chug Control): 220 Hz, Q=1.4 — tames palm-mute / low-mid buildup dynamically
//   Band 1 (De-Harsh):     4200 Hz, Q=1.8 — tames harsh cymbal/guitar fizz spikes dynamically
class DynamicEQ {
public:
    DynamicEQ() {
        init(44100.0f);
    }

    void init(float sampleRate) {
        sr = (sampleRate > 8000.0f) ? sampleRate : 44100.0f;
        bands[0].freqHz = 220.0f;
        bands[0].q = 1.4f;
        bands[0].threshDb = -20.0f;
        bands[0].attackCoeff = 1.0f - std::exp(-1.0f / (sr * 0.005f));
        bands[0].releaseCoeff = 1.0f - std::exp(-1.0f / (sr * 0.080f));

        bands[1].freqHz = 4200.0f;
        bands[1].q = 1.8f;
        bands[1].threshDb = -22.0f;
        bands[1].attackCoeff = 1.0f - std::exp(-1.0f / (sr * 0.002f));
        bands[1].releaseCoeff = 1.0f - std::exp(-1.0f / (sr * 0.060f));

        for (int b = 0; b < 2; ++b) {
            float w0 = 2.0f * 3.14159265358979323846f * bands[b].freqHz / sr;
            bands[b].cosW = std::cos(w0);
            bands[b].alpha = std::sin(w0) / (2.0f * bands[b].q);
            bands[b].sc.setParams(FilterType::BandPass, sr, bands[b].freqHz, bands[b].q, 0.0f);
            bands[b].eq.setParams(FilterType::Peak, sr, bands[b].freqHz, bands[b].q, 0.0f);
        }
        reset();
    }

    void reset() {
        for (int b = 0; b < 2; ++b) {
            bands[b].sc.reset();
            bands[b].eq.reset();
            bands[b].envDb = -90.0f;
            bands[b].currentCutDb = 0.0f;
            bands[b].grDecay = 0.0f;
        }
    }

    void setChugAmount(float percent) {
        bands[0].amount = std::clamp(percent, 0.0f, 100.0f) / 100.0f;
    }

    void setHarshAmount(float percent) {
        bands[1].amount = std::clamp(percent, 0.0f, 100.0f) / 100.0f;
    }

    void process(float* left, float* right, int numSamples) {
        if (!left || numSamples <= 0) return;

        for (int b = 0; b < 2; ++b) {
            Band& band = bands[b];
            if (band.amount <= 0.001f) {
                band.grDecay *= 0.90f;
                if (band.grDecay < 1e-4f) band.grDecay = 0.0f;
                continue;
            }

            float maxCutThisBlock = 0.0f;
            // Threshold drops slightly as amount increases for musical sensitivity
            float effThresh = band.threshDb - band.amount * 8.0f;

            for (int i = 0; i < numSamples; ++i) {
                float inL = left[i];
                float inR = right ? right[i] : inL;

                float scL = inL, scR = inR;
                band.sc.processSample(scL, scR);
                float scPeak = std::max(std::abs(scL), std::abs(scR));
                float scDb = fastLinearToDb(scPeak);

                float coeff = (scDb > band.envDb) ? band.attackCoeff : band.releaseCoeff;
                band.envDb += (scDb - band.envDb) * coeff;

                // Update dynamic Peak filter every 64 samples without trig/pow
                if ((i & 63) == 0) {
                    float over = std::max(0.0f, band.envDb - effThresh);
                    float cutDb = -std::min(6.0f, over * 0.45f * band.amount);
                    if (std::abs(cutDb - band.currentCutDb) > 0.04f) {
                        band.currentCutDb = cutDb;
                        band.eq.setPeakFast(band.cosW, band.alpha, cutDb);
                    }
                    if (-cutDb > maxCutThisBlock) maxCutThisBlock = -cutDb;
                }

                band.eq.processSample(inL, inR);
                left[i] = inL;
                if (right) right[i] = inR;
            }

            if (maxCutThisBlock > band.grDecay) {
                band.grDecay = maxCutThisBlock;
            } else {
                band.grDecay *= 0.92f;
                if (band.grDecay < 1e-4f) band.grDecay = 0.0f;
            }
            if (band.envDb < -100.0f) band.envDb = -100.0f;
        }
    }

    float getChugGR() const { return bands[0].grDecay; }
    float getHarshGR() const { return bands[1].grDecay; }

    float getCurrentCutDb(int bandIndex) const {
        if (bandIndex < 0 || bandIndex >= 2) return 0.0f;
        return bands[bandIndex].currentCutDb;
    }

private:
    struct Band {
        float freqHz = 220.0f;
        float q = 1.4f;
        float threshDb = -20.0f;
        float amount = 0.2f;
        float attackCoeff = 0.01f;
        float releaseCoeff = 0.001f;
        float envDb = -90.0f;
        float currentCutDb = 0.0f;
        float grDecay = 0.0f;
        float cosW = 1.0f;
        float alpha = 0.01f;
        StereoBiquad sc;
        StereoBiquad eq;
    };

    float sr = 44100.0f;
    Band bands[2];
};

} // namespace Asster
