#pragma once
#include <cmath>
#include <algorithm>
#include <atomic>

namespace Asster {

class GlueCompressor {
public:
    GlueCompressor() {
        init(44100.0f);
    }

    void init(float sampleRate) {
        sr = (sampleRate > 8000.0f) ? sampleRate : 44100.0f;
        updateBallistics();
        reset();
    }

    void reset() {
        envelopeDb = -90.0f;
        gainReductionDb = 0.0f;
        grDecay = 0.0f;
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

            // Sidechain detector: stereo link via max peak
            float sidechain = std::max(std::abs(inL), std::abs(inR));
            float sidechainDb = (sidechain > 1e-6f) ? 20.0f * std::log10(sidechain) : -120.0f;

            // Attack/Release envelope follower in dB domain
            float coeff = (sidechainDb > envelopeDb) ? attackCoeff : releaseCoeff;
            envelopeDb += (sidechainDb - envelopeDb) * coeff;

            // Soft-knee gain calculation
            float overDb = envelopeDb - thresholdDb;
            float grDb = 0.0f;

            if (overDb > halfKnee) {
                // Above knee
                grDb = overDb * invRatioFactor;
            } else if (overDb > -halfKnee) {
                // In knee
                float k = overDb + halfKnee;
                grDb = (k * k * invTwoKnee) * invRatioFactor;
            }

            float gainLinear = makeupGain;
            if (grDb > 0.0001f) {
                if (grDb > blockMaxGR) blockMaxGR = grDb;
                // Fast exp equivalent to 10^(-grDb/20) = exp(-grDb * 0.11512925f)
                gainLinear = std::exp(-grDb * 0.1151292546497022842f) * makeupGain;
            }

            left[i] *= gainLinear;
            if (right) right[i] *= gainLinear;
        }

        gainReductionDb = blockMaxGR;
        // Peak hold / smooth release for meter
        if (blockMaxGR > grDecay) {
            grDecay = blockMaxGR;
        } else {
            grDecay *= 0.95f;
            if (grDecay < 1e-5f) grDecay = 0.0f;
        }
        if (envelopeDb < -100.0f) envelopeDb = -100.0f;
    }

    float getGainReductionDb() const {
        return grDecay;
    }

private:
    void updateBallistics() {
        attackCoeff = 1.0f - std::exp(-1.0f / (sr * (attackTimeMs * 0.001f)));
        releaseCoeff = 1.0f - std::exp(-1.0f / (sr * (releaseTimeMs * 0.001f)));
    }

    float sr = 44100.0f;
    float thresholdDb = -12.0f;
    float ratio = 1.8f;
    float attackTimeMs = 25.0f;  // 25ms lets kick/snare attack breathe
    float releaseTimeMs = 120.0f;
    float kneeDb = 5.0f;         // 5 dB soft knee
    bool autoRelease = true;
    float makeupGain = 1.0f;

    float attackCoeff = 0.01f;
    float releaseCoeff = 0.001f;
    float envelopeDb = -90.0f;
    float gainReductionDb = 0.0f;
    float grDecay = 0.0f;
};

} // namespace Asster
