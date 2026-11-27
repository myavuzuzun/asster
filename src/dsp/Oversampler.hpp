#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include "Biquad.hpp"

namespace Asster {

// 8th-order (4+4 all-pass) polyphase half-band IIR oversampler (>80 dB stopband rejection).
// Zero allocations during real-time processing for standard DAW block sizes.
class Oversampler {
public:
    Oversampler() {
        buf2xL.resize(8192, 0.0f);
        buf2xR.resize(8192, 0.0f);
        buf4xL.resize(16384, 0.0f);
        buf4xR.resize(16384, 0.0f);
        reset();
    }

    void reset() {
        up1.reset();
        up2.reset();
        dn1.reset();
        dn2.reset();
    }

    int getLatencySamples(int factor) const {
        if (factor == 2) return 2;
        if (factor == 4) return 4;
        return 0;
    }

    template <typename Fn>
    void process(float* left, float* right, int numSamples, int factor, Fn&& fn) {
        if (!left || numSamples <= 0) return;

        if (factor <= 1) {
            fn(left, right, numSamples);
            return;
        }

        const int n2 = numSamples * 2;
        if (static_cast<int>(buf2xL.size()) < n2) {
            buf2xL.resize(n2, 0.0f);
            buf2xR.resize(n2, 0.0f);
        }

        // Stage 1: Upsample 1x -> 2x
        upsample2xStereo(left, right, buf2xL.data(), right ? buf2xR.data() : nullptr, numSamples, up1);

        if (factor == 2) {
            fn(buf2xL.data(), right ? buf2xR.data() : nullptr, n2);
        } else {
            // 4x oversampling
            const int n4 = numSamples * 4;
            if (static_cast<int>(buf4xL.size()) < n4) {
                buf4xL.resize(n4, 0.0f);
                buf4xR.resize(n4, 0.0f);
            }

            // Stage 2: Upsample 2x -> 4x
            upsample2xStereo(buf2xL.data(), right ? buf2xR.data() : nullptr, buf4xL.data(), right ? buf4xR.data() : nullptr, n2, up2);

            fn(buf4xL.data(), right ? buf4xR.data() : nullptr, n4);

            // Stage 2: Downsample 4x -> 2x
            downsample2xStereo(buf4xL.data(), right ? buf4xR.data() : nullptr, buf2xL.data(), right ? buf2xR.data() : nullptr, n2, dn2);
        }

        // Stage 1: Downsample 2x -> 1x
        downsample2xStereo(buf2xL.data(), right ? buf2xR.data() : nullptr, left, right, numSamples, dn1);
    }

private:
    struct AllPassSection {
        __m128 x1 = _mm_setzero_ps();
        __m128 y1 = _mm_setzero_ps();

        void reset() {
            x1 = _mm_setzero_ps();
            y1 = _mm_setzero_ps();
        }

        inline __m128 process(__m128 in, float coeff) {
            __m128 c = _mm_set1_ps(coeff);
            __m128 out = _mm_add_ps(_mm_mul_ps(c, _mm_sub_ps(in, y1)), x1);
            x1 = in;
            y1 = out;
            return out;
        }
    };

    struct HalfBandFilter {
        // 4 sections per polyphase branch
        AllPassSection pathA[4];
        AllPassSection pathB[4];

        void reset() {
            for (int i = 0; i < 4; ++i) {
                pathA[i].reset();
                pathB[i].reset();
            }
        }

        inline __m128 processPathA(__m128 x) {
            static constexpr float cA[4] = {
                0.036681502163648017f,
                0.13654762463195794f,
                0.27463175937945444f,
                0.42313861743656711f
            };
            x = pathA[0].process(x, cA[0]);
            x = pathA[1].process(x, cA[1]);
            x = pathA[2].process(x, cA[2]);
            x = pathA[3].process(x, cA[3]);
            return x;
        }

        inline __m128 processPathB(__m128 x) {
            static constexpr float cB[4] = {
                0.007200462816296424f,
                0.07721162205941739f,
                0.20084385094575955f,
                0.34901980141741595f
            };
            x = pathB[0].process(x, cB[0]);
            x = pathB[1].process(x, cB[1]);
            x = pathB[2].process(x, cB[2]);
            x = pathB[3].process(x, cB[3]);
            return x;
        }
    };

    static inline void upsample2xStereo(const float* srcL, const float* srcR,
                                        float* dstL, float* dstR, int n, HalfBandFilter& f) {
        for (int i = 0; i < n; ++i) {
            __m128 x = _mm_set_ps(0.0f, 0.0f, srcR ? srcR[i] : 0.0f, srcL[i]);
            __m128 a = f.processPathA(x);
            __m128 b = f.processPathB(x);
            alignas(16) float arrA[4], arrB[4];
            _mm_store_ps(arrA, a);
            _mm_store_ps(arrB, b);
            dstL[i * 2]     = arrA[0];
            dstL[i * 2 + 1] = arrB[0];
            if (dstR) {
                dstR[i * 2]     = arrA[1];
                dstR[i * 2 + 1] = arrB[1];
            }
        }
    }

    static inline void downsample2xStereo(const float* srcL, const float* srcR,
                                          float* dstL, float* dstR, int n, HalfBandFilter& f) {
        const __m128 half = _mm_set1_ps(0.5f);
        for (int i = 0; i < n; ++i) {
            __m128 inA = _mm_set_ps(0.0f, 0.0f, srcR ? srcR[i * 2]     : 0.0f, srcL[i * 2]);
            __m128 inB = _mm_set_ps(0.0f, 0.0f, srcR ? srcR[i * 2 + 1] : 0.0f, srcL[i * 2 + 1]);
            __m128 out = _mm_mul_ps(half, _mm_add_ps(f.processPathA(inA), f.processPathB(inB)));
            alignas(16) float arr[4];
            _mm_store_ps(arr, out);
            dstL[i] = arr[0];
            if (dstR) dstR[i] = arr[1];
        }
    }

    HalfBandFilter up1, up2;
    HalfBandFilter dn1, dn2;
    std::vector<float> buf2xL, buf2xR;
    std::vector<float> buf4xL, buf4xR;
};

} // namespace Asster
