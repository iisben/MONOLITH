#pragma once

#include <algorithm>
#include <array>
#include <cmath>
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

[[nodiscard]] inline float fastTanh (float x) noexcept
{
    if (x > 4.5f)  return 1.0f;
    if (x < -4.5f) return -1.0f;
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
        // Map unsigned 32-bit integer to [-1, 1]
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

        // writePos points to next write slot; most recent sample is at writePos - 1
        const size_t idx0 = (writePos - 1 - intDelay + 1) & mask;
        const size_t idx1 = (writePos - 1 - intDelay)     & mask;
        const size_t idx2 = (writePos - 1 - intDelay - 1) & mask;
        const size_t idx3 = (writePos - 1 - intDelay - 2) & mask;

        const float y0 = buffer[idx0];
        const float y1 = buffer[idx1];
        const float y2 = buffer[idx2];
        const float y3 = buffer[idx3];

        // Catmull-Rom spline coefficients
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
    Shared, read-only, band-limited Liljencrants-Fant (LF) glottal flow
    derivative wavetable bank. Precomputed across 8 Tension/Effort slices and
    10 mip-map harmonic tiers for alias-free synthesis from 44.1 kHz to 192 kHz.
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

    /** Sample band-limited LF glottal waveform with bilinear mip/tension/phase interpolation. */
    [[nodiscard]] inline float sample (float phase01, float tension01,
                                       float freqHz, float sampleRate) const noexcept
    {
        const float clampedFreq = std::max (freqHz, 12.0f);
        const float maxAllowedHarmonic = (0.45f * sampleRate) / clampedFreq;

        int mip = kNumMipLevels - 1;
        for (int m = 0; m < kNumMipLevels; ++m)
        {
            if (static_cast<float> (kMipMaxHarmonic[static_cast<size_t> (m)]) <= maxAllowedHarmonic)
            {
                mip = m;
                break;
            }
        }

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

        const auto& row0 = tables[static_cast<size_t> (t0)][static_cast<size_t> (mip)];
        const auto& row1 = tables[static_cast<size_t> (t1)][static_cast<size_t> (mip)];

        const float s0 = row0[static_cast<size_t> (i0)]
                       + frac * (row0[static_cast<size_t> (i0 + 1)] - row0[static_cast<size_t> (i0)]);
        const float s1 = row1[static_cast<size_t> (i0)]
                       + frac * (row1[static_cast<size_t> (i0 + 1)] - row1[static_cast<size_t> (i0)]);

        return s0 + tFrac * (s1 - s0);
    }

    /** Returns [0, 1] glottal-open envelope pulse used to modulate aspirated breath noise. */
    [[nodiscard]] inline float glottalOpenGate (float phase01, float tension01) const noexcept
    {
        const float t  = std::clamp (tension01, 0.0f, 1.0f);
        const float te = 0.72f - 0.30f * t; // Open quotient shrinks as vocal tension rises
        if (phase01 >= te || phase01 < 0.0f)
            return 0.08f;

        const float norm = phase01 / std::max (te, 0.05f);
        return 0.08f + 0.92f * std::sin (kPi * norm);
    }

private:
    static constexpr std::array<int, kNumMipLevels> kMipMaxHarmonic {
        256, 128, 64, 32, 16, 8, 4, 2, 1, 1
    };

    using WaveRow = std::array<float, kTableSize + 1>;
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

            // Continuous LF glottal derivative parameters morphing from airy falsetto (0) to fortissimo (1)
            const double te = 0.72 - 0.30 * tension; // Glottal closure instant
            const double tp = 0.44 - 0.16 * tension; // Peak flow instant
            const double ta = 0.065 - 0.056 * tension; // Return phase time constant (sharpness)
            const double alpha = 1.2 + 3.4 * tension;  // Growth exponent during open phase

            // Generate raw continuous LF flow derivative waveform
            double mean = 0.0;
            for (int n = 0; n < kRawN; ++n)
            {
                const double t = static_cast<double> (n) / static_cast<double> (kRawN);
                double val = 0.0;
                if (t < te)
                {
                    // Exponentially growing half-sine opening -> steep negative closure spike
                    val = std::exp (alpha * (t - te)) * std::sin (3.14159265358979323846 * t / tp);
                }
                else
                {
                    // Exponential return from negative excitation peak E_e back toward zero
                    const double eAtTe = std::exp (0.0) * std::sin (3.14159265358979323846 * te / tp);
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

            // Enforce exact zero DC mean
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

            // Compute DFT harmonic coefficients (1..256)
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

            // Reconstruct each mip level using band-limited additive synthesis
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
                        // Gentle raised-cosine rolloff on top 2 harmonics to prevent Gibbs ringing
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
                out[kTableSize] = out[0]; // Guard sample for branchless linear interpolation
            }
        }
    }
};

} // namespace monolith::dsp
