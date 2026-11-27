#pragma once
#include <cmath>
#include <algorithm>
#include "Biquad.hpp"

namespace Asster {

// 3-Band Mastering Multiband Compressor with 4th-order Linkwitz-Riley (LR4) crossovers (160 Hz, 2.5 kHz)
class MultibandComp {
public:
    MultibandComp() {
        init(44100.0f);
    }

    void init(float sampleRate) {
        sr = (sampleRate > 8000.0f) ? sampleRate : 44100.0f;
        constexpr float kButterQ = 0.70710678f;

        for (int s = 0; s < 2; ++s) {
            xoverLow[s].setParams(FilterType::LowPass,    sr, 160.0f,  kButterQ, 0.0f);
            xoverMidHp[s].setParams(FilterType::HighPass, sr, 160.0f,  kButterQ, 0.0f);
            xoverMidLp[s].setParams(FilterType::LowPass,  sr, 2500.0f, kButterQ, 0.0f);
            xoverHigh[s].setParams(FilterType::HighPass,  sr, 2500.0f, kButterQ, 0.0f);
        }

        // Tailored ballistics per frequency band
        bands[0].init(sr, 25.0f, 140.0f); // Low: tight sub control without distortion
        bands[1].init(sr, 15.0f, 100.0f); // Mid: guitar/vocal density
        bands[2].init(sr,  8.0f,  70.0f); // High: smooth cymbal/air control
        reset();
    }

    void reset() {
        for (int s = 0; s < 2; ++s) {
            xoverLow[s].reset();
            xoverMidHp[s].reset();
            xoverMidLp[s].reset();
            xoverHigh[s].reset();
        }
        for (int b = 0; b < 3; ++b) bands[b].reset();
    }

    void setLowAmount(float pct)  { bands[0].setAmount(pct); }
    void setMidAmount(float pct)  { bands[1].setAmount(pct); }
    void setHighAmount(float pct) { bands[2].setAmount(pct); }

    void process(float* left, float* right, int numSamples) {
        if (!left || numSamples <= 0) return;

        if (bands[0].amount <= 0.001f && bands[1].amount <= 0.001f && bands[2].amount <= 0.001f) {
            for (int b = 0; b < 3; ++b) {
                bands[b].grDecay *= 0.90f;
                if (bands[b].grDecay < 1e-4f) bands[b].grDecay = 0.0f;
            }
            return;
        }

        float maxGr[3] = { 0.0f, 0.0f, 0.0f };

        for (int i = 0; i < numSamples; ++i) {
            float inL = left[i];
            float inR = right ? right[i] : inL;
            __m128 inV = _mm_set_ps(0.0f, 0.0f, inR, inL);

            // LR4 Crossover 1 (160 Hz)
            __m128 lowV = xoverLow[1].processVec(xoverLow[0].processVec(inV));
            __m128 mhV  = xoverMidHp[1].processVec(xoverMidHp[0].processVec(inV));

            // LR4 Crossover 2 (2.5 kHz)
            __m128 midV  = xoverMidLp[1].processVec(xoverMidLp[0].processVec(mhV));
            __m128 highV = xoverHigh[1].processVec(xoverHigh[0].processVec(mhV));

            alignas(16) float lowArr[4], midArr[4], highArr[4];
            _mm_store_ps(lowArr, lowV);
            _mm_store_ps(midArr, midV);
            _mm_store_ps(highArr, highV);

            float lowL = lowArr[0], lowR = lowArr[1];
            float midL = midArr[0], midR = midArr[1];
            float highL = highArr[0], highR = highArr[1];

            // Compress each band independently
            maxGr[0] = std::max(maxGr[0], bands[0].processSample(lowL, lowR));
            maxGr[1] = std::max(maxGr[1], bands[1].processSample(midL, midR));
            maxGr[2] = std::max(maxGr[2], bands[2].processSample(highL, highR));

            left[i] = lowL + midL + highL;
            if (right) right[i] = lowR + midR + highR;
        }

        for (int b = 0; b < 3; ++b) {
            if (maxGr[b] > bands[b].grDecay) {
                bands[b].grDecay = maxGr[b];
            } else {
                bands[b].grDecay *= 0.94f;
                if (bands[b].grDecay < 1e-4f) bands[b].grDecay = 0.0f;
            }
        }
    }

    float getLowGR() const  { return bands[0].grDecay; }
    float getMidGR() const  { return bands[1].grDecay; }
    float getHighGR() const { return bands[2].grDecay; }

private:
    struct BandComp {
        float amount = 0.2f;
        float threshDb = -12.0f;
        float ratio = 1.9f;
        float attackCoeff = 0.01f;
        float releaseCoeff = 0.001f;
        float envDb = -90.0f;
        float grDecay = 0.0f;

        void init(float sr, float attMs, float relMs) {
            attackCoeff  = 1.0f - std::exp(-1.0f / (sr * (attMs * 0.001f)));
            releaseCoeff = 1.0f - std::exp(-1.0f / (sr * (relMs * 0.001f)));
        }

        void reset() {
            envDb = -90.0f;
            grDecay = 0.0f;
        }

        void setAmount(float pct) {
            amount = std::clamp(pct, 0.0f, 100.0f) / 100.0f;
            threshDb = -8.0f - amount * 20.0f; // -8 dB to -28 dB
            ratio = 1.5f + amount * 2.0f;      // 1.5:1 to 3.5:1
        }

        inline float processSample(float& l, float& r) {
            if (amount <= 0.001f) return 0.0f;

            float peak = std::max(std::abs(l), std::abs(r));
            float peakDb = fastLinearToDb(peak);
            float coeff = (peakDb > envDb) ? attackCoeff : releaseCoeff;
            envDb += (peakDb - envDb) * coeff;

            constexpr float knee = 4.0f;
            constexpr float halfKnee = knee * 0.5f;
            float over = envDb - threshDb;
            float invRatio = 1.0f - 1.0f / ratio;
            float grDb = 0.0f;

            if (over > halfKnee) {
                grDb = over * invRatio;
            } else if (over > -halfKnee) {
                float k = over + halfKnee;
                grDb = (k * k / (2.0f * knee)) * invRatio;
            }

            if (grDb > 0.0001f) {
                float gain = fastDbToLinear(-grDb);
                l *= gain;
                r *= gain;
            }
            if (envDb < -100.0f) envDb = -100.0f;
            return grDb;
        }
    };

    float sr = 44100.0f;
    StereoBiquad xoverLow[2];
    StereoBiquad xoverMidHp[2];
    StereoBiquad xoverMidLp[2];
    StereoBiquad xoverHigh[2];
    BandComp bands[3];
};

} // namespace Asster
