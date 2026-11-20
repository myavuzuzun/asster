#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <atomic>

#include "Biquad.hpp"
#include "LoudnessMeter.hpp"
#include "AutoTrim.hpp"
#include "LowEndMaster.hpp"
#include "MasteringEQ.hpp"
#include "HarmonicSaturator.hpp"
#include "StereoProcessor.hpp"
#include "GlueCompressor.hpp"
#include "SoftClipper.hpp"
#include "TruePeakLimiter.hpp"
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

    NUM_PARAMS
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
        {"preset",           "Genre Preset",       "",      0.0f,    6.0f,    0.0f, 1.0f,  true},
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
        {"bypass",           "Bypass Master",      "",      0.0f,    1.0f,    0.0f, 1.0f,  true}
    };
    return defs;
}

class AssterEngine {
public:
    AssterEngine() {
        setSampleRate(44100.0f);
        loadPreset(0); // Default to Extreme Metal
    }

    void setSampleRate(float sr) {
        sampleRate = (sr > 8000.0f) ? sr : 44100.0f;
        autoTrim.init(sampleRate);
        lowEnd.init(sampleRate);
        eq.init(sampleRate);
        saturator.init(sampleRate);
        stereo.init(sampleRate);
        compressor.init(sampleRate);
        limiter.init(sampleRate);
        outMeter.init(sampleRate);
        reset();
    }

    void reset() {
        silenceBlocks = 0;
        autoTrim.reset();
        lowEnd.reset();
        eq.reset();
        saturator.reset();
        stereo.reset();
        compressor.reset();
        limiter.reset();
        outMeter.reset();
    }

    int getLatencySamples() const {
        return limiter.getLatencySamples();
    }

    void loadPreset(int presetIndex) {
        const auto& presets = getMetalPresets();
        if (presetIndex < 0 || presetIndex >= static_cast<int>(presets.size())) return;

        currentPreset = presetIndex;
        const auto& p = presets[presetIndex];

        params[PARAM_PRESET] = static_cast<float>(presetIndex);
        params[PARAM_SUB_CUT] = p.subCutHz;
        params[PARAM_MONO_FREQ] = p.monoCrossoverHz;
        params[PARAM_EQ_BASS] = p.eqLowGain;
        params[PARAM_EQ_MUD] = p.eqLowMidGain;
        params[PARAM_EQ_BITE] = p.eqHighMidGain;
        params[PARAM_EQ_AIR] = p.eqAirGain;
        params[PARAM_SAT_DRIVE] = p.satDrive;
        params[PARAM_SAT_MIX] = p.satMix;
        params[PARAM_SAT_TYPE] = static_cast<float>(p.satType);
        params[PARAM_STEREO_WIDTH] = p.stereoWidth;
        params[PARAM_CLIPPER_DRIVE] = p.clipperDrive;
        params[PARAM_MASTER_PUSH] = p.limiterDrive;
        params[PARAM_CEILING] = p.limiterCeiling;

        applyAllParams();
    }

    void setParameter(int index, float value) {
        if (index < 0 || index >= NUM_PARAMS) return;
        const auto* defs = getParameterDefs();
        params[index] = std::clamp(value, defs[index].minVal, defs[index].maxVal);

        if (index == PARAM_PRESET) {
            loadPreset(static_cast<int>(params[index]));
            return;
        }

        if (index == PARAM_CALIBRATE_TRIGGER) {
            if (value > 0.5f) {
                autoTrim.startLearning();
            }
            return;
        }

        applyParam(index);
    }

    float getParameter(int index) const {
        if (index < 0 || index >= NUM_PARAMS) return 0.0f;
        return params[index];
    }

    int getCurrentPreset() const { return currentPreset; }

    void triggerCalibration() {
        autoTrim.startLearning();
    }

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

