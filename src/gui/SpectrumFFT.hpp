#pragma once
#include <cmath>
#include <vector>
#include <algorithm>
#include "../dsp/FastMath.hpp"

namespace AssterUI {

// Precomputed-twiddle 2048-point Radix-2 Cooley-Tukey FFT with fast silence skip
class SpectrumFFT {
public:
    static constexpr int N = 2048;

    SpectrumFFT() {
        constexpr float kPi = 3.14159265358979323846f;
        constexpr float kTwoPi = 6.28318530717958647692f;

        for (int i = 0; i < N; ++i) {
            window[i] = 0.5f * (1.0f - std::cos(kTwoPi * static_cast<float>(i) / static_cast<float>(N - 1)));
        }

        // Precompute bit-reversal table
        for (int i = 0; i < N; ++i) {
            int rev = 0;
            int x = i;
            for (int b = 0; b < 11; ++b) {
                rev = (rev << 1) | (x & 1);
                x >>= 1;
            }
            bitRev[i] = rev;
        }

        // Precompute stage twiddle steps
        for (int s = 0; s < 11; ++s) {
            int len = 1 << (s + 1);
            float ang = -kTwoPi / static_cast<float>(len);
            stageCos[s] = std::cos(ang);
            stageSin[s] = std::sin(ang);
        }

        (void)kPi;
        std::fill(std::begin(smoothedDb), std::end(smoothedDb), -90.0f);
    }

    void compute(const float* samples, float sampleRate, float* outDb, int numPoints, float minFreq = 20.0f, float maxFreq = 20000.0f) {
        if (!samples || !outDb || numPoints <= 0) return;
        if (sampleRate <= 0.0f) sampleRate = 44100.0f;

        const int pts = std::min(numPoints, MAX_POINTS);

        // Fast silence skip: decay spectrum directly without running 2048-pt FFT
        float maxAbs = 0.0f;
        for (int i = 0; i < N; i += 4) {
            float a = std::abs(samples[i]);
            if (a > maxAbs) maxAbs = a;
            if (maxAbs > 1e-6f) break;
        }
        if (maxAbs <= 1e-6f) {
            for (int p = 0; p < pts; ++p) {
                smoothedDb[p] += (-90.0f - smoothedDb[p]) * 0.22f;
                outDb[p] = smoothedDb[p];
            }
            return;
        }

        // Windowed + bit-reversed copy in one pass
        for (int i = 0; i < N; ++i) {
            int r = bitRev[i];
            re[r] = samples[i] * window[i];
            im[r] = 0.0f;
        }

        // Iterative Cooley-Tukey radix-2 FFT using precomputed twiddle steps
        for (int s = 0; s < 11; ++s) {
            int len = 1 << (s + 1);
            int half = len >> 1;
            float wlenRe = stageCos[s];
            float wlenIm = stageSin[s];
            for (int i = 0; i < N; i += len) {
                float wRe = 1.0f;
                float wIm = 0.0f;
                for (int j = 0; j < half; ++j) {
                    float uRe = re[i + j];
                    float uIm = im[i + j];
                    float vRe = re[i + j + half] * wRe - im[i + j + half] * wIm;
                    float vIm = re[i + j + half] * wIm + im[i + j + half] * wRe;
                    re[i + j]        = uRe + vRe;
                    im[i + j]        = uIm + vIm;
                    re[i + j + half] = uRe - vRe;
                    im[i + j + half] = uIm - vIm;
                    float nextWRe = wRe * wlenRe - wIm * wlenIm;
                    wIm = wRe * wlenIm + wIm * wlenRe;
                    wRe = nextWRe;
                }
            }
        }

        if (cachedSr != sampleRate || cachedPts != pts || cachedMinF != minFreq || cachedMaxF != maxFreq) {
            cachedSr = sampleRate;
            cachedPts = pts;
            cachedMinF = minFreq;
            cachedMaxF = maxFreq;
            float binHz = sampleRate / static_cast<float>(N);
            float logMin = std::log10(minFreq);
            float logMax = std::log10(maxFreq);
            for (int p = 0; p < pts; ++p) {
                float t0 = static_cast<float>(p) / static_cast<float>(pts);
                float t1 = static_cast<float>(p + 1) / static_cast<float>(pts);
                float f0 = std::pow(10.0f, logMin + t0 * (logMax - logMin));
                float f1 = std::pow(10.0f, logMin + t1 * (logMax - logMin));
                binStart[p] = std::clamp(static_cast<int>(std::floor(f0 / binHz)), 1, N / 2 - 1);
                binEnd[p]   = std::clamp(static_cast<int>(std::ceil(f1 / binHz)), binStart[p], N / 2 - 1);
            }
        }

        const float normScale = 2.0f / static_cast<float>(N);
        for (int p = 0; p < pts; ++p) {
            float maxMagSq = 0.0f;
            int b0 = binStart[p];
            int b1 = binEnd[p];
            for (int b = b0; b <= b1; ++b) {
                float m2 = re[b] * re[b] + im[b] * im[b];
                if (m2 > maxMagSq) maxMagSq = m2;
            }

            float mag = std::sqrt(maxMagSq) * normScale;
            float db = (mag > 1e-6f) ? Asster::fastLinearToDb(mag) : -90.0f;
            db = std::clamp(db, -90.0f, 0.0f);

            if (db > smoothedDb[p]) {
                smoothedDb[p] += (db - smoothedDb[p]) * 0.55f;
            } else {
                smoothedDb[p] += (db - smoothedDb[p]) * 0.18f;
            }
            outDb[p] = smoothedDb[p];
        }
    }

private:
    static constexpr int MAX_POINTS = 256;
    float window[N] = {0.0f};
    int bitRev[N] = {0};
    float stageCos[11] = {0.0f};
    float stageSin[11] = {0.0f};
    float re[N] = {0.0f};
    float im[N] = {0.0f};
    float smoothedDb[MAX_POINTS] = {0.0f};

    float cachedSr = 0.0f;
    int cachedPts = 0;
    float cachedMinF = 0.0f;
    float cachedMaxF = 0.0f;
    int binStart[MAX_POINTS] = {0};
    int binEnd[MAX_POINTS] = {0};
};

} // namespace AssterUI
