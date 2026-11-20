#pragma once
#include <cmath>
#include <algorithm>
#include <atomic>
#include "LoudnessMeter.hpp"

namespace Asster {

enum AutoTrimMode {
    AUTOTRIM_OFF = 0,
    AUTOTRIM_LEARN = 1,
    AUTOTRIM_CONTINUOUS = 2
};

class AutoTrim {
public:
    static constexpr float INTERNAL_TARGET_LUFS = -18.0f; // Professional nominal mastering sweet spot

    AutoTrim() {
        reset();
    }

    void init(float sampleRate) {
        sr = (sampleRate > 8000.0f) ? sampleRate : 44100.0f;
        inputMeter.init(sr);
        slewCoeff = 1.0f - std::exp(-1.0f / (sr * 0.040f)); // 40ms smoothing time constant
        reset();
    }

    void reset() {
        currentGainLinear = 1.0f;
        targetGainLinear = 1.0f;
        currentGainDb = 0.0f;
        calibratedGainDb = 0.0f;
        learnSampleCount = 0;
        isLearning = false;
        learningComplete = false;
        inputMeter.reset();
    }

    void setMode(AutoTrimMode mode) {
        this->mode = mode;
        if (mode == AUTOTRIM_OFF) {
            targetGainLinear = std::pow(10.0f, (calibratedGainDb + manualTrimDb) / 20.0f);
        }
    }

    void startLearning() {
        isLearning = true;
        learningComplete = false;
        learnSampleCount = 0;
        inputMeter.reset();
    }

    bool isLearningActive() const { return isLearning; }
    bool isLearnComplete() const { return learningComplete; }

    float getCalibratedGainDb() const { return calibratedGainDb; }
    void setManualTrimDb(float db) {
        manualTrimDb = db;
        targetGainLinear = std::pow(10.0f, (calibratedGainDb + manualTrimDb) / 20.0f);
    }

    // Force an immediate target calibration based on known LUFS
    void calibrateToLUFS(float incomingLUFS) {
        if (incomingLUFS > -70.0f) {
            calibratedGainDb = std::clamp(INTERNAL_TARGET_LUFS - incomingLUFS, -30.0f, 30.0f);
        } else {
            calibratedGainDb = 0.0f;
        }
        targetGainLinear = std::pow(10.0f, (calibratedGainDb + manualTrimDb) / 20.0f);
        learningComplete = true;
        isLearning = false;
    }

    // Process a block of audio, applying smooth gain calibration
    void process(float* left, float* right, int numSamples) {
        if (!left || numSamples <= 0) return;

        // Measure incoming level BEFORE applying trim
        inputMeter.process(left, right, numSamples);

        if (isLearning) {
            learnSampleCount += numSamples;
            // Learn over ~1.5 seconds of audio
            int targetLearnSamples = static_cast<int>(sr * 1.5f);
            if (learnSampleCount >= targetLearnSamples) {
                float measuredLUFS = inputMeter.getShortTermLUFS();
                calibrateToLUFS(measuredLUFS);
            }
        } else if (mode == AUTOTRIM_CONTINUOUS) {
            // Musical adaptive leveling: time constant ~2.0s
            float measuredLUFS = inputMeter.getShortTermLUFS();
            if (measuredLUFS > -60.0f) {
                float idealDb = std::clamp(INTERNAL_TARGET_LUFS - measuredLUFS, -30.0f, 30.0f);
                float dt = static_cast<float>(numSamples) / sr;
                float alpha = 1.0f - std::exp(-dt / 2.0f);
                calibratedGainDb += (idealDb - calibratedGainDb) * alpha;
                targetGainLinear = std::pow(10.0f, (calibratedGainDb + manualTrimDb) / 20.0f);
            }
        } else {
            targetGainLinear = std::pow(10.0f, (calibratedGainDb + manualTrimDb) / 20.0f);
        }

        // Apply smooth sample-by-sample gain interpolation to prevent clicks
        for (int i = 0; i < numSamples; ++i) {
            currentGainLinear += (targetGainLinear - currentGainLinear) * slewCoeff;
            left[i] *= currentGainLinear;
            if (right) right[i] *= currentGainLinear;
        }

        if (std::abs(targetGainLinear - currentGainLinear) < 1e-6f) {
            currentGainLinear = targetGainLinear;
        }

        currentGainDb = 20.0f * std::log10(std::max(1e-5f, currentGainLinear));
    }

    float getCurrentGainDb() const { return currentGainDb; }
    float getIncomingLUFS() const { return inputMeter.getShortTermLUFS(); }

private:
    float sr = 44100.0f;
    float slewCoeff = 0.0005f;
    AutoTrimMode mode = AUTOTRIM_LEARN;
    LoudnessMeter inputMeter;

    float currentGainLinear = 1.0f;
    float targetGainLinear = 1.0f;
    float currentGainDb = 0.0f;
    float calibratedGainDb = 0.0f;
    float manualTrimDb = 0.0f;

    bool isLearning = false;
    bool learningComplete = false;
    int learnSampleCount = 0;
};

} // namespace Asster
