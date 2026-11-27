#pragma once
#include <cmath>
#include <vector>
#include <algorithm>
#include <cstdint>
#include "FastMath.hpp"

namespace Asster {

enum LimiterStyle {
    LIMITER_CLEAN = 0,
    LIMITER_PUNCH = 1,
    LIMITER_CRUSH = 2
};

// True-Peak Lookahead Brickwall Limiter:
//   - Dynamic lookahead ring (128 @ 1x, 256 @ 2x, 512 @ 4x) for ~2.9ms lookahead at any oversampling rate
//   - 4-tap Catmull-Rom 4x inter-sample peak detection
//   - Dual-stage program-dependent release with CLEAN / PUNCH / CRUSH algorithms
//   - Hard post-gain ceiling clamp guaranteeing 0 clipping samples
class TruePeakLimiter {
public:
    static constexpr int BASE_LOOKAHEAD = 128; // ~2.9ms at 44.1kHz

    TruePeakLimiter() {
        init(44100.0f, 1);
    }

    void init(float sampleRate, int osFactor = 1) {
        sr = (sampleRate > 8000.0f) ? sampleRate : 44100.0f;
        int f = (osFactor == 4) ? 4 : ((osFactor == 2) ? 2 : 1);
        this->osFactor = f;
        lookaheadSamples = BASE_LOOKAHEAD * f;
        lookaheadMask = lookaheadSamples - 1;
        updateStyleCoeffs();
        setCeilingDb(ceilingDb);
        setDriveDb(driveDb);
        reset();
    }

    void reset() {
        delayBufferL.assign(lookaheadSamples, 0.0f);
        delayBufferR.assign(lookaheadSamples, 0.0f);
        delayIndex = 0;
        sampleCounter = 0;
        dqHead = 0;
        dqTail = 0;
        currentGain = 1.0f;
        fastGainEnv = 1.0f;
        slowGainEnv = 1.0f;
        limiterGainReductionDb = 0.0f;
        grDecay = 0.0f;
        for (int i = 0; i < 4; ++i) {
            histL[i] = 0.0f;
            histR[i] = 0.0f;
        }
    }

    int getBaseLatencySamples() const {
        return BASE_LOOKAHEAD;
    }

    void setCeilingDb(float ceilingDb) {
        this->ceilingDb = std::clamp(ceilingDb, -3.0f, -0.1f);
        ceilingLinear = std::pow(10.0f, this->ceilingDb / 20.0f);
    }

    void setDriveDb(float driveDb) {
        this->driveDb = std::clamp(driveDb, 0.0f, 24.0f);
        driveLinear = std::pow(10.0f, this->driveDb / 20.0f);
    }

    void setRelease(float releaseMs) {
        this->releaseMs = std::clamp(releaseMs, 10.0f, 800.0f);
        updateStyleCoeffs();
    }

    void setStyle(LimiterStyle s) {
        style = s;
        updateStyleCoeffs();
    }

    void process(float* left, float* right, int numSamples, float* deltaOutL = nullptr, float* deltaOutR = nullptr) {
        if (!left || numSamples <= 0) return;

        float minCurrentGain = 1.0f;

        for (int i = 0; i < numSamples; ++i) {
            float inL = left[i] * driveLinear;
            float inR = right ? right[i] * driveLinear : inL;

            if (style == LIMITER_CRUSH) {
                inL = inL / (1.0f + 0.12f * std::abs(inL));
                inR = inR / (1.0f + 0.12f * std::abs(inR));
            }

            float maxPeak;
            if (osFactor == 4) {
                maxPeak = std::max(std::abs(inL), std::abs(inR));
            } else {
                histL[0] = histL[1]; histL[1] = histL[2]; histL[2] = histL[3]; histL[3] = inL;
                histR[0] = histR[1]; histR[1] = histR[2]; histR[2] = histR[3]; histR[3] = inR;

                if (osFactor == 2) {
                    maxPeak = std::max(estimateTruePeak2x(histL), estimateTruePeak2x(histR));
                } else {
                    maxPeak = std::max(estimateTruePeak4x(histL), estimateTruePeak4x(histR));
                }
            }

            float reqGain = (maxPeak > ceilingLinear) ? (ceilingLinear / maxPeak) : 1.0f;

            // O(1) amortized sliding-window minimum via fixed-size monotonic deque
            while (dqHead != dqTail && (deque[dqHead].idx + static_cast<uint64_t>(lookaheadSamples) <= sampleCounter)) {
                dqHead = (dqHead + 1) & DEQUE_MASK;
            }
            while (dqHead != dqTail) {
                int prevTail = (dqTail - 1) & DEQUE_MASK;
                if (deque[prevTail].gain >= reqGain) {
                    dqTail = prevTail;
                } else {
                    break;
                }
            }
            deque[dqTail] = { sampleCounter, reqGain };
            dqTail = (dqTail + 1) & DEQUE_MASK;
            ++sampleCounter;

            float windowMinGain = deque[dqHead].gain;

            if (windowMinGain < currentGain) {
                currentGain += (windowMinGain - currentGain) * attackCoeff;
                fastGainEnv = std::min(fastGainEnv, currentGain);
                slowGainEnv = std::min(slowGainEnv, currentGain);
            } else {
                fastGainEnv += (windowMinGain - fastGainEnv) * fastReleaseCoeff;
                slowGainEnv += (windowMinGain - slowGainEnv) * slowReleaseCoeff;
                currentGain = std::min(fastGainEnv, slowGainEnv);
            }

            float delayedL = delayBufferL[delayIndex];
            float delayedR = delayBufferR[delayIndex];

            delayBufferL[delayIndex] = inL;
            delayBufferR[delayIndex] = inR;
            delayIndex = (delayIndex + 1) & lookaheadMask;

            float delayedPeak = std::max(std::abs(delayedL), std::abs(delayedR));
            if (delayedPeak * currentGain > ceilingLinear && delayedPeak > 1e-9f) {
                currentGain = ceilingLinear / delayedPeak;
                fastGainEnv = std::min(fastGainEnv, currentGain);
                slowGainEnv = std::min(slowGainEnv, currentGain);
            }

            if (currentGain < minCurrentGain) {
                minCurrentGain = currentGain;
            }

            float outL = std::clamp(delayedL * currentGain, -ceilingLinear, ceilingLinear);
            float outR = std::clamp(delayedR * currentGain, -ceilingLinear, ceilingLinear);

            if (deltaOutL) deltaOutL[i] = delayedL - outL;
            if (deltaOutR) deltaOutR[i] = delayedR - outR;

            left[i] = outL;
            if (right) right[i] = outR;
        }

        float blockMaxGR = (minCurrentGain < 1.0f) ? -20.0f * fastLog10(minCurrentGain) : 0.0f;
        limiterGainReductionDb = blockMaxGR;
        if (blockMaxGR > grDecay) {
            grDecay = blockMaxGR;
        } else {
            grDecay *= 0.94f;
            if (grDecay < 1e-5f) grDecay = 0.0f;
        }

        if (std::abs(1.0f - currentGain) < 1e-6f) currentGain = 1.0f;
        for (int k = 0; k < 4; ++k) {
            if (std::abs(histL[k]) < 1e-15f) histL[k] = 0.0f;
            if (std::abs(histR[k]) < 1e-15f) histR[k] = 0.0f;
        }
    }

