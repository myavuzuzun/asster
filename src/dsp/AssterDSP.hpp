#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <atomic>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <mutex>

#include "Biquad.hpp"
#include "LoudnessMeter.hpp"
#include "AutoTrim.hpp"
#include "LowEndMaster.hpp"
#include "MasteringEQ.hpp"
#include "DynamicEQ.hpp"
#include "SpectralStabilizer.hpp"
#include "HarmonicSaturator.hpp"
#include "StereoProcessor.hpp"
#include "GlueCompressor.hpp"
#include "MultibandComp.hpp"
#include "TransientShaper.hpp"
#include "SoftClipper.hpp"
#include "TruePeakLimiter.hpp"
#include "Oversampler.hpp"
#include "Dither.hpp"
#include "AutoMaster.hpp"
#include "AssterPresets.hpp"

namespace Asster {

enum ParameterID {
    PARAM_PRESET = 0,
    PARAM_AUTOTRIM_MODE,
    PARAM_CALIBRATE_TRIGGER,
    PARAM_IN_TRIM,
    PARAM_MASTER_PUSH,
    PARAM_CEILING,

    PARAM_SUB_CUT,
    PARAM_MONO_FREQ,
    PARAM_EQ_BASS,
    PARAM_EQ_MUD,
    PARAM_EQ_BITE,
    PARAM_EQ_AIR,

    PARAM_SAT_DRIVE,
    PARAM_SAT_MIX,
    PARAM_SAT_TYPE,

    PARAM_STEREO_WIDTH,
    PARAM_GLUE_AMOUNT,
    PARAM_CLIPPER_DRIVE,
    PARAM_BYPASS,

    // v2 Pro Engine Parameters
    PARAM_OVERSAMPLING,
    PARAM_DYN_CHUG,
    PARAM_DYN_HARSH,
    PARAM_GLUE_SC_HPF,
    PARAM_GLUE_MIX,
    PARAM_AUDITION_MODE,
    PARAM_MATCH_BYPASS,
    PARAM_DITHER_MODE,

    // v3 Advanced Mastering Parameters
    PARAM_MB_LOW,
    PARAM_MB_MID,
    PARAM_MB_HIGH,
    PARAM_TRANS_PUNCH,
    PARAM_TRANS_SUSTAIN,
    PARAM_LIMITER_STYLE,
    PARAM_EQ_BASS_FREQ,
    PARAM_EQ_MUD_FREQ,
    PARAM_EQ_BITE_FREQ,
    PARAM_EQ_AIR_FREQ,
    PARAM_EQ_MUD_Q,
    PARAM_EQ_BITE_Q,

    // v4 Transparent & Pro Mastering Parameters
    PARAM_INTENSITY,
    PARAM_MOD_EQ,
    PARAM_MOD_DYN,
    PARAM_MOD_MB,
    PARAM_MOD_GLUE,
    PARAM_MOD_SAT,
    PARAM_MOD_STEREO,
    PARAM_EQ_BASS_ROUTE,
    PARAM_EQ_MUD_ROUTE,
    PARAM_EQ_BITE_ROUTE,
    PARAM_EQ_AIR_ROUTE,
    PARAM_TARGET_LUFS_ON,
    PARAM_TARGET_LUFS,

    // v5 Auto-Mastering Parameters
    PARAM_STABILIZE,
    PARAM_AUTOMASTER_MODE,

