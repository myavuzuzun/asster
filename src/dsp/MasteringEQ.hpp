#pragma once
#include <cmath>
#include <algorithm>
#include "Biquad.hpp"

namespace Asster {

enum BandRouting {
    ROUTE_STEREO = 0,
    ROUTE_MID = 1,
    ROUTE_SIDE = 2
};

struct EQBandSettings {
    FilterType type;
    float freqHz;
    float q;
    float gainDb;
    BandRouting routing = ROUTE_STEREO;
};

class MasteringEQ {
public:
    MasteringEQ() {
        init(44100.0f);
    }

    void init(float sampleRate) {
        sr = (sampleRate > 8000.0f) ? sampleRate : 44100.0f;
        updateFilters();
        reset();
    }

    void reset() {
        for (int i = 0; i < 4; ++i) {
            bandL[i].reset();
            bandR[i].reset();
            bandS[i].reset();
        }
    }

    void setBand(int bandIndex, FilterType type, float freqHz, float q, float gainDb) {
        if (bandIndex < 0 || bandIndex >= 4) return;
        BandRouting r = bands[bandIndex].routing;
        bands[bandIndex] = { type, freqHz, q, gainDb, r };
        bandL[bandIndex].setParams(type, sr, freqHz, q, gainDb);
        bandR[bandIndex].setParams(type, sr, freqHz, q, gainDb);
        bandS[bandIndex].setParams(type, sr, freqHz, q, gainDb);
    }

    void setBandGain(int bandIndex, float gainDb) {
        if (bandIndex < 0 || bandIndex >= 4) return;
        bands[bandIndex].gainDb = gainDb;
        bandL[bandIndex].setParams(bands[bandIndex].type, sr, bands[bandIndex].freqHz, bands[bandIndex].q, gainDb);
        bandR[bandIndex].setParams(bands[bandIndex].type, sr, bands[bandIndex].freqHz, bands[bandIndex].q, gainDb);
        bandS[bandIndex].setParams(bands[bandIndex].type, sr, bands[bandIndex].freqHz, bands[bandIndex].q, gainDb);
    }

    void setBandFreq(int bandIndex, float freqHz) {
        if (bandIndex < 0 || bandIndex >= 4) return;
        bands[bandIndex].freqHz = std::clamp(freqHz, 20.0f, 18000.0f);
        bandL[bandIndex].setParams(bands[bandIndex].type, sr, bands[bandIndex].freqHz, bands[bandIndex].q, bands[bandIndex].gainDb);
        bandR[bandIndex].setParams(bands[bandIndex].type, sr, bands[bandIndex].freqHz, bands[bandIndex].q, bands[bandIndex].gainDb);
        bandS[bandIndex].setParams(bands[bandIndex].type, sr, bands[bandIndex].freqHz, bands[bandIndex].q, bands[bandIndex].gainDb);
    }

    void setBandQ(int bandIndex, float q) {
        if (bandIndex < 0 || bandIndex >= 4) return;
        bands[bandIndex].q = std::clamp(q, 0.3f, 5.0f);
        bandL[bandIndex].setParams(bands[bandIndex].type, sr, bands[bandIndex].freqHz, bands[bandIndex].q, bands[bandIndex].gainDb);
        bandR[bandIndex].setParams(bands[bandIndex].type, sr, bands[bandIndex].freqHz, bands[bandIndex].q, bands[bandIndex].gainDb);
        bandS[bandIndex].setParams(bands[bandIndex].type, sr, bands[bandIndex].freqHz, bands[bandIndex].q, bands[bandIndex].gainDb);
    }

    void setBandRouting(int bandIndex, BandRouting routing) {
        if (bandIndex < 0 || bandIndex >= 4) return;
        bands[bandIndex].routing = routing;
    }

    const EQBandSettings& getBand(int bandIndex) const {
        return bands[std::clamp(bandIndex, 0, 3)];
    }

    void process(float* left, float* right, int numSamples) {
        if (!left || numSamples <= 0) return;

        bool anyMS = false;
        for (int b = 0; b < 4; ++b) {
            if (bands[b].routing != ROUTE_STEREO) {
                anyMS = true;
                break;
            }
        }

        if (!anyMS || !right) {
            for (int i = 0; i < numSamples; ++i) {
                float inL = left[i];
                float inR = right ? right[i] : inL;
                __m128 v = _mm_set_ps(0.0f, 0.0f, inR, inL);
                for (int b = 0; b < 4; ++b) {
                    if (std::abs(bands[b].gainDb) < 0.005f) continue;
                    v = bandS[b].processVec(v);
                }
                alignas(16) float out[4];
                _mm_store_ps(out, v);
                left[i] = out[0];
                if (right) right[i] = out[1];
            }
            return;
        }

        for (int i = 0; i < numSamples; ++i) {
            float inL = left[i];
            float inR = right[i];

            for (int b = 0; b < 4; ++b) {
                if (std::abs(bands[b].gainDb) < 0.005f) continue;
                if (bands[b].routing == ROUTE_STEREO) {
                    inL = bandL[b].process(inL);
                    inR = bandR[b].process(inR);
                } else {
                    float mid  = 0.5f * (inL + inR);
                    float side = 0.5f * (inL - inR);
                    if (bands[b].routing == ROUTE_MID) {
                        mid = bandL[b].process(mid);
                    } else {
                        side = bandR[b].process(side);
                    }
                    inL = mid + side;
                    inR = mid - side;
                }
            }

            left[i] = inL;
            right[i] = inR;
        }
    }

    float getMagnitudeDb(float freqHz) const {
        float db = 0.0f;
        for (int b = 0; b < 4; ++b) {
            db += bandL[b].getMagnitudeDb(freqHz, sr);
        }
        return db;
    }

private:
    void updateFilters() {
        bands[0] = { FilterType::LowShelf,  80.0f, 0.71f, 0.0f, ROUTE_STEREO };
        bands[1] = { FilterType::Peak,     450.0f, 1.2f,  0.0f, ROUTE_STEREO };
        bands[2] = { FilterType::Peak,    3500.0f, 1.4f,  0.0f, ROUTE_STEREO };
        bands[3] = { FilterType::HighShelf, 10000.0f, 0.71f, 0.0f, ROUTE_STEREO };

        for (int b = 0; b < 4; ++b) {
            bandL[b].setParams(bands[b].type, sr, bands[b].freqHz, bands[b].q, bands[b].gainDb);
            bandR[b].setParams(bands[b].type, sr, bands[b].freqHz, bands[b].q, bands[b].gainDb);
            bandS[b].setParams(bands[b].type, sr, bands[b].freqHz, bands[b].q, bands[b].gainDb);
        }
    }

    float sr = 44100.0f;
    EQBandSettings bands[4];
    Biquad bandL[4];
    Biquad bandR[4];
    StereoBiquad bandS[4];
};

} // namespace Asster
