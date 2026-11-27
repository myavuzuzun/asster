#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>
#include "SpectrumFFT.hpp"
#include "../dsp/AutoMaster.hpp"
#include "../dsp/Biquad.hpp"
#include "../dsp/LoudnessMeter.hpp"

namespace AssterUI {

// Zero-dependency RIFF WAV parser + averaged FFT spectrum & ReferenceProfile analyzer
class ReferenceAnalyzer {
public:
    static constexpr int NUM_BINS = 96;

    ReferenceAnalyzer() {
        clear();
    }

    void clear() {
        loaded = false;
        fileName.clear();
        profile = Asster::ReferenceProfile{};
        std::fill(std::begin(refBinsDb), std::end(refBinsDb), -90.0f);
    }

    bool isLoaded() const { return loaded; }
    const float* getBinsDb() const { return refBinsDb; }
    const std::wstring& getFileName() const { return fileName; }
    const Asster::ReferenceProfile& getProfile() const { return profile; }

    bool loadWav(const std::wstring& path) {
        std::ifstream ifs{std::filesystem::path{path}, std::ios::binary};
        if (!ifs.is_open()) return false;

        char riff[12];
        if (!ifs.read(riff, 12)) return false;
        if (std::memcmp(riff, "RIFF", 4) != 0 || std::memcmp(riff + 8, "WAVE", 4) != 0) return false;

        uint16_t audioFormat = 0;
        uint16_t numChannels = 0;
        uint32_t sampleRate = 44100;
        uint16_t bitsPerSample = 0;
        bool hasFmt = false;
        std::vector<float> leftSamples, rightSamples, monoSamples;

        while (ifs.good()) {
            char chunkId[4];
            uint32_t chunkSize = 0;
            if (!ifs.read(chunkId, 4)) break;
            if (!ifs.read(reinterpret_cast<char*>(&chunkSize), 4)) break;

            if (std::memcmp(chunkId, "fmt ", 4) == 0 && chunkSize >= 16) {
                uint8_t fmt[16];
                ifs.read(reinterpret_cast<char*>(fmt), 16);
                std::memcpy(&audioFormat,   fmt + 0,  2);
                std::memcpy(&numChannels,   fmt + 2,  2);
                std::memcpy(&sampleRate,    fmt + 4,  4);
                std::memcpy(&bitsPerSample, fmt + 14, 2);
                if (chunkSize > 16) ifs.seekg(chunkSize - 16, std::ios::cur);
                hasFmt = true;
            } else if (std::memcmp(chunkId, "data", 4) == 0 && hasFmt && numChannels > 0) {
                uint32_t bytesPerSample = bitsPerSample / 8;
                if (bytesPerSample == 0) break;
                uint32_t frameSize = bytesPerSample * numChannels;
                uint32_t maxFrames = std::min(chunkSize / frameSize, sampleRate * 30u);
                leftSamples.reserve(maxFrames);
                rightSamples.reserve(maxFrames);
                monoSamples.reserve(maxFrames);

                std::vector<uint8_t> frame(frameSize);
                for (uint32_t f = 0; f < maxFrames; ++f) {
                    if (!ifs.read(reinterpret_cast<char*>(frame.data()), frameSize)) break;
                    float chVals[2] = { 0.0f, 0.0f };
                    for (uint16_t ch = 0; ch < std::min<uint16_t>(numChannels, 2); ++ch) {
                        const uint8_t* p = frame.data() + ch * bytesPerSample;
                        float s = 0.0f;
                        if (audioFormat == 1 && bitsPerSample == 16) {
                            int16_t v = 0;
                            std::memcpy(&v, p, 2);
                            s = static_cast<float>(v) / 32768.0f;
                        } else if (audioFormat == 1 && bitsPerSample == 24) {
                            int32_t v = (static_cast<int32_t>(p[0])) |
                                        (static_cast<int32_t>(p[1]) << 8) |
                                        (static_cast<int32_t>(static_cast<int8_t>(p[2])) << 16);
                            s = static_cast<float>(v) / 8388608.0f;
                        } else if (audioFormat == 3 && bitsPerSample == 32) {
                            std::memcpy(&s, p, 4);
                        }
                        chVals[ch] = s;
                    }
                    if (numChannels == 1) chVals[1] = chVals[0];
                    leftSamples.push_back(chVals[0]);
                    rightSamples.push_back(chVals[1]);
                    monoSamples.push_back(0.5f * (chVals[0] + chVals[1]));
                }
                break;
            } else {
                ifs.seekg(chunkSize + (chunkSize & 1u), std::ios::cur);
            }
        }

        if (monoSamples.size() < SpectrumFFT::N) return false;

        SpectrumFFT fft;
        double sumLin[NUM_BINS] = {0.0};
        float tmpDb[NUM_BINS] = {0.0f};
        int frames = 0;

        for (size_t off = 0; off + SpectrumFFT::N <= monoSamples.size(); off += SpectrumFFT::N) {
            fft.compute(monoSamples.data() + off, static_cast<float>(sampleRate), tmpDb, NUM_BINS, 20.0f, 20000.0f);
            for (int b = 0; b < NUM_BINS; ++b) {
                sumLin[b] += std::pow(10.0, static_cast<double>(tmpDb[b]) / 20.0);
            }
            frames++;
        }
        if (frames == 0) return false;

        for (int b = 0; b < NUM_BINS; ++b) {
            double avg = sumLin[b] / static_cast<double>(frames);
            refBinsDb[b] = std::clamp(static_cast<float>(20.0 * std::log10(std::max(1e-6, avg))), -90.0f, 0.0f);
        }

        // Compute ReferenceProfile (4-band + 5-band stabilizer + SideRatio + Target LUFS)
        float sr = static_cast<float>(sampleRate);
        Asster::Biquad fLow, fMud, fBite, fAir;
        Asster::Biquad s0, s1, s2, s3, s4;
        fLow.setParams(Asster::FilterType::LowPass,   sr, 120.0f,  0.7071f, 0.0f);
        fMud.setParams(Asster::FilterType::BandPass,  sr, 400.0f,  1.0f,    0.0f);
        fBite.setParams(Asster::FilterType::BandPass, sr, 3200.0f, 1.0f,    0.0f);
        fAir.setParams(Asster::FilterType::HighPass,  sr, 8000.0f, 0.7071f, 0.0f);

        s0.setParams(Asster::FilterType::LowPass,  sr, 80.0f,    0.71f, 0.0f);
        s1.setParams(Asster::FilterType::BandPass, sr, 350.0f,   1.10f, 0.0f);
        s2.setParams(Asster::FilterType::BandPass, sr, 1600.0f,  1.00f, 0.0f);
        s3.setParams(Asster::FilterType::BandPass, sr, 4500.0f,  1.40f, 0.0f);
        s4.setParams(Asster::FilterType::HighPass, sr, 11000.0f, 0.71f, 0.0f);

        Asster::LoudnessMeter lufsMeter;
        lufsMeter.init(sr);
        lufsMeter.process(leftSamples.data(), rightSamples.data(), static_cast<int>(monoSamples.size()));

        double totE = 0.0, lowE = 0.0, mudE = 0.0, biteE = 0.0, airE = 0.0;
        double stE[5] = {0.0};
        double midE = 0.0, sideE = 0.0;

        for (size_t i = 0; i < monoSamples.size(); ++i) {
            float m = monoSamples[i];
            float sd = 0.5f * (leftSamples[i] - rightSamples[i]);
            totE  += static_cast<double>(m * m);
            midE  += static_cast<double>(m * m);
            sideE += static_cast<double>(sd * sd);

            float lo = fLow.process(m);  lowE  += lo * lo;
            float md = fMud.process(m);  mudE  += md * md;
            float bt = fBite.process(m); biteE += bt * bt;
            float ar = fAir.process(m);  airE  += ar * ar;

            float x0 = s0.process(m); stE[0] += x0 * x0;
            float x1 = s1.process(m); stE[1] += x1 * x1;
            float x2 = s2.process(m); stE[2] += x2 * x2;
            float x3 = s3.process(m); stE[3] += x3 * x3;
            float x4 = s4.process(m); stE[4] += x4 * x4;
        }

        double n = static_cast<double>(monoSamples.size());
        double rms = std::sqrt(totE / n) + 1e-9;
        auto rel = [&](double e) -> float {
            return static_cast<float>(20.0 * std::log10((std::sqrt(e / n) + 1e-9) / rms));
        };

        profile.valid = true;
        profile.lowRel  = rel(lowE);
        profile.mudRel  = rel(mudE);
        profile.biteRel = rel(biteE);
        profile.airRel  = rel(airE);
        for (int b = 0; b < 5; ++b) profile.stabTargets[b] = rel(stE[b]);
        profile.sideRatio = static_cast<float>(std::sqrt(sideE / (midE + 1e-9)));
        float integ = lufsMeter.getIntegratedLUFS();
        profile.targetLufs = (integ > -60.0f) ? std::clamp(integ, -16.0f, -6.0f) : -9.0f;

        fileName = std::filesystem::path{path}.filename().wstring();
        loaded = true;
        return true;
    }

private:
    bool loaded = false;
    std::wstring fileName;
    float refBinsDb[NUM_BINS] = {0.0f};
    Asster::ReferenceProfile profile;
};

} // namespace AssterUI
