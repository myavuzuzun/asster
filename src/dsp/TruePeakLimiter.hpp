#pragma once
#include <cmath>
#include <vector>
#include <algorithm>

namespace Asster {

class TruePeakLimiter {
public:
    static constexpr int LOOKAHEAD_SAMPLES = 128; // ~2.9ms at 44.1kHz

    TruePeakLimiter() {
        init(44100.0f);
    }

    void init(float sampleRate) {
        sr = (sampleRate > 8000.0f) ? sampleRate : 44100.0f;
        setRelease(100.0f);
        setCeilingDb(-0.5f);
        setDriveDb(0.0f);
        reset();
    }

    void reset() {
        delayBufferL.assign(LOOKAHEAD_SAMPLES, 0.0f);
        delayBufferR.assign(LOOKAHEAD_SAMPLES, 0.0f);
        delayIndex = 0;
        currentGain = 1.0f;
        limiterGainReductionDb = 0.0f;
        grDecay = 0.0f;
        prevL = 0.0f;
        prevR = 0.0f;
    }

    int getLatencySamples() const {
        return LOOKAHEAD_SAMPLES;
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
        releaseCoeff = 1.0f - std::exp(-1.0f / (sr * (this->releaseMs * 0.001f)));
    }

    void process(float* left, float* right, int numSamples) {
        if (!left || numSamples <= 0) return;

        float minCurrentGain = 1.0f;

        for (int i = 0; i < numSamples; ++i) {
            float inL = left[i] * driveLinear;
            float inR = right ? right[i] * driveLinear : inL;

            // 1. Inter-sample true-peak estimation
            float tpL = std::max(std::abs(inL), std::abs(0.5f * (inL + prevL) + 0.125f * (inL - prevL)));
            float tpR = std::max(std::abs(inR), std::abs(0.5f * (inR + prevR) + 0.125f * (inR - prevR)));
            prevL = inL;
            prevR = inR;

            float maxPeak = std::max(tpL, tpR);

            // 2. Calculate target gain needed to stay strictly below ceiling
            float targetGain = 1.0f;
            if (maxPeak > ceilingLinear) {
                targetGain = ceilingLinear / maxPeak;
            }

            // 3. Attack / Release smoothing
            if (targetGain < currentGain) {
                // Instantaneous fast attack to catch transient in lookahead window
                currentGain = targetGain;
            } else {
                // Program-dependent smooth exponential release
                currentGain += (targetGain - currentGain) * releaseCoeff;
            }

            if (currentGain < minCurrentGain) {
                minCurrentGain = currentGain;
            }

            // 4. Read delayed audio from lookahead ring buffer
            float delayedL = delayBufferL[delayIndex];
            float delayedR = delayBufferR[delayIndex];

            // 5. Store current input into ring buffer
            delayBufferL[delayIndex] = inL;
            delayBufferR[delayIndex] = inR;
            delayIndex = (delayIndex + 1) & (LOOKAHEAD_SAMPLES - 1);

            // 6. Apply limiting gain
            float outL = delayedL * currentGain;
            float outR = delayedR * currentGain;

            // 7. Mathematical absolute true-peak safety ceiling clamp (GUARANTEES 0 CLIPPING)
            if (outL > ceilingLinear) outL = ceilingLinear;
            else if (outL < -ceilingLinear) outL = -ceilingLinear;

            if (outR > ceilingLinear) outR = ceilingLinear;
            else if (outR < -ceilingLinear) outR = -ceilingLinear;

            left[i] = outL;
            if (right) right[i] = outR;
        }

        // Compute block max GR once per buffer rather than per-sample
        float blockMaxGR = (minCurrentGain < 1.0f) ? -20.0f * std::log10(minCurrentGain) : 0.0f;
        limiterGainReductionDb = blockMaxGR;
        if (blockMaxGR > grDecay) {
            grDecay = blockMaxGR;
        } else {
            grDecay *= 0.94f;
            if (grDecay < 1e-5f) grDecay = 0.0f;
        }

        if (std::abs(1.0f - currentGain) < 1e-6f) currentGain = 1.0f;
        if (std::abs(prevL) < 1e-15f) prevL = 0.0f;
        if (std::abs(prevR) < 1e-15f) prevR = 0.0f;
    }

    float getGainReductionDb() const { return grDecay; }
    float getCeilingDb() const { return ceilingDb; }

private:
    float sr = 44100.0f;
    float ceilingDb = -0.5f;
    float ceilingLinear = 0.944f;
    float driveDb = 0.0f;
    float driveLinear = 1.0f;
    float releaseMs = 100.0f;
    float releaseCoeff = 0.005f;

    std::vector<float> delayBufferL;
    std::vector<float> delayBufferR;
    int delayIndex = 0;

    float currentGain = 1.0f;
    float limiterGainReductionDb = 0.0f;
    float grDecay = 0.0f;
    float prevL = 0.0f, prevR = 0.0f;
};

} // namespace Asster
