#pragma once
#include <string>
#include <vector>
#include "Biquad.hpp"
#include "HarmonicSaturator.hpp"

namespace Asster {

struct PresetSettings {
    const char* name;
    const char* description;
    float targetLUFS;

    // Sub & Mono
    float subCutHz;
    float monoCrossoverHz;
    bool subCutEnabled;
    bool monoEnabled;

    // EQ: Low, Low-Mid, High-Mid, High
    float eqLowFreq;
    float eqLowGain;
    float eqLowMidFreq;
    float eqLowMidQ;
    float eqLowMidGain;
    float eqHighMidFreq;
    float eqHighMidQ;
    float eqHighMidGain;
    float eqAirFreq;
    float eqAirGain;

    // Dynamic EQ
    float dynChug;
    float dynHarsh;

    // Saturator
    SaturationType satType;
    float satDrive;
    float satMix;
    float satTone;

    // Stereo
    float stereoWidth;
    float sideAirDb;

    // Glue Compressor
    float compThreshold;
    float compRatio;
    float compAttack;
    float compRelease;
    float glueScHpf;
    float glueMix;

    // Clipper
    float clipperDrive;

    // True Peak Limiter
    float limiterDrive;
    float limiterCeiling;
    float limiterRelease;
};

inline const std::vector<PresetSettings>& getMetalPresets() {
    static const std::vector<PresetSettings> presets = {
        // 0: Extreme Metal (Death / Black / Grindcore)
        {
            "Extreme Metal",
            "Razor-sharp blast beat clarity, double-bass kick beater click (4.2kHz), de-harsh cymbals, high-density punch",
            -8.0f,
            32.0f, 105.0f, true, true,
            65.0f, 1.5f,
            450.0f, 1.8f, -3.2f,
            4200.0f, 1.6f, 2.8f,
            11000.0f, 1.2f,
            25.0f, 30.0f,
            SAT_CONSOLE, 22.0f, 40.0f, 1.0f,
            125.0f, 1.5f,
            -14.0f, 2.2f, 15.0f, 75.0f, 110.0f, 100.0f,
            1.5f,
            9.5f, -0.8f, 70.0f
        },

        // 1: Doom Metal (Slow, crushing, monolithic low end)
        {
            "Doom Metal",
            "Crushing low-end weight (50-80Hz), warm vintage tape saturation, relaxed breathing dynamics",
            -10.5f,
            24.0f, 80.0f, true, true,
            55.0f, 3.8f,
            260.0f, 1.0f, 1.6f,
            3500.0f, 1.2f, -1.2f,
            9000.0f, 0.5f,
            10.0f, 15.0f,
            SAT_TAPE, 38.0f, 60.0f, -1.5f,
            135.0f, 0.8f,
            -12.0f, 1.6f, 35.0f, 220.0f, 75.0f, 100.0f,
            0.8f,
            7.0f, -0.5f, 160.0f
        },

        // 2: Sludge Metal (NOLA, raw, swampy, aggressive grit)
        {
            "Sludge Metal",
            "Gritty tube grit, swampy low-end thud, scooped cardboard boxiness (400Hz), abrasive guitar presence",
            -9.0f,
            28.0f, 90.0f, true, true,
            70.0f, 2.5f,
            380.0f, 1.5f, -3.0f,
            3000.0f, 1.4f, 2.2f,
            10000.0f, 1.0f,
            20.0f, 25.0f,
            SAT_TUBE, 35.0f, 55.0f, 0.5f,
            120.0f, 1.2f,
            -13.0f, 2.0f, 20.0f, 110.0f, 95.0f, 100.0f,
            2.0f,
            8.5f, -0.6f, 90.0f
        },

        // 3: Groove Metal (Pantera / Lamb of God / Sepultura)
        {
            "Groove Metal",
            "Tight punchy low end, deep mid-pocket scoop (550Hz), snappy chug attack (3.2kHz), aggressive VCA glue",
            -8.5f,
            28.0f, 95.0f, true, true,
            75.0f, 2.2f,
            550.0f, 1.7f, -3.8f,
            3200.0f, 1.5f, 2.5f,
            12000.0f, 1.8f,
            35.0f, 22.0f,
            SAT_CONSOLE, 25.0f, 45.0f, 0.0f,
            130.0f, 1.8f,
            -15.0f, 2.5f, 25.0f, 90.0f, 105.0f, 100.0f,
            1.8f,
            9.0f, -0.5f, 80.0f
        },

        // 4: Modern Metalcore / Djent (Periphery, Architects)
        {
            "Modern Metalcore / Djent",
            "Ultra-tight low-end anchoring, crystalline air sheen, modern transient clipping, loud commercial master",
            -7.5f,
            30.0f, 115.0f, true, true,
            70.0f, 1.8f,
            500.0f, 2.0f, -2.8f,
            3800.0f, 1.6f, 2.2f,
            13000.0f, 2.5f,
            40.0f, 35.0f,
            SAT_CONSOLE, 20.0f, 35.0f, 1.5f,
            128.0f, 2.0f,
            -16.0f, 2.8f, 18.0f, 65.0f, 120.0f, 100.0f,
            2.2f,
            10.2f, -0.5f, 60.0f
        },

        // 5: Classic Heavy Metal (Iron Maiden, Judas Priest, Black Sabbath)
        {
            "Classic Heavy Metal",
            "Organic dynamic breathing, warm tape glue, natural vocal and guitar midrange, vintage master",
            -11.0f,
            25.0f, 80.0f, true, true,
            80.0f, 1.5f,
            400.0f, 1.0f, -1.0f,
            2800.0f, 1.2f, 1.5f,
            10000.0f, 1.0f,
            10.0f, 15.0f,
            SAT_TAPE, 25.0f, 50.0f, -0.5f,
            115.0f, 0.8f,
            -12.0f, 1.5f, 30.0f, 150.0f, 85.0f, 100.0f,
            0.6f,
            6.5f, -0.5f, 130.0f
        },

        // 6: Transparent Master (All-Genre / Streaming Safe)
        {
            "Transparent Master (Streaming Safe)",
            "Pure dynamic preservation, zero coloration, compliant with Spotify/Apple/Tidal -14 LUFS / -1.0 dBTP standard",
            -14.0f,
            25.0f, 85.0f, true, true,
            80.0f, 0.0f,
            500.0f, 1.0f, 0.0f,
            3000.0f, 1.0f, 0.0f,
            10000.0f, 0.0f,
            0.0f, 0.0f,
            SAT_CONSOLE, 0.0f, 0.0f, 0.0f,
            105.0f, 0.0f,
            -10.0f, 1.3f, 30.0f, 180.0f, 80.0f, 100.0f,
            0.0f,
            3.5f, -1.0f, 150.0f
        },

        // 7: Modern Pop (Glossy vocals, crisp air, tight sub, commercial radio loudness)
        {
            "Modern Pop",
            "Glossy vocal clarity, polished top-end air (12kHz), tight controlled sub, wide commercial stereo sheen",
            -9.5f,
            30.0f, 95.0f, true, true,
            65.0f, 1.4f,
            350.0f, 1.3f, -1.8f,
            3200.0f, 1.2f, 1.6f,
            12000.0f, 2.4f,
            15.0f, 22.0f,
            SAT_CONSOLE, 16.0f, 32.0f, 1.0f,
            122.0f, 1.6f,
            -13.0f, 1.8f, 20.0f, 95.0f, 105.0f, 90.0f,
            1.2f,
            7.8f, -0.5f, 85.0f
        },

        // 8: Hip-Hop / Trap (Deep 808 sub weight, vocal pocket, punchy kick & snare)
        {
            "Hip-Hop / Trap",
            "Deep 808 sub weight (50Hz), 130Hz sidechain HPF so bass doesn't pump vocals, punchy transient clipper",
            -8.5f,
            22.0f, 90.0f, true, true,
            50.0f, 3.2f,
            300.0f, 1.4f, -2.2f,
            3600.0f, 1.3f, 1.8f,
            11500.0f, 2.0f,
            22.0f, 18.0f,
            SAT_TUBE, 28.0f, 45.0f, 0.5f,
            118.0f, 1.4f,
            -13.5f, 2.0f, 22.0f, 85.0f, 130.0f, 85.0f,
            2.0f,
            8.8f, -0.5f, 75.0f
        }
    };

    return presets;
}

} // namespace Asster
