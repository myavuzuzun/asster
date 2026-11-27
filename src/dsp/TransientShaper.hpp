#pragma once
#include <cmath>
#include <algorithm>
#include "FastMath.hpp"

namespace Asster {

// Dual-Envelope Transient Shaper (Attack Punch & Sustain Control)
class TransientShaper {
public:
    TransientShaper() {
        init(44100.0f);
    }

    void init(float sampleRate) {
        sr = (sampleRate > 8000.0f) ? sampleRate : 44100.0f;
        fastAtt = 1.0f - std::exp(-1.0f / (sr * 0.001f)); // 1 ms
        fastRel = 1.0f - std::exp(-1.0f / (sr * 0.015f)); // 15 ms
        slowAtt = 1.0f - std::exp(-1.0f / (sr * 0.025f)); // 25 ms
        slowRel = 1.0f - std::exp(-1.0f / (sr * 0.120f)); // 120 ms
        gainSlew = 1.0f - std::exp(-1.0f / (sr * 0.003f));
        reset();
    }

    void reset() {
        fastEnv = 0.0f;
        slowEnv = 0.0f;
        currentGain = 1.0f;
    }

    void setPunch(float pct) {
        punch = std::clamp(pct, -100.0f, 100.0f) / 100.0f;
    }

    void setSustain(float pct) {
        sustain = std::clamp(pct, -100.0f, 100.0f) / 100.0f;
    }

    void process(float* left, float* right, int numSamples) {
        if (!left || numSamples <= 0) return;
        if (std::abs(punch) < 0.001f && std::abs(sustain) < 0.001f) return;

        for (int i = 0; i < numSamples; ++i) {
            float inL = left[i];
            float inR = right ? right[i] : inL;
            float peak = std::max(std::abs(inL), std::abs(inR));

            fastEnv += (peak - fastEnv) * ((peak > fastEnv) ? fastAtt : fastRel);
            slowEnv += (peak - slowEnv) * ((peak > slowEnv) ? slowAtt : slowRel);

            float denom = std::max(slowEnv, 1e-4f);
            float transRatio = std::max(0.0f, (fastEnv - slowEnv) / denom);
            float sustainRatio = std::max(0.0f, (slowEnv - fastEnv) / denom);

            float gainDb = punch * std::min(1.0f, transRatio * 1.5f) * 9.0f
                         + sustain * std::min(1.0f, sustainRatio * 1.5f) * 6.0f;

            float targetGain = std::clamp(fastDbToLinear(gainDb), 0.25f, 3.0f);
            currentGain += (targetGain - currentGain) * gainSlew;

            left[i] = inL * currentGain;
            if (right) right[i] = inR * currentGain;
        }

        if (fastEnv < 1e-12f) fastEnv = 0.0f;
        if (slowEnv < 1e-12f) slowEnv = 0.0f;
    }

private:
    float sr = 44100.0f;
    float punch = 0.0f;
    float sustain = 0.0f;
    float fastAtt = 0.02f, fastRel = 0.0015f;
    float slowAtt = 0.001f, slowRel = 0.0002f;
    float gainSlew = 0.01f;
    float fastEnv = 0.0f;
    float slowEnv = 0.0f;
    float currentGain = 1.0f;
};

} // namespace Asster
