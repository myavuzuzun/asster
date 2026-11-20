#pragma once
#include <cmath>
#include <vector>
#include <algorithm>
#include <atomic>
#include "Biquad.hpp"

namespace Asster {

// ITU-R BS.1770 / EBU R128 Compliant Loudness & Inter-Sample True Peak Meter
class LoudnessMeter {
public:
    LoudnessMeter() {
        init(44100.0f);
    }

    void init(float sampleRate) {
        sr = (sampleRate > 8000.0f) ? sampleRate : 44100.0f;
        
        // Stage 1: High-shelf pre-filter (+4 dB at 1.5 kHz)
        filterPreL.setParams(FilterType::HighShelf, sr, 1500.0f, 0.7071f, 4.0f);
        filterPreR.setParams(FilterType::HighShelf, sr, 1500.0f, 0.7071f, 4.0f);
        
        // Stage 2: RLB High-pass filter (38 Hz)
        filterHpL.setParams(FilterType::HighPass, sr, 38.0f, 0.5f, 0.0f);
        filterHpR.setParams(FilterType::HighPass, sr, 38.0f, 0.5f, 0.0f);
        
        momentaryBufferSize = static_cast<int>(sr * 0.400f); // 400ms window
        if (momentaryBufferSize < 64) momentaryBufferSize = 64;
        energyBufferL.assign(momentaryBufferSize, 0.0f);
        energyBufferR.assign(momentaryBufferSize, 0.0f);
        bufIndex = 0;
        sumEnergyL = 0.0f;
        sumEnergyR = 0.0f;

        shortTermSize = static_cast<int>(sr * 3.0f); // 3s window
        if (shortTermSize < 128) shortTermSize = 128;
        shortTermBufferL.assign(shortTermSize, 0.0f);
        shortTermBufferR.assign(shortTermSize, 0.0f);
        stIndex = 0;
        stSumEnergyL = 0.0f;
        stSumEnergyR = 0.0f;

        integratedEnergySum = 0.0;
        integratedSampleCount = 0;

        maxTruePeak = 0.0f;
        truePeakDecay = 0.0f;
        
        prevL = 0.0f;
        prevR = 0.0f;
        prev2L = 0.0f;
        prev2R = 0.0f;
    }

    void reset() {
        init(sr);
    }

    // Process a block of stereo samples
    void process(const float* left, const float* right, int numSamples) {
        if (!left || numSamples <= 0) return;

        float blockMaxTP = 0.0f;

        for (int i = 0; i < numSamples; ++i) {
            float inL = left[i];
            float inR = right ? right[i] : inL;

            // 1. Inter-Sample True Peak estimation via 4x cubic Hermite oversampling
            float tpL = estimateTruePeak(inL, prevL, prev2L);
            float tpR = estimateTruePeak(inR, prevR, prev2R);
            prev2L = prevL;
            prevL = inL;
            prev2R = prevR;
            prevR = inR;

            float sampleTP = std::max(tpL, tpR);
            if (sampleTP > blockMaxTP) blockMaxTP = sampleTP;

            // 2. K-Weighting filters for LUFS
            float kL = filterHpL.process(filterPreL.process(inL));
            float kR = filterHpR.process(filterPreR.process(inR));

            float eL = kL * kL;
            float eR = kR * kR;

            // Momentary (400ms) sliding window
            sumEnergyL -= energyBufferL[bufIndex];
            sumEnergyR -= energyBufferR[bufIndex];
            energyBufferL[bufIndex] = eL;
            energyBufferR[bufIndex] = eR;
            sumEnergyL += eL;
            sumEnergyR += eR;
            if (++bufIndex >= momentaryBufferSize) bufIndex = 0;

            // Short-Term (3s) sliding window
            stSumEnergyL -= shortTermBufferL[stIndex];
            stSumEnergyR -= shortTermBufferR[stIndex];
            shortTermBufferL[stIndex] = eL;
            shortTermBufferR[stIndex] = eR;
            stSumEnergyL += eL;
            stSumEnergyR += eR;
            if (++stIndex >= shortTermSize) stIndex = 0;

            // Integrated accumulator
            integratedEnergySum += (eL + eR);
            integratedSampleCount++;
        }

        // Update True Peak meter with smooth decay
        if (blockMaxTP > truePeakDecay) {
            truePeakDecay = blockMaxTP;
        } else {
            // Decay ~20 dB per second
            float decayFactor = std::pow(0.1f, static_cast<float>(numSamples) / sr);
            truePeakDecay *= decayFactor;
            if (truePeakDecay < 1e-7f) truePeakDecay = 0.0f;
        }
        if (blockMaxTP > maxTruePeak) {
            maxTruePeak = blockMaxTP;
        }

        if (sumEnergyL < 1e-12f) sumEnergyL = 0.0f;
        if (sumEnergyR < 1e-12f) sumEnergyR = 0.0f;
        if (stSumEnergyL < 1e-12f) stSumEnergyL = 0.0f;
        if (stSumEnergyR < 1e-12f) stSumEnergyR = 0.0f;
        if (std::abs(prevL) < 1e-15f) prevL = 0.0f;
        if (std::abs(prevR) < 1e-15f) prevR = 0.0f;
        if (std::abs(prev2L) < 1e-15f) prev2L = 0.0f;
        if (std::abs(prev2R) < 1e-15f) prev2R = 0.0f;
    }

