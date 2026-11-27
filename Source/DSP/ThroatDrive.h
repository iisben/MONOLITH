#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace monolith::dsp
{

/**
    64-bit throat / drive waveshaper with closed-form antiderivatives for ADAA.

    f(x)  = Padé [3/2] tanh  x(27 + x^2) / (27 + 9x^2)  for |x| < 3, sign(x) beyond (C1 at the knee)
          = x/9 + (8/3) x / (x^2 + 3)
    F1(x) = x^2/18 + (4/3) ln(1 + x^2/3)                         (even, F1(0) = 0)
    F2(x) = x^3/54 + (4/3) [x ln(1 + x^2/3) - 2x + 2 sqrt3 atan(x / sqrt3)]   (odd, F2(0) = 0)

    adaa1 / adaa2 fall back to the midpoint value when the divided difference is
    ill-conditioned (|dx| < kEps). kEps balances double cancellation error
    (~2^-52 |F1| / dx) against the midpoint error (~|f''| dx^2 / 24): both < 1e-9.
*/
struct ThroatShaper
{
    static constexpr double kKnee  = 3.0;
    static constexpr double kEps   = 1.0e-5;
    static constexpr double kSqrt3 = 1.7320508075688772935;

    [[nodiscard]] static double f (double x) noexcept
    {
        if (x >= kKnee)  return 1.0;
        if (x <= -kKnee) return -1.0;
        const double x2 = x * x;
        return x * (27.0 + x2) / (27.0 + 9.0 * x2);
    }

    [[nodiscard]] static double F1 (double x) noexcept
    {
        const double ax = std::abs (x);
        if (ax >= kKnee)
            return ax + kF1Outer;
        const double x2 = x * x;
        return x2 * (1.0 / 18.0) + (4.0 / 3.0) * std::log1p (x2 * (1.0 / 3.0));
    }

    [[nodiscard]] static double F2 (double x) noexcept
    {
        const double ax = std::abs (x);
        const double s  = (x < 0.0) ? -1.0 : 1.0;
        if (ax >= kKnee)
        {
            const double d = ax - kKnee;
            return s * (F2Inner (kKnee) + d * (kKnee + kF1Outer) + 0.5 * d * d);
        }
        return F2Inner (x);
    }

    /** First-order ADAA: mean of f over [x1, x] (half-sample group delay). */
    [[nodiscard]] static double adaa1 (double x, double x1) noexcept
    {
        const double dx = x - x1;
        if (std::abs (dx) < kEps)
            return f (0.5 * (x + x1));
        return (F1 (x) - F1 (x1)) / dx;
    }

    /** ADAA1 with the caller caching F1(x1) (one log per sample). */
    [[nodiscard]] static double adaa1Cached (double x, double x1, double F1x, double F1x1) noexcept
    {
        const double dx = x - x1;
        if (std::abs (dx) < kEps)
            return f (0.5 * (x + x1));
        return (F1x - F1x1) / dx;
    }

    /** Second-order ADAA (one-sample group delay). */
    [[nodiscard]] static double adaa2 (double x, double x1, double x2) noexcept
    {
        const double d02 = x - x2;
        if (std::abs (d02) < kEps)
        {
            // Nearly constant window: midpoint of the three-sample span
            return f ((x + x1 + x2) * (1.0 / 3.0));
        }
        return 2.0 * (dd2 (x, x1) - dd2 (x1, x2)) / d02;
    }

private:
    static constexpr double kF1Outer = -2.5 + (4.0 / 3.0) * 1.3862943611198906188; // F1(3) - 3

    [[nodiscard]] static double F2Inner (double x) noexcept
    {
        const double x2 = x * x;
        return x2 * x * (1.0 / 54.0)
             + (4.0 / 3.0) * (x * std::log1p (x2 * (1.0 / 3.0)) - 2.0 * x + 2.0 * kSqrt3 * std::atan (x / kSqrt3));
    }

    /** (F2(a) - F2(b)) / (a - b) with midpoint fallback F1((a+b)/2). */
    [[nodiscard]] static double dd2 (double a, double b) noexcept
    {
        const double d = a - b;
        if (std::abs (d) < kEps)
            return F1 (0.5 * (a + b));
        return (F2 (a) - F2 (b)) / d;
    }
};

// ============================================================================
/**
    Linear-phase polyphase 2x resampling stage (mono).
      up   : even-length Type II Kaiser windowed-sinc interpolator (group delay (nUp-1)/2 OS samples)
      down : odd-length Type I Kaiser windowed-sinc decimator     (group delay (nDn-1)/2 OS samples)
    The Type II half-sample offset is designed to cancel the ADAA1 half-sample delay so the total
    round-trip latency is an exact integer number of base-rate samples.
    All buffers are allocated in design(); up()/down() are allocation-free.
*/
class LinearPhase2x
{
public:
    /** fcNorm / transitionNorm in cycles per OVERSAMPLED sample. */
    void design (int nUp, int nDn, double fcNorm, double attenuationDb, int maxInputBlock)
    {
        nUpTaps = std::max (2, nUp + (nUp & 1));       // even
        nDnTaps = std::max (3, nDn | 1);               // odd
        maxIn   = std::max (1, maxInputBlock);

        const double beta = kaiserBeta (attenuationDb);
        const std::vector<double> hu = windowedSinc (nUpTaps, fcNorm, beta);
        const std::vector<double> hd = windowedSinc (nDnTaps, fcNorm, beta);

        // Up: two polyphase branches, reversed for forward-contiguous dot products, x2 gain
        phaseLen    = nUpTaps / 2;
        phaseLenPad = roundUp8 (phaseLen);
        upPh0.assign (static_cast<size_t> (phaseLenPad), 0.0f);
        upPh1.assign (static_cast<size_t> (phaseLenPad), 0.0f);
        for (int j = 0; j < phaseLen; ++j)
        {
            const int k = phaseLen - 1 - j;
            upPh0[static_cast<size_t> (j)] = static_cast<float> (2.0 * hu[static_cast<size_t> (2 * k)]);
            upPh1[static_cast<size_t> (j)] = static_cast<float> (2.0 * hu[static_cast<size_t> (2 * k + 1)]);
        }

        dnLenPad = roundUp8 (nDnTaps);
        dnTaps.assign (static_cast<size_t> (dnLenPad), 0.0f);
        for (int j = 0; j < nDnTaps; ++j)
            dnTaps[static_cast<size_t> (j)] = static_cast<float> (hd[static_cast<size_t> (j)]);

        upHist.assign (static_cast<size_t> (phaseLen - 1 + maxIn + phaseLenPad + 8), 0.0f);
        dnHist.assign (static_cast<size_t> (nDnTaps - 1 + 2 * maxIn + dnLenPad + 8), 0.0f);
    }

    void reset() noexcept
    {
        std::fill (upHist.begin(), upHist.end(), 0.0f);
        std::fill (dnHist.begin(), dnHist.end(), 0.0f);
    }

    /** Round-trip group delay in oversampled samples (excluding any processing between up/down). */
    [[nodiscard]] double roundTripDelayOs() const noexcept
    {
        return 0.5 * static_cast<double> (nUpTaps - 1) + 0.5 * static_cast<double> (nDnTaps - 1);
    }
    [[nodiscard]] double upDelayOs() const noexcept   { return 0.5 * static_cast<double> (nUpTaps - 1); }
    [[nodiscard]] double downDelayOs() const noexcept { return 0.5 * static_cast<double> (nDnTaps - 1); }

    /** in[n] -> out[2n]. */
    void up (const float* in, float* out, int n) noexcept
    {
        n = std::min (n, maxIn);
        const int tail = phaseLen - 1;
        float* h = upHist.data();
        for (int m = 0; m < n; ++m)
            h[tail + m] = std::isfinite (in[m]) ? in[m] : 0.0f;
        for (int m = 0; m < n; ++m)
        {
            out[2 * m]     = dot (upPh0.data(), h + m, phaseLenPad);
            out[2 * m + 1] = dot (upPh1.data(), h + m, phaseLenPad);
        }
        if (tail > 0)
            std::memmove (h, h + n, static_cast<size_t> (tail) * sizeof (float));
    }

    /** in[2n] -> out[n]. */
    void down (const float* in, float* out, int n) noexcept
    {
        n = std::min (n, maxIn);
        const int tail = nDnTaps - 1;
        float* h = dnHist.data();
        for (int i = 0; i < 2 * n; ++i)
            h[tail + i] = std::isfinite (in[i]) ? in[i] : 0.0f;
        for (int m = 0; m < n; ++m)
            out[m] = dot (dnTaps.data(), h + 2 * m, dnLenPad);
        std::memmove (h, h + 2 * n, static_cast<size_t> (tail) * sizeof (float));
    }

private:
    int nUpTaps = 2, nDnTaps = 3, maxIn = 1;
    int phaseLen = 1, phaseLenPad = 8, dnLenPad = 8;
    std::vector<float> upPh0, upPh1, dnTaps, upHist, dnHist;

    static int roundUp8 (int v) noexcept { return (v + 7) & ~7; }

    /** 8 independent lanes -> SLP-vectorizes to SIMD FMAs without fast-math reassociation. */
    static inline float dot (const float* a, const float* b, int n) noexcept
    {
        float acc[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
        for (int i = 0; i < n; i += 8)
            for (int j = 0; j < 8; ++j)
                acc[j] += a[i + j] * b[i + j];
        return ((acc[0] + acc[4]) + (acc[1] + acc[5])) + ((acc[2] + acc[6]) + (acc[3] + acc[7]));
    }

    static double besselI0 (double x) noexcept
    {
        double sum = 1.0, term = 1.0;
        const double q = 0.25 * x * x;
        for (int k = 1; k < 64; ++k)
        {
            term *= q / static_cast<double> (k * k);
            sum += term;
            if (term < 1.0e-17 * sum)
                break;
        }
        return sum;
    }

    static double kaiserBeta (double a) noexcept
    {
        if (a > 50.0) return 0.1102 * (a - 8.7);
        if (a >= 21.0) return 0.5842 * std::pow (a - 21.0, 0.4) + 0.07886 * (a - 21.0);
        return 0.0;
    }

    static std::vector<double> windowedSinc (int len, double fc, double beta)
    {
        std::vector<double> h (static_cast<size_t> (len), 0.0);
        const double centre = 0.5 * static_cast<double> (len - 1);
        const double i0b = besselI0 (beta);
        double sum = 0.0;
        for (int i = 0; i < len; ++i)
        {
            const double t = static_cast<double> (i) - centre;
            const double arg = 2.0 * 3.14159265358979323846 * fc * t;
            const double sinc = (std::abs (t) < 1.0e-12) ? 2.0 * fc : std::sin (arg) / (3.14159265358979323846 * t);
            const double r = (centre > 0.0) ? t / centre : 0.0;
            const double w = besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0b;
            h[static_cast<size_t> (i)] = sinc * w;
            sum += h[static_cast<size_t> (i)];
        }
        for (auto& v : h)
            v /= sum; // unity DC gain
        return h;
    }
};

} // namespace monolith::dsp