    void process(float** inputs, float** outputs, int sampleFrames) {
        if (!inputs || !outputs || sampleFrames <= 0) return;

        const float* inL = inputs[0];
        const float* inR = (inputs[1] != nullptr) ? inputs[1] : inL;
        float* outL = outputs[0];
        float* outR = (outputs[1] != nullptr) ? outputs[1] : outL;

        // Check Bypass
        if (params[PARAM_BYPASS] > 0.5f) {
            if (outL != inL) std::memcpy(outL, inL, sampleFrames * sizeof(float));
            if (outR != inR) std::memcpy(outR, inR, sampleFrames * sizeof(float));
            outMeter.process(outL, outR, sampleFrames);
            return;
        }

        // Copy input to output buffers for in-place processing chain
        if (outL != inL) std::memcpy(outL, inL, sampleFrames * sizeof(float));
        if (outR != inR) std::memcpy(outR, inR, sampleFrames * sizeof(float));

        // Fast silence gate: when playback stops in DAW, bypass DSP after tail clears to guarantee 0% idle CPU
        float maxInput = 0.0f;
        for (int i = 0; i < sampleFrames; ++i) {
            float absL = std::abs(outL[i]);
            float absR = std::abs(outR[i]);
            if (absL > maxInput) maxInput = absL;
            if (absR > maxInput) maxInput = absR;
            if (maxInput > 1e-7f) break;
        }

        if (maxInput <= 1e-7f) {
            if (++silenceBlocks > 4) {
                std::memset(outL, 0, sampleFrames * sizeof(float));
                std::memset(outR, 0, sampleFrames * sizeof(float));
                outMeter.process(outL, outR, sampleFrames);
                return;
            }
        } else {
            silenceBlocks = 0;
        }

        // 1. Stage 0: AutoTrim Input Calibration (normalizes to -18 LUFS nominal sweet spot)
        autoTrim.process(outL, outR, sampleFrames);

        // 2. Stage 1: Subsonic Filter & Mono-Bass Crossover
        lowEnd.process(outL, outR, sampleFrames);

        // 3. Stage 2: 4-Band Mastering EQ (Contour mud, boost bite and air)
        eq.process(outL, outR, sampleFrames);

        // 4. Stage 3: Harmonic Saturator (Analog Tape/Tube density & glue)
        saturator.process(outL, outR, sampleFrames);

        // 5. Stage 4: Stereo M/S Processor (Width & high-frequency side air)
        stereo.process(outL, outR, sampleFrames);

        // 6. Stage 5: Master Bus Glue Compressor
        compressor.process(outL, outR, sampleFrames);

        // 7. Stage 6: Pro Soft Clipper (Shaves extreme micro-peaks)
        clipper.process(outL, outR, sampleFrames);

        // 8. Stage 7: True-Peak Lookahead Brickwall Limiter (Guaranteed 0 Clipping!)
        limiter.process(outL, outR, sampleFrames);

        // 9. Output Loudness & True Peak Metering
        outMeter.process(outL, outR, sampleFrames);
    }

    void process(const float* const* inputs, float* const* outputs, int sampleFrames) {
        process(const_cast<float**>(const_cast<float* const*>(inputs)),
                const_cast<float**>(outputs),
                sampleFrames);
    }

private:
    void applyAllParams() {
        for (int i = 0; i < NUM_PARAMS; ++i) {
            applyParam(i);
        }
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
            case PARAM_EQ_BASS:
                eq.setBandGain(0, params[PARAM_EQ_BASS]);
                break;
            case PARAM_EQ_MUD:
                eq.setBandGain(1, params[PARAM_EQ_MUD]);
                break;
            case PARAM_EQ_BITE:
                eq.setBandGain(2, params[PARAM_EQ_BITE]);
                break;
            case PARAM_EQ_AIR:
                eq.setBandGain(3, params[PARAM_EQ_AIR]);
                break;
            case PARAM_SAT_DRIVE:
                saturator.setDrive(params[PARAM_SAT_DRIVE]);
                break;
            case PARAM_SAT_MIX:
                saturator.setMix(params[PARAM_SAT_MIX]);
                break;
            case PARAM_SAT_TYPE:
                saturator.setType(static_cast<SaturationType>(static_cast<int>(params[PARAM_SAT_TYPE])));
                break;
            case PARAM_STEREO_WIDTH:
                stereo.setWidth(params[PARAM_STEREO_WIDTH]);
                break;
            case PARAM_GLUE_AMOUNT:
                // Map glue amount (0..6dB) to threshold
                compressor.setThreshold(-10.0f - params[PARAM_GLUE_AMOUNT] * 2.0f);
                break;
            case PARAM_CLIPPER_DRIVE:
                clipper.setDriveDb(params[PARAM_CLIPPER_DRIVE]);
                break;
            case PARAM_MASTER_PUSH:
                limiter.setDriveDb(params[PARAM_MASTER_PUSH]);
                break;
            case PARAM_CEILING:
                limiter.setCeilingDb(params[PARAM_CEILING]);
                clipper.setCeilingDb(params[PARAM_CEILING]);
                break;
            default:
                break;
        }
    }

    float sampleRate = 44100.0f;
    int currentPreset = 0;
    int silenceBlocks = 0;
    float params[NUM_PARAMS] = {0};

    AutoTrim autoTrim;
    LowEndMaster lowEnd;
    MasteringEQ eq;
    HarmonicSaturator saturator;
    StereoProcessor stereo;
    GlueCompressor compressor;
    SoftClipper clipper;
    TruePeakLimiter limiter;
    LoudnessMeter outMeter;
};

} // namespace Asster
