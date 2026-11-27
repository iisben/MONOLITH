#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

namespace monolith::dsp
{

inline constexpr float kPi    = 3.14159265358979323846f;
inline constexpr float kTwoPi = 6.28318530717958647692f;

[[nodiscard]] inline float sanitize (float x) noexcept
{
    return std::isfinite (x) ? x : 0.0f;
}

/**
    C1-continuous Padé [3/2] tanh approximation.
    Exact 0 slope and value +/-1.0 at x = +/-3.0 (zero overshoot, zero clamp discontinuity).
*/
[[nodiscard]] inline float fastTanh (float x) noexcept
{
    if (x >= 3.0f)  return 1.0f;
    if (x <= -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

/**
    Full-range sine of a phase in TURNS (1 turn = 2 pi): range-reduced to [-1/4, 1/4] turn, then an
    odd Taylor polynomial to x^11 (|truncation| < 6e-8). Validated against std::sin: max |err| < 2.5e-7
    over 1e7 phases (float rounding bound) -> << 0.01 dB on harmonic levels, << 0.5 cent on vibrato.
*/
[[nodiscard]] inline float sinTurns (float t) noexcept
{
    t -= std::floor (t + 0.5f);                                    // [-0.5, 0.5)
    const float a = (t > 0.25f) ? 0.5f - t : ((t < -0.25f) ? -0.5f - t : t);
    const float x  = a * 6.28318530717958647692f;
    const float x2 = x * x;
    return x * (1.0f + x2 * (-1.6666667e-1f + x2 * (8.3333333e-3f + x2 * (-1.9841270e-4f
                     + x2 * (2.7557319e-6f + x2 * (-2.5052108e-8f))))));
}

/** sin / cos of an angle in radians via sinTurns (no libm call). */
[[nodiscard]] inline float fastSin (float rad) noexcept { return sinTurns (rad * 0.15915494309189533577f); }
[[nodiscard]] inline float fastCos (float rad) noexcept { return sinTurns (rad * 0.15915494309189533577f + 0.25f); }

/** Transparent safety limiter: linear up to 0.75, soft asymptotic ceiling at 0.985. */
[[nodiscard]] inline float softSafetyLimit (float x) noexcept
{
    x = sanitize (x);
    const float ax = std::abs (x);
    constexpr float kThreshold = 0.75f;
    constexpr float kHeadroom  = 0.235f; // 0.75 + 0.235 = 0.985 ceiling
    if (ax <= kThreshold)
        return x;

    const float excess = (ax - kThreshold) / kHeadroom;
    const float shaped = kThreshold + kHeadroom * std::tanh (excess);
    return (x >= 0.0f) ? shaped : -shaped;
}

[[nodiscard]] inline float midiToFreq (float midiNote) noexcept
{
    return 440.0f * std::exp2 ((midiNote - 69.0f) / 12.0f);
}

[[nodiscard]] inline float semitonesToRatio (float semitones) noexcept
{
    return std::exp2 (semitones / 12.0f);
}

/** Fast, smooth pitch-ratio polynomial for small detune/vibrato offsets in cents. */
[[nodiscard]] inline float fastCentsToRatio (float cents) noexcept
{
    if (std::abs (cents) > 150.0f)
        return std::exp2 (cents / 1200.0f);

    const float x = cents * 5.776226504666211e-4f; // ln(2) / 1200
    return 1.0f + x * (1.0f + x * (0.5f + x * 0.16666667f));
}

/** C0-continuous IEEE-754 fast log2 for positive normal floats (zero branch, exact at 2^k). */
[[nodiscard]] inline float fastLog2 (float x) noexcept
{
    const float safe = std::max (x, 1.0e-12f);
    const uint32_t bits = std::bit_cast<uint32_t> (safe);
    const float expPart = static_cast<float> (static_cast<int32_t> ((bits >> 23) & 0xFFu) - 127);
    const float mant    = std::bit_cast<float> ((bits & 0x007FFFFFu) | 0x3F800000u) - 1.0f;
    return expPart + mant * (1.44269504f - 0.44269504f * mant);
}

// ============================================================================
/** Sample-rate-scaled 15 Hz DC blocking filter. */
class DCBlocker
{
public:
    void prepare (double sampleRate) noexcept
    {
        const double sr = std::max (8000.0, sampleRate);
        r = static_cast<float> (std::exp (-2.0 * 3.14159265358979323846 * 15.0 / sr));
        reset();
    }

    void reset() noexcept
    {
        x1 = 0.0f;
        y1 = 0.0f;
    }

    [[nodiscard]] inline float process (float x) noexcept
    {
        const float y = x - x1 + r * y1;
        x1 = x;
        y1 = sanitize (y);
        return y1;
    }

private:
    float r  = 0.998f;
    float x1 = 0.0f;
    float y1 = 0.0f;
};

// ============================================================================
/** 64-bit one-pole/one-zero DC blocker with rate-scaled corner (default ~5 Hz). */
class DCBlocker64
{
public:
    void prepare (double sampleRate, double cornerHz = 5.0) noexcept
    {
        const double sr = std::max (8000.0, sampleRate);
        r = std::exp (-2.0 * 3.14159265358979323846 * cornerHz / sr);
        reset();
    }

    void reset() noexcept
    {
        x1 = 0.0;
        y1 = 0.0;
    }

    [[nodiscard]] inline double process (double x) noexcept
    {
        const double y = x - x1 + r * y1;
        x1 = x;
        y1 = std::isfinite (y) ? y : 0.0;
        return y1;
    }

private:
    double r  = 0.9993;
    double x1 = 0.0;
    double y1 = 0.0;
};

// ============================================================================
/** Real-time deterministic xorshift32 PRNG + Paul Kellet 3-stage pink noise generator. */
class PinkNoiseGen
{
public:
    void seed (uint32_t s) noexcept
    {
        state = (s != 0u) ? s : 0xA511E9B3u;
        b0 = b1 = b2 = 0.0f;
    }

    void resetFilter() noexcept
    {
        b0 = b1 = b2 = 0.0f;
    }

    [[nodiscard]] inline float nextWhite() noexcept
    {
        uint32_t x = state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state = x;
        return static_cast<float> (static_cast<int32_t> (x)) * 4.656612873e-10f;
    }

    [[nodiscard]] inline float nextPink() noexcept
    {
        const float white = nextWhite();
        b0 = 0.99765f * b0 + white * 0.0990460f;
        b1 = 0.96300f * b1 + white * 0.2965164f;
        b2 = 0.57000f * b2 + white * 1.0526913f;
        return (b0 + b1 + b2 + white * 0.1848f) * 0.22f;
    }

private:
    uint32_t state = 0x12345678u;
    float b0 = 0.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;
};

// ============================================================================
/**
    Zero-Delay-Feedback (TPT) State Variable Filter with coefficient caching.
    Supports unit-peak Bandpass (ideal for parallel vocal formants), Lowpass,
    and Highpass topologies, unconditionally stable from 44.1 kHz to 192 kHz.
*/
class ZdfSvf
{
public:
    void reset() noexcept
    {
        ic1eq = 0.0f;
        ic2eq = 0.0f;
    }

    void invalidateCache() noexcept
    {
        lastCutoff = -1.0f;
        lastQ      = -1.0f;
        lastSr     = -1.0f;
    }

    inline void setParameters (float cutoffHz, float q, float sampleRate) noexcept
    {
        const float sr = std::max (8000.0f, sampleRate);
        const float clampedFc = std::clamp (cutoffHz, 20.0f, 0.44f * sr);
        const float clampedQ  = std::clamp (q, 0.25f, 22.0f);

        if (std::abs (clampedFc - lastCutoff) < 1.0e-3f
            && std::abs (clampedQ - lastQ) < 1.0e-4f
            && std::abs (sr - lastSr) < 1.0f)
        {
            return;
        }

        lastCutoff = clampedFc;
        lastQ      = clampedQ;
        lastSr     = sr;

        g  = std::tan (kPi * clampedFc / sr);
        k  = 1.0f / clampedQ;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    /** Returns unit-peak bandpass output (0 dB at fc regardless of Q). */
    [[nodiscard]] inline float processBandpass (float v0) noexcept
    {
        const float v3 = v0 - ic2eq;
        const float v1 = a1 * ic1eq + a2 * v3;
        const float v2 = ic2eq + a2 * ic1eq + a3 * v3;
        ic1eq = sanitize (2.0f * v1 - ic1eq);
        ic2eq = sanitize (2.0f * v2 - ic2eq);
        return v1 * k;
    }

    [[nodiscard]] inline float processLowpass (float v0) noexcept
    {
        const float v3 = v0 - ic2eq;
        const float v1 = a1 * ic1eq + a2 * v3;
        const float v2 = ic2eq + a2 * ic1eq + a3 * v3;
        ic1eq = sanitize (2.0f * v1 - ic1eq);
        ic2eq = sanitize (2.0f * v2 - ic2eq);
        return v2;
    }

    [[nodiscard]] inline float processHighpass (float v0) noexcept
    {
        const float v3 = v0 - ic2eq;
        const float v1 = a1 * ic1eq + a2 * v3;
        const float v2 = ic2eq + a2 * ic1eq + a3 * v3;
        ic1eq = sanitize (2.0f * v1 - ic1eq);
        ic2eq = sanitize (2.0f * v2 - ic2eq);
        return v0 - k * v1 - v2;
    }

private:
    float g = 0.1f, k = 1.0f, a1 = 1.0f, a2 = 0.0f, a3 = 0.0f;
    float ic1eq = 0.0f, ic2eq = 0.0f;
    float lastCutoff = -1.0f, lastQ = -1.0f, lastSr = -1.0f;
};

// ============================================================================
/** Pre-allocated power-of-2 ring buffer with 4-point Catmull-Rom cubic interpolation. */
class CubicDelayLine
{
public:
    void prepare (size_t minCapacitySamples)
    {
        size_t cap = 16;
        while (cap < minCapacitySamples + 8)
            cap <<= 1;
        buffer.assign (cap, 0.0f);
        mask = cap - 1;
        writePos = 0;
    }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
    }

    inline void push (float sample) noexcept
    {
        buffer[writePos] = sanitize (sample);
        writePos = (writePos + 1) & mask;
    }

    [[nodiscard]] inline float readCubic (float delaySamples) const noexcept
    {
        const float maxDelay = static_cast<float> (mask - 4);
        const float d = std::clamp (delaySamples, 1.0f, maxDelay);

        const size_t intDelay = static_cast<size_t> (d);
        const float  frac     = d - static_cast<float> (intDelay);

        const size_t idx0 = (writePos - 1 - intDelay + 1) & mask;
        const size_t idx1 = (writePos - 1 - intDelay)     & mask;
        const size_t idx2 = (writePos - 1 - intDelay - 1) & mask;
        const size_t idx3 = (writePos - 1 - intDelay - 2) & mask;

        const float y0 = buffer[idx0];
        const float y1 = buffer[idx1];
        const float y2 = buffer[idx2];
        const float y3 = buffer[idx3];

        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    [[nodiscard]] inline float readLinear (float delaySamples) const noexcept
    {
        const float maxDelay = static_cast<float> (mask - 2);
        const float d = std::clamp (delaySamples, 1.0f, maxDelay);
        const size_t intDelay = static_cast<size_t> (d);
        const float  frac     = d - static_cast<float> (intDelay);
        const size_t idx1 = (writePos - 1 - intDelay) & mask;
        const size_t idx2 = (idx1 - 1) & mask;
        return buffer[idx1] + frac * (buffer[idx2] - buffer[idx1]);
    }

private:
    std::vector<float> buffer;
    size_t mask     = 0;
    size_t writePos = 0;
};

// ============================================================================
/**
    Shared, read-only, band-limited Liljencrants-Fant (LF, Rd-parametrized) glottal
    flow-derivative wavetable bank.

    - Harmonic amplitudes come from the CLOSED-FORM Fourier series of the LF model
      (open-phase exponential sinusoid + exponential return phase, epsilon/alpha solved
      for return-phase continuity and zero net flow) -> analytical spectral tilt set by Ta,
      open quotient set by Te, no time-domain sampling aliasing.
    - Tension/Effort maps to Rd (1.6 lax -> 0.3 pressed): 12 slices across OQ & tilt.
    - 3 mip tables per octave (256 -> 1 harmonics). Mip selection keeps the brighter table
      <= 0.466 fs and the darker table <= 0.37 fs (no component ever exceeds Nyquist).
    - 4-point cubic Hermite phase interpolation, bilinear across tension x mip.
    - Companion band-limited glottal AIRFLOW table (integral of the derivative,
      normalized 0..1) drives open-phase-coupled aspiration noise.
*/
class LfGlottalTables
{
public:
    static constexpr int kNumTensionSlices = 12;
    static constexpr int kMipsPerOctave    = 3;
    static constexpr int kNumMipLevels     = 25;
    static constexpr int kMaxHarmonics     = 256;
    static constexpr int kTableSize        = 2048;
    static constexpr int kRowStride        = kTableSize + 3; // [x(N-1), x(0..N-1), x(0), x(1)]
    static constexpr int kFlowSize         = 256;
    static constexpr float kDarkEdge       = 0.37f;          // fraction of fs for the darker mip
    static constexpr int kCalibLoH         = 4;              // legacy-loudness calibration band (formant region,
    static constexpr int kCalibHiH         = 32;             //  excludes h1-h3 which formants barely pass)

    static const LfGlottalTables& instance()
    {
        static const LfGlottalTables singleton;
        return singleton;
    }

    /** Tension/Effort 0..1 -> LF Rd shape parameter. */
    [[nodiscard]] static double tensionToRd (double tension01) noexcept
    {
        return 1.6 - 1.3 * std::pow (std::clamp (tension01, 0.0, 1.0), 1.2);
    }

    /** Band-limited LF glottal flow derivative (RMS-normalized ~0.65). */
    [[nodiscard]] inline float sample (float phase01, float tension01,
                                       float freqHz, float sampleRate) const noexcept
    {
        const float f  = std::max (freqHz, 12.0f);
        const float sr = std::max (sampleRate, 8000.0f);
        return sampleLog2 (phase01, tension01, std::log2 (f), std::log2 (sr));
    }

    /**
        Same as sample(), with log2(freq) / log2(sampleRate) supplied by the caller (the swarm derives
        log2 f exactly from MIDI pitch + cents, so the per-singer per-sample log2 disappears).
    */
    [[nodiscard]] inline float sampleLog2 (float phase01, float tension01,
                                           float log2FreqHz, float log2SampleRate) const noexcept
    {
        constexpr float kLog2MaxOverEdge = 9.4344029f; // log2 (kMaxHarmonics / kDarkEdge) = log2 (256 / 0.37)
        static_assert (kMaxHarmonics == 256 && kDarkEdge > 0.3699f && kDarkEdge < 0.3701f, "update kLog2MaxOverEdge");
        const float mipFloat = std::clamp (static_cast<float> (kMipsPerOctave)
                                               * (kLog2MaxOverEdge + std::max (log2FreqHz, 3.5849625f) - log2SampleRate),
                                           0.0f, static_cast<float> (kNumMipLevels - 2));
        const int   m0    = static_cast<int> (mipFloat);
        const float mFrac = mipFloat - static_cast<float> (m0);

        const float tScaled = std::clamp (tension01, 0.0f, 1.0f) * static_cast<float> (kNumTensionSlices - 1);
        const int   t0      = std::min (static_cast<int> (tScaled), kNumTensionSlices - 2);
        const float tFrac   = tScaled - static_cast<float> (t0);

        float p = phase01 - std::floor (phase01);
        const float pos = p * static_cast<float> (kTableSize);
        int i = static_cast<int> (pos);
        const float fr = pos - static_cast<float> (i);
        i &= (kTableSize - 1);

        // Catmull-Rom cubic Hermite weights
        const float fr2 = fr * fr, fr3 = fr2 * fr;
        const float wm1 = -0.5f * fr + fr2 - 0.5f * fr3;
        const float w0  = 1.0f - 2.5f * fr2 + 1.5f * fr3;
        const float w1  = 0.5f * fr + 2.0f * fr2 - 1.5f * fr3;
        const float w2  = -0.5f * fr2 + 0.5f * fr3;

        auto tap = [&] (int t, int m) noexcept
        {
            const float* r = row (t, m) + i;
            return wm1 * r[0] + w0 * r[1] + w1 * r[2] + w2 * r[3];
        };

        const float s00 = tap (t0, m0),     s01 = tap (t0, m0 + 1);
        const float s10 = tap (t0 + 1, m0), s11 = tap (t0 + 1, m0 + 1);
        const float s0 = s00 + mFrac * (s01 - s00);
        const float s1 = s10 + mFrac * (s11 - s10);
        return s0 + tFrac * (s1 - s0);
    }

    /** Normalized [0, 1] band-limited glottal airflow at this phase (drives aspiration). */
    [[nodiscard]] inline float glottalOpenGate (float phase01, float tension01) const noexcept
    {
        float p = phase01 - std::floor (phase01);
        const float pos = p * static_cast<float> (kFlowSize);
        int i = static_cast<int> (pos);
        const float fr = pos - static_cast<float> (i);
        i &= (kFlowSize - 1);

        const float tScaled = std::clamp (tension01, 0.0f, 1.0f) * static_cast<float> (kNumTensionSlices - 1);
        const int   t0      = std::min (static_cast<int> (tScaled), kNumTensionSlices - 2);
        const float tFrac   = tScaled - static_cast<float> (t0);

        const auto& a = flow[static_cast<size_t> (t0)];
        const auto& b = flow[static_cast<size_t> (t0 + 1)];
        const size_t ui = static_cast<size_t> (i);
        const float fa = a[ui] + fr * (a[ui + 1] - a[ui]);
        const float fb = b[ui] + fr * (b[ui + 1] - b[ui]);
        return fa + tFrac * (fb - fa);
    }

    [[nodiscard]] static int mipMaxHarmonic (int mip) noexcept
    {
        const double h = static_cast<double> (kMaxHarmonics)
                       * std::pow (2.0, -static_cast<double> (mip) / static_cast<double> (kMipsPerOctave));
        return std::max (1, static_cast<int> (std::floor (h + 1.0e-9)));
    }

private:
    std::vector<float> tables;
    std::array<std::array<float, kFlowSize + 1>, kNumTensionSlices> flow {};

    [[nodiscard]] inline const float* row (int t, int m) const noexcept
    {
        return tables.data() + static_cast<size_t> (t * kNumMipLevels + m) * static_cast<size_t> (kRowStride);
    }

    struct LfShape
    {
        double te = 0.6, tp = 0.4, ta = 0.03, eps = 30.0, alpha = 2.0, e0 = 1.0, wg = 7.85;
    };

    static LfShape solveLf (double rd) noexcept
    {
        constexpr double pi = 3.14159265358979323846;
        const double rap = (-1.0 + 4.8 * rd) / 100.0;
        const double rkp = (22.4 + 11.8 * rd) / 100.0;
        const double rgp = 0.25 * rkp / ((0.11 * rd / (0.5 + 1.2 * rkp)) - rap);

        LfShape s;
        s.tp = 1.0 / (2.0 * rgp);
        s.te = std::min (s.tp * (1.0 + rkp), 0.97);
        s.ta = std::max (rap, 1.0e-3);
        s.wg = pi / s.tp;

        // Return phase: eps * Ta = 1 - exp(-eps * (Tc - Te)), Tc = 1
        double eps = 1.0 / s.ta;
        for (int it = 0; it < 80; ++it)
            eps = (1.0 - std::exp (-eps * (1.0 - s.te))) / s.ta;
        s.eps = eps;

        // Zero net flow: open-phase area + return-phase area = 0 (Ee normalized to 1)
        const double tc = 1.0 - s.te;
        const double retArea = -(1.0 / (eps * s.ta)) * ((1.0 - std::exp (-eps * tc)) / eps - tc * std::exp (-eps * tc));
        auto net = [&] (double a) noexcept
        {
            const double e0 = -1.0 / (std::exp (a * s.te) * std::sin (s.wg * s.te));
            const double openArea = e0 * (std::exp (a * s.te) * (a * std::sin (s.wg * s.te) - s.wg * std::cos (s.wg * s.te)) + s.wg)
                                  / (a * a + s.wg * s.wg);
            return openArea + retArea;
        };

        double lo = -40.0, hi = -40.0, prev = net (-40.0);
        bool bracketed = false;
        for (double a = -39.75; a <= 400.0; a += 0.25)
        {
            const double v = net (a);
            if ((v > 0.0) != (prev > 0.0)) { lo = a - 0.25; hi = a; bracketed = true; break; }
            prev = v;
        }
        double alpha = 0.0;
        if (bracketed)
        {
            double flo = net (lo);
            for (int it = 0; it < 100; ++it)
            {
                const double mid = 0.5 * (lo + hi);
                const double fm = net (mid);
                if ((fm > 0.0) == (flo > 0.0)) { lo = mid; flo = fm; } else { hi = mid; }
            }
            alpha = 0.5 * (lo + hi);
        }
        s.alpha = alpha;
        s.e0 = -1.0 / (std::exp (alpha * s.te) * std::sin (s.wg * s.te));
        return s;
    }

    /** Closed-form complex Fourier coefficient c_h of the LF derivative (period 1). */
    static std::complex<double> lfCoeff (const LfShape& s, int h) noexcept
    {
        using C = std::complex<double>;
        constexpr double pi = 3.14159265358979323846;
        const double w = 2.0 * pi * static_cast<double> (h);
        const C j (0.0, 1.0);

        auto expInt = [&] (C sv, double T) noexcept -> C
        {
            if (std::abs (sv) < 1.0e-12) return C (T, 0.0);
            return (std::exp (sv * T) - 1.0) / sv;
        };
        const C s1 (s.alpha, s.wg - w);
        const C s2 (s.alpha, -(s.wg + w));
        const C open = (s.e0 / (2.0 * j)) * (expInt (s1, s.te) - expInt (s2, s.te));

        const double k = std::exp (-s.eps * (1.0 - s.te));
        const C eTe = std::exp (-j * w * s.te);
        const C e1  = std::exp (-j * w);
        const C i1  = (eTe - k * e1) / (C (s.eps, w));
        const C i2  = k * (eTe - e1) / (j * w);
        const C ret = -(1.0 / (s.eps * s.ta)) * (i1 - i2);
        return open + ret;
    }

    /** Formant-band (kCalibLoH..kCalibHiH) harmonic power of the legacy MONOLITH pulse at RMS 0.65. */
    static double legacyBandPower (double tension) noexcept
    {
        constexpr double pi = 3.14159265358979323846;
        constexpr int n = 2048;
        std::array<double, n> raw {};
        const double te = 0.72 - 0.30 * tension, tp = 0.44 - 0.16 * tension;
        const double ta = 0.065 - 0.056 * tension, alpha = 1.2 + 3.4 * tension;
        const double eAtTe = std::sin (pi * te / tp);
        const double endDecay = std::exp (-(1.0 - te) / ta);
        double mean = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double t = static_cast<double> (i) / n;
            const double v = (t < te) ? std::exp (alpha * (t - te)) * std::sin (pi * t / tp)
                                      : eAtTe * (std::exp (-(t - te) / ta) - endDecay) / std::max (1.0 - endDecay, 1.0e-6);
            raw[static_cast<size_t> (i)] = v;
            mean += v / n;
        }
        double pw = 0.0;
        for (auto& v : raw) { v -= mean; pw += v * v / n; }
        const double sc2 = (0.65 * 0.65) / std::max (pw, 1.0e-18);
        double band = 0.0;
        for (int h = kCalibLoH; h <= kCalibHiH; ++h)
        {
            double c = 0.0, sn = 0.0;
            for (int i = 0; i < n; ++i)
            {
                const double ang = 2.0 * pi * h * i / n;
                c  += raw[static_cast<size_t> (i)] * std::cos (ang);
                sn += raw[static_cast<size_t> (i)] * std::sin (ang);
            }
            band += 2.0 * (c * c + sn * sn) / (static_cast<double> (n) * n) * sc2;
        }
        return band;
    }

    static void inverseRealFft (std::vector<std::complex<double>>& a)
    {
        // ifft via conj(fft(conj(X))), no 1/N scaling (X holds Fourier-series coefficients)
        for (auto& v : a) v = std::conj (v);
        const size_t n = a.size();
        for (size_t i = 1, jj = 0; i < n; ++i)
        {
            size_t bit = n >> 1;
            for (; (jj & bit) != 0; bit >>= 1) jj ^= bit;
            jj ^= bit;
            if (i < jj) std::swap (a[i], a[jj]);
        }
        for (size_t len = 2; len <= n; len <<= 1)
        {
            const size_t half = len / 2;
            for (size_t kk = 0; kk < half; ++kk)
            {
                const double ang = -2.0 * 3.14159265358979323846 * static_cast<double> (kk) / static_cast<double> (len);
                const std::complex<double> tw (std::cos (ang), std::sin (ang));
                for (size_t i = 0; i < n; i += len)
                {
                    const auto u = a[i + kk];
                    const auto v = a[i + kk + half] * tw;
                    a[i + kk] = u + v;
                    a[i + kk + half] = u - v;
                }
            }
        }
        for (auto& v : a) v = std::conj (v);
    }

    LfGlottalTables()
    {
        constexpr double pi = 3.14159265358979323846;
        tables.assign (static_cast<size_t> (kNumTensionSlices * kNumMipLevels * kRowStride), 0.0f);
        std::vector<std::complex<double>> coeff (static_cast<size_t> (kMaxHarmonics + 1));
        std::vector<std::complex<double>> spec (static_cast<size_t> (kTableSize));

        for (int slice = 0; slice < kNumTensionSlices; ++slice)
        {
            const double tension = static_cast<double> (slice) / static_cast<double> (kNumTensionSlices - 1);
            const LfShape shape = solveLf (tensionToRd (tension));

            double pw = 0.0;
            for (int h = 1; h <= kMaxHarmonics; ++h)
            {
                coeff[static_cast<size_t> (h)] = lfCoeff (shape, h);
                pw += 2.0 * std::norm (coeff[static_cast<size_t> (h)]);
            }
            const double rms = std::sqrt (pw);
            double normScale = (rms > 1.0e-12) ? (0.65 / rms) : 1.0;

            // Loudness calibration: match the formant-band (h 4..32) energy of the legacy MONOLITH
            // glottal pulse at this tension so factory preset gain staging is preserved.
            double bandNew = 0.0;
            for (int h = kCalibLoH; h <= kCalibHiH; ++h)
                bandNew += 2.0 * std::norm (coeff[static_cast<size_t> (h)]) * normScale * normScale;
            const double bandOld = legacyBandPower (tension);
            if (bandNew > 1.0e-15 && bandOld > 1.0e-15)
                normScale *= std::sqrt (bandOld / bandNew);

            for (int mip = 0; mip < kNumMipLevels; ++mip)
            {
                const int maxH = mipMaxHarmonic (mip);
                std::fill (spec.begin(), spec.end(), std::complex<double> (0.0, 0.0));
                for (int h = 1; h <= maxH; ++h)
                {
                    double window = 1.0;
                    if (maxH >= 4 && h > maxH - 2)
                    {
                        const double u = static_cast<double> (h - (maxH - 2)) / 3.0;
                        window = 0.5 * (1.0 + std::cos (pi * u));
                    }
                    const auto c = coeff[static_cast<size_t> (h)] * (window * normScale);
                    spec[static_cast<size_t> (h)] = c;
                    spec[static_cast<size_t> (kTableSize - h)] = std::conj (c);
                }
                inverseRealFft (spec);

                float* out = tables.data() + static_cast<size_t> (slice * kNumMipLevels + mip) * static_cast<size_t> (kRowStride);
                for (int i = 0; i < kTableSize; ++i)
                    out[i + 1] = static_cast<float> (spec[static_cast<size_t> (i)].real());
                out[0]              = out[kTableSize];
                out[kTableSize + 1] = out[1];
                out[kTableSize + 2] = out[2];
            }

            // Band-limited airflow U(phi) = integral of derivative, normalized to [0, 1]
            std::array<double, kFlowSize> u {};
            double uMin = 1.0e30, uMax = -1.0e30;
            for (int n = 0; n < kFlowSize; ++n)
            {
                const double ph = static_cast<double> (n) / static_cast<double> (kFlowSize);
                double acc = 0.0;
                for (int h = 1; h <= kMaxHarmonics; ++h)
                {
                    const double w = 2.0 * pi * static_cast<double> (h);
                    const auto c = coeff[static_cast<size_t> (h)] / std::complex<double> (0.0, w);
                    acc += 2.0 * (c * std::exp (std::complex<double> (0.0, w * ph))).real();
                }
                u[static_cast<size_t> (n)] = acc;
                uMin = std::min (uMin, acc);
                uMax = std::max (uMax, acc);
            }
            auto& fl = flow[static_cast<size_t> (slice)];
            const double span = std::max (uMax - uMin, 1.0e-12);
            for (int n = 0; n < kFlowSize; ++n)
                fl[static_cast<size_t> (n)] = static_cast<float> ((u[static_cast<size_t> (n)] - uMin) / span);
            fl[kFlowSize] = fl[0];
        }
    }
};

} // namespace monolith::dsp