    NUM_PARAMS
};

enum AuditionMode {
    AUDITION_STEREO = 0,
    AUDITION_MONO = 1,
    AUDITION_MID = 2,
    AUDITION_SIDE = 3,
    AUDITION_DELTA = 4
};

struct ParameterDef {
    const char* id;
    const char* name;
    const char* label;
    float minVal;
    float maxVal;
    float defaultVal;
    float step;
    bool isInt;
};

inline const ParameterDef* getParameterDefs() {
    static const ParameterDef defs[NUM_PARAMS] = {
        {"preset",           "Genre Preset",       "",      0.0f,    8.0f,    0.0f, 1.0f,  true},
        {"autotrim_mode",    "Auto-Trim Mode",     "",      0.0f,    2.0f,    1.0f, 1.0f,  true},
        {"calibrate_btn",    "Calibrate Now",      "",      0.0f,    1.0f,    0.0f, 1.0f,  true},
        {"in_trim",          "Input Trim",         "dB",  -24.0f,   24.0f,    0.0f, 0.1f,  false},
        {"master_push",      "Master Push",        "dB",    0.0f,   18.0f,    8.0f, 0.1f,  false},
        {"ceiling",          "True Peak Ceiling",  "dBTP", -2.0f,   -0.1f,   -0.5f, 0.1f,  false},

        {"sub_cut",          "Sub Cut (HPF)",      "Hz",   15.0f,   50.0f,   28.0f, 1.0f,  false},
        {"mono_freq",        "Mono Bass Crossover","Hz",   40.0f,  180.0f,   95.0f, 1.0f,  false},
        {"eq_bass",          "Bass Punch",         "dB",   -8.0f,    8.0f,    2.0f, 0.1f,  false},
        {"eq_mud",           "Mud Scoop (500Hz)",  "dB",   -8.0f,    8.0f,   -2.5f, 0.1f,  false},
        {"eq_bite",          "Bite / Attack (4k)", "dB",   -8.0f,    8.0f,    2.5f, 0.1f,  false},
        {"eq_air",           "Air Sheen (11k)",    "dB",   -8.0f,    8.0f,    1.5f, 0.1f,  false},

        {"sat_drive",        "Analog Drive",       "%",     0.0f,  100.0f,   25.0f, 0.5f,  false},
        {"sat_mix",          "Analog Mix",         "%",     0.0f,  100.0f,   45.0f, 0.5f,  false},
        {"sat_type",         "Saturator Type",     "",      0.0f,    2.0f,    0.0f, 1.0f,  true},

        {"stereo_width",     "Stereo Width",       "%",     0.0f,  200.0f,  125.0f, 1.0f,  false},
        {"glue_amount",      "Bus Glue (GR)",      "dB",    0.0f,    6.0f,    1.5f, 0.1f,  false},
        {"clipper_drive",    "Clipper Drive",      "dB",    0.0f,    6.0f,    1.5f, 0.1f,  false},
        {"bypass",           "Bypass Master",      "",      0.0f,    1.0f,    0.0f, 1.0f,  true},

        {"oversampling",     "Oversampling",       "",      0.0f,    2.0f,    1.0f, 1.0f,  true},
        {"dyn_chug",         "Dynamic Chug Ctrl",  "%",     0.0f,  100.0f,   25.0f, 1.0f,  false},
        {"dyn_harsh",        "Dynamic De-Harsh",   "%",     0.0f,  100.0f,   30.0f, 1.0f,  false},
        {"glue_sc_hpf",      "Glue Sidechain HPF", "Hz",   20.0f,  200.0f,  100.0f, 1.0f,  false},
        {"glue_mix",         "Glue Parallel Mix",  "%",     0.0f,  100.0f,  100.0f, 1.0f,  false},
        {"audition_mode",    "Audition Mode",      "",      0.0f,    4.0f,    0.0f, 1.0f,  true},
        {"match_bypass",     "Loudness Match Byp", "",      0.0f,    1.0f,    1.0f, 1.0f,  true},
        {"dither_mode",      "Output Dither",      "",      0.0f,    2.0f,    0.0f, 1.0f,  true},

        {"mb_low",           "Multiband Low",      "%",     0.0f,  100.0f,   20.0f, 1.0f,  false},
        {"mb_mid",           "Multiband Mid",      "%",     0.0f,  100.0f,   15.0f, 1.0f,  false},
        {"mb_high",          "Multiband High",     "%",     0.0f,  100.0f,   15.0f, 1.0f,  false},
        {"trans_punch",      "Transient Punch",    "%",  -100.0f,  100.0f,    0.0f, 1.0f,  false},
        {"trans_sustain",    "Transient Sustain",  "%",  -100.0f,  100.0f,    0.0f, 1.0f,  false},
        {"limiter_style",    "Limiter Style",      "",      0.0f,    2.0f,    1.0f, 1.0f,  true},
        {"eq_bass_freq",     "Bass Freq",          "Hz",   30.0f,  200.0f,   70.0f, 1.0f,  false},
        {"eq_mud_freq",      "Mud Freq",           "Hz",  180.0f, 1000.0f,  450.0f, 5.0f,  false},
        {"eq_bite_freq",     "Bite Freq",          "Hz", 1200.0f, 6500.0f, 3800.0f, 25.0f, false},
        {"eq_air_freq",      "Air Freq",           "Hz", 6000.0f,16000.0f,11000.0f, 50.0f, false},
        {"eq_mud_q",         "Mud Q",              "",      0.4f,    4.0f,    1.5f, 0.05f, false},
        {"eq_bite_q",        "Bite Q",             "",      0.4f,    4.0f,    1.5f, 0.05f, false},

        {"intensity",        "Master Intensity",   "%",     0.0f,  100.0f,  100.0f, 1.0f,  false},
        {"mod_eq",           "EQ Module On",       "",      0.0f,    1.0f,    1.0f, 1.0f,  true},
        {"mod_dyn",          "Dyn EQ Module On",   "",      0.0f,    1.0f,    1.0f, 1.0f,  true},
        {"mod_mb",           "Multiband Module On","",      0.0f,    1.0f,    1.0f, 1.0f,  true},
        {"mod_glue",         "Glue Module On",     "",      0.0f,    1.0f,    1.0f, 1.0f,  true},
        {"mod_sat",          "Saturator Module On","",      0.0f,    1.0f,    1.0f, 1.0f,  true},
        {"mod_stereo",       "Stereo Module On",   "",      0.0f,    1.0f,    1.0f, 1.0f,  true},
        {"eq_bass_route",    "Bass Routing",       "",      0.0f,    2.0f,    0.0f, 1.0f,  true},
        {"eq_mud_route",     "Mud Routing",        "",      0.0f,    2.0f,    0.0f, 1.0f,  true},
        {"eq_bite_route",    "Bite Routing",       "",      0.0f,    2.0f,    0.0f, 1.0f,  true},
        {"eq_air_route",     "Air Routing",        "",      0.0f,    2.0f,    0.0f, 1.0f,  true},
        {"target_lufs_on",   "Target LUFS Lock",   "",      0.0f,    1.0f,    0.0f, 1.0f,  true},
        {"target_lufs",      "Target LUFS",        "LUFS",-24.0f,   -3.0f,  -14.0f, 0.1f,  false},
        {"stabilize",        "Spectral Stabilizer","%",     0.0f,  100.0f,   15.0f, 1.0f,  false},
        {"automaster_mode",  "Auto-Master Mode",   "",      0.0f,    2.0f,    1.0f, 1.0f,  true}
    };
    return defs;
}

class AssterEngine {
public:
    struct StateSnapshot {
        float params[NUM_PARAMS] = {0.0f};
        int32_t currentPreset = 0;
    };

    struct SerializedState {
        uint32_t magic = 0x54535341u; // 'ASST'
        uint32_t version = 5u;
        int32_t activeSlot = 0;
        int32_t currentPreset = 0;
        float calibratedGainDb = 0.0f;
        float slotAParams[NUM_PARAMS] = {0.0f};
        float slotBParams[NUM_PARAMS] = {0.0f};
    };

    AssterEngine() {
        const auto* defs = getParameterDefs();
        for (int i = 0; i < NUM_PARAMS; ++i) {
            params[i] = defs[i].defaultVal;
            atomicParams[i].store(defs[i].defaultVal, std::memory_order_relaxed);
            slotA.params[i] = defs[i].defaultVal;
            slotB.params[i] = defs[i].defaultVal;
        }
        dryBufL.resize(4096, 0.0f);
        dryBufR.resize(4096, 0.0f);
        deltaBufL.resize(16384, 0.0f);
        deltaBufR.resize(16384, 0.0f);

        setSampleRate(44100.0f);
        loadPreset(0);
        slotA.currentPreset = currentPreset.load(std::memory_order_relaxed);
        slotB.currentPreset = slotA.currentPreset;
        for (int i = 0; i < NUM_PARAMS; ++i) {
            slotA.params[i] = params[i];
            slotB.params[i] = params[i];
        }
    }

    void setSampleRate(float sr) {
        sampleRate = (sr > 8000.0f) ? sr : 44100.0f;
        dcCoeff = 1.0f - (2.0f * 3.14159265358979323846f * 5.0f / sampleRate);
        autoTrim.init(sampleRate);
        lowEnd.init(sampleRate);
        eq.init(sampleRate);
        dynamicEq.init(sampleRate);
        stabilizer.init(sampleRate);
        stereo.init(sampleRate);
        compressor.init(sampleRate);
        multiband.init(sampleRate);
        transientShaper.init(sampleRate);
        outMeter.init(sampleRate);
        outMeter.setTelemetryEnabled(true);
        autoMaster.init(sampleRate);
        dither.reset();
        updateOversampledRates();
        applyAllParams();
        reset();
    }

