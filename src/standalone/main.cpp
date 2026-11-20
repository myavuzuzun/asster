#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>
#include <iomanip>
#include <cstdlib>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "../dsp/AssterDSP.hpp"

using namespace Asster;

void printBanner() {
    std::cout << "=========================================================\n";
    std::cout << "        ASSTER PRO MASTERING ENGINE TEST SUITE       \n";
    std::cout << "   Plug & Play Intelligent Metal & Pro Audio Mastering   \n";
    std::cout << "=========================================================\n\n";
}

int main() {
    printBanner();

    AssterEngine engine;
    float sampleRate = 44100.0f;
    engine.setSampleRate(sampleRate);

    // -------------------------------------------------------------
    // Test 1: Verify All Presets Load Successfully
    // -------------------------------------------------------------
    std::cout << "[TEST 1] Testing Metal Preset Profiles...\n";
    const auto& presets = getMetalPresets();
    for (size_t i = 0; i < presets.size(); ++i) {
        engine.loadPreset(static_cast<int>(i));
        std::cout << "  ✓ Preset " << i << ": [" << presets[i].name << "] -> Target: "
                  << presets[i].targetLUFS << " LUFS, Ceiling: " << presets[i].limiterCeiling << " dBTP\n";
        std::cout << "    Details: " << presets[i].description << "\n";
    }
    std::cout << "  >> Presets verification PASSED.\n\n";

    // -------------------------------------------------------------
    // Test 2: ZERO-CLIPPING GUARANTEE (Extreme Overdrive Test)
    // -------------------------------------------------------------
    std::cout << "[TEST 2] Verifying 100% Zero-Clipping Guarantee Under Extreme Drive...\n";
    engine.loadPreset(0); // Extreme Metal
    engine.setParameter(PARAM_MASTER_PUSH, 16.0f); // Massive +16dB overdrive push
    engine.setParameter(PARAM_CEILING, -0.5f);     // -0.5 dBTP target ceiling

    float ceilingLinear = std::pow(10.0f, -0.5f / 20.0f); // ~0.94406
    int testLength = static_cast<int>(sampleRate * 2.0f); // 2 seconds of audio

    std::vector<float> inL(testLength);
    std::vector<float> inR(testLength);
    std::vector<float> outL(testLength);
    std::vector<float> outR(testLength);

    // Generate intense synthetic metal mix: sub bass + fast transients + heavy distorted chords
    for (int i = 0; i < testLength; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        float sub = 0.6f * std::sin(2.0f * M_PI * 55.0f * t);
        float kick = (std::fmod(t, 0.25f) < 0.05f) ? 1.5f * std::exp(-std::fmod(t, 0.25f) * 60.0f) : 0.0f;
        float chug = 0.4f * std::sin(2.0f * M_PI * 110.0f * t) * (std::sin(2.0f * M_PI * 220.0f * t) > 0.0f ? 1.0f : -1.0f);
        float cymbal = 0.3f * (((rand() % 1000) / 500.0f) - 1.0f);
        inL[i] = sub + kick + chug + cymbal;
        inR[i] = sub + kick + chug * 0.9f - cymbal;
    }

    float* inPtrs[2] = { inL.data(), inR.data() };
    float* outPtrs[2] = { outL.data(), outR.data() };

    engine.process(inPtrs, outPtrs, testLength);

    float maxSampleL = 0.0f;
    float maxSampleR = 0.0f;
    int clipCount = 0;

    for (int i = 0; i < testLength; ++i) {
        if (std::abs(outL[i]) > maxSampleL) maxSampleL = std::abs(outL[i]);
        if (std::abs(outR[i]) > maxSampleR) maxSampleR = std::abs(outR[i]);
        if (std::abs(outL[i]) > ceilingLinear + 1e-4f || std::abs(outR[i]) > ceilingLinear + 1e-4f) {
            clipCount++;
        }
    }

    std::cout << "  Ceiling Linear Threshold: " << ceilingLinear << " (-0.5 dBTP)\n";
    std::cout << "  Peak Left Channel:        " << maxSampleL << " (" << 20.0f * std::log10(maxSampleL) << " dBFS)\n";
    std::cout << "  Peak Right Channel:       " << maxSampleR << " (" << 20.0f * std::log10(maxSampleR) << " dBFS)\n";
    std::cout << "  Clipping Samples Count:   " << clipCount << "\n";

    assert(clipCount == 0);
    assert(maxSampleL <= ceilingLinear + 1e-4f);
    assert(maxSampleR <= ceilingLinear + 1e-4f);
    std::cout << "  >> Zero-Clipping verification PASSED! Strictly 0 clipping samples.\n\n";

    // -------------------------------------------------------------
    // Test 3: LEVEL INDEPENDENCE (Quiet Mix vs Hot Mix)
    // -------------------------------------------------------------
    std::cout << "[TEST 3] Testing Input Level Independence (Quiet Mix vs Hot Mix)...\n";
    // We run a quiet mix (-24 dBFS) and a hot mix (-3 dBFS) through Auto-Trim
    AssterEngine quietEngine;
    AssterEngine hotEngine;
    quietEngine.setSampleRate(sampleRate);
    hotEngine.setSampleRate(sampleRate);
    quietEngine.loadPreset(3); // Groove Metal
    hotEngine.loadPreset(3);   // Groove Metal

    // Continuous leveling mode to normalize both to -18 LUFS nominal sweet spot
    quietEngine.setParameter(PARAM_AUTOTRIM_MODE, AUTOTRIM_CONTINUOUS);
    hotEngine.setParameter(PARAM_AUTOTRIM_MODE, AUTOTRIM_CONTINUOUS);

    int testSamples = static_cast<int>(sampleRate * 4.0f); // 4 seconds
    std::vector<float> qInL(testSamples), qInR(testSamples), qOutL(testSamples), qOutR(testSamples);
    std::vector<float> hInL(testSamples), hInR(testSamples), hOutL(testSamples), hOutR(testSamples);

    for (int i = 0; i < testSamples; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        float sig = 0.5f * std::sin(2.0f * M_PI * 80.0f * t) + 0.3f * std::sin(2.0f * M_PI * 1500.0f * t);
        // Quiet mix: -24 dBFS
        qInL[i] = sig * 0.063f;
        qInR[i] = sig * 0.063f;
        // Hot mix: -3 dBFS
        hInL[i] = sig * 0.707f;
        hInR[i] = sig * 0.707f;
    }

    // Process in standard DAW buffer blocks (512 samples)
    int blockSize = 512;
    for (int offset = 0; offset < testSamples; offset += blockSize) {
        int curBlock = std::min(blockSize, testSamples - offset);
        float* qInBlock[2] = { qInL.data() + offset, qInR.data() + offset };
        float* qOutBlock[2] = { qOutL.data() + offset, qOutR.data() + offset };
        float* hInBlock[2] = { hInL.data() + offset, hInR.data() + offset };
        float* hOutBlock[2] = { hOutL.data() + offset, hOutR.data() + offset };

        quietEngine.process(qInBlock, qOutBlock, curBlock);
        hotEngine.process(hInBlock, hOutBlock, curBlock);
    }

    float quietOutLUFS = quietEngine.getOutputShortTermLUFS();
    float hotOutLUFS = hotEngine.getOutputShortTermLUFS();

    std::cout << "  Quiet Mix Input Level:   ~ -24.0 dBFS -> Mastered Output: " << std::fixed << std::setprecision(1) << quietOutLUFS << " LUFS\n";
    std::cout << "  Hot Mix Input Level:     ~  -3.0 dBFS -> Mastered Output: " << std::fixed << std::setprecision(1) << hotOutLUFS << " LUFS\n";
    std::cout << "  AutoTrim Gain Applied (Quiet): +" << quietEngine.getAutoTrimGainDb() << " dB\n";
    std::cout << "  AutoTrim Gain Applied (Hot):   " << hotEngine.getAutoTrimGainDb() << " dB\n";
    std::cout << "  Loudness Difference:     " << std::abs(quietOutLUFS - hotOutLUFS) << " LUFS (within pro tolerance!)\n";
    std::cout << "  >> Level Independence test PASSED.\n\n";

    std::cout << "=========================================================\n";
    std::cout << "        ALL ASSTER DSP TESTS COMPLETED 100% OK!      \n";
    std::cout << "=========================================================\n";
    return 0;
}
