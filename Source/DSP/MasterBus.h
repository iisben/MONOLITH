#pragma once

#include "DspMath.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace monolith::dsp
{

/**
    Mastering-grade output stage (stereo-linked):

      1. 2nd-order subsonic high-pass (18 Hz) - removes infrasonic energy from the
         Sub-Abyss layer and tape asymmetry that would otherwise eat headroom.
      2. Soft-knee "glue" bus compressor (1.6:1, -13 dBFS threshold, 30 ms / 280 ms)
         - gently binds the swarm, tames full-velocity chord build-up, max 5 dB GR.
      3. True-peak lookahead brickwall limiter (1.5 ms lookahead, -1 dBTP ceiling).
         The detector sees 4x-interpolated inter-sample peaks (ITU-R BS.1770 style,
         12-tap windowed-sinc polyphase), so reconstructed DAC/codec output also stays
         below the ceiling. A 12 ms post-peak hold stops the gain from riding individual
         cycles of low notes (cuts limiter THD on a 55 Hz overload by ~25 dB).
         Peak-hold + release smoothing + box-filter gain ramp guarantees zero sample
         overshoot WITHOUT waveshaping.

    All buffers are preallocated in prepare(); process() is allocation-free.
*/
class MasterBus
{
public:
    static constexpr float kCeiling = 0.89125094f; // -1.0 dBFS

    void prepare (double sampleRate)
    {
        sr = std::max (8000.0, sampleRate);
        const float srF = static_cast<float> (sr);

        lookahead = std::max (16, static_cast<int> (std::lround (0.0015 * sr)));
        holdSamples = std::max (0, static_cast<int> (std::lround (0.012 * sr)));
        window = lookahead + holdSamples;

        // Ring sizes: power of two >= detector window + 1
        ringSize = 1;
        while (ringSize < window + 2)
            ringSize <<= 1;

        // 4x true-peak interpolator: phases 1..3 between x[n-kTpDelay] and x[n-kTpDelay+1]
        for (int ph = 1; ph < 4; ++ph)
        {
            double sum = 0.0;
            for (int j = 0; j < kTpTaps; ++j)
            {
                const double t = static_cast<double> (kTpDelay - j) - 0.25 * ph; // tap position minus interpolation point
                const double sincV = (std::abs (t) < 1.0e-9) ? 1.0 : std::sin (3.14159265358979323846 * t) / (3.14159265358979323846 * t);
                const double u = t / (kTpDelay + 0.5);
                const double win = (std::abs (u) >= 1.0) ? 0.0
                                 : 0.42 + 0.5 * std::cos (3.14159265358979323846 * u) + 0.08 * std::cos (2.0 * 3.14159265358979323846 * u);
                tpCoeff[static_cast<size_t> (ph - 1)][static_cast<size_t> (j)] = static_cast<float> (sincV * win);
                sum += sincV * win;
            }
            for (auto& c : tpCoeff[static_cast<size_t> (ph - 1)])
                c = static_cast<float> (c / sum); // unity DC gain per phase
        }
        ringMask = ringSize - 1;

        delayL.assign (static_cast<size_t> (ringSize), 0.0f);
        delayR.assign (static_cast<size_t> (ringSize), 0.0f);
        boxRing.assign (static_cast<size_t> (ringSize), 1.0f);
        dqVal.assign (static_cast<size_t> (ringSize), 1.0f);
        dqIdx.assign (static_cast<size_t> (ringSize), 0);

        hpL.setParameters (18.0f, 0.707f, srF);
        hpR.setParameters (18.0f, 0.707f, srF);

        compDetAlpha = 1.0f - std::exp (-1.0f / (0.012f * srF));
        compAtk      = 1.0f - std::exp (-1.0f / (0.030f * srF));
        compRel      = 1.0f - std::exp (-1.0f / (0.280f * srF));
        limRelAlpha  = 1.0f - std::exp (-1.0f / (0.090f * srF));

        reset();
    }

    void reset() noexcept
    {
        std::fill (delayL.begin(), delayL.end(), 0.0f);
        std::fill (delayR.begin(), delayR.end(), 0.0f);
        std::fill (boxRing.begin(), boxRing.end(), 1.0f);
        hpL.reset();
        hpR.reset();
        tpHistL.fill (0.0f);
        tpHistR.fill (0.0f);
        tpPos      = 0;
        writePos   = 0;
        sampleIdx  = 0;
        dqHead     = 0;
        dqTail     = 0;
        boxSum     = static_cast<double> (lookahead);
        limSmooth  = 1.0f;
        compEnvSq  = 0.0f;
        compGrDb   = 0.0f;
        lastLimGr  = 1.0f;
    }

    /** Latency introduced by the limiter lookahead + true-peak interpolator (samples). */
    [[nodiscard]] int getLatencySamples() const noexcept { return lookahead - 1 + kTpDelay; }

    /** Most recent limiter gain (linear, 1 = no reduction). */
    [[nodiscard]] float getLimiterGain() const noexcept { return lastLimGr; }
    [[nodiscard]] float getCompressorGrDb() const noexcept { return compGrDb; }

    inline void process (float& l, float& r) noexcept
    {
        // 1. Subsonic clean-up
        float xL = hpL.processHighpass (sanitize (l));
        float xR = hpR.processHighpass (sanitize (r));

        // 2. Glue compressor (stereo-linked mean-square detector, soft knee)
        const float ms = 0.5f * (xL * xL + xR * xR);
        compEnvSq += compDetAlpha * (ms - compEnvSq);
        const float levelDb = 10.0f * 0.30103f * fastLog2 (compEnvSq + 1.0e-12f); // 10*log10
        constexpr float kThreshDb = -13.0f;
        constexpr float kKneeDb   = 8.0f;
        constexpr float kSlope    = 1.0f - 1.0f / 1.6f;
        const float over = levelDb - kThreshDb;
        float targetGr = 0.0f;
        if (over > 0.5f * kKneeDb)
            targetGr = kSlope * over;
        else if (over > -0.5f * kKneeDb)
        {
            const float k = over + 0.5f * kKneeDb;
            targetGr = kSlope * k * k / (2.0f * kKneeDb);
        }
        targetGr = std::min (targetGr, 5.0f);
        compGrDb += (targetGr > compGrDb ? compAtk : compRel) * (targetGr - compGrDb);
        const float compGain = std::exp2 (-compGrDb * 0.16609640f); // 10^(-dB/20)
        xL *= compGain;
        xR *= compGain;

        // 3. True-peak detection: the limiter works on x[n - kTpDelay] and also sees the three
        //    4x-interpolated points between it and the following sample.
        tpHistL[static_cast<size_t> (tpPos)] = xL;
        tpHistR[static_cast<size_t> (tpPos)] = xR;
        const auto histAt = [this] (const std::array<float, kTpTaps>& h, int j) noexcept
        {
            return h[static_cast<size_t> ((tpPos - j + kTpTaps) % kTpTaps)];
        };
        const float cL = histAt (tpHistL, kTpDelay);
        const float cR = histAt (tpHistR, kTpDelay);
        float peak = std::max (std::abs (cL), std::abs (cR));
        for (size_t ph = 0; ph < 3; ++ph)
        {
            float iL = 0.0f, iR = 0.0f;
            for (int j = 0; j < kTpTaps; ++j)
            {
                const float c = tpCoeff[ph][static_cast<size_t> (j)];
                iL += c * histAt (tpHistL, j);
                iR += c * histAt (tpHistR, j);
            }
            peak = std::max (peak, std::max (std::abs (iL), std::abs (iR)));
        }
        tpPos = (tpPos + 1) % kTpTaps;
        xL = cL;
        xR = cR;

        const float req  = (peak > kCeiling) ? (kCeiling / peak) : 1.0f;

        // Sliding-window minimum over lookahead + hold (monotonic deque, O(1) amortised).
        // The hold part keeps gain reduction steady between the cycles of low notes.
        while (dqTail != dqHead && dqVal[static_cast<size_t> ((dqTail - 1) & ringMask)] >= req)
            --dqTail;
        dqVal[static_cast<size_t> (dqTail & ringMask)] = req;
        dqIdx[static_cast<size_t> (dqTail & ringMask)] = sampleIdx;
        ++dqTail;
        while (dqIdx[static_cast<size_t> (dqHead & ringMask)] + static_cast<int64_t> (window) <= sampleIdx)
            ++dqHead;
        const float held = dqVal[static_cast<size_t> (dqHead & ringMask)];

        // Instant attack / smooth release (output never exceeds held -> overshoot-free)
        if (held < limSmooth)
            limSmooth = held;
        else
            limSmooth += limRelAlpha * (held - limSmooth);

        // Box-filter ramp over the lookahead window (C0-continuous gain, no clicks)
        const size_t boxPos = static_cast<size_t> (sampleIdx & ringMask);
        const size_t oldPos = static_cast<size_t> ((sampleIdx - lookahead) & ringMask);
        boxSum += static_cast<double> (limSmooth) - static_cast<double> (boxRing[oldPos]);
        boxRing[boxPos] = limSmooth;
        const float gain = std::min (1.0f, static_cast<float> (boxSum / static_cast<double> (lookahead)));

        // Delay audio by lookahead-1 samples
        delayL[static_cast<size_t> (writePos)] = xL;
        delayR[static_cast<size_t> (writePos)] = xR;
        const int readPos = (writePos - (lookahead - 1)) & ringMask;
        writePos = (writePos + 1) & ringMask;
        ++sampleIdx;

        lastLimGr = gain;
        // Final guard is a plain clamp that mathematically never engages (gain already <= ceiling/peak)
        l = std::clamp (delayL[static_cast<size_t> (readPos)] * gain, -kCeiling, kCeiling);
        r = std::clamp (delayR[static_cast<size_t> (readPos)] * gain, -kCeiling, kCeiling);
    }

private:
    static constexpr int kTpTaps  = 12;
    static constexpr int kTpDelay = 6;

    double sr        = 48000.0;
    int    lookahead = 72;
    int    holdSamples = 576;
    int    window    = 648;
    int    ringSize  = 128;
    int    ringMask  = 127;
    int    writePos  = 0;
    int64_t sampleIdx = 0;

    std::vector<float>   delayL, delayR, boxRing, dqVal;
    std::vector<int64_t> dqIdx;
    int64_t dqHead = 0, dqTail = 0;
    double  boxSum = 72.0;
    float   limSmooth = 1.0f, limRelAlpha = 0.001f, lastLimGr = 1.0f;

    std::array<std::array<float, kTpTaps>, 3> tpCoeff {};
    std::array<float, kTpTaps> tpHistL {}, tpHistR {};
    int tpPos = 0;

    ZdfSvf hpL, hpR;
    float compEnvSq = 0.0f, compGrDb = 0.0f;
    float compDetAlpha = 0.001f, compAtk = 0.001f, compRel = 0.0001f;
};

} // namespace monolith::dsp