    void reset() {
        silenceBlocks = 0;
        matchGainLinear = 1.0f;
        autoTargetDriveDb = params[PARAM_MASTER_PUSH];
        dcPrevInL = dcPrevInR = dcPrevOutL = dcPrevOutR = 0.0f;
        autoTrim.reset();
        lowEnd.reset();
        eq.reset();
        dynamicEq.reset();
        stabilizer.reset();
        saturator.reset();
        stereo.reset();
        compressor.reset();
        multiband.reset();
        transientShaper.reset();
        limiter.reset();
        oversampler.reset();
        outMeter.reset();
        autoMaster.reset();
        dither.reset();
    }

    int getOversamplingFactor() const {
        int m = static_cast<int>(std::round(atomicParams[PARAM_OVERSAMPLING].load(std::memory_order_relaxed)));
        if (m == 1) return 2;
        if (m == 2) return 4;
        return 1;
    }

    int getLatencySamples() const {
        return limiter.getBaseLatencySamples() + oversampler.getLatencySamples(getOversamplingFactor());
    }

    void loadPreset(int presetIndex) {
        const auto& presets = getMetalPresets();
        if (presetIndex < 0 || presetIndex >= static_cast<int>(presets.size())) return;

        currentPreset.store(presetIndex, std::memory_order_relaxed);
        const auto& p = presets[presetIndex];

        auto setP = [&](int id, float v) {
            params[id] = v;
            atomicParams[id].store(v, std::memory_order_relaxed);
        };

        setP(PARAM_PRESET, static_cast<float>(presetIndex));
        setP(PARAM_SUB_CUT, p.subCutHz);
        setP(PARAM_MONO_FREQ, p.monoCrossoverHz);
        setP(PARAM_EQ_BASS, p.eqLowGain);
        setP(PARAM_EQ_MUD, p.eqLowMidGain);
        setP(PARAM_EQ_BITE, p.eqHighMidGain);
        setP(PARAM_EQ_AIR, p.eqAirGain);
        setP(PARAM_EQ_BASS_FREQ, p.eqLowFreq);
        setP(PARAM_EQ_MUD_FREQ, p.eqLowMidFreq);
        setP(PARAM_EQ_BITE_FREQ, p.eqHighMidFreq);
        setP(PARAM_EQ_AIR_FREQ, p.eqAirFreq);
        setP(PARAM_EQ_MUD_Q, p.eqLowMidQ);
        setP(PARAM_EQ_BITE_Q, p.eqHighMidQ);
        setP(PARAM_DYN_CHUG, p.dynChug);
        setP(PARAM_DYN_HARSH, p.dynHarsh);
        setP(PARAM_SAT_DRIVE, p.satDrive);
        setP(PARAM_SAT_MIX, p.satMix);
        setP(PARAM_SAT_TYPE, static_cast<float>(p.satType));
        setP(PARAM_STEREO_WIDTH, p.stereoWidth);
        setP(PARAM_GLUE_SC_HPF, p.glueScHpf);
        setP(PARAM_GLUE_MIX, p.glueMix);
        setP(PARAM_CLIPPER_DRIVE, p.clipperDrive);
        setP(PARAM_MASTER_PUSH, p.limiterDrive);
        setP(PARAM_CEILING, p.limiterCeiling);
        setP(PARAM_TARGET_LUFS, std::clamp(p.targetLUFS, -24.0f, -3.0f));

        if (presetIndex == 6) {
            setP(PARAM_INTENSITY, 25.0f);
            setP(PARAM_STABILIZE, 0.0f);
            setP(PARAM_MB_LOW, 0.0f); setP(PARAM_MB_MID, 0.0f); setP(PARAM_MB_HIGH, 0.0f);
            setP(PARAM_LIMITER_STYLE, static_cast<float>(LIMITER_CLEAN));
        } else if (presetIndex == 7) {
            setP(PARAM_INTENSITY, 80.0f);
            setP(PARAM_STABILIZE, 20.0f);
            setP(PARAM_MB_LOW, 22.0f); setP(PARAM_MB_MID, 16.0f); setP(PARAM_MB_HIGH, 20.0f);
            setP(PARAM_LIMITER_STYLE, static_cast<float>(LIMITER_PUNCH));
        } else if (presetIndex == 8) {
            setP(PARAM_INTENSITY, 90.0f);
            setP(PARAM_STABILIZE, 18.0f);
            setP(PARAM_MB_LOW, 32.0f); setP(PARAM_MB_MID, 15.0f); setP(PARAM_MB_HIGH, 18.0f);
            setP(PARAM_LIMITER_STYLE, static_cast<float>(LIMITER_PUNCH));
        } else if (presetIndex == 0 || presetIndex == 4) {
            setP(PARAM_INTENSITY, 100.0f);
            setP(PARAM_STABILIZE, 20.0f);
            setP(PARAM_MB_LOW, 28.0f); setP(PARAM_MB_MID, 20.0f); setP(PARAM_MB_HIGH, 25.0f);
            setP(PARAM_LIMITER_STYLE, static_cast<float>(LIMITER_CRUSH));
        } else {
            setP(PARAM_INTENSITY, 100.0f);
            setP(PARAM_STABILIZE, 15.0f);
            setP(PARAM_MB_LOW, 20.0f); setP(PARAM_MB_MID, 15.0f); setP(PARAM_MB_HIGH, 15.0f);
            setP(PARAM_LIMITER_STYLE, static_cast<float>(LIMITER_PUNCH));
        }

        if (!hasReferenceProfile()) {
            stabilizer.setTargetCurve(getGenreTargetProfile(presetIndex).stabTargets);
        }

        autoTargetDriveDb = params[PARAM_MASTER_PUSH];
        dirtyMask.fetch_or((1ULL << NUM_PARAMS) - 1ULL, std::memory_order_release);
        applyAllParams();
    }

    void setParameter(int index, float value) {
        if (index < 0 || index >= NUM_PARAMS) return;
        const auto* defs = getParameterDefs();
        float clamped = std::clamp(value, defs[index].minVal, defs[index].maxVal);
        if (defs[index].isInt) clamped = std::round(clamped);

        if (index == PARAM_PRESET) {
            loadPreset(static_cast<int>(clamped));
            return;
        }

        if (index == PARAM_CALIBRATE_TRIGGER) {
            if (clamped > 0.5f) {
                pendingCalibrate.store(true, std::memory_order_release);
            }
            return;
        }

        atomicParams[index].store(clamped, std::memory_order_relaxed);
        dirtyMask.fetch_or(1ULL << index, std::memory_order_release);
    }

    float getParameter(int index) const {
        if (index < 0 || index >= NUM_PARAMS) return 0.0f;
        return atomicParams[index].load(std::memory_order_relaxed);
    }

    int getCurrentPreset() const { return currentPreset.load(std::memory_order_relaxed); }

    void triggerCalibration() {
        pendingCalibrate.store(true, std::memory_order_release);
    }

