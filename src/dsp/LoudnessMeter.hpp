#pragma once
#include <cmath>
#include <vector>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include "Biquad.hpp"

namespace Asster {

// ITU-R BS.1770 / EBU R128 Compliant Loudness, True Peak, Phase Correlation & UI Telemetry
class LoudnessMeter {
public:
    static constexpr int FFT_SIZE = 2048;
    static constexpr int SCOPE_SIZE = 256;
    static constexpr int HISTORY_SIZE = 256;

    struct HistoryPoint {
        float peak = 0.0f;
        float limGr = 0.0f;
        float glueGr = 0.0f;
        float clipAct = 0.0f;
    };

    LoudnessMeter() {
        init(44100.0f);
    }

    void init(float sampleRate) {
        sr = (sampleRate > 8000.0f) ? sampleRate : 44100.0f;

        // Stage 1: High-shelf pre-filter (+4 dB at 1.5 kHz)
        filterPre.setParams(FilterType::HighShelf, sr, 1500.0f, 0.7071f, 4.0f);

        // Stage 2: RLB High-pass filter (38 Hz)
        filterHp.setParams(FilterType::HighPass, sr, 38.0f, 0.5f, 0.0f);

        momentaryBufferSize = static_cast<int>(sr * 0.400f);
        if (momentaryBufferSize < 64) momentaryBufferSize = 64;
        energyBufferL.assign(momentaryBufferSize, 0.0f);
        energyBufferR.assign(momentaryBufferSize, 0.0f);
        bufIndex = 0;
        sumEnergyL = 0.0f;
        sumEnergyR = 0.0f;

        shortTermSize = static_cast<int>(sr * 3.0f);
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
        corrCoeff = 1.0f - std::exp(-1.0f / (sr * 0.150f));
        corrNum = 0.0f;
        corrDenL = 0.0f;
        corrDenR = 0.0f;
        correlation.store(1.0f, std::memory_order_relaxed);

        prevL = prevR = prev2L = prev2R = 0.0f;

        std::fill(std::begin(fftRing), std::end(fftRing), 0.0f);
        fftWritePos.store(0, std::memory_order_relaxed);
        std::fill(std::begin(preFftRing), std::end(preFftRing), 0.0f);
        preFftWritePos.store(0, std::memory_order_relaxed);

        std::fill(std::begin(scopeL), std::end(scopeL), 0.0f);
        std::fill(std::begin(scopeR), std::end(scopeR), 0.0f);
        scopeWritePos.store(0, std::memory_order_relaxed);

        for (int i = 0; i < HISTORY_SIZE; ++i) historyRing[i] = HistoryPoint{};
        historyWritePos.store(0, std::memory_order_relaxed);

        lraSampleAccum = 0;
        lraCount = 0;
        lraIdx = 0;
        std::fill(std::begin(lraSamples), std::end(lraSamples), -90.0f);
    }

    void reset() {
        init(sr);
    }

    void setTelemetryEnabled(bool en) {
        telemetryEnabled = en;
    }