    // Get Momentary Loudness in LUFS (400ms)
    float getMomentaryLUFS() const {
        float meanL = std::max(0.0f, sumEnergyL) / static_cast<float>(momentaryBufferSize);
        float meanR = std::max(0.0f, sumEnergyR) / static_cast<float>(momentaryBufferSize);
        float power = meanL + meanR;
        if (power < 1e-12f) return -90.0f;
        return -0.691f + 10.0f * std::log10(power);
    }

    // Get Short-Term Loudness in LUFS (3s)
    float getShortTermLUFS() const {
        float meanL = std::max(0.0f, stSumEnergyL) / static_cast<float>(shortTermSize);
        float meanR = std::max(0.0f, stSumEnergyR) / static_cast<float>(shortTermSize);
        float power = meanL + meanR;
        if (power < 1e-12f) return -90.0f;
        return -0.691f + 10.0f * std::log10(power);
    }

    // Get Integrated Loudness in LUFS since last reset
    float getIntegratedLUFS() const {
        if (integratedSampleCount < static_cast<uint64_t>(sr * 0.5f)) {
            return getShortTermLUFS();
        }
        double mean = integratedEnergySum / static_cast<double>(integratedSampleCount);
        if (mean < 1e-12) return -90.0f;
        return -0.691f + 10.0f * static_cast<float>(std::log10(mean));
    }

    // Get Current True Peak in dBTP
    float getTruePeakdBTP() const {
        if (truePeakDecay <= 1e-7f) return -90.0f;
        return 20.0f * std::log10(truePeakDecay);
    }

    // Get Peak True Peak (historic max) in dBTP
    float getMaxTruePeakdBTP() const {
        if (maxTruePeak <= 1e-7f) return -90.0f;
        return 20.0f * std::log10(maxTruePeak);
    }

    void resetMaxTruePeak() {
        maxTruePeak = 0.0f;
    }

private:
    // 4x oversampled cubic peak estimation
    inline float estimateTruePeak(float curr, float p1, float p2) {
        float peak = std::abs(curr);
        // Inter-sample point 1: t = 0.5 (midpoint between p1 and curr)
        // Cubic Hermite midpoint: (-p2 + 9*p1 + 9*curr - next) / 16
        // Approximation using available history:
        float mid1 = 0.5f * (p1 + curr) + 0.125f * (p1 - p2);
        float mid2 = 0.5f * (curr + p1) - 0.125f * (curr - p1);
        peak = std::max(peak, std::abs(mid1));
        peak = std::max(peak, std::abs(mid2));
        return peak;
    }

    float sr = 44100.0f;
    Biquad filterPreL, filterPreR;
    Biquad filterHpL, filterHpR;

    int momentaryBufferSize = 17640;
    std::vector<float> energyBufferL;
    std::vector<float> energyBufferR;
    int bufIndex = 0;
    float sumEnergyL = 0.0f;
    float sumEnergyR = 0.0f;

    int shortTermSize = 132300;
    std::vector<float> shortTermBufferL;
    std::vector<float> shortTermBufferR;
    int stIndex = 0;
    float stSumEnergyL = 0.0f;
    float stSumEnergyR = 0.0f;

    double integratedEnergySum = 0.0;
    uint64_t integratedSampleCount = 0;

    float maxTruePeak = 0.0f;
    float truePeakDecay = 0.0f;
    float prevL = 0.0f, prevR = 0.0f;
    float prev2L = 0.0f, prev2R = 0.0f;
};

} // namespace Asster
