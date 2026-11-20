#pragma once
#include <cmath>
#include <algorithm>

namespace Asster {

class SoftClipper {
public:
    SoftClipper() {
        setCeilingDb(-0.5f);
        setDriveDb(0.0f);
    }

    void setCeilingDb(float ceilingDb) {
        thresholdLinear = std::pow(10.0f, ceilingDb / 20.0f);
        kneeWidth = thresholdLinear * 0.25f; // 25% smooth knee
    }

    void setDriveDb(float driveDb) {
        driveLinear = std::pow(10.0f, driveDb / 20.0f);
    }

    void process(float* left, float* right, int numSamples) {
        if (!left || numSamples <= 0) return;

        bool wasClipping = false;

        for (int i = 0; i < numSamples; ++i) {
            float inL = left[i] * driveLinear;
            float inR = right ? right[i] * driveLinear : inL;

            float clippedL = clipSample(inL, wasClipping);
            float clippedR = clipSample(inR, wasClipping);

            left[i] = clippedL;
            if (right) right[i] = clippedR;
        }

        if (wasClipping) {
            clipActivity = 1.0f;
        } else {
            clipActivity *= 0.90f; // Fast decay for meter/LED
            if (clipActivity < 1e-5f) clipActivity = 0.0f;
        }
    }

    float getClipActivity() const { return clipActivity; }

private:
    inline float clipSample(float x, bool& clippingFlag) {
        float absX = std::abs(x);
        float lowerKnee = thresholdLinear - kneeWidth;

        if (absX <= lowerKnee) {
            return x; // Completely clean & linear
        }

        clippingFlag = true;
        float sign = (x >= 0.0f) ? 1.0f : -1.0f;

        if (absX < thresholdLinear + kneeWidth) {
            // Cubic Hermite soft-knee transition
            float t = (absX - lowerKnee) / (2.0f * kneeWidth);
            float kneeVal = lowerKnee + (absX - lowerKnee) * (1.0f - t * t * (3.0f - 2.0f * t) * 0.4f);
            return sign * kneeVal;
        }

        // Asymptotic saturation above threshold: completely prevents hard digital wrapping
        float excess = absX - thresholdLinear;
        float saturated = thresholdLinear + kneeWidth * (excess / (kneeWidth + excess));
        return sign * saturated;
    }

    float thresholdLinear = 0.944f; // ~ -0.5 dBFS
    float kneeWidth = 0.2f;
    float driveLinear = 1.0f;
    float clipActivity = 0.0f;
};

} // namespace Asster