    void setReferenceProfile(const ReferenceProfile& prof) {
        std::lock_guard<std::mutex> lk(refMutex);
        activeRefProfile = prof;
        if (prof.valid) {
            stabilizer.setTargetCurve(prof.stabTargets);
        }
    }

    void clearReferenceProfile() {
        std::lock_guard<std::mutex> lk(refMutex);
        activeRefProfile = ReferenceProfile{};
        stabilizer.setTargetCurve(getGenreTargetProfile(getCurrentPreset()).stabTargets);
    }

    bool hasReferenceProfile() const {
        std::lock_guard<std::mutex> lk(refMutex);
        return activeRefProfile.valid;
    }

    std::string getDiagnosticSummary() const {
        std::lock_guard<std::mutex> lk(diagMutex);
        return diagnosticSummary;
    }

    void triggerAutoMaster() {
        pendingAutoMaster.store(true, std::memory_order_release);
    }

    bool isAutoMastering() const { return autoMaster.isAnalyzing() || pendingAutoMaster.load(std::memory_order_relaxed); }
    float getAutoMasterProgress() const { return autoMaster.getProgress(); }

    // Undo / Redo
    void pushUndoState() {
        std::lock_guard<std::mutex> lk(historyMutex);
        StateSnapshot snap;
        snap.currentPreset = currentPreset.load(std::memory_order_relaxed);
        for (int i = 0; i < NUM_PARAMS; ++i) {
            snap.params[i] = atomicParams[i].load(std::memory_order_relaxed);
        }
        undoStack.push_back(snap);
        if (undoStack.size() > 32) undoStack.erase(undoStack.begin());
        redoStack.clear();
    }

    bool canUndo() const {
        std::lock_guard<std::mutex> lk(historyMutex);
        return !undoStack.empty();
    }

    bool canRedo() const {
        std::lock_guard<std::mutex> lk(historyMutex);
        return !redoStack.empty();
    }

    bool undo() {
        std::lock_guard<std::mutex> lk(historyMutex);
        if (undoStack.empty()) return false;
        StateSnapshot cur;
        cur.currentPreset = currentPreset.load(std::memory_order_relaxed);
        for (int i = 0; i < NUM_PARAMS; ++i) cur.params[i] = atomicParams[i].load(std::memory_order_relaxed);
        redoStack.push_back(cur);

        const StateSnapshot prev = undoStack.back();
        undoStack.pop_back();
        applySnapshot(prev);
        return true;
    }

    bool redo() {
        std::lock_guard<std::mutex> lk(historyMutex);
        if (redoStack.empty()) return false;
        StateSnapshot cur;
        cur.currentPreset = currentPreset.load(std::memory_order_relaxed);
        for (int i = 0; i < NUM_PARAMS; ++i) cur.params[i] = atomicParams[i].load(std::memory_order_relaxed);
        undoStack.push_back(cur);

        const StateSnapshot next = redoStack.back();
        redoStack.pop_back();
        applySnapshot(next);
        return true;
    }

    // JSON User Preset Save / Load
    bool savePresetJson(const std::wstring& path) const {
        std::ofstream ofs{std::filesystem::path{path}, std::ios::trunc};
        if (!ofs.is_open()) return false;
        const auto* defs = getParameterDefs();
        ofs << "{\n  \"version\": 4,\n  \"preset\": " << getCurrentPreset() << ",\n  \"params\": {\n";
        for (int i = 0; i < NUM_PARAMS; ++i) {
            ofs << "    \"" << defs[i].id << "\": " << atomicParams[i].load(std::memory_order_relaxed);
            if (i + 1 < NUM_PARAMS) ofs << ",";
            ofs << "\n";
        }
        ofs << "  }\n}\n";
        return ofs.good();
    }

    bool loadPresetJson(const std::wstring& path) {
        std::ifstream ifs{std::filesystem::path{path}};
        if (!ifs.is_open()) return false;
        std::ostringstream ss;
        ss << ifs.rdbuf();
        std::string content = ss.str();

        pushUndoState();
        const auto* defs = getParameterDefs();
        int loadedCount = 0;
        for (int i = 0; i < NUM_PARAMS; ++i) {
            std::string key = std::string("\"") + defs[i].id + "\"";
            size_t pos = content.find(key);
            if (pos != std::string::npos) {
                size_t colon = content.find(':', pos + key.size());
                if (colon != std::string::npos) {
                    char* endPtr = nullptr;
                    float v = std::strtof(content.c_str() + colon + 1, &endPtr);
                    if (endPtr != content.c_str() + colon + 1) {
                        float clamped = std::clamp(v, defs[i].minVal, defs[i].maxVal);
                        if (defs[i].isInt) clamped = std::round(clamped);
                        atomicParams[i].store(clamped, std::memory_order_relaxed);
                        loadedCount++;
                    }
                }
            }
        }
        if (loadedCount == 0) return false;
        dirtyMask.fetch_or((1ULL << NUM_PARAMS) - 1ULL, std::memory_order_release);
        applyAllParams();
        return true;
    }

    // A/B Comparison
    void selectSlot(int slot) {
        int target = (slot == 1) ? 1 : 0;
        int cur = activeSlot.load(std::memory_order_relaxed);
        if (target == cur) return;

        StateSnapshot& saveDst = (cur == 0) ? slotA : slotB;
        saveDst.currentPreset = currentPreset.load(std::memory_order_relaxed);
        for (int i = 0; i < NUM_PARAMS; ++i) {
            saveDst.params[i] = atomicParams[i].load(std::memory_order_relaxed);
        }

        const StateSnapshot& loadSrc = (target == 0) ? slotA : slotB;
        currentPreset.store(loadSrc.currentPreset, std::memory_order_relaxed);
        for (int i = 0; i < NUM_PARAMS; ++i) {
            atomicParams[i].store(loadSrc.params[i], std::memory_order_relaxed);
        }
        activeSlot.store(target, std::memory_order_relaxed);
        dirtyMask.fetch_or((1ULL << NUM_PARAMS) - 1ULL, std::memory_order_release);
    }

    void copySlotAToB() {
        int cur = activeSlot.load(std::memory_order_relaxed);
        if (cur == 0) {
            slotA.currentPreset = currentPreset.load(std::memory_order_relaxed);
            for (int i = 0; i < NUM_PARAMS; ++i) {
                slotA.params[i] = atomicParams[i].load(std::memory_order_relaxed);
            }
            slotB = slotA;
        } else {
            slotB = slotA;
            currentPreset.store(slotB.currentPreset, std::memory_order_relaxed);
            for (int i = 0; i < NUM_PARAMS; ++i) {
                atomicParams[i].store(slotB.params[i], std::memory_order_relaxed);
            }
            dirtyMask.fetch_or((1ULL << NUM_PARAMS) - 1ULL, std::memory_order_release);
        }
    }

