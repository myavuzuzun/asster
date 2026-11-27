#pragma once
#include <cmath>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include "Biquad.hpp"
#include "LoudnessMeter.hpp"
#include "TruePeakLimiter.hpp"

namespace Asster {

enum AutoMasterMode {
    AUTOMASTER_INVISIBLE = 0,
    AUTOMASTER_POLISH    = 1,
    AUTOMASTER_LOUD      = 2
};

struct ReferenceProfile {
    bool valid = false;
    float lowRel = -6.0f;
    float mudRel = -10.5f;
    float biteRel = -13.0f;
    float airRel = -18.0f;
    float stabTargets[5] = { -6.0f, -10.0f, -12.0f, -14.0f, -18.0f };
    float sideRatio = 0.35f;
    float targetLufs = -9.0f;
};

inline ReferenceProfile getGenreTargetProfile(int presetIndex) {
    ReferenceProfile p;
    p.valid = true;
    switch (presetIndex) {
        case 0: // Extreme Metal
            p.lowRel = -6.5f; p.mudRel = -12.0f; p.biteRel = -11.5f; p.airRel = -16.5f;
            p.stabTargets[0] = -6.5f; p.stabTargets[1] = -11.5f; p.stabTargets[2] = -11.5f; p.stabTargets[3] = -12.5f; p.stabTargets[4] = -16.5f;
            p.sideRatio = 0.38f; p.targetLufs = -8.0f;
            break;
        case 1: // Doom Metal
            p.lowRel = -4.5f; p.mudRel = -9.0f; p.biteRel = -14.5f; p.airRel = -20.0f;
            p.stabTargets[0] = -4.5f; p.stabTargets[1] = -9.0f; p.stabTargets[2] = -12.5f; p.stabTargets[3] = -15.5f; p.stabTargets[4] = -20.0f;
            p.sideRatio = 0.42f; p.targetLufs = -10.5f;
            break;
        case 2: // Sludge Metal
            p.lowRel = -5.5f; p.mudRel = -10.5f; p.biteRel = -12.5f; p.airRel = -18.0f;
            p.stabTargets[0] = -5.5f; p.stabTargets[1] = -10.5f; p.stabTargets[2] = -12.0f; p.stabTargets[3] = -13.5f; p.stabTargets[4] = -18.0f;
            p.sideRatio = 0.34f; p.targetLufs = -9.0f;
            break;
        case 3: // Groove Metal
            p.lowRel = -5.8f; p.mudRel = -12.0f; p.biteRel = -12.0f; p.airRel = -17.0f;
            p.stabTargets[0] = -5.8f; p.stabTargets[1] = -11.5f; p.stabTargets[2] = -12.0f; p.stabTargets[3] = -13.0f; p.stabTargets[4] = -17.0f;
            p.sideRatio = 0.40f; p.targetLufs = -8.5f;
            break;
        case 4: // Modern Metalcore / Djent
            p.lowRel = -6.2f; p.mudRel = -12.5f; p.biteRel = -11.0f; p.airRel = -15.5f;
            p.stabTargets[0] = -6.2f; p.stabTargets[1] = -12.0f; p.stabTargets[2] = -11.5f; p.stabTargets[3] = -12.0f; p.stabTargets[4] = -15.5f;
            p.sideRatio = 0.40f; p.targetLufs = -7.5f;
            break;
        case 5: // Classic Heavy Metal
            p.lowRel = -6.8f; p.mudRel = -10.0f; p.biteRel = -13.0f; p.airRel = -18.5f;
            p.stabTargets[0] = -6.8f; p.stabTargets[1] = -10.0f; p.stabTargets[2] = -12.0f; p.stabTargets[3] = -14.0f; p.stabTargets[4] = -18.5f;
            p.sideRatio = 0.32f; p.targetLufs = -11.0f;
            break;
        case 6: // Transparent Master
            p.lowRel = -6.5f; p.mudRel = -10.5f; p.biteRel = -13.0f; p.airRel = -18.0f;
            p.stabTargets[0] = -6.5f; p.stabTargets[1] = -10.5f; p.stabTargets[2] = -12.0f; p.stabTargets[3] = -14.0f; p.stabTargets[4] = -18.0f;
            p.sideRatio = 0.35f; p.targetLufs = -14.0f;
            break;
        case 7: // Modern Pop
            p.lowRel = -6.0f; p.mudRel = -11.5f; p.biteRel = -11.8f; p.airRel = -15.5f;
            p.stabTargets[0] = -6.0f; p.stabTargets[1] = -11.5f; p.stabTargets[2] = -11.5f; p.stabTargets[3] = -13.0f; p.stabTargets[4] = -15.5f;
            p.sideRatio = 0.38f; p.targetLufs = -9.5f;
            break;
        case 8: // Hip-Hop / Trap
        default:
            p.lowRel = -4.2f; p.mudRel = -11.0f; p.biteRel = -12.2f; p.airRel = -16.0f;
            p.stabTargets[0] = -4.2f; p.stabTargets[1] = -11.0f; p.stabTargets[2] = -12.0f; p.stabTargets[3] = -13.5f; p.stabTargets[4] = -16.0f;
            p.sideRatio = 0.36f; p.targetLufs = -8.5f;
            break;
    }
    return p;
}

struct AutoMasterResult {
    float eqBass = 0.0f;
    float eqMud = 0.0f;
    float eqBite = 0.0f;
    float eqAir = 0.0f;
    float dynChug = 20.0f;
    float dynHarsh = 20.0f;
    float mbLow = 20.0f;
    float mbMid = 15.0f;
    float mbHigh = 15.0f;
    float glueAmount = 1.5f;
    float stereoWidth = 115.0f;
    float monoFreq = 95.0f;
    float intensity = 75.0f;
    int limiterStyle = LIMITER_PUNCH;
    float targetLufsOn = 0.0f;
    float targetLufs = -9.0f;
    float masterPush = 8.0f;
    float stabilize = 25.0f;
    float stabTargets[5] = { -6.0f, -10.0f, -12.0f, -14.0f, -18.0f };
    char summary[256] = {0};
};

// v5 3-Second Spectral, Stereo, Sub-Phase & Loudness Auto-Master Engine
class AutoMaster {
public:
    AutoMaster() {
        init(44100.0f);
    }

