#pragma once
#include <cstdint>
#include <algorithm>
#include <cmath>

namespace Asster {

enum DitherMode {
    DITHER_OFF = 0,
    DITHER_24BIT = 1,
    DITHER_16BIT = 2
};

// TPDF Dithering with 1st-order high-pass noise shaping and strict ceiling clamp
class Dither {
public:
    Dither() {
        reset();
    }

    void reset() {
        rngStateL = 0x12345678u;
        rngStateR = 0x9ABCDEF0u;
        prevTpdfL = 0.0f;
        prevTpdfR = 0.0f;
    }

    void setMode(DitherMode m) {
        mode = m;
    }

    DitherMode getMode() const { return mode; }

    void process(float* left, float* right, int numSamples, float ceilingLinear) {
        if (!left || numSamples <= 0 || mode == DITHER_OFF) return;

        // 1 LSB amplitude for 24-bit or 16-bit PCM
        const float lsb = (mode == DITHER_16BIT) ? (1.0f / 32768.0f) : (1.0f / 8388608.0f);

        for (int i = 0; i < numSamples; ++i) {
            float tpdfL = (nextUnitFloat(rngStateL) - nextUnitFloat(rngStateL)) * lsb;
            float shapedL = tpdfL - 0.5f * prevTpdfL;
            prevTpdfL = tpdfL;
            left[i] = std::clamp(left[i] + shapedL, -ceilingLinear, ceilingLinear);

            if (right) {
                float tpdfR = (nextUnitFloat(rngStateR) - nextUnitFloat(rngStateR)) * lsb;
                float shapedR = tpdfR - 0.5f * prevTpdfR;
                prevTpdfR = tpdfR;
                right[i] = std::clamp(right[i] + shapedR, -ceilingLinear, ceilingLinear);
            }
        }
    }

private:
    static inline float nextUnitFloat(uint32_t& state) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float>(state & 0x00FFFFFFu) * (1.0f / 16777216.0f);
    }

    DitherMode mode = DITHER_OFF;
    uint32_t rngStateL = 0x12345678u;
    uint32_t rngStateR = 0x9ABCDEF0u;
    float prevTpdfL = 0.0f;
    float prevTpdfR = 0.0f;
};

} // namespace Asster