    int getActiveSlot() const { return activeSlot.load(std::memory_order_relaxed); }

    // State Persistence
    void serializeState(std::vector<uint8_t>& out) const {
        SerializedState st;
        st.activeSlot = activeSlot.load(std::memory_order_relaxed);
        st.currentPreset = currentPreset.load(std::memory_order_relaxed);
        st.calibratedGainDb = autoTrim.getCalibratedGainDb();

        for (int i = 0; i < NUM_PARAMS; ++i) {
            float curVal = atomicParams[i].load(std::memory_order_relaxed);
            st.slotAParams[i] = (st.activeSlot == 0) ? curVal : slotA.params[i];
            st.slotBParams[i] = (st.activeSlot == 1) ? curVal : slotB.params[i];
        }

        out.resize(sizeof(SerializedState));
        std::memcpy(out.data(), &st, sizeof(SerializedState));
    }

    bool deserializeState(const void* data, size_t size) {
        if (!data || size < sizeof(SerializedState)) return false;
        SerializedState st;
        std::memcpy(&st, data, sizeof(SerializedState));
        if (st.magic != 0x54535341u || st.version != 5u) return false;

        const auto* defs = getParameterDefs();
        for (int i = 0; i < NUM_PARAMS; ++i) {
            slotA.params[i] = std::clamp(st.slotAParams[i], defs[i].minVal, defs[i].maxVal);
            slotB.params[i] = std::clamp(st.slotBParams[i], defs[i].minVal, defs[i].maxVal);
        }

        int slot = (st.activeSlot == 1) ? 1 : 0;
        activeSlot.store(slot, std::memory_order_relaxed);
        currentPreset.store(std::clamp(st.currentPreset, 0, 8), std::memory_order_relaxed);
        slotA.currentPreset = currentPreset.load(std::memory_order_relaxed);
        slotB.currentPreset = currentPreset.load(std::memory_order_relaxed);

        const StateSnapshot& src = (slot == 0) ? slotA : slotB;
        for (int i = 0; i < NUM_PARAMS; ++i) {
            atomicParams[i].store(src.params[i], std::memory_order_relaxed);
        }
        autoTrim.setCalibratedGainDb(st.calibratedGainDb);
        dirtyMask.fetch_or((1ULL << NUM_PARAMS) - 1ULL, std::memory_order_release);
        applyAllParams();
        return true;
    }

    // Telemetry Getters
    bool isCalibrating() const { return autoTrim.isLearningActive(); }
    float getAutoTrimGainDb() const { return autoTrim.getCalibratedGainDb(); }
    float getIncomingLUFS() const { return autoTrim.getIncomingLUFS(); }
    float getOutputMomentaryLUFS() const { return outMeter.getMomentaryLUFS(); }
    float getOutputShortTermLUFS() const { return outMeter.getShortTermLUFS(); }
    float getOutputIntegratedLUFS() const { return outMeter.getIntegratedLUFS(); }
    float getOutputTruePeakdBTP() const { return outMeter.getTruePeakdBTP(); }
    float getGlueGR() const { return compressor.getGainReductionDb(); }
    float getLimiterGR() const { return limiter.getGainReductionDb(); }
    float getClipActivity() const { return clipper.getClipActivity(); }
    float getCorrelation() const { return outMeter.getCorrelation(); }
    float getPLR() const { return outMeter.getPLR(); }
    float getLRA() const { return outMeter.getLRA(); }
    float getSpotifyPenaltyDb() const { return outMeter.getSpotifyPenaltyDb(); }
    float getChugGR() const { return dynamicEq.getChugGR(); }
    float getHarshGR() const { return dynamicEq.getHarshGR(); }
    float getMBLowGR() const { return multiband.getLowGR(); }
    float getMBMidGR() const { return multiband.getMidGR(); }
    float getMBHighGR() const { return multiband.getHighGR(); }
    const EQBandSettings& getEQBand(int idx) const { return eq.getBand(idx); }
    float getEffectiveLimiterDriveDb() const { return effectiveDriveDb.load(std::memory_order_relaxed); }

    float getEQMagnitudeDb(float freqHz) const {
        float m = lowEnd.getMagnitudeDb(freqHz);
        if (atomicParams[PARAM_MOD_EQ].load(std::memory_order_relaxed) > 0.5f) {
            m += eq.getMagnitudeDb(freqHz);
        }
        return m;
    }

    void copyFFTSamples(float* dst, int count) const { outMeter.copyRecentSamples(dst, count); }
    void copyPreFFTSamples(float* dst, int count) const { outMeter.copyPreFFTSamples(dst, count); }
    void copyScopeSamples(float* dstL, float* dstR, int count) const { outMeter.copyScopeSamples(dstL, dstR, count); }
    void copyHistory(LoudnessMeter::HistoryPoint* dst, int count) const { outMeter.copyHistory(dst, count); }