    void init(float sampleRate) {
        sr = (sampleRate > 8000.0f) ? sampleRate : 44100.0f;
        targetSamples = static_cast<int>(sr * 3.0f);
        lowFilter.setParams(FilterType::LowPass,  sr, 120.0f,  0.7071f, 0.0f);
        mudFilter.setParams(FilterType::BandPass, sr, 400.0f,  1.0f,    0.0f);
        biteFilter.setParams(FilterType::BandPass, sr, 3200.0f, 1.0f,    0.0f);
        airFilter.setParams(FilterType::HighPass, sr, 8000.0f, 0.7071f, 0.0f);
        subLpL.setParams(FilterType::LowPass, sr, 120.0f, 0.7071f, 0.0f);
        subLpR.setParams(FilterType::LowPass, sr, 120.0f, 0.7071f, 0.0f);
        meter.init(sr);
        reset();
    }

    void reset() {
        analyzing.store(false, std::memory_order_relaxed);
        ready.store(false, std::memory_order_relaxed);
        progress.store(0.0f, std::memory_order_relaxed);
        sampleCount = 0;
        totalEnergy = lowEnergy = mudEnergy = biteEnergy = airEnergy = 0.0;
        midEnergy = sideEnergy = 0.0;
        subCorrNum = subEnergyL = subEnergyR = 0.0;
        peakAbs = 0.0f;
        lowFilter.reset(); mudFilter.reset(); biteFilter.reset(); airFilter.reset();
        subLpL.reset(); subLpR.reset();
        meter.reset();
    }

    void startAnalyze(const ReferenceProfile& targetProf, AutoMasterMode mode, bool isRefWav) {
        reset();
        activeTarget = targetProf;
        activeMode = mode;
        usingRefWav = isRefWav;
        analyzing.store(true, std::memory_order_release);
    }

    bool isAnalyzing() const { return analyzing.load(std::memory_order_relaxed); }
    float getProgress() const { return progress.load(std::memory_order_relaxed); }

    bool consumeResult(AutoMasterResult& out) {
        if (ready.exchange(false, std::memory_order_acquire)) {
            out = lastResult;
            return true;
        }
        return false;
    }