    float getGainReductionDb() const { return grDecay; }
    float getCeilingDb() const { return ceilingDb; }
    float getCeilingLinear() const { return ceilingLinear; }
    float getDriveLinear() const { return driveLinear; }

private:
    void updateStyleCoeffs() {
        float attScale = (style == LIMITER_CLEAN) ? 0.45f : (style == LIMITER_CRUSH ? 0.20f : 0.35f);
        float fastMs   = (style == LIMITER_CLEAN) ? 50.0f : (style == LIMITER_CRUSH ? 15.0f : 30.0f);
        float slowMs   = (style == LIMITER_CLEAN) ? std::max(releaseMs, 140.0f)
                       : (style == LIMITER_CRUSH ? std::max(25.0f, releaseMs * 0.6f) : releaseMs);

        attackCoeff = 1.0f - std::exp(-1.0f / (static_cast<float>(lookaheadSamples) * attScale));
        fastReleaseCoeff = 1.0f - std::exp(-1.0f / (sr * (fastMs * 0.001f)));
        slowReleaseCoeff = 1.0f - std::exp(-1.0f / (sr * (slowMs * 0.001f)));
    }

    static inline float catmullRom(float p0, float p1, float p2, float p3, float t) {
        float t2 = t * t;
        float t3 = t2 * t;
        return 0.5f * (
            (2.0f * p1) +
            (-p0 + p2) * t +
            (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
            (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
        );
    }

    static inline float estimateTruePeak2x(const float h[4]) {
        float mid = 0.5625f * (h[1] + h[2]) - 0.0625f * (h[0] + h[3]);
        return std::max(std::max(std::abs(h[1]), std::abs(h[2])), std::abs(mid));
    }

    static inline float estimateTruePeak4x(const float h[4]) {
        float peak = std::max(std::abs(h[1]), std::abs(h[2]));
        peak = std::max(peak, std::abs(catmullRom(h[0], h[1], h[2], h[3], 0.25f)));
        peak = std::max(peak, std::abs(catmullRom(h[0], h[1], h[2], h[3], 0.50f)));
        peak = std::max(peak, std::abs(catmullRom(h[0], h[1], h[2], h[3], 0.75f)));
        return peak;
    }

    float sr = 44100.0f;
    int osFactor = 1;
    int lookaheadSamples = BASE_LOOKAHEAD;
    int lookaheadMask = BASE_LOOKAHEAD - 1;
    LimiterStyle style = LIMITER_PUNCH;
    float ceilingDb = -0.5f;
    float ceilingLinear = 0.944f;
    float driveDb = 0.0f;
    float driveLinear = 1.0f;
    float releaseMs = 100.0f;
    float attackCoeff = 0.02f;
    float fastReleaseCoeff = 0.001f;
    float slowReleaseCoeff = 0.0002f;

    static constexpr int DEQUE_CAP = 1024;
    static constexpr int DEQUE_MASK = DEQUE_CAP - 1;
    struct DequeEntry {
        uint64_t idx = 0;
        float gain = 1.0f;
    };

    std::vector<float> delayBufferL;
    std::vector<float> delayBufferR;
    DequeEntry deque[DEQUE_CAP];
    int dqHead = 0;
    int dqTail = 0;
    uint64_t sampleCounter = 0;
    int delayIndex = 0;

    float currentGain = 1.0f;
    float fastGainEnv = 1.0f;
    float slowGainEnv = 1.0f;
    float limiterGainReductionDb = 0.0f;
    float grDecay = 0.0f;
    float histL[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float histR[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};

} // namespace Asster