    void process(float** inputs, float** outputs, int sampleFrames) {
        if (!inputs || !outputs || sampleFrames <= 0) return;

        syncParametersAudioThread();

        const float* inL = inputs[0];
        const float* inR = (inputs[1] != nullptr) ? inputs[1] : inL;
        float* outL = outputs[0];
        float* outR = (outputs[1] != nullptr) ? outputs[1] : outL;

        if (static_cast<int>(dryBufL.size()) < sampleFrames) {
            dryBufL.resize(sampleFrames, 0.0f);
            dryBufR.resize(sampleFrames, 0.0f);
        }

        std::memcpy(dryBufL.data(), inL, sampleFrames * sizeof(float));
        std::memcpy(dryBufR.data(), inR, sampleFrames * sizeof(float));

        if (outL != inL) std::memcpy(outL, inL, sampleFrames * sizeof(float));
        if (outR != inR) std::memcpy(outR, inR, sampleFrames * sizeof(float));

        // Fast silence gate
        float maxInput = 0.0f;
        for (int i = 0; i < sampleFrames; ++i) {
            float absL = std::abs(outL[i]);
            float absR = std::abs(outR[i]);
            if (absL > maxInput) maxInput = absL;
            if (absR > maxInput) maxInput = absR;
            if (maxInput > 1e-7f) break;
        }

        if (maxInput <= 1e-7f) {
            if (++silenceBlocks > 8) {
                std::memset(outL, 0, sampleFrames * sizeof(float));
                std::memset(outR, 0, sampleFrames * sizeof(float));
                outMeter.pushPreSamples(outL, outR, sampleFrames);
                outMeter.process(outL, outR, sampleFrames);
                outMeter.pushHistory(0.0f, 0.0f, 0.0f, 0.0f);
                return;
            }
        } else {
            silenceBlocks = 0;
        }

        const bool isBypassed = (params[PARAM_BYPASS] > 0.5f);
        if (isBypassed) {
            outMeter.pushPreSamples(dryBufL.data(), dryBufR.data(), sampleFrames);
            if (params[PARAM_MATCH_BYPASS] > 0.5f) {
                float targetGain = computeMatchGainLinear();
                float coeff = 1.0f - std::exp(-static_cast<float>(sampleFrames) / (sampleRate * 0.050f));
                matchGainLinear += (targetGain - matchGainLinear) * coeff;
                for (int i = 0; i < sampleFrames; ++i) {
                    outL[i] = dryBufL[i] * matchGainLinear;
                    outR[i] = dryBufR[i] * matchGainLinear;
                }
            } else {
                std::memcpy(outL, dryBufL.data(), sampleFrames * sizeof(float));
                std::memcpy(outR, dryBufR.data(), sampleFrames * sizeof(float));
            }
            outMeter.process(outL, outR, sampleFrames);
            outMeter.pushHistory(outMeter.getCurrentBlockPeakLinear(), 0.0f, 0.0f, 0.0f);
            return;
        }

        // 0. Automatic 5 Hz DC Offset Blocker
        for (int i = 0; i < sampleFrames; ++i) {
            float xL = outL[i];
            float yL = xL - dcPrevInL + dcCoeff * dcPrevOutL;
            dcPrevInL = xL;
            dcPrevOutL = flushDenormal(yL);
            outL[i] = yL;

            if (outR != outL) {
                float xR = outR[i];
                float yR = xR - dcPrevInR + dcCoeff * dcPrevOutR;
                dcPrevInR = xR;
                dcPrevOutR = flushDenormal(yR);
                outR[i] = yR;
            }
        }

        // 1. AutoTrim Input Calibration
        autoTrim.process(outL, outR, sampleFrames);

        // Record Pre-Master spectrum
        outMeter.pushPreSamples(outL, outR, sampleFrames);

        // Feed AutoMaster Assistant if active
        autoMaster.process(outL, outR, sampleFrames);
        AutoMasterResult amRes;
        if (autoMaster.consumeResult(amRes)) {
            applyAutoMasterResult(amRes);
        }

        const float intensity = params[PARAM_INTENSITY] / 100.0f;

        // 2. Subsonic Filter & Mono-Bass Crossover
        if (intensity > 0.001f) {
            lowEnd.process(outL, outR, sampleFrames);
        }

        // 3. 4-Band Mastering EQ (with Mid/Side routing)
        if (params[PARAM_MOD_EQ] > 0.5f && intensity > 0.001f) {
            eq.process(outL, outR, sampleFrames);
        }

        // 4. 2-Band Dynamic Mastering EQ
        if (params[PARAM_MOD_DYN] > 0.5f && intensity > 0.001f) {
            dynamicEq.process(outL, outR, sampleFrames);
        }

        // 4b. Continuous 5-Band Adaptive Spectral Stabilizer
        if (intensity > 0.001f) {
            stabilizer.process(outL, outR, sampleFrames);
        }

        // 5. Stereo M/S Processor
        if (params[PARAM_MOD_STEREO] > 0.5f && intensity > 0.001f) {
            stereo.process(outL, outR, sampleFrames);
        }

        // 6. 3-Band Multiband Compressor (LR4)
        if (params[PARAM_MOD_MB] > 0.5f && intensity > 0.001f) {
            multiband.process(outL, outR, sampleFrames);
        }

        // 7. Bus Glue Compressor
        if (params[PARAM_MOD_GLUE] > 0.5f && intensity > 0.001f) {
            compressor.process(outL, outR, sampleFrames);
        }

        // 8. Transient Shaper
        if (intensity > 0.001f) {
            transientShaper.process(outL, outR, sampleFrames);
        }

        // Target LUFS Auto-Loudness Lock
        if (params[PARAM_TARGET_LUFS_ON] > 0.5f) {
            float stLufs = outMeter.getShortTermLUFS();
            if (stLufs > -60.0f) {
                float err = params[PARAM_TARGET_LUFS] - stLufs;
                float dt = static_cast<float>(sampleFrames) / sampleRate;
                float alpha = 1.0f - std::exp(-dt / 1.2f);
                autoTargetDriveDb = std::clamp(autoTargetDriveDb + err * alpha, 0.0f, 18.0f);
            }
            limiter.setDriveDb(autoTargetDriveDb);
            effectiveDriveDb.store(autoTargetDriveDb, std::memory_order_relaxed);
        } else {
            limiter.setDriveDb(params[PARAM_MASTER_PUSH]);
            effectiveDriveDb.store(params[PARAM_MASTER_PUSH], std::memory_order_relaxed);
        }

        // 9. True Oversampled Nonlinear + Brickwall Limiter Stage
        int osFactor = getOversamplingFactor();
        const bool satOn = (params[PARAM_MOD_SAT] > 0.5f && intensity > 0.001f);
        const int audition = static_cast<int>(std::round(params[PARAM_AUDITION_MODE]));

        oversampler.process(outL, outR, sampleFrames, osFactor, [&](float* osL, float* osR, int osN) {
            if (satOn) {
                saturator.process(osL, osR, osN);
            }
            if (intensity > 0.001f) {
                clipper.process(osL, osR, osN);
            }
            if (audition == AUDITION_DELTA) {
                if (static_cast<int>(deltaBufL.size()) < osN) {
                    deltaBufL.resize(osN, 0.0f);
                    deltaBufR.resize(osN, 0.0f);
                }
                limiter.process(osL, osR, osN, deltaBufL.data(), deltaBufR.data());
                std::memcpy(osL, deltaBufL.data(), osN * sizeof(float));
                if (osR) std::memcpy(osR, deltaBufR.data(), osN * sizeof(float));
            } else {
                limiter.process(osL, osR, osN);
            }
        });

        // Post-downsample safety ceiling clamp (guarantees 0 clipping samples at 1x, 2x, and 4x)
        const float ceilingLinear = limiter.getCeilingLinear();
        if (audition != AUDITION_DELTA) {
            for (int i = 0; i < sampleFrames; ++i) {
                outL[i] = std::clamp(outL[i], -ceilingLinear, ceilingLinear);
                if (outR != outL) outR[i] = std::clamp(outR[i], -ceilingLinear, ceilingLinear);
            }
            // 10. TPDF Dither + Noise Shaping
            dither.process(outL, outR, sampleFrames, ceilingLinear);
        }

        // 11. Output Loudness, True Peak & Telemetry
        outMeter.process(outL, outR, sampleFrames);
        float combinedCompGR = (params[PARAM_MOD_GLUE] > 0.5f ? compressor.getGainReductionDb() : 0.0f) +
            (params[PARAM_MOD_MB] > 0.5f ? 0.33f * (multiband.getLowGR() + multiband.getMidGR() + multiband.getHighGR()) : 0.0f);
        outMeter.pushHistory(
            outMeter.getCurrentBlockPeakLinear(),
            limiter.getGainReductionDb(),
            combinedCompGR,
            clipper.getClipActivity()
        );
    }