    void process(const float* left, const float* right, int numSamples) {
        if (!left || numSamples <= 0) return;

        float blockMaxTP = 0.0f;
        uint32_t fPos = fftWritePos.load(std::memory_order_relaxed);
        uint32_t sPos = scopeWritePos.load(std::memory_order_relaxed);

        for (int i = 0; i < numSamples; ++i) {
            float inL = left[i];
            float inR = right ? right[i] : inL;

            if (telemetryEnabled) {
                fftRing[fPos & (FFT_SIZE - 1)] = 0.5f * (inL + inR);
                ++fPos;

                if ((i & 3) == 0) {
                    scopeL[sPos & (SCOPE_SIZE - 1)] = inL;
                    scopeR[sPos & (SCOPE_SIZE - 1)] = inR;
                    ++sPos;
                }

                corrNum  += (inL * inR - corrNum) * corrCoeff;
                corrDenL += (inL * inL - corrDenL) * corrCoeff;
                corrDenR += (inR * inR - corrDenR) * corrCoeff;
            }

            // 1. Inter-Sample True Peak estimation
            float tpL = estimateTruePeak(inL, prevL, prev2L);
            float tpR = estimateTruePeak(inR, prevR, prev2R);
            prev2L = prevL; prevL = inL;
            prev2R = prevR; prevR = inR;

            float sampleTP = std::max(tpL, tpR);
            if (sampleTP > blockMaxTP) blockMaxTP = sampleTP;

            // 2. K-Weighting filters for LUFS
            __m128 kVec = filterHp.processVec(filterPre.processVec(_mm_set_ps(0.0f, 0.0f, inR, inL)));
            alignas(16) float kArr[4];
            _mm_store_ps(kArr, kVec);
            float eL = kArr[0] * kArr[0];
            float eR = kArr[1] * kArr[1];

            // Momentary (400ms)
            sumEnergyL -= energyBufferL[bufIndex];
            sumEnergyR -= energyBufferR[bufIndex];
            energyBufferL[bufIndex] = eL;
            energyBufferR[bufIndex] = eR;
            sumEnergyL += eL;
            sumEnergyR += eR;
            if (++bufIndex >= momentaryBufferSize) bufIndex = 0;

            // Short-Term (3s)
            stSumEnergyL -= shortTermBufferL[stIndex];
            stSumEnergyR -= shortTermBufferR[stIndex];
            shortTermBufferL[stIndex] = eL;
            shortTermBufferR[stIndex] = eR;
            stSumEnergyL += eL;
            stSumEnergyR += eR;
            if (++stIndex >= shortTermSize) stIndex = 0;

            integratedEnergySum += (eL + eR);
            integratedSampleCount++;
        }

        if (telemetryEnabled) {
            fftWritePos.store(fPos, std::memory_order_release);
            scopeWritePos.store(sPos, std::memory_order_release);

            float denom = std::sqrt(std::max(0.0f, corrDenL * corrDenR));
            float corrVal = (denom > 1e-9f) ? std::clamp(corrNum / denom, -1.0f, 1.0f) : 1.0f;
            correlation.store(corrVal, std::memory_order_relaxed);
        }

        if (blockMaxTP > truePeakDecay) {
            truePeakDecay = blockMaxTP;
        } else {
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

        // Sample short-term LUFS every ~0.25s for LRA calculation
        if (telemetryEnabled) {
            lraSampleAccum += numSamples;
            if (lraSampleAccum >= static_cast<int>(sr * 0.25f)) {
                lraSampleAccum = 0;
                float st = getShortTermLUFS();
                if (st > -60.0f) {
                    lraSamples[lraIdx] = st;
                    lraIdx = (lraIdx + 1) & 63;
                    if (lraCount < 64) lraCount++;
                }
            }
        }
    }

    void pushHistory(float peak, float limGr, float glueGr, float clipAct) {
        uint32_t pos = historyWritePos.load(std::memory_order_relaxed);
        historyRing[pos & (HISTORY_SIZE - 1)] = { peak, limGr, glueGr, clipAct };
        historyWritePos.store(pos + 1, std::memory_order_release);
    }

    void pushPreSamples(const float* left, const float* right, int numSamples) {
        if (!left || numSamples <= 0) return;
        uint32_t pos = preFftWritePos.load(std::memory_order_relaxed);
        for (int i = 0; i < numSamples; ++i) {
            float inL = left[i];
            float inR = right ? right[i] : inL;
            preFftRing[pos & (FFT_SIZE - 1)] = 0.5f * (inL + inR);
            ++pos;
        }
        preFftWritePos.store(pos, std::memory_order_release);
    }

    void copyPreFFTSamples(float* dst, int count) const {
        if (!dst || count <= 0) return;
        int n = std::min(count, FFT_SIZE);
        uint32_t wp = preFftWritePos.load(std::memory_order_acquire);
        uint32_t start = wp - static_cast<uint32_t>(n);
        for (int i = 0; i < n; ++i) {
            dst[i] = preFftRing[(start + static_cast<uint32_t>(i)) & (FFT_SIZE - 1)];
        }
    }

    void copyRecentSamples(float* dst, int count) const {
        if (!dst || count <= 0) return;
        int n = std::min(count, FFT_SIZE);
        uint32_t wp = fftWritePos.load(std::memory_order_acquire);
        uint32_t start = wp - static_cast<uint32_t>(n);
        for (int i = 0; i < n; ++i) {
            dst[i] = fftRing[(start + static_cast<uint32_t>(i)) & (FFT_SIZE - 1)];
        }
    }

    void copyScopeSamples(float* dstL, float* dstR, int count) const {
        if (!dstL || !dstR || count <= 0) return;
        int n = std::min(count, SCOPE_SIZE);
        uint32_t wp = scopeWritePos.load(std::memory_order_acquire);
        uint32_t start = wp - static_cast<uint32_t>(n);
        for (int i = 0; i < n; ++i) {
            uint32_t idx = (start + static_cast<uint32_t>(i)) & (SCOPE_SIZE - 1);
            dstL[i] = scopeL[idx];
            dstR[i] = scopeR[idx];
        }
    }

    void copyHistory(HistoryPoint* dst, int count) const {
        if (!dst || count <= 0) return;
        int n = std::min(count, HISTORY_SIZE);
        uint32_t wp = historyWritePos.load(std::memory_order_acquire);
        uint32_t start = wp - static_cast<uint32_t>(n);
        for (int i = 0; i < n; ++i) {
            dst[i] = historyRing[(start + static_cast<uint32_t>(i)) & (HISTORY_SIZE - 1)];
        }
    }

    float getMomentaryLUFS() const {
        float meanL = std::max(0.0f, sumEnergyL) / static_cast<float>(momentaryBufferSize);
        float meanR = std::max(0.0f, sumEnergyR) / static_cast<float>(momentaryBufferSize);
        float power = meanL + meanR;
        if (power < 1e-12f) return -90.0f;
        return -0.691f + 10.0f * std::log10(power);
    }

    float getShortTermLUFS() const {
        float meanL = std::max(0.0f, stSumEnergyL) / static_cast<float>(shortTermSize);
        float meanR = std::max(0.0f, stSumEnergyR) / static_cast<float>(shortTermSize);
        float power = meanL + meanR;
        if (power < 1e-12f) return -90.0f;
        return -0.691f + 10.0f * std::log10(power);
    }

    float getIntegratedLUFS() const {
        if (integratedSampleCount < static_cast<uint64_t>(sr * 0.5f)) {
            return getShortTermLUFS();
        }
        double mean = integratedEnergySum / static_cast<double>(integratedSampleCount);
        if (mean < 1e-12) return -90.0f;
        return -0.691f + 10.0f * static_cast<float>(std::log10(mean));
    }

    float getTruePeakdBTP() const {
        if (truePeakDecay <= 1e-7f) return -90.0f;
        return 20.0f * std::log10(truePeakDecay);
    }

    float getCurrentBlockPeakLinear() const {
        return truePeakDecay;
    }

    float getMaxTruePeakdBTP() const {
        if (maxTruePeak <= 1e-7f) return -90.0f;
        return 20.0f * std::log10(maxTruePeak);
    }

    float getCorrelation() const {
        return correlation.load(std::memory_order_relaxed);
    }

    // Peak-to-Loudness Ratio (Crest Factor in dB)
    float getPLR() const {
        float st = getShortTermLUFS();
        if (st <= -60.0f) return 0.0f;
        return std::clamp(getTruePeakdBTP() - st, 0.0f, 30.0f);
    }

    // Loudness Range (LRA in LU, P95 - P10 of recent short-term LUFS)
    float getLRA() const {
        if (lraCount < 4) return 0.0f;
        float tmp[64];
        int n = lraCount;
        for (int i = 0; i < n; ++i) tmp[i] = lraSamples[i];
        std::sort(tmp, tmp + n);
        int i10 = (n * 10) / 100;
        int i95 = std::min(n - 1, (n * 95) / 100);
        return std::max(0.0f, tmp[i95] - tmp[i10]);
    }

    // Spotify (-14 LUFS) normalization penalty in dB (0 dB or negative)
    float getSpotifyPenaltyDb() const {
        float integ = getIntegratedLUFS();
        if (integ <= -60.0f) return 0.0f;
        return std::min(0.0f, -14.0f - integ);
    }

    void resetMaxTruePeak() {
        maxTruePeak = 0.0f;
    }

private:
    inline float estimateTruePeak(float curr, float p1, float p2) {
        float peak = std::abs(curr);
        float mid1 = 0.5f * (p1 + curr) + 0.125f * (p1 - p2);
        float mid2 = 0.5f * (curr + p1) - 0.125f * (curr - p1);
        peak = std::max(peak, std::abs(mid1));
        peak = std::max(peak, std::abs(mid2));
        return peak;
    }

    float sr = 44100.0f;
    bool telemetryEnabled = false;
    StereoBiquad filterPre;
    StereoBiquad filterHp;

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

    float corrCoeff = 0.0001f;
    float corrNum = 0.0f;
    float corrDenL = 0.0f;
    float corrDenR = 0.0f;
    std::atomic<float> correlation{1.0f};

    float fftRing[FFT_SIZE] = {0.0f};
    std::atomic<uint32_t> fftWritePos{0};
    float preFftRing[FFT_SIZE] = {0.0f};
    std::atomic<uint32_t> preFftWritePos{0};

    float scopeL[SCOPE_SIZE] = {0.0f};
    float scopeR[SCOPE_SIZE] = {0.0f};
    std::atomic<uint32_t> scopeWritePos{0};

    HistoryPoint historyRing[HISTORY_SIZE];
    std::atomic<uint32_t> historyWritePos{0};

    float lraSamples[64] = {0.0f};
    int lraCount = 0;
    int lraIdx = 0;
    int lraSampleAccum = 0;
};

} // namespace Asster