    void process(const float* left, const float* right, int numSamples) {
        if (!analyzing.load(std::memory_order_acquire) || !left || numSamples <= 0) return;

        meter.process(left, right, numSamples);

        for (int i = 0; i < numSamples; ++i) {
            float inL = left[i];
            float inR = right ? right[i] : inL;
            float mid  = 0.5f * (inL + inR);
            float side = 0.5f * (inL - inR);
            peakAbs = std::max(peakAbs, std::max(std::abs(inL), std::abs(inR)));

            float lo = lowFilter.process(mid);
            float md = mudFilter.process(mid);
            float bt = biteFilter.process(mid);
            float ar = airFilter.process(mid);

            totalEnergy += static_cast<double>(mid * mid);
            lowEnergy   += static_cast<double>(lo * lo);
            mudEnergy   += static_cast<double>(md * md);
            biteEnergy  += static_cast<double>(bt * bt);
            airEnergy   += static_cast<double>(ar * ar);

            midEnergy   += static_cast<double>(mid * mid);
            sideEnergy  += static_cast<double>(side * side);

            float sL = subLpL.process(inL);
            float sR = subLpR.process(inR);
            subCorrNum += static_cast<double>(sL * sR);
            subEnergyL += static_cast<double>(sL * sL);
            subEnergyR += static_cast<double>(sR * sR);
        }

        sampleCount += numSamples;
        progress.store(std::clamp(static_cast<float>(sampleCount) / static_cast<float>(targetSamples), 0.0f, 1.0f),
                       std::memory_order_relaxed);

        if (sampleCount >= targetSamples) {
            finalizeAnalysis();
        }
    }

private:
    void finalizeAnalysis() {
        double n = static_cast<double>(std::max(1, sampleCount));
        double rms = std::sqrt(totalEnergy / n) + 1e-9;
        auto bandDb = [&](double e) -> float {
            double bRms = std::sqrt(e / n) + 1e-9;
            return static_cast<float>(20.0 * std::log10(bRms / rms));
        };

        float lowRel  = bandDb(lowEnergy);
        float mudRel  = bandDb(mudEnergy);
        float biteRel = bandDb(biteEnergy);
        float airRel  = bandDb(airEnergy);

        float crestDb = static_cast<float>(20.0 * std::log10(static_cast<double>(peakAbs) / rms + 1e-9));
        float sideRatio = static_cast<float>(std::sqrt(sideEnergy / (midEnergy + 1e-9)));
        double subDen = std::sqrt(std::max(1e-18, subEnergyL * subEnergyR));
        float subCorr = static_cast<float>(std::clamp(subCorrNum / subDen, -1.0, 1.0));

        float eqCap = (activeMode == AUTOMASTER_INVISIBLE) ? 1.5f
                    : (activeMode == AUTOMASTER_POLISH ? 3.0f : 4.5f);

        AutoMasterResult r;
        r.eqBass = std::clamp((activeTarget.lowRel  - lowRel)  * 0.50f, -eqCap, eqCap);
        r.eqMud  = std::clamp((activeTarget.mudRel  - mudRel)  * 0.55f, -eqCap, eqCap);
        r.eqBite = std::clamp((activeTarget.biteRel - biteRel) * 0.45f, -eqCap, eqCap);
        r.eqAir  = std::clamp((activeTarget.airRel  - airRel)  * 0.40f, -eqCap, eqCap);

        r.dynChug  = std::clamp(20.0f + (mudRel - activeTarget.mudRel) * 4.0f, 5.0f, 60.0f);
        r.dynHarsh = std::clamp(20.0f + (biteRel - activeTarget.biteRel) * 4.0f, 5.0f, 60.0f);

        // Stereo width matching
        float widthDelta = (activeTarget.sideRatio - sideRatio) * 85.0f;
        r.stereoWidth = std::clamp(105.0f + widthDelta, 85.0f, 145.0f);

        // Sub-bass phase protection
        bool subPhaseFixed = (subCorr < 0.75f);
        r.monoFreq = subPhaseFixed ? std::clamp(95.0f + (0.75f - subCorr) * 60.0f, 105.0f, 145.0f) : 90.0f;

        // Automatic over-compression protection
        bool alreadySquashed = (crestDb < 6.5f);
        float crestFactorNorm = std::clamp((crestDb - 6.0f) / 10.0f, 0.0f, 1.0f);
        r.mbLow  = alreadySquashed ? 5.0f : std::clamp(15.0f + crestFactorNorm * 28.0f, 10.0f, 55.0f);
        r.mbMid  = alreadySquashed ? 5.0f : std::clamp(12.0f + crestFactorNorm * 22.0f, 10.0f, 50.0f);
        r.mbHigh = alreadySquashed ? 5.0f : std::clamp(12.0f + crestFactorNorm * 20.0f, 10.0f, 50.0f);
        r.glueAmount = alreadySquashed ? 0.4f : (activeMode == AUTOMASTER_LOUD ? 2.2f : 1.4f);

        for (int b = 0; b < 5; ++b) r.stabTargets[b] = activeTarget.stabTargets[b];

        if (activeMode == AUTOMASTER_INVISIBLE) {
            r.intensity = 30.0f;
            r.limiterStyle = LIMITER_CLEAN;
            r.targetLufsOn = 1.0f;
            r.targetLufs = std::clamp(activeTarget.targetLufs, -24.0f, -3.0f);
            r.stabilize = 15.0f;
            r.masterPush = std::clamp((r.targetLufs - (-18.0f)) * 0.80f, 2.0f, 14.0f);
        } else if (activeMode == AUTOMASTER_POLISH) {
            r.intensity = 75.0f;
            r.limiterStyle = LIMITER_PUNCH;
            r.targetLufsOn = usingRefWav ? 1.0f : 0.0f;
            r.targetLufs = std::clamp(activeTarget.targetLufs, -24.0f, -3.0f);
            r.stabilize = 25.0f;
            r.masterPush = std::clamp((r.targetLufs - (-18.0f)) * 0.85f, 2.0f, 15.0f);
        } else {
            r.intensity = 100.0f;
            r.limiterStyle = LIMITER_CRUSH;
            r.targetLufsOn = 0.0f;
            r.targetLufs = std::clamp(activeTarget.targetLufs, -24.0f, -3.0f);
            r.stabilize = 35.0f;
            r.masterPush = std::clamp((r.targetLufs - (-18.0f)) * 0.95f, 4.0f, 16.0f);
        }

        const char* modeStr = (activeMode == AUTOMASTER_INVISIBLE) ? "INVISIBLE"
                            : (activeMode == AUTOMASTER_POLISH ? "POLISH" : "LOUD");
        std::snprintf(
            r.summary, sizeof(r.summary),
            "Auto-Master [%s%s]: SubCorr %.2f -> Mono %.0fHz | Crest %.1fdB%s | EQ (%+.1f, %+.1f, %+.1f, %+.1f)",
            modeStr,
            usingRefWav ? "+REF" : "",
            subCorr, r.monoFreq,
            crestDb, alreadySquashed ? " (Squashed: Comp Backed Off)" : "",
            r.eqBass, r.eqMud, r.eqBite, r.eqAir
        );

        lastResult = r;
        analyzing.store(false, std::memory_order_release);
        ready.store(true, std::memory_order_release);
    }

    float sr = 44100.0f;
    int targetSamples = 132300;
    int sampleCount = 0;

    ReferenceProfile activeTarget;
    AutoMasterMode activeMode = AUTOMASTER_POLISH;
    bool usingRefWav = false;

    double totalEnergy = 0.0, lowEnergy = 0.0, mudEnergy = 0.0, biteEnergy = 0.0, airEnergy = 0.0;
    double midEnergy = 0.0, sideEnergy = 0.0;
    double subCorrNum = 0.0, subEnergyL = 0.0, subEnergyR = 0.0;
    float peakAbs = 0.0f;

    Biquad lowFilter, mudFilter, biteFilter, airFilter;
    Biquad subLpL, subLpR;
    LoudnessMeter meter;

    AutoMasterResult lastResult;
    std::atomic<bool> analyzing{false};
    std::atomic<bool> ready{false};
    std::atomic<float> progress{0.0f};
};

} // namespace Asster
