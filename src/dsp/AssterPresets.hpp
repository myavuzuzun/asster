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
        // High speed, double-bass kick articulation, aggressive bite, tight sub control, zero mud
        {
            "Extreme Metal",
            "Razor-sharp blast beat clarity, double-bass kick beater click (4.2kHz), de-harsh cymbals, high-density punch",
            -8.0f,
            32.0f,  // 32 Hz steep HPF
            105.0f, // Mono below 105 Hz
            true, true,
            // EQ: +1.5dB 65Hz punch, -3.2dB 450Hz boxy scoop, +2.8dB 4.2kHz beater click, +1.2dB 11kHz
            65.0f, 1.5f,
            450.0f, 1.8f, -3.2f,
            4200.0f, 1.6f, 2.8f,
            11000.0f, 1.2f,
            // Saturator: Console punch, 20% drive, 40% mix
            SAT_CONSOLE, 22.0f, 40.0f, 1.0f,
            // Stereo: 125% width, +1.5dB side air
            125.0f, 1.5f,
            // Glue: Fast attack/release for rapid transients
            -14.0f, 2.2f, 15.0f, 75.0f,
            // Clipper: +1.5 dB drive to catch snare/kick peaks
            1.5f,
            // Limiter: -8 LUFS target push, -0.8 dBTP ceiling (zero clip), 70ms release
            9.5f, -0.8f, 70.0f
        },

        // 1: Doom Metal (Slow, crushing, monolithic low end)
        // Earth-shaking sub weight, warm low-mids, dark vintage tape fuzz, wide breathing dynamic room
        {
            "Doom Metal",
            "Crushing low-end weight (50-80Hz), warm vintage tape saturation, relaxed breathing dynamics",
            -10.5f,
            24.0f,  // Deep 24 Hz sub
            80.0f,  // Mono below 80 Hz
            true, true,
            // EQ: +3.8dB 55Hz sub weight, +1.6dB 260Hz low-mid warmth, -1.2dB 3.5kHz smoothing, +0.5dB 9kHz
            55.0f, 3.8f,
            260.0f, 1.0f, 1.6f,
            3500.0f, 1.2f, -1.2f,
            9000.0f, 0.5f,
            // Saturator: Heavy warm tape saturation, 38% drive, 60% mix, warm dark tone
            SAT_TAPE, 38.0f, 60.0f, -1.5f,
            // Stereo: 135% wide wall-of-fuzz
            135.0f, 0.8f,
            // Glue: Slow attack to let giant drums thud through
            -12.0f, 1.6f, 35.0f, 220.0f,
            // Clipper: Moderate 0.8 dB drive
            0.8f,
            // Limiter: -10.5 LUFS push, -0.5 dBTP ceiling, 160ms organic release
            7.0f, -0.5f, 160.0f
        },

        // 2: Sludge Metal (NOLA, raw, swampy, aggressive grit)
        // Aggressive tube drive, scooped cardboard mids, raw biting guitar attack, swampy bass push
        {
            "Sludge Metal",
            "Gritty tube grit, swampy low-end thud, scooped cardboard boxiness (400Hz), abrasive guitar presence",
            -9.0f,
            28.0f,  // 28 Hz HPF
            90.0f,  // Mono below 90 Hz
            true, true,
            // EQ: +2.5dB 70Hz, -3.0dB 380Hz cardboard scoop, +2.2dB 3.0kHz raw guitar bite, +1.0dB 10kHz
            70.0f, 2.5f,
            380.0f, 1.5f, -3.0f,
            3000.0f, 1.4f, 2.2f,
            10000.0f, 1.0f,
            // Saturator: Tube distortion, 35% drive, 55% mix
            SAT_TUBE, 35.0f, 55.0f, 0.5f,
            // Stereo: 120% width
            120.0f, 1.2f,
            // Glue: Punchy glue
            -13.0f, 2.0f, 20.0f, 110.0f,
            // Clipper: Aggressive 2.0 dB drive for raw punch
            2.0f,
            // Limiter: -9.0 LUFS push, -0.6 dBTP ceiling, 90ms release
            8.5f, -0.6f, 90.0f
        },

        // 3: Groove Metal (Pantera / Lamb of God / Sepultura)
        // Contoured mid scoop, tight 65Hz punch, snappy snare crack, rhythm guitar chug attack, VCA snap
        {
            "Groove Metal",
            "Tight punchy low end, deep mid-pocket scoop (550Hz), snappy chug attack (3.2kHz), aggressive VCA glue",
            -8.5f,
            28.0f,
            95.0f,
            true, true,
            // EQ: +2.2dB 75Hz kick weight, -3.8dB 550Hz chug pocket, +2.5dB 3.2kHz snare crack, +1.8dB 12kHz
            75.0f, 2.2f,
            550.0f, 1.7f, -3.8f,
            3200.0f, 1.5f, 2.5f,
            12000.0f, 1.8f,
            // Saturator: Console drive for analog transient punch
            SAT_CONSOLE, 25.0f, 45.0f, 0.0f,
            // Stereo: 130% wide stereo guitars, +1.8dB side air
            130.0f, 1.8f,
            // Glue: Punchy VCA compressor
            -15.0f, 2.5f, 25.0f, 90.0f,
            // Clipper: 1.8 dB drive
            1.8f,
            // Limiter: -8.5 LUFS push, -0.5 dBTP ceiling, 80ms release
            9.0f, -0.5f, 80.0f
        },

        // 4: Modern Metalcore / Djent (Periphery, Architects)
        // Hyper-controlled low end, crystalline high-end air, tight surgical clipper, -7.5 LUFS competitive loudness
        {
            "Modern Metalcore / Djent",
            "Ultra-tight low-end anchoring, crystalline air sheen, modern transient clipping, loud commercial master",
            -7.5f,
            30.0f,
            115.0f,
            true, true,
            70.0f, 1.8f,
            500.0f, 2.0f, -2.8f,
            3800.0f, 1.6f, 2.2f,
            13000.0f, 2.5f,
            SAT_CONSOLE, 20.0f, 35.0f, 1.5f,
            128.0f, 2.0f,
            -16.0f, 2.8f, 18.0f, 65.0f,
            2.2f,
            10.2f, -0.5f, 60.0f
        },

        // 5: Classic Heavy Metal (Iron Maiden, Judas Priest, Black Sabbath)
        // Natural dynamics, warm low-mids, singing guitar presence, analog tape glue
        {
            "Classic Heavy Metal",
            "Organic dynamic breathing, warm tape glue, natural vocal and guitar midrange, vintage master",
            -11.0f,
            25.0f,
            80.0f,
            true, true,
            80.0f, 1.5f,
            400.0f, 1.0f, -1.0f,
            2800.0f, 1.2f, 1.5f,
            10000.0f, 1.0f,
            SAT_TAPE, 25.0f, 50.0f, -0.5f,
            115.0f, 0.8f,
            -12.0f, 1.5f, 30.0f, 150.0f,
            0.6f,
            6.5f, -0.5f, 130.0f
        },

        // 6: Transparent Master (All-Genre / Streaming Safe)
        // Clean, linear, non-coloring, perfectly compliant to Spotify/Apple Music -14 LUFS / -1.0 dBTP
        {
            "Transparent Master (Streaming Safe)",
            "Pure dynamic preservation, zero coloration, compliant with Spotify/Apple/Tidal -14 LUFS / -1.0 dBTP standard",
            -14.0f,
            25.0f,
            85.0f,
            true, true,
            80.0f, 0.0f,
            500.0f, 1.0f, 0.0f,
            3000.0f, 1.0f, 0.0f,
            10000.0f, 0.0f,
            SAT_CONSOLE, 0.0f, 0.0f, 0.0f,
            105.0f, 0.0f,
            -10.0f, 1.3f, 30.0f, 180.0f,
            0.0f,
            3.5f, -1.0f, 150.0f
        }
    };

    return presets;
}

} // namespace Asster