    void process(const float* const* inputs, float* const* outputs, int sampleFrames) {
        process(const_cast<float**>(const_cast<float* const*>(inputs)),
                const_cast<float**>(outputs),
                sampleFrames);
    }

private:
    void applySnapshot(const StateSnapshot& snap) {
        currentPreset.store(snap.currentPreset, std::memory_order_relaxed);
        for (int i = 0; i < NUM_PARAMS; ++i) {
            atomicParams[i].store(snap.params[i], std::memory_order_relaxed);
        }
        dirtyMask.fetch_or((1ULL << NUM_PARAMS) - 1ULL, std::memory_order_release);
    }

    void applyAutoMasterResult(const AutoMasterResult& r) {
        auto setAtomic = [&](int id, float val) {
            const auto* defs = getParameterDefs();
            float c = std::clamp(val, defs[id].minVal, defs[id].maxVal);
            if (defs[id].isInt) c = std::round(c);
            params[id] = c;
            atomicParams[id].store(c, std::memory_order_relaxed);
            applyParam(id);
        };
        setAtomic(PARAM_EQ_BASS, r.eqBass);
        setAtomic(PARAM_EQ_MUD, r.eqMud);
        setAtomic(PARAM_EQ_BITE, r.eqBite);
        setAtomic(PARAM_EQ_AIR, r.eqAir);
        setAtomic(PARAM_DYN_CHUG, r.dynChug);
        setAtomic(PARAM_DYN_HARSH, r.dynHarsh);
        setAtomic(PARAM_MB_LOW, r.mbLow);
        setAtomic(PARAM_MB_MID, r.mbMid);
        setAtomic(PARAM_MB_HIGH, r.mbHigh);
        setAtomic(PARAM_GLUE_AMOUNT, r.glueAmount);
        setAtomic(PARAM_STEREO_WIDTH, r.stereoWidth);
        setAtomic(PARAM_MONO_FREQ, r.monoFreq);
        setAtomic(PARAM_INTENSITY, r.intensity);
        setAtomic(PARAM_LIMITER_STYLE, static_cast<float>(r.limiterStyle));
        setAtomic(PARAM_TARGET_LUFS_ON, r.targetLufsOn);
        setAtomic(PARAM_TARGET_LUFS, r.targetLufs);
        setAtomic(PARAM_MASTER_PUSH, r.masterPush);
        setAtomic(PARAM_STABILIZE, r.stabilize);
        stabilizer.setTargetCurve(r.stabTargets);
        applyEffectiveColorParams();

        {
            std::lock_guard<std::mutex> lk(diagMutex);
            diagnosticSummary = r.summary;
        }
    }

    float computeMatchGainLinear() const {
        float inLufs = autoTrim.getIncomingLUFS();
        float outLufs = outMeter.getShortTermLUFS();
        if (inLufs <= -70.0f || outLufs <= -70.0f) return 1.0f;
        float diffDb = std::clamp(outLufs - inLufs, -18.0f, 18.0f);
        return std::pow(10.0f, diffDb / 20.0f);
    }

    void syncParametersAudioThread() {
        uint64_t mask = dirtyMask.exchange(0ULL, std::memory_order_acquire);
        if (mask != 0ULL) {
            for (int i = 0; i < NUM_PARAMS; ++i) {
                if (mask & (1ULL << i)) {
                    params[i] = atomicParams[i].load(std::memory_order_relaxed);
                    applyParam(i);
                }
            }
        }
        if (pendingCalibrate.exchange(false, std::memory_order_acquire)) {
            autoTrim.startLearning();
        }
        if (pendingAutoMaster.exchange(false, std::memory_order_acquire)) {
            ReferenceProfile prof;
            bool isRef = false;
            {
                std::lock_guard<std::mutex> lk(refMutex);
                if (activeRefProfile.valid) {
                    prof = activeRefProfile;
                    isRef = true;
                } else {
                    prof = getGenreTargetProfile(getCurrentPreset());
                }
            }
            auto mode = static_cast<AutoMasterMode>(static_cast<int>(std::round(params[PARAM_AUTOMASTER_MODE])));
            autoMaster.startAnalyze(prof, mode, isRef);
        }
    }

    void updateOversampledRates() {
        int f = getOversamplingFactor();
        float osSr = sampleRate * static_cast<float>(f);
        saturator.init(osSr);
        saturator.setDrive(params[PARAM_SAT_DRIVE]);
        saturator.setType(static_cast<SaturationType>(static_cast<int>(params[PARAM_SAT_TYPE])));
        limiter.init(osSr, f);
        limiter.setStyle(static_cast<LimiterStyle>(static_cast<int>(params[PARAM_LIMITER_STYLE])));
        limiter.setCeilingDb(params[PARAM_CEILING]);
        applyEffectiveColorParams();
    }

    void applyEffectiveColorParams() {
        float intensity = std::clamp(params[PARAM_INTENSITY] / 100.0f, 0.0f, 1.0f);
        eq.setBandGain(0, params[PARAM_EQ_BASS] * intensity);
        eq.setBandGain(1, params[PARAM_EQ_MUD]  * intensity);
        eq.setBandGain(2, params[PARAM_EQ_BITE] * intensity);
        eq.setBandGain(3, params[PARAM_EQ_AIR]  * intensity);
        dynamicEq.setChugAmount(params[PARAM_DYN_CHUG] * intensity);
        dynamicEq.setHarshAmount(params[PARAM_DYN_HARSH] * intensity);
        multiband.setLowAmount(params[PARAM_MB_LOW] * intensity);
        multiband.setMidAmount(params[PARAM_MB_MID] * intensity);
        multiband.setHighAmount(params[PARAM_MB_HIGH] * intensity);
        compressor.setMix(params[PARAM_GLUE_MIX] * intensity);
        saturator.setMix(params[PARAM_SAT_MIX] * intensity);
        stereo.setWidth(100.0f + (params[PARAM_STEREO_WIDTH] - 100.0f) * intensity);
        transientShaper.setPunch(params[PARAM_TRANS_PUNCH] * intensity);
        transientShaper.setSustain(params[PARAM_TRANS_SUSTAIN] * intensity);
        clipper.setDriveDb(params[PARAM_CLIPPER_DRIVE] * intensity);
        stabilizer.setAmount(params[PARAM_STABILIZE] * intensity);
    }

