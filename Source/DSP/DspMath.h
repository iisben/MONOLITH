#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <vector>

namespace monolith::dsp
{

inline constexpr float kPi     = 3.14159265358979323846f;
inline constexpr float kTwoPi  = 6.28318530717958647692f;
inline constexpr double kPiD   = 3.14159265358979323846;
inline constexpr double kTwoPiD = 6.28318530717958647692;

[[nodiscard]] inline float sanitize (float x) noexcept
{
    return std::isfinite (x) ? x : 0.0f;
}

[[nodiscard]] inline double finiteOrZero (double x) noexcept
{
    return std::isfinite (x) ? x : 0.0;
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
    // C1 at the knee: d/dx fastTanh(0) = 1, so the shelf meets the linear region.
    const float shaped = kThreshold + kHeadroom * fastTanh (excess);
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

/**
    Smoothstep fade for an additive partial as it approaches Nyquist.
    Unity below 0.34*fs, zero at 0.47*fs, C1 at both ends so glide never clicks.
*/
[[nodiscard]] inline float partialNyquistGain (float freqHz, float sampleRate) noexcept
{
    const float nyStart = 0.34f * sampleRate;
    const float nyEnd   = 0.47f * sampleRate;
    if (freqHz <= nyStart)
        return 1.0f;
    if (freqHz >= nyEnd)
        return 0.0f;

    const float u = (freqHz - nyStart) / (nyEnd - nyStart);
    const float s = u * u * (3.0f - 2.0f * u);
    return 1.0f - s;
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
/**
    Sample-rate-scaled 8 Hz one-pole DC blocker.
    y[n] = x[n] - x[n-1] + r y[n-1], r = exp(-2π·8/fs).
    8 Hz sits inside the 5–10 Hz range so chest and sub-octave energy is kept
    while saturation / feedback offset is still removed. State is double so the
    pole (r ≈ 0.999) does not quantise the low-frequency subtraction.
*/
class DCBlocker
{
public:
    void prepare (double sampleRate) noexcept
    {
        const double sr = std::max (8000.0, sampleRate);
        r = std::exp (-kTwoPiD * 8.0 / sr);
        reset();
    }

    void reset() noexcept
    {
        x1 = 0.0;
        y1 = 0.0;
    }

    [[nodiscard]] inline float process (float x) noexcept
    {
        const double xd = static_cast<double> (x);
        const double y  = xd - x1 + r * y1;
        x1 = xd;
        y1 = finiteOrZero (y);
        return static_cast<float> (y1);
    }

private:
    double r  = 0.999;
    double x1 = 0.0;
    double y1 = 0.0;
};

// ============================================================================
/**
    Double-precision quadrature oscillator.
    Advance is an exact complex rotation by a prepare-time cis(ω); no per-sample
    transcendental, and the amplitude is renormalised occasionally so roundoff
    cannot drift. Output is continuous across 2π (unlike fmod-then-multiply).
*/
struct QuadratureLfo
{
    double s = 0.0;
    double c = 1.0;
    double sInc = 0.0;
    double cInc = 1.0;
    int norm = 0;

    void setHz (double hz, double sampleRate) noexcept
    {
        const double w = kTwoPiD * hz / std::max (8000.0, sampleRate);
        sInc = std::sin (w);
        cInc = std::cos (w);
    }

    void setPhaseRad (double phase) noexcept
    {
        s = std::sin (phase);
        c = std::cos (phase);
        norm = 0;
    }

    inline void tick() noexcept
    {
        const double ns = s * cInc + c * sInc;
        const double nc = c * cInc - s * sInc;
        s = ns;
        c = nc;

        if (++norm >= 2048)
        {
            norm = 0;
            const double mag = std::sqrt (s * s + c * c);
            if (mag > 1.0e-18)
            {
                s /= mag;
                c /= mag;
            }
        }
    }

    [[nodiscard]] inline float sinOut() const noexcept { return static_cast<float> (s); }
    [[nodiscard]] inline float cosOut() const noexcept { return static_cast<float> (c); }
};

// ============================================================================
/**
    4096-bin sine LUT with 4-point Catmull-Rom interpolation.
    Replaces libm sin/cos on per-sample additive partials and window functions.
    Knot error is zero; inter-bin error is far below -120 dB.
*/
class FastTrig
{
public:
    static constexpr int kSize = 4096;

    static const FastTrig& instance()
    {
        static const FastTrig table;
        return table;
    }

    /** Sine of a phase expressed in cycles (1.0 == 2π). */
    [[nodiscard]] inline float sinTurns (float turns) const noexcept
    {
        float p = turns - std::floor (turns);
        if (p < 0.0f)
            p += 1.0f;

        const float pos = p * static_cast<float> (kSize);
        const int i0    = static_cast<int> (pos) & (kSize - 1);
        const float f   = pos - std::floor (pos);

        const int im = (i0 == 0) ? (kSize - 1) : (i0 - 1);
        const float ym = table[static_cast<size_t> (im)];
        const float y0 = table[static_cast<size_t> (i0)];
        const float y1 = table[static_cast<size_t> (i0 + 1)];
        const float y2 = table[static_cast<size_t> (i0 + 2)];

        const float c0 = y0;
        const float c1 = 0.5f * (y1 - ym);
        const float c2 = ym - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - ym) + 1.5f * (y0 - y1);
        return ((c3 * f + c2) * f + c1) * f + c0;
    }

    [[nodiscard]] inline float cosTurns (float turns) const noexcept
    {
        return sinTurns (turns + 0.25f);
    }

private:
    std::array<float, kSize + 3> table {};

    FastTrig()
    {
        for (int i = 0; i < kSize; ++i)
            table[static_cast<size_t> (i)] = static_cast<float> (
                std::sin (kTwoPiD * static_cast<double> (i) / static_cast<double> (kSize)));

        table[kSize]     = table[0];
        table[kSize + 1] = table[1];
        table[kSize + 2] = table[2];
    }
};

// ============================================================================
/**
    Deterministic xorshift32 + Paul Kellet 3-pole pink noise.
    The published poles (0.99765 / 0.963 / 0.570) are a 44.1 kHz design.
    prepare() warps each pole to the same corner frequency at the session rate
    and rescales the feed gain so every stage keeps its original DC gain.
*/
class PinkNoiseGen
{
public:
    void prepare (double sampleRate) noexcept
    {
        const double sr = std::max (8000.0, sampleRate);
        auto warp = [sr] (double pRef, double gRef, double& pole, double& gain)
        {
            const double fc    = -std::log (pRef) * 44100.0 / kTwoPiD;
            const double fcUse = std::min (fc, 0.45 * sr);
            pole = std::exp (-kTwoPiD * fcUse / sr);
            const double dc = gRef / (1.0 - pRef);
            gain = dc * (1.0 - pole);
        };

        warp (0.99765, 0.0990460, p0, g0);
        warp (0.96300, 0.2965164, p1, g1);
        warp (0.57000, 1.0526913, p2, g2);
    }

    void seed (uint32_t s) noexcept
    {
        state = (s != 0u) ? s : 0xA511E9B3u;
        b0 = b1 = b2 = 0.0;
    }

    void resetFilter() noexcept
    {
        b0 = b1 = b2 = 0.0;
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
        const double white = static_cast<double> (nextWhite());
        b0 = p0 * b0 + white * g0;
        b1 = p1 * b1 + white * g1;
        b2 = p2 * b2 + white * g2;
        return static_cast<float> ((b0 + b1 + b2 + white * 0.1848) * 0.22);
    }

private:
    uint32_t state = 0x12345678u;
    double p0 = 0.99765, g0 = 0.0990460;
    double p1 = 0.96300, g1 = 0.2965164;
    double p2 = 0.57000, g2 = 1.0526913;
    double b0 = 0.0, b1 = 0.0, b2 = 0.0;
};

// ============================================================================
/**
    Zero-delay-feedback (TPT) state-variable filter.
    Coefficients and integrators are double; audio crosses the boundary as float.
    g = tan(π·fc/fs), k = 1/Q, trapezoidal integrators — stable for any fc in
    (0, 0.44·fs) and any Q, including audio-rate cutoff modulation.
    Bandpass is scaled by k so the peak is 0 dB at fc (parallel formant bank).
*/
class ZdfSvf
{
public:
    void reset() noexcept
    {
        ic1eq = 0.0;
        ic2eq = 0.0;
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

        const double fc = static_cast<double> (clampedFc);
        const double fs = static_cast<double> (sr);
        const double qD = static_cast<double> (clampedQ);
        g  = std::tan (kPiD * fc / fs);
        k  = 1.0 / qD;
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    /** Unit-peak bandpass (0 dB at fc regardless of Q). */
    [[nodiscard]] inline float processBandpass (float v0) noexcept
    {
        double v1 = 0.0, v2 = 0.0;
        tick (static_cast<double> (v0), v1, v2);
        return static_cast<float> (v1 * k);
    }

    [[nodiscard]] inline float processLowpass (float v0) noexcept
    {
        double v1 = 0.0, v2 = 0.0;
        tick (static_cast<double> (v0), v1, v2);
        return static_cast<float> (v2);
    }

    [[nodiscard]] inline float processHighpass (float v0) noexcept
    {
        const double in = static_cast<double> (v0);
        double v1 = 0.0, v2 = 0.0;
        tick (in, v1, v2);
        return static_cast<float> (in - k * v1 - v2);
    }

private:
    inline void tick (double v0, double& v1, double& v2) noexcept
    {
        const double v3 = v0 - ic2eq;
        v1 = a1 * ic1eq + a2 * v3;
        v2 = ic2eq + a2 * ic1eq + a3 * v3;
        ic1eq = finiteOrZero (2.0 * v1 - ic1eq);
        ic2eq = finiteOrZero (2.0 * v2 - ic2eq);
    }

    double g = 0.1, k = 1.0, a1 = 1.0, a2 = 0.0, a3 = 0.0;
    double ic1eq = 0.0, ic2eq = 0.0;
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

        const double y0 = buffer[idx0];
        const double y1 = buffer[idx1];
        const double y2 = buffer[idx2];
        const double y3 = buffer[idx3];
        const double f  = static_cast<double> (frac);

        // 4-point Catmull-Rom / Hermite. Exact at the integer knots (c0 = y1, sum@1 = y2).
        const double c0 = y1;
        const double c1 = 0.5 * (y2 - y0);
        const double c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
        const double c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);

        return static_cast<float> (((c3 * f + c2) * f + c1) * f + c0);
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
    Shared, read-only, band-limited Liljencrants-Fant (LF) glottal flow
    derivative wavetable bank. Precomputed across 8 Tension/Effort slices and
    10 mip-map harmonic tiers. Phase is 4-point Catmull-Rom; tension and mip
    crossfade linearly (zero aliasing and zero mip-switch clicks).
*/
class LfGlottalTables
{
public:
    static constexpr int kNumTensionSlices = 8;
    static constexpr int kNumMipLevels     = 10;
    static constexpr int kTableSize        = 1024;

    static const LfGlottalTables& instance()
    {
        static const LfGlottalTables singleton;
        return singleton;
    }

    /** Band-limited LF glottal sample: cubic phase, linear tension and mip. */
    [[nodiscard]] inline float sample (float phase01, float tension01,
                                       float freqHz, float sampleRate) const noexcept
    {
        const float clampedFreq = std::max (freqHz, 12.0f);
        // baseH = 0.23 * sr / freq guarantees even the brighter mip0 stays < 0.46 * sr (alias-free)
        // while crossfading continuously across octave tiers (zero vibrato/glide clicks).
        const float baseH = (0.23f * sampleRate) / clampedFreq;
        const float mipFloat = std::clamp (8.0f - fastLog2 (std::max (baseH, 1.0f)),
                                           0.0f,
                                           static_cast<float> (kNumMipLevels - 2));
        const int   m0    = static_cast<int> (mipFloat);
        const int   m1    = m0 + 1;
        const float mFrac = mipFloat - static_cast<float> (m0);

        const float tScaled = std::clamp (tension01, 0.0f, 1.0f)
                            * static_cast<float> (kNumTensionSlices - 1);
        const int   t0      = std::min (static_cast<int> (tScaled), kNumTensionSlices - 2);
        const int   t1      = t0 + 1;
        const float tFrac   = tScaled - static_cast<float> (t0);

        float p = phase01 - std::floor (phase01);
        if (p < 0.0f) p += 1.0f;

        const float pos  = p * static_cast<float> (kTableSize);
        const int   i0   = static_cast<int> (pos) & (kTableSize - 1);
        const float frac = pos - std::floor (pos);

        const auto& r00 = tables[static_cast<size_t> (t0)][static_cast<size_t> (m0)];
        const auto& r01 = tables[static_cast<size_t> (t0)][static_cast<size_t> (m1)];
        const auto& r10 = tables[static_cast<size_t> (t1)][static_cast<size_t> (m0)];
        const auto& r11 = tables[static_cast<size_t> (t1)][static_cast<size_t> (m1)];

        const auto hermite = [frac] (const WaveRow& row, int bin) noexcept -> float
        {
            const int im = (bin == 0) ? (kTableSize - 1) : (bin - 1);
            const float ym = row[static_cast<size_t> (im)];
            const float y0 = row[static_cast<size_t> (bin)];
            const float y1 = row[static_cast<size_t> (bin + 1)];
            const float y2 = row[static_cast<size_t> (bin + 2)];
            const float c0 = y0;
            const float c1 = 0.5f * (y1 - ym);
            const float c2 = ym - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
            const float c3 = 0.5f * (y2 - ym) + 1.5f * (y0 - y1);
            return ((c3 * frac + c2) * frac + c1) * frac + c0;
        };

        const float s00 = hermite (r00, i0);
        const float s01 = hermite (r01, i0);
        const float s10 = hermite (r10, i0);
        const float s11 = hermite (r11, i0);

        const float s0 = s00 + mFrac * (s01 - s00);
        const float s1 = s10 + mFrac * (s11 - s10);
        return s0 + tFrac * (s1 - s0);
    }

    /** Returns [0, 1] glottal-open envelope pulse used to modulate aspirated breath noise. */
    [[nodiscard]] inline float glottalOpenGate (float phase01, float tension01) const noexcept
    {
        const float t  = std::clamp (tension01, 0.0f, 1.0f);
        const float te = 0.72f - 0.30f * t;
        if (phase01 >= te || phase01 < 0.0f)
            return 0.08f;

        const float norm = phase01 / std::max (te, 0.05f);
        // Smooth raised parabolic pulse matching sin(pi * norm) with zero trig overhead
        return 0.08f + 0.92f * (4.0f * norm * (1.0f - norm));
    }

private:
    static constexpr std::array<int, kNumMipLevels> kMipMaxHarmonic {
        256, 128, 64, 32, 16, 8, 4, 2, 1, 1
    };

    using WaveRow = std::array<float, kTableSize + 3>;
    std::array<std::array<WaveRow, kNumMipLevels>, kNumTensionSlices> tables {};

    LfGlottalTables()
    {
        constexpr int kRawN = 2048;
        std::array<double, kRawN> raw {};
        std::array<double, 257> cosCoeff {};
        std::array<double, 257> sinCoeff {};

        for (int slice = 0; slice < kNumTensionSlices; ++slice)
        {
            const double tension = static_cast<double> (slice) / static_cast<double> (kNumTensionSlices - 1);

            const double te = 0.72 - 0.30 * tension;
            const double tp = 0.44 - 0.16 * tension;
            const double ta = 0.065 - 0.056 * tension;
            const double alpha = 1.2 + 3.4 * tension;

            double mean = 0.0;
            for (int n = 0; n < kRawN; ++n)
            {
                const double t = static_cast<double> (n) / static_cast<double> (kRawN);
                double val = 0.0;
                if (t < te)
                {
                    val = std::exp (alpha * (t - te)) * std::sin (3.14159265358979323846 * t / tp);
                }
                else
                {
                    const double eAtTe = std::sin (3.14159265358979323846 * te / tp);
                    const double dt = t - te;
                    const double rem = std::max (1.0 - te, 1.0e-4);
                    const double decay = std::exp (-dt / std::max (ta, 1.0e-4));
                    const double endDecay = std::exp (-rem / std::max (ta, 1.0e-4));
                    val = eAtTe * (decay - endDecay) / std::max (1.0 - endDecay, 1.0e-6);
                }
                raw[static_cast<size_t> (n)] = val;
                mean += val;
            }
            mean /= static_cast<double> (kRawN);

            double rms = 0.0;
            for (int n = 0; n < kRawN; ++n)
            {
                raw[static_cast<size_t> (n)] -= mean;
                rms += raw[static_cast<size_t> (n)] * raw[static_cast<size_t> (n)];
            }
            rms = std::sqrt (rms / static_cast<double> (kRawN));
            const double normScale = (rms > 1.0e-9) ? (0.65 / rms) : 1.0;
            for (int n = 0; n < kRawN; ++n)
                raw[static_cast<size_t> (n)] *= normScale;

            for (int h = 1; h <= 256; ++h)
            {
                double c = 0.0;
                double s = 0.0;
                const double w = (2.0 * 3.14159265358979323846 * static_cast<double> (h)) / static_cast<double> (kRawN);
                for (int n = 0; n < kRawN; ++n)
                {
                    const double ang = w * static_cast<double> (n);
                    c += raw[static_cast<size_t> (n)] * std::cos (ang);
                    s += raw[static_cast<size_t> (n)] * std::sin (ang);
                }
                cosCoeff[static_cast<size_t> (h)] = (2.0 / static_cast<double> (kRawN)) * c;
                sinCoeff[static_cast<size_t> (h)] = (2.0 / static_cast<double> (kRawN)) * s;
            }

            for (int mip = 0; mip < kNumMipLevels; ++mip)
            {
                const int maxH = kMipMaxHarmonic[static_cast<size_t> (mip)];
                auto& out = tables[static_cast<size_t> (slice)][static_cast<size_t> (mip)];
                for (int i = 0; i < kTableSize; ++i)
                {
                    const double phase = (2.0 * 3.14159265358979323846 * static_cast<double> (i))
                                       / static_cast<double> (kTableSize);
                    double acc = 0.0;
                    for (int h = 1; h <= maxH; ++h)
                    {
                        double window = 1.0;
                        if (maxH >= 4 && h > maxH - 2)
                        {
                            const double u = static_cast<double> (h - (maxH - 2)) / 3.0;
                            window = 0.5 * (1.0 + std::cos (3.14159265358979323846 * u));
                        }
                        const double ang = static_cast<double> (h) * phase;
                        acc += window * (cosCoeff[static_cast<size_t> (h)] * std::cos (ang)
                                       + sinCoeff[static_cast<size_t> (h)] * std::sin (ang));
                    }
                    out[static_cast<size_t> (i)] = static_cast<float> (acc);
                }
                out[kTableSize]     = out[0];
                out[kTableSize + 1] = out[1];
                out[kTableSize + 2] = out[2];
            }
        }
    }
};

} // namespace monolith::dsp
