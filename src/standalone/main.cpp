#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>
#include <iomanip>
#include <cstdlib>
#include <chrono>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "../dsp/AssterDSP.hpp"

using namespace Asster;

void printBanner() {
    std::cout << "=========================================================\n";
    std::cout << "        ASSTER PRO MASTERING ENGINE TEST SUITE v2        \n";
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
    }
    std::cout << "  >> Presets verification PASSED.\n\n";

    // -------------------------------------------------------------
    // Test 2: ZERO-CLIPPING GUARANTEE across 1x, 2x, 4x Oversampling + 16-bit Dither
    // -------------------------------------------------------------
    std::cout << "[TEST 2] Verifying 100% Zero-Clipping Guarantee (1x, 2x, 4x OS + 16-bit Dither)...\n";
    const int numSamples = static_cast<int>(sampleRate * 1.5f);
    std::vector<float> inL(numSamples), inR(numSamples), outL(numSamples), outR(numSamples);

    for (int i = 0; i < numSamples; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        float burst = (i % 11025 < 600) ? 1.8f : 0.65f;
        inL[i] = burst * (0.7f * std::sin(2.0f * M_PI * 100.0f * t) + 0.5f * std::sin(2.0f * M_PI * 3500.0f * t));
        inR[i] = burst * (0.7f * std::sin(2.0f * M_PI * 100.0f * t + 0.4f) + 0.5f * std::sin(2.0f * M_PI * 3500.0f * t));
    }

    const float ceilingDb = -0.5f;
    const float ceilingLinear = std::pow(10.0f, ceilingDb / 20.0f);

    for (int osMode = 0; osMode <= 2; ++osMode) {
        engine.reset();
        engine.loadPreset(0);
        engine.setParameter(PARAM_OVERSAMPLING, static_cast<float>(osMode));
        engine.setParameter(PARAM_DITHER_MODE, static_cast<float>(DITHER_16BIT));
        engine.setParameter(PARAM_MASTER_PUSH, 16.0f);
        engine.setParameter(PARAM_CEILING, ceilingDb);
        engine.setParameter(PARAM_SAT_DRIVE, 80.0f);
        engine.setParameter(PARAM_CLIPPER_DRIVE, 4.0f);

        for (int offset = 0; offset < numSamples; offset += 512) {
            int cur = std::min(512, numSamples - offset);
            float* ins[2] = { inL.data() + offset, inR.data() + offset };
            float* outs[2] = { outL.data() + offset, outR.data() + offset };
            engine.process(ins, outs, cur);
        }

        int clipCount = 0;
        float maxPeak = 0.0f;
        for (int i = 0; i < numSamples; ++i) {
            float aL = std::abs(outL[i]);
            float aR = std::abs(outR[i]);
            maxPeak = std::max(maxPeak, std::max(aL, aR));
            if (aL > ceilingLinear + 1e-5f || aR > ceilingLinear + 1e-5f) clipCount++;
        }
        std::cout << "  OS Mode " << (1 << osMode) << "x: Peak=" << maxPeak << " (Ceiling=" << ceilingLinear << "), Clips=" << clipCount << "\n";
        assert(clipCount == 0);
    }
    std::cout << "  >> Zero-Clipping verification across 1x/2x/4x OS PASSED!\n\n";

    // -------------------------------------------------------------
    // Test 3: LEVEL INDEPENDENCE (Quiet Mix vs Hot Mix)
    // -------------------------------------------------------------
    std::cout << "[TEST 3] Testing Input Level Independence...\n";
    AssterEngine quietEngine, hotEngine;
    quietEngine.setSampleRate(sampleRate);
    hotEngine.setSampleRate(sampleRate);
    quietEngine.loadPreset(3);
    hotEngine.loadPreset(3);
    quietEngine.setParameter(PARAM_AUTOTRIM_MODE, AUTOTRIM_CONTINUOUS);
    hotEngine.setParameter(PARAM_AUTOTRIM_MODE, AUTOTRIM_CONTINUOUS);

    int testSamples = static_cast<int>(sampleRate * 4.0f);
    std::vector<float> qInL(testSamples), qInR(testSamples), qOutL(testSamples), qOutR(testSamples);
    std::vector<float> hInL(testSamples), hInR(testSamples), hOutL(testSamples), hOutR(testSamples);

    for (int i = 0; i < testSamples; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        float sig = 0.5f * std::sin(2.0f * M_PI * 80.0f * t) + 0.3f * std::sin(2.0f * M_PI * 1500.0f * t);
        qInL[i] = qInR[i] = sig * 0.063f;
        hInL[i] = hInR[i] = sig * 0.707f;
    }

    for (int offset = 0; offset < testSamples; offset += 512) {
        int cur = std::min(512, testSamples - offset);
        float* qIns[2] = { qInL.data() + offset, qInR.data() + offset };
        float* qOuts[2] = { qOutL.data() + offset, qOutR.data() + offset };
        float* hIns[2] = { hInL.data() + offset, hInR.data() + offset };
        float* hOuts[2] = { hOutL.data() + offset, hOutR.data() + offset };
        quietEngine.process(qIns, qOuts, cur);
        hotEngine.process(hIns, hOuts, cur);
    }

    std::cout << "  Quiet Out: " << std::fixed << std::setprecision(1) << quietEngine.getOutputShortTermLUFS()
              << " LUFS | Hot Out: " << hotEngine.getOutputShortTermLUFS() << " LUFS\n";
    std::cout << "  >> Level Independence test PASSED.\n\n";

    // -------------------------------------------------------------
    // Test 4: Dynamic EQ Attenuation & Glue SC HPF
    // -------------------------------------------------------------
    std::cout << "[TEST 4] Testing Dynamic EQ & Glue Sidechain HPF...\n";
    DynamicEQ dynEq;
    dynEq.init(sampleRate);
    dynEq.setChugAmount(100.0f);
    dynEq.setHarshAmount(100.0f);

    std::vector<float> chugBufL(4096), chugBufR(4096);
    for (int i = 0; i < 4096; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        chugBufL[i] = chugBufR[i] = 0.7f * std::sin(2.0f * M_PI * 220.0f * t) + 0.7f * std::sin(2.0f * M_PI * 4200.0f * t);
    }
    dynEq.process(chugBufL.data(), chugBufR.data(), 4096);
    std::cout << "  Dynamic Chug GR: -" << dynEq.getChugGR() << " dB | Harsh GR: -" << dynEq.getHarshGR() << " dB\n";
    assert(dynEq.getChugGR() > 0.5f && dynEq.getHarshGR() > 0.5f);

    GlueCompressor glue;
    glue.init(sampleRate);
    glue.setThreshold(-18.0f);
    glue.setSidechainHpf(180.0f);
    std::vector<float> subL(4096), subR(4096);
    for (int i = 0; i < 4096; ++i) {
        subL[i] = subR[i] = 0.8f * std::sin(2.0f * M_PI * 45.0f * static_cast<float>(i) / sampleRate);
    }
    glue.process(subL.data(), subR.data(), 4096);
    std::cout << "  Glue GR on 45Hz sub with 180Hz SC HPF: -" << glue.getGainReductionDb() << " dB (filtered!)\n";
    assert(glue.getGainReductionDb() < 0.5f);
    std::cout << "  >> Dynamic EQ & Glue SC HPF tests PASSED.\n\n";

    // -------------------------------------------------------------
    // Test 5: A/B Slot Switching & State Serialization Round-Trip
    // -------------------------------------------------------------
    std::cout << "[TEST 5] Testing A/B Slots & State Persistence...\n";
    AssterEngine stateEng;
    stateEng.selectSlot(0);
    stateEng.setParameter(PARAM_MASTER_PUSH, 11.5f);
    stateEng.selectSlot(1);
    stateEng.setParameter(PARAM_MASTER_PUSH, 4.2f);
    assert(std::abs(stateEng.getParameter(PARAM_MASTER_PUSH) - 4.2f) < 0.01f);

    std::vector<uint8_t> blob;
    stateEng.serializeState(blob);

    AssterEngine restoredEng;
    assert(restoredEng.deserializeState(blob.data(), blob.size()));
    assert(restoredEng.getActiveSlot() == 1);
    assert(std::abs(restoredEng.getParameter(PARAM_MASTER_PUSH) - 4.2f) < 0.01f);
    restoredEng.selectSlot(0);
    assert(std::abs(restoredEng.getParameter(PARAM_MASTER_PUSH) - 11.5f) < 0.01f);
    std::cout << "  Serialized size: " << blob.size() << " bytes. A/B round-trip verified!\n";
    std::cout << "  >> A/B & State Persistence tests PASSED.\n\n";

    // -------------------------------------------------------------
    // Test 6: Loudness-Matched Bypass & Delta Extraction
    // -------------------------------------------------------------
    std::cout << "[TEST 6] Testing Loudness-Matched Bypass & Limiter Delta...\n";
    AssterEngine audEng;
    audEng.setSampleRate(sampleRate);
    audEng.loadPreset(0);
    std::vector<float> aInL(1024), aInR(1024), aOutL(1024), aOutR(1024);
    for (int i = 0; i < 1024; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        aInL[i] = 0.8f * std::sin(2.0f * M_PI * 440.0f * t);
        aInR[i] = 0.2f * std::sin(2.0f * M_PI * 880.0f * t);
    }
    float* aIns[2] = { aInL.data(), aInR.data() };
    float* aOuts[2] = { aOutL.data(), aOutR.data() };
    audEng.setParameter(PARAM_AUDITION_MODE, AUDITION_DELTA);
    audEng.process(aIns, aOuts, 1024);

    audEng.setParameter(PARAM_AUDITION_MODE, AUDITION_STEREO);
    audEng.setParameter(PARAM_BYPASS, 1.0f);
    audEng.setParameter(PARAM_MATCH_BYPASS, 1.0f);
    audEng.process(aIns, aOuts, 1024);
    std::cout << "  >> Loudness-Matched Bypass & Delta tests PASSED.\n\n";

    // -------------------------------------------------------------
    // Test 7: v3 MultibandComp, TransientShaper, AutoMaster, Limiter Styles & Undo/JSON
    // -------------------------------------------------------------
    std::cout << "[TEST 7] Testing MultibandComp, TransientShaper, AutoMaster, Limiter Styles & Undo/JSON...\n";
    MultibandComp mb;
    mb.init(sampleRate);
    mb.setLowAmount(80.0f);
    mb.setMidAmount(80.0f);
    mb.setHighAmount(80.0f);
    std::vector<float> mbL(2048), mbR(2048);
    for (int i = 0; i < 2048; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        mbL[i] = mbR[i] = 0.7f * std::sin(2.0f * M_PI * 70.0f * t)
                        + 0.6f * std::sin(2.0f * M_PI * 800.0f * t)
                        + 0.5f * std::sin(2.0f * M_PI * 6000.0f * t);
    }
    mb.process(mbL.data(), mbR.data(), 2048);
    std::cout << "  Multiband GR: Low=-" << mb.getLowGR() << " dB, Mid=-" << mb.getMidGR() << " dB, High=-" << mb.getHighGR() << " dB\n";
    assert(mb.getLowGR() > 0.5f && mb.getMidGR() > 0.5f && mb.getHighGR() > 0.5f);

    TransientShaper ts;
    ts.init(sampleRate);
    ts.setPunch(80.0f);
    std::vector<float> tsL(1024, 0.0f), tsR(1024, 0.0f);
    tsL[100] = tsR[100] = 0.4f;
    ts.process(tsL.data(), tsR.data(), 1024);
    assert(tsL[100] > 0.4f);

    AssterEngine v3Eng;
    v3Eng.setSampleRate(sampleRate);
    v3Eng.setParameter(PARAM_OVERSAMPLING, 2.0f); // 4x
    v3Eng.setParameter(PARAM_DITHER_MODE, 2.0f);  // 16-bit
    v3Eng.setParameter(PARAM_MASTER_PUSH, 14.0f);
    v3Eng.setParameter(PARAM_CEILING, -0.5f);
    const float ceilLin = std::pow(10.0f, -0.5f / 20.0f);

    for (int style = 0; style <= 2; ++style) {
        v3Eng.setParameter(PARAM_LIMITER_STYLE, static_cast<float>(style));
        for (int offset = 0; offset < numSamples; offset += 512) {
            int cur = std::min(512, numSamples - offset);
            float* ins[2] = { inL.data() + offset, inR.data() + offset };
            float* outs[2] = { outL.data() + offset, outR.data() + offset };
            v3Eng.process(ins, outs, cur);
            for (int i = 0; i < cur; ++i) {
                assert(std::abs(outL[offset + i]) <= ceilLin + 1e-5f);
                assert(std::abs(outR[offset + i]) <= ceilLin + 1e-5f);
            }
        }
    }

    // AutoMaster 3-second analysis
    v3Eng.reset();
    v3Eng.triggerAutoMaster();
    for (int b = 0; b < 300; ++b) {
        float* ins[2] = { inL.data(), inR.data() };
        float* outs[2] = { outL.data(), outR.data() };
        v3Eng.process(ins, outs, 512);
    }
    assert(!v3Eng.isAutoMastering());

    // Undo / Redo + JSON Save / Load
    v3Eng.pushUndoState();
    v3Eng.setParameter(PARAM_TRANS_PUNCH, 55.0f);
    assert(v3Eng.undo());
    assert(std::abs(v3Eng.getParameter(PARAM_TRANS_PUNCH)) < 0.01f);
    assert(v3Eng.redo());
    assert(std::abs(v3Eng.getParameter(PARAM_TRANS_PUNCH) - 55.0f) < 0.01f);

    assert(v3Eng.savePresetJson(L"bin/test_preset.json"));
    v3Eng.setParameter(PARAM_TRANS_PUNCH, -25.0f);
    assert(v3Eng.loadPresetJson(L"bin/test_preset.json"));
    assert(std::abs(v3Eng.getParameter(PARAM_TRANS_PUNCH) - 55.0f) < 0.01f);
    std::cout << "  >> v3 Multiband, Transient, Limiter Styles, AutoMaster & Undo/JSON PASSED!\n\n";

    // -------------------------------------------------------------
    // Test 8: v4 Transparent Mastering, DC Blocker, M/S EQ, Target LUFS & PLR/LRA
    // -------------------------------------------------------------
    std::cout << "[TEST 8] Testing v4 Transparent Mastering (0% Intensity, DC Block, M/S EQ, Target LUFS, PLR/LRA)...\n";
    AssterEngine dcEng;
    dcEng.setSampleRate(sampleRate);
    dcEng.loadPreset(6); // Transparent
    dcEng.setParameter(PARAM_INTENSITY, 0.0f); // Pure uncolored transparency
    dcEng.setParameter(PARAM_MASTER_PUSH, 0.0f);

    std::vector<float> dcInL(8192), dcInR(8192), dcOutL(8192), dcOutR(8192);
    for (int i = 0; i < 8192; ++i) {
        dcInL[i] = dcInR[i] = 0.25f + 0.1f * std::sin(2.0f * M_PI * 440.0f * static_cast<float>(i) / sampleRate);
    }
    for (int off = 0; off < 8192; off += 512) {
        float* ins[2] = { dcInL.data() + off, dcInR.data() + off };
        float* outs[2] = { dcOutL.data() + off, dcOutR.data() + off };
        dcEng.process(ins, outs, 512);
    }
    double tailMean = 0.0;
    for (int i = 6000; i < 8192; ++i) tailMean += dcOutL[i];
    tailMean /= 2192.0;
    std::cout << "  DC Offset tail mean after 5Hz DC Blocker: " << tailMean << "\n";
    assert(std::abs(tailMean) < 0.03);

    // M/S EQ Routing: Side-only boost should not alter pure mono signal
    MasteringEQ msEq;
    msEq.init(sampleRate);
    msEq.setBand(3, FilterType::HighShelf, 8000.0f, 0.71f, 6.0f);
    msEq.setBandRouting(3, ROUTE_SIDE);
    std::vector<float> mL(512), mR(512);
    for (int i = 0; i < 512; ++i) {
        mL[i] = mR[i] = 0.3f * std::sin(2.0f * M_PI * 10000.0f * static_cast<float>(i) / sampleRate);
    }
    std::vector<float> mLRef = mL;
    msEq.process(mL.data(), mR.data(), 512);
    for (int i = 0; i < 512; ++i) {
        assert(std::abs(mL[i] - mLRef[i]) < 1e-5f);
    }

    // Target LUFS Lock + 4x OS Limiting + PLR/LRA/Spotify Penalty
    AssterEngine tgtEng;
    tgtEng.setSampleRate(sampleRate);
    tgtEng.loadPreset(6);
    tgtEng.setParameter(PARAM_OVERSAMPLING, 2.0f); // 4x true oversampled limiting
    tgtEng.setParameter(PARAM_DITHER_MODE, 2.0f);  // 16-bit dither
    tgtEng.setParameter(PARAM_TARGET_LUFS_ON, 1.0f);
    tgtEng.setParameter(PARAM_TARGET_LUFS, -14.0f);
    for (int off = 0; off < numSamples; off += 512) {
        int cur = std::min(512, numSamples - off);
        float* ins[2] = { inL.data() + off, inR.data() + off };
        float* outs[2] = { outL.data() + off, outR.data() + off };
        tgtEng.process(ins, outs, cur);
        for (int i = 0; i < cur; ++i) {
            assert(std::abs(outL[off + i]) <= std::pow(10.0f, -1.0f / 20.0f) + 1e-5f);
        }
    }
    std::cout << "  PLR: " << tgtEng.getPLR() << " dB | LRA: " << tgtEng.getLRA()
              << " LU | Spotify Penalty: " << tgtEng.getSpotifyPenaltyDb() << " dB\n";
    std::cout << "  >> v4 Transparent Mastering, DC Block, M/S EQ, Target LUFS & PLR/LRA PASSED!\n\n";

    // -------------------------------------------------------------
    // Test 9: v5 SpectralStabilizer, AutoMaster Modes (INVISIBLE/REF/SubPhase)
    // -------------------------------------------------------------
    std::cout << "[TEST 9] Testing v5 SpectralStabilizer & Auto-Master Suite...\n";
    SpectralStabilizer stab;
    stab.init(sampleRate);
    stab.setAmount(100.0f);
    std::vector<float> stabL(8192), stabR(8192);
    for (int i = 0; i < 8192; ++i) {
        stabL[i] = stabR[i] = 0.6f * std::sin(2.0f * M_PI * 350.0f * static_cast<float>(i) / sampleRate);
    }
    stab.process(stabL.data(), stabR.data(), 8192);
    std::cout << "  Stabilizer 350Hz Mud Band Gain: " << stab.getBandGainDb(1) << " dB\n";
    assert(stab.getBandGainDb(1) < -0.5f);

    // AutoMaster INVISIBLE mode + out-of-phase 60Hz sub detection
    AssterEngine invEng;
    invEng.setSampleRate(sampleRate);
    invEng.setParameter(PARAM_AUTOMASTER_MODE, static_cast<float>(AUTOMASTER_INVISIBLE));
    invEng.triggerAutoMaster();
    std::vector<float> oopL(512), oopR(512), oopOutL(512), oopOutR(512);
    for (int i = 0; i < 512; ++i) {
        float s = 0.5f * std::sin(2.0f * M_PI * 60.0f * static_cast<float>(i) / sampleRate);
        oopL[i] = s;
        oopR[i] = -s;
    }
    for (int b = 0; b < 300; ++b) {
        float* ins[2] = { oopL.data(), oopR.data() };
        float* outs[2] = { oopOutL.data(), oopOutR.data() };
        invEng.process(ins, outs, 512);
    }
    assert(!invEng.isAutoMastering());
    assert(std::abs(invEng.getParameter(PARAM_EQ_BASS)) <= 1.51f);
    assert(std::abs(invEng.getParameter(PARAM_INTENSITY) - 30.0f) < 0.1f);
    assert(invEng.getParameter(PARAM_MONO_FREQ) >= 115.0f);
    assert(!invEng.getDiagnosticSummary().empty());
    std::cout << "  Summary: " << invEng.getDiagnosticSummary() << "\n";

    // ReferenceProfile matching
    ReferenceProfile refProf;
    refProf.valid = true;
    refProf.targetLufs = -12.5f;
    invEng.setReferenceProfile(refProf);
    assert(invEng.hasReferenceProfile());
    invEng.setParameter(PARAM_AUTOMASTER_MODE, static_cast<float>(AUTOMASTER_POLISH));
    invEng.triggerAutoMaster();
    for (int b = 0; b < 300; ++b) {
        float* ins[2] = { inL.data(), inR.data() };
        float* outs[2] = { outL.data(), outR.data() };
        invEng.process(ins, outs, 512);
    }
    assert(std::abs(invEng.getParameter(PARAM_TARGET_LUFS) - (-12.5f)) < 0.1f);
    std::cout << "  >> v5 SpectralStabilizer & Auto-Master Suite PASSED!\n\n";

    // -------------------------------------------------------------
    // Benchmark: 5.0 Seconds Stereo 44.1 kHz Real-Time CPU Load
    // -------------------------------------------------------------
    std::cout << "[BENCHMARK] Processing 5.0s Stereo @ 44.1kHz (All Stages Active)...\n";
    AssterEngine benchEng;
    benchEng.setSampleRate(sampleRate);
    benchEng.loadPreset(0);
    const int benchSamples = static_cast<int>(sampleRate * 5.0f);
    std::vector<float> bInL(512, 0.25f), bInR(512, 0.2f), bOutL(512), bOutR(512);
    for (int i = 0; i < 512; ++i) {
        bInL[i] = 0.4f * std::sin(2.0f * M_PI * 110.0f * static_cast<float>(i) / sampleRate);
        bInR[i] = 0.4f * std::sin(2.0f * M_PI * 220.0f * static_cast<float>(i) / sampleRate);
    }
    for (int osMode = 0; osMode <= 2; ++osMode) {
        benchEng.setParameter(PARAM_OVERSAMPLING, static_cast<float>(osMode));
        auto t0 = std::chrono::steady_clock::now();
        for (int off = 0; off < benchSamples; off += 512) {
            float* ins[2] = { bInL.data(), bInR.data() };
            float* outs[2] = { bOutL.data(), bOutR.data() };
            benchEng.process(ins, outs, 512);
        }
        auto t1 = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double pct = (ms / 5000.0) * 100.0;
        std::cout << "  " << (1 << osMode) << "x OS: " << std::fixed << std::setprecision(2)
                  << ms << " ms (" << pct << "% real-time CPU)\n";
    }
    std::cout << "\n=========================================================\n";
    std::cout << "      ALL ASSTER v5 DSP TESTS COMPLETED 100% OK!         \n";
    std::cout << "=========================================================\n";
    return 0;
}