    void applyAllParams() {
        for (int i = 0; i < NUM_PARAMS; ++i) {
            applyParam(i);
        }
        applyEffectiveColorParams();
    }

    void applyParam(int index) {
        switch (index) {
            case PARAM_AUTOTRIM_MODE:
                autoTrim.setMode(static_cast<AutoTrimMode>(static_cast<int>(params[PARAM_AUTOTRIM_MODE])));
                break;
            case PARAM_IN_TRIM:
                autoTrim.setManualTrimDb(params[PARAM_IN_TRIM]);
                break;
            case PARAM_SUB_CUT:
                lowEnd.setSubCutFreq(params[PARAM_SUB_CUT]);
                break;
            case PARAM_MONO_FREQ:
                lowEnd.setMonoCrossoverFreq(params[PARAM_MONO_FREQ]);
                break;
            case PARAM_EQ_BASS_FREQ: eq.setBandFreq(0, params[PARAM_EQ_BASS_FREQ]); break;
            case PARAM_EQ_MUD_FREQ:  eq.setBandFreq(1, params[PARAM_EQ_MUD_FREQ]);  break;
            case PARAM_EQ_BITE_FREQ: eq.setBandFreq(2, params[PARAM_EQ_BITE_FREQ]); break;
            case PARAM_EQ_AIR_FREQ:  eq.setBandFreq(3, params[PARAM_EQ_AIR_FREQ]);  break;
            case PARAM_EQ_MUD_Q:     eq.setBandQ(1, params[PARAM_EQ_MUD_Q]);        break;
            case PARAM_EQ_BITE_Q:    eq.setBandQ(2, params[PARAM_EQ_BITE_Q]);       break;
            case PARAM_EQ_BASS_ROUTE: eq.setBandRouting(0, static_cast<BandRouting>(static_cast<int>(params[PARAM_EQ_BASS_ROUTE]))); break;
            case PARAM_EQ_MUD_ROUTE:  eq.setBandRouting(1, static_cast<BandRouting>(static_cast<int>(params[PARAM_EQ_MUD_ROUTE])));  break;
            case PARAM_EQ_BITE_ROUTE: eq.setBandRouting(2, static_cast<BandRouting>(static_cast<int>(params[PARAM_EQ_BITE_ROUTE]))); break;
            case PARAM_EQ_AIR_ROUTE:  eq.setBandRouting(3, static_cast<BandRouting>(static_cast<int>(params[PARAM_EQ_AIR_ROUTE])));  break;
            case PARAM_SAT_DRIVE:
                saturator.setDrive(params[PARAM_SAT_DRIVE]);
                break;
            case PARAM_SAT_TYPE:
                saturator.setType(static_cast<SaturationType>(static_cast<int>(params[PARAM_SAT_TYPE])));
                break;
            case PARAM_GLUE_AMOUNT:
                compressor.setThreshold(-10.0f - params[PARAM_GLUE_AMOUNT] * 2.0f);
                break;
            case PARAM_GLUE_SC_HPF:
                compressor.setSidechainHpf(params[PARAM_GLUE_SC_HPF]);
                break;
            case PARAM_MASTER_PUSH:
                limiter.setDriveDb(params[PARAM_MASTER_PUSH]);
                autoTargetDriveDb = params[PARAM_MASTER_PUSH];
                break;
            case PARAM_CEILING:
                limiter.setCeilingDb(params[PARAM_CEILING]);
                clipper.setCeilingDb(params[PARAM_CEILING]);
                break;
            case PARAM_OVERSAMPLING:
                oversampler.reset();
                updateOversampledRates();
                break;
            case PARAM_DITHER_MODE:
                dither.setMode(static_cast<DitherMode>(static_cast<int>(params[PARAM_DITHER_MODE])));
                break;
            case PARAM_LIMITER_STYLE:
                limiter.setStyle(static_cast<LimiterStyle>(static_cast<int>(params[PARAM_LIMITER_STYLE])));
                break;
            case PARAM_INTENSITY:
            case PARAM_EQ_BASS:
            case PARAM_EQ_MUD:
            case PARAM_EQ_BITE:
            case PARAM_EQ_AIR:
            case PARAM_DYN_CHUG:
            case PARAM_DYN_HARSH:
            case PARAM_MB_LOW:
            case PARAM_MB_MID:
            case PARAM_MB_HIGH:
            case PARAM_GLUE_MIX:
            case PARAM_SAT_MIX:
            case PARAM_STEREO_WIDTH:
            case PARAM_TRANS_PUNCH:
            case PARAM_TRANS_SUSTAIN:
            case PARAM_CLIPPER_DRIVE:
            case PARAM_STABILIZE:
                applyEffectiveColorParams();
                break;
            default:
                break;
        }
    }

    float sampleRate = 44100.0f;
    float dcCoeff = 0.999f;
    float dcPrevInL = 0.0f, dcPrevInR = 0.0f;
    float dcPrevOutL = 0.0f, dcPrevOutR = 0.0f;

    std::atomic<int> currentPreset{0};
    std::atomic<int> activeSlot{0};
    int silenceBlocks = 0;
    float matchGainLinear = 1.0f;
    float autoTargetDriveDb = 8.0f;
    std::atomic<float> effectiveDriveDb{8.0f};

    float params[NUM_PARAMS] = {0.0f};
    std::atomic<float> atomicParams[NUM_PARAMS];
    std::atomic<uint64_t> dirtyMask{0ULL};
    std::atomic<bool> pendingCalibrate{false};
    std::atomic<bool> pendingAutoMaster{false};

    mutable std::mutex refMutex;
    ReferenceProfile activeRefProfile;
    mutable std::mutex diagMutex;
    std::string diagnosticSummary;

    StateSnapshot slotA;
    StateSnapshot slotB;
    mutable std::mutex historyMutex;
    std::vector<StateSnapshot> undoStack;
    std::vector<StateSnapshot> redoStack;

    std::vector<float> dryBufL, dryBufR;
    std::vector<float> deltaBufL, deltaBufR;

    AutoTrim autoTrim;
    LowEndMaster lowEnd;
    MasteringEQ eq;
    DynamicEQ dynamicEq;
    SpectralStabilizer stabilizer;
    HarmonicSaturator saturator;
    StereoProcessor stereo;
    GlueCompressor compressor;
    MultibandComp multiband;
    TransientShaper transientShaper;
    SoftClipper clipper;
    TruePeakLimiter limiter;
    Oversampler oversampler;
    Dither dither;
    LoudnessMeter outMeter;
    AutoMaster autoMaster;
};

} // namespace Asster
