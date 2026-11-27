#pragma once
#include <cmath>
#include <algorithm>
#include "Biquad.hpp"

namespace Asster {

class GlueCompressor {
public:
    GlueCompressor() {
        init(44100.0f);
    }

    void init(float sampleRate) {
        sr = (sampleRate > 8000.0f) ? sampleRate : 44100.0f;
        updateBallistics();
        updateSidechainFilter();
        reset();
    }

    void reset() {
        envelopeDb = -90.0f;
        fastEnvDb = -90.0f;
        slowEnvDb = -90.0f;
        gainReductionDb = 0.0f;
        grDecay = 0.0f;
        scHpL.reset();
        scHpR.reset();
    }

    void setThreshold(float threshDb) {
        thresholdDb = std::clamp(threshDb, -36.0f, 0.0f);
    }

    void setRatio(float r) {
        ratio = std::clamp(r, 1.2f, 8.0f);
    }

    void setAttack(float attackMs) {
        attackTimeMs = std::clamp(attackMs, 1.0f, 100.0f);
        updateBallistics();
    }

    void setRelease(float releaseMs) {
        releaseTimeMs = std::clamp(releaseMs, 20.0f, 1200.0f);
        updateBallistics();
    }

    void setAutoRelease(bool autoRel) {
        autoRelease = autoRel;
    }

    void setSidechainHpf(float freqHz) {
        scHpfHz = std::clamp(freqHz, 20.0f, 200.0f);
        updateSidechainFilter();
    }

    void setMix(float percent) {
        mix = std::clamp(percent, 0.0f, 100.0f) / 100.0f;
    }

    void setMakeup(float db) {
        makeupGain = std::pow(10.0f, db / 20.0f);
    }

    void process(float* left, float* right, int numSamples) {
        if (!left || numSamples <= 0) return;

        float blockMaxGR = 0.0f;
        const float invRatioFactor = 1.0f - 1.0f / ratio;
        const float halfKnee = kneeDb * 0.5f;
        const float invTwoKnee = 1.0f / (2.0f * kneeDb);

        for (int i = 0; i < numSamples; ++i) {
            float inL = left[i];
            float inR = right ? right[i] : inL;

            // Sidechain HPF so kick/sub-bass doesn't pump the whole master
            float scL = scHpL.process(inL);
            float scR = scHpR.process(inR);
            float sidechain = std::max(std::abs(scL), std::abs(scR));
            float sidechainDb = fastLinearToDb(sidechain);

            // Dual-stage program-dependent auto-release (SSL-style) or manual release
            if (autoRelease) {
                float cFast = (sidechainDb > fastEnvDb) ? attackCoeff : fastReleaseCoeff;
                float cSlow = (sidechainDb > slowEnvDb) ? attackCoeff : slowReleaseCoeff;
                fastEnvDb += (sidechainDb - fastEnvDb) * cFast;
                slowEnvDb += (sidechainDb - slowEnvDb) * cSlow;
                envelopeDb = std::max(fastEnvDb, slowEnvDb);
            } else {
                float coeff = (sidechainDb > envelopeDb) ? attackCoeff : releaseCoeff;
                envelopeDb += (sidechainDb - envelopeDb) * coeff;
            }

            // Soft-knee gain calculation
            float overDb = envelopeDb - thresholdDb;
            float grDb = 0.0f;

            if (overDb > halfKnee) {
                grDb = overDb * invRatioFactor;
            } else if (overDb > -halfKnee) {
                float k = overDb + halfKnee;
                grDb = (k * k * invTwoKnee) * invRatioFactor;
            }

            float gainLinear = makeupGain;
            if (grDb > 0.0001f) {
                if (grDb > blockMaxGR) blockMaxGR = grDb;
                gainLinear = fastDbToLinear(-grDb) * makeupGain;
            }

            // Parallel wet/dry blend
            float wetL = inL * gainLinear;
            left[i] = inL * (1.0f - mix) + wetL * mix;
            if (right) {
                float wetR = inR * gainLinear;
                right[i] = inR * (1.0f - mix) + wetR * mix;
            }
        }

        gainReductionDb = blockMaxGR;
        if (blockMaxGR > grDecay) {
            grDecay = blockMaxGR;
        } else {
            grDecay *= 0.95f;
            if (grDecay < 1e-5f) grDecay = 0.0f;
        }
        if (envelopeDb < -100.0f) envelopeDb = -100.0f;
        if (fastEnvDb < -100.0f) fastEnvDb = -100.0f;
        if (slowEnvDb < -100.0f) slowEnvDb = -100.0f;
    }

    float getGainReductionDb() const {
        return grDecay;
    }

private:
    void updateBallistics() {
        attackCoeff = 1.0f - std::exp(-1.0f / (sr * (attackTimeMs * 0.001f)));
        releaseCoeff = 1.0f - std::exp(-1.0f / (sr * (releaseTimeMs * 0.001f)));
        fastReleaseCoeff = 1.0f - std::exp(-1.0f / (sr * 0.050f)); // 50 ms fast transient recovery
        slowReleaseCoeff = 1.0f - std::exp(-1.0f / (sr * 0.400f)); // 400 ms slow sustain glue
    }

    void updateSidechainFilter() {
        scHpL.setParams(FilterType::HighPass, sr, scHpfHz, 0.7071f, 0.0f);
        scHpR.setParams(FilterType::HighPass, sr, scHpfHz, 0.7071f, 0.0f);
    }

    float sr = 44100.0f;
    float thresholdDb = -12.0f;
    float ratio = 1.8f;
    float attackTimeMs = 25.0f;
    float releaseTimeMs = 120.0f;
    float kneeDb = 5.0f;
    bool autoRelease = true;
    float scHpfHz = 100.0f;
    float mix = 1.0f;
    float makeupGain = 1.0f;

    float attackCoeff = 0.01f;
    float releaseCoeff = 0.001f;
    float fastReleaseCoeff = 0.0005f;
    float slowReleaseCoeff = 0.00006f;
    float envelopeDb = -90.0f;
    float fastEnvDb = -90.0f;
    float slowEnvDb = -90.0f;
    float gainReductionDb = 0.0f;
    float grDecay = 0.0f;

    Biquad scHpL, scHpR;
};

} // namespace Asster
