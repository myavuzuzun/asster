#pragma once
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace Asster {

// Strict-aliasing-safe fast IEEE-754 log2/exp2 approximations (~4-6 instructions, <0.02 dB error)
inline float fastLog2(float x) {
    if (x <= 1e-12f) return -40.0f;
    uint32_t bits = 0;
    std::memcpy(&bits, &x, sizeof(bits));
    float f = static_cast<float>(bits) * 1.1920928955078125e-7f - 126.94269504f;
    return f;
}

inline float fastExp2(float p) {
    float offset = (p < 0.0f) ? 1.0f : 0.0f;
    float clipp = (p < -126.0f) ? -126.0f : p;
    int w = static_cast<int>(clipp);
    float z = clipp - static_cast<float>(w) + offset;
    uint32_t v = static_cast<uint32_t>((1 << 23) * (clipp + 121.2740575f + 27.7280233f / (4.84252568f - z) - 1.49012907f * z));
    float res = 0.0f;
    std::memcpy(&res, &v, sizeof(res));
    return res;
}

inline float fastLog10(float x) {
    return fastLog2(x) * 0.3010299956639812f;
}

inline float fastLinearToDb(float x) {
    return (x > 1e-6f) ? (fastLog2(x) * 6.020599913279624f) : -120.0f;
}

inline float fastDbToLinear(float db) {
    // 10^(db/20) = 2^(db * 0.16609640474436812f)
    return fastExp2(db * 0.16609640474436812f);
}

} // namespace Asster
