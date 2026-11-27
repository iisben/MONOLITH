#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#include "PhonemeField.h"

namespace monolith::dsp
{

/**
    64-bit Cytomic TPT/ZDF state-variable core with a free output mix:
        y = c0*x + c1*v1 + c2*v2      (v1 = band state, v2 = low state, hp = x - k*v1 - v2)
    Any stable digital biquad can be loaded: poles map to (g, k), zeros to the output mix,
    so matched (Vicanek) designs keep the SVF's modulation-friendly state topology.
*/
class TptSvf64
{
public:
    void reset() noexcept { ic1 = ic2 = 0.0; }
    [[nodiscard]] bool isFinite() const noexcept { return std::isfinite (ic1) && std::isfinite (ic2); }

    void setFromBiquad (double b0, double b1, double b2, double a1, double a2, bool zeroAtDc = false) noexcept
    {
        const double sPlus  = std::max (1.0e-300, 1.0 + a1 + a2); // = 4 g^2 / a0'
        const double sMinus = std::max (1.0e-300, 1.0 - a1 + a2); // = 4 / a0'
        g = std::sqrt (sPlus / sMinus);
        k = 2.0 * (1.0 - a2) / (sMinus * g);
        const double a0p = 4.0 / sMinus;
        const double mHp = (b0 - b1 + b2) * a0p * 0.25;
        const double mBp = (b0 - b2) * a0p / (2.0 * g);
        const double mLp = zeroAtDc ? 0.0 : (b0 + b1 + b2) * a0p / (4.0 * g * g);
        m1 = 1.0 / (1.0 + g * (g + k));
        m2 = g * m1;
        m3 = g * m2;
        c0 = mHp;
        c1 = mBp - mHp * k;
        c2 = mLp - mHp;
    }

    /** Bilinear SVF with explicit analog mix H = hpGain*HP + bandGain*(s/w)/D + lpGain*LP (prewarped at fc). */
    void setAnalogMix (double fc, double q, double sr, double hpGain, double bandGain, double lpGain) noexcept
    {
        g  = std::tan (kPiD * std::clamp (fc, 1.0, 0.49 * sr) / sr);
        k  = 1.0 / std::max (0.05, q);
        m1 = 1.0 / (1.0 + g * (g + k));
        m2 = g * m1;
        m3 = g * m2;
        c0 = hpGain;
        c1 = bandGain - hpGain * k;
        c2 = lpGain - hpGain;
    }

    struct Coeffs { double m1, m2, m3, c0, c1, c2; };
    [[nodiscard]] Coeffs coeffs() const noexcept { return { m1, m2, m3, c0, c1, c2 }; }

    inline double process (double x) noexcept
    {
        const double v3 = x - ic2;
        const double v1 = m1 * ic1 + m2 * v3;
        const double v2 = ic2 + m2 * ic1 + m3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        return c0 * x + c1 * v1 + c2 * v2;
    }

    static constexpr double kPiD = 3.14159265358979323846;

private:
    double g = 0.1, k = 1.0, m1 = 1.0, m2 = 0.0, m3 = 0.0;
    double c0 = 0.0, c1 = 0.0, c2 = 0.0;
    double ic1 = 0.0, ic2 = 0.0;
};

/** 64-bit first-order allpass (c + z^-1) / (1 + c z^-1). */
class Allpass1st64
{
public:
    void reset() noexcept { z1 = 0.0; }
    void setCoeff (double c) noexcept { coeff = c; }
    [[nodiscard]] bool isFinite() const noexcept { return std::isfinite (z1); }

    inline double process (double x) noexcept
    {
        const double y = coeff * x + z1;
        z1 = x - coeff * y;
        return y;
    }

    /** Coefficient whose digital phase at fc equals the analog -2*atan(fc/fa) with fa = ratio*fc (exact at every rate). */
    static double phaseMatchedCoeff (double fc, double ratio, double sr) noexcept
    {
        const double t = std::tan (TptSvf64::kPiD * std::clamp (fc, 1.0, 0.49 * sr) / sr);
        const double kk = ratio * t;
        return (kk - 1.0) / (kk + 1.0);
    }

private:
    double coeff = 1.0, z1 = 0.0;
};

/**
    64-bit drop-in for ZdfSvf's unity-peak bandpass (same clamps, same coefficient cache) — used for the
    formant-tracking onset and aspiration noise shapers so no vocal-tract filter keeps float state.
*/
class Bandpass64
{
public:
    void reset() noexcept { svf.reset(); }
    void invalidateCache() noexcept { lastFc = lastQ = lastSr = -1.0f; }

    void setParameters (float cutoffHz, float q, float sampleRate) noexcept
    {
        const float sr = std::max (8000.0f, sampleRate);
        const float fc = std::clamp (cutoffHz, 20.0f, 0.44f * sr);
        const float qq = std::clamp (q, 0.25f, 22.0f);
        if (std::abs (fc - lastFc) < 1.0e-3f && std::abs (qq - lastQ) < 1.0e-4f && std::abs (sr - lastSr) < 1.0f)
            return;
        lastFc = fc; lastQ = qq; lastSr = sr;
        svf.setAnalogMix (fc, qq, sr, 0.0, 1.0 / static_cast<double> (qq), 0.0);
    }

    [[nodiscard]] float processBandpass (float x) noexcept
    {
        const double y = svf.process (static_cast<double> (x));
        if (! std::isfinite (y)) { svf.reset(); return 0.0f; }
        return static_cast<float> (y);
    }

private:
    TptSvf64 svf;
    float lastFc = -1.0f, lastQ = -1.0f, lastSr = -1.0f;
};

/**
    Vicanek "matched" second-order bandpass (unity peak at fc): impulse-invariant poles, zero at DC,
    magnitude matched to the analog prototype at fc and at Nyquist — no bilinear cramping of F4/F5
    (or throat-shifted formants) approaching Nyquist.
*/
inline void designMatchedBandpass (double fc, double q, double sr, TptSvf64& svf) noexcept
{
    constexpr double pi = TptSvf64::kPiD;
    const double w0   = 2.0 * pi * std::clamp (fc, 10.0, 0.46 * sr) / sr;
    const double zeta = 0.5 / std::max (0.5, q);
    const double r    = std::exp (-zeta * w0);
    const double a1   = -2.0 * r * std::cos (std::sqrt (std::max (0.0, 1.0 - zeta * zeta)) * w0);
    const double a2   = r * r;

    const double A0 = (1.0 + a1 + a2) * (1.0 + a1 + a2);
    const double A1 = (1.0 - a1 + a2) * (1.0 - a1 + a2);
    const double A2 = -4.0 * a2;
    const double sh = std::sin (0.5 * w0);
    const double p1 = sh * sh, p0 = 1.0 - p1, p2 = 4.0 * p0 * p1;
    const double den = A0 * p0 + A1 * p1 + A2 * p2;

    const double qq  = std::max (0.5, q);
    const double u   = pi / w0;
    const double num = (u / qq) * (u / qq);
    const double hn2 = num / ((1.0 - u * u) * (1.0 - u * u) + num);

    const double B1 = A1 * hn2;
    const double B2 = (den - B1 * p1) / std::max (1.0e-300, p2);
    const double b1 = -0.5 * std::sqrt (B1);
    const double b0 = 0.5 * (-b1 + std::sqrt (std::max (0.0, b1 * b1 + B2)));
    const double b2 = -b0 - b1;
    svf.setFromBiquad (b0, b1, b2, a1, a2, true);
}

/**
    Notch-free Klatt parallel vocal tract (64-bit).

    The Klatt alternating-polarity sum (+F1 -F2 +F3 -F4 +F5) is kept where it shapes the voice: the +F1 / -F2
    relationship sets the low-harmonic balance below F1 that the factory presets are voiced around. Above F2 the
    plain sum meets in antiphase wherever a strong formant's skirt overlaps its neighbours (e.g. /u/ ~1.9 kHz,
    -23.6 dB vs the incoherent sum). Here every branch keeps its exact unity-peak bandpass MAGNITUDE (formant levels
    and the Singer's Formant cluster are untouched), but F2..F5 are rotated by phase-matched first-order allpasses
    (-69/-59/-131/-126 deg at their resonances; F3..F5 polarity +), chosen so no pair cancels: worst notch -8.8 dB
    over 600 random phoneme/register/throat/f0 states, source-weighted loudness within 0.07 dB (mean) of the legacy sum.
*/
class VocalTract
{
public:
    static constexpr int kNumFormants = 5;
    static constexpr std::array<double, kNumFormants> kRotationRatio { 0.0, 1.462, 1.784, 0.456, 0.505 }; // fa / Fk (0 = none)
    static constexpr std::array<double, kNumFormants> kPolarity      { 1.0, -1.0, 1.0, 1.0, 1.0 };

    void reset() noexcept
    {
        for (auto& b : bp) b.reset();
        for (auto& a : ap) a.reset();
    }

    [[nodiscard]] bool isFinite() const noexcept
    {
        for (const auto& b : bp) if (! b.isFinite()) return false;
        for (const auto& a : ap) if (! a.isFinite()) return false;
        return true;
    }

    /** Q mapping identical to the legacy tract (fc / bw, clamped 0.5..20). */
    static double qFor (double fc, double bw) noexcept { return std::clamp (fc / std::max (bw, 20.0), 0.5, 20.0); }

    void setFormant (int k, double fc, double q, double sr) noexcept
    {
        const auto ku = static_cast<size_t> (k);
        designMatchedBandpass (fc, q, sr, bp[ku]);
        if (kRotationRatio[ku] > 0.0)
            ap[ku].setCoeff (Allpass1st64::phaseMatchedCoeff (fc, kRotationRatio[ku], sr));
        polarity[ku] = kPolarity[ku];
    }

    /** One sample: fills per-branch outputs (unity-peak, rotated, signed); caller applies gains. */
    inline void processBranches (double x, std::array<double, kNumFormants>& out) noexcept
    {
        out[0] = bp[0].process (x);
        for (size_t k = 1; k < static_cast<size_t> (kNumFormants); ++k)
            out[k] = polarity[k] * ap[k].process (bp[k].process (x));
    }

    /** Voiced-tract sum as used by SwarmVoice (upperMul scales F2..F5, e.g. closed-hum onset suppression). */
    inline double process (double x, const std::array<float, kNumFormants>& gain, double upperMul = 1.0) noexcept
    {
        std::array<double, kNumFormants> b {};
        processBranches (x, b);
        return (b[0] * gain[0] + (b[1] * gain[1] + b[2] * gain[2] + b[3] * gain[3] + b[4] * gain[4]) * upperMul) * kOutputGain;
    }

    static constexpr double kOutputGain = 4.2;

private:
    std::array<TptSvf64, kNumFormants> bp {};
    std::array<Allpass1st64, kNumFormants> ap {};
    std::array<double, kNumFormants> polarity { 1.0, -1.0, 1.0, 1.0, 1.0 };
};

/**
    Structure-of-Arrays SIMD bank of kLanes notch-free Klatt tracts (one lane per swarm singer).
    Bit-for-bit the same recurrences as VocalTract::processBranches, laid out formant-major over
    contiguous lane arrays so each stage vectorizes across singers (2 doubles/NEON, 4/AVX).
    Parked / silent singer lanes inside the active width ring down on zero input (masked output):
    a settled silent singer re-enters from a decayed tract instead of a frozen one.
*/
class VocalTractBank
{
public:
    static constexpr int kLanes = 8;
    static constexpr int kF     = VocalTract::kNumFormants;

    void reset (int lane) noexcept
    {
        const auto l = static_cast<size_t> (lane);
        for (size_t k = 0; k < static_cast<size_t> (kF); ++k)
            ic1[k][l] = ic2[k][l] = z1[k][l] = 0.0;
    }

    [[nodiscard]] bool isFinite (int lane) const noexcept
    {
        const auto l = static_cast<size_t> (lane);
        for (size_t k = 0; k < static_cast<size_t> (kF); ++k)
            if (! (std::isfinite (ic1[k][l]) && std::isfinite (ic2[k][l]) && std::isfinite (z1[k][l])))
                return false;
        return true;
    }

    void setFormant (int lane, int k, double fc, double q, double sr) noexcept
    {
        const auto l  = static_cast<size_t> (lane);
        const auto ku = static_cast<size_t> (k);
        TptSvf64 proto;
        designMatchedBandpass (fc, q, sr, proto);
        const auto c = proto.coeffs();
        m1[ku][l] = c.m1; m2[ku][l] = c.m2; m3[ku][l] = c.m3;
        c0[ku][l] = c.c0; c1[ku][l] = c.c1; c2[ku][l] = c.c2;
        apC[ku][l] = (VocalTract::kRotationRatio[ku] > 0.0)
                         ? Allpass1st64::phaseMatchedCoeff (fc, VocalTract::kRotationRatio[ku], sr) : 0.0;
    }

    /**
        One sample for all lanes: sum[l] = (sum_k pol_k * branch_k * gain[k]) * kOutputGain for active lanes
        (0 elsewhere); lane0Branch[k] = gained branch k of lane 0 (for formant telemetry).
    */
    inline void process (const double* x, const double* gain, const bool* activeMask,
                         double* sum, double* lane0Branch, int laneCount = kLanes) noexcept
    {
        // Lanes >= laneCount are parked (callers pass the highest active singer + 1). Compile-time widths
        // (2 / 4 / 8) let each formant stage fully unroll into SIMD without runtime trip / alias checks.
        if (laneCount <= 0)      return;
        if (laneCount <= 2)      processN<2> (x, gain, activeMask, sum, lane0Branch);
        else if (laneCount <= 4) processN<4> (x, gain, activeMask, sum, lane0Branch);
        else                     processN<8> (x, gain, activeMask, sum, lane0Branch);
    }

private:
#if defined(__clang__) || defined(__GNUC__)
    using V2 = double __attribute__ ((vector_size (16))); // 2 x f64: one NEON / SSE2 register
#else
    struct V2 // portable fallback (MSVC): same arithmetic, scalar pairs
    {
        double v[2];
        double& operator[] (int i) noexcept { return v[i]; }
        double operator[] (int i) const noexcept { return v[i]; }
        friend V2 operator+ (V2 a, V2 b) noexcept { return { { a.v[0] + b.v[0], a.v[1] + b.v[1] } }; }
        friend V2 operator- (V2 a, V2 b) noexcept { return { { a.v[0] - b.v[0], a.v[1] - b.v[1] } }; }
        friend V2 operator* (V2 a, V2 b) noexcept { return { { a.v[0] * b.v[0], a.v[1] * b.v[1] } }; }
    };
#endif
    static inline V2 ld (const double* p) noexcept { V2 r; std::memcpy (&r, p, sizeof (V2)); return r; }
    static inline void st (double* p, V2 v) noexcept { std::memcpy (p, &v, sizeof (V2)); }
    static inline V2 splat (double a) noexcept { V2 r; r[0] = a; r[1] = a; return r; }

    /** Fused SoA kernel: all 5 formant stages for nl lanes, 2 lanes per SIMD register, states in registers per stage. */
    template <int nl>
    inline void processN (const double* x, const double* gain, const bool* activeMask,
                          double* sum, double* lane0Branch) noexcept
    {
        // Parked lanes inside [0, nl) run on zero excitation (callers zero x) and simply ring down; their
        // output is masked, so no per-lane branching is needed inside the kernel.
        constexpr int kV = nl / 2;
        V2 X[kV], acc[kV];
        for (int v = 0; v < kV; ++v)
        {
            X[v]   = ld (x + 2 * v);
            acc[v] = splat (0.0);
        }

        for (size_t k = 0; k < static_cast<size_t> (kF); ++k)
        {
            const V2 two = splat (2.0);
            const V2 gk  = splat (gain[k] * VocalTract::kPolarity[k]);
            const bool rot = VocalTract::kRotationRatio[k] > 0.0;
            for (int v = 0; v < kV; ++v)
            {
                const size_t o = static_cast<size_t> (2 * v);
                const V2 i1 = ld (&ic1[k][o]), i2 = ld (&ic2[k][o]);
                const V2 a2 = ld (&m2[k][o]);
                const V2 v3 = X[v] - i2;
                const V2 v1 = ld (&m1[k][o]) * i1 + a2 * v3;
                const V2 v2 = i2 + a2 * i1 + ld (&m3[k][o]) * v3;
                st (&ic1[k][o], two * v1 - i1);
                st (&ic2[k][o], two * v2 - i2);
                V2 y = ld (&c0[k][o]) * X[v] + ld (&c1[k][o]) * v1 + ld (&c2[k][o]) * v2;
                if (rot)
                {
                    const V2 ac = ld (&apC[k][o]);
                    const V2 zz = ld (&z1[k][o]);
                    const V2 ya = ac * y + zz;
                    st (&z1[k][o], y - ac * ya);
                    y = ya;
                }
                y = y * gk;
                acc[v] = acc[v] + y;
                if (v == 0)
                    lane0Branch[k] = y[0];
            }
        }

        const double kOut = VocalTract::kOutputGain;
        for (int v = 0; v < kV; ++v)
            for (int j = 0; j < 2; ++j)
            {
                const int l = 2 * v + j;
                sum[l] = activeMask[l] ? acc[v][j] * kOut : 0.0;
            }
    }

    using Lane = std::array<double, kLanes>;
    alignas (64) std::array<Lane, kF> ic1 {}, ic2 {}, z1 {};
    alignas (64) std::array<Lane, kF> m1 {}, m2 {}, m3 {}, c0 {}, c1 {}, c2 {}, apC {};
};

/**
    Klatt-style nasal pole-zero pair (64-bit SVF): pole fixed at 270 Hz (100 Hz bw), zero rising with nasalDip.
    Exact identity at nasalDip = 0 (zero cancels pole); at nasalDip = 1 the anti-resonance sits ~330 Hz and the
    spectrum above it is lowered by the same 0.35 factor the legacy upper-formant nasal multiplier applied.
*/
class NasalPoleZero
{
public:
    void reset() noexcept { svf.reset(); }
    [[nodiscard]] bool isFinite() const noexcept { return svf.isFinite(); }

    void set (double nasalDip, double sr) noexcept
    {
        const double nd = std::clamp (nasalDip, 0.0, 1.0);
        constexpr double fp = 270.0, bwp = 100.0, bwz = 100.0;
        const double hfGain = 1.0 - 0.35 * nd;                 // (wp/wz)^2
        const double rz     = std::sqrt (hfGain);              // wp / wz (prewarped ratio)
        const double gp     = std::tan (TptSvf64::kPiD * fp / sr);
        const double fz     = std::atan (gp / rz) * sr / TptSvf64::kPiD;
        const double qz     = fz / bwz;
        svf.setAnalogMix (fp, fp / bwp, sr, rz * rz, rz / qz, 1.0);
    }

    inline double process (double x) noexcept { return svf.process (x); }

private:
    TptSvf64 svf;
};

/**
    Log-domain formant smoother with epsilon settling: frequencies and bandwidths glide exponentially in
    log space (perceptually even, no undershoot toward 0 Hz); once every track is within kEps of its target
    it snaps and reports clean, so steady vowels skip all coefficient recomputation.
*/
class FormantSmoother
{
public:
    static constexpr double kEps = 1.0e-6; // log units (~0.0001 %)

    void prime (const FormantTarget& t) noexcept
    {
        for (size_t k = 0; k < 5; ++k)
        {
            refreshTarget (k, t);
            lf[k] = tf[k];
            lb[k] = tb[k];
            hz[k] = std::exp (lf[k]);
            bwHz[k] = std::exp (lb[k]);
        }
        primed = true;
        dirty  = true;
    }

    /** Advance one control tick (alpha = 1 - exp(-dt/tau)). Returns true when values moved (coefficients dirty). */
    bool advance (const FormantTarget& t, double alpha) noexcept
    {
        if (! primed) { prime (t); return true; }
        bool moved = false;
        for (size_t k = 0; k < 5; ++k)
        {
            refreshTarget (k, t);
            if (step (lf[k], tf[k], alpha)) { hz[k]   = std::exp (lf[k]); moved = true; }
            if (step (lb[k], tb[k], alpha)) { bwHz[k] = std::exp (lb[k]); moved = true; }
        }
        dirty = moved;
        return moved;
    }

    [[nodiscard]] double freq (size_t k) const noexcept { return hz[k]; }
    [[nodiscard]] double bw (size_t k) const noexcept { return bwHz[k]; }
    [[nodiscard]] bool isDirty() const noexcept { return dirty; }
    void invalidate() noexcept { primed = false; dirty = true; }

private:
    void refreshTarget (size_t k, const FormantTarget& t) noexcept
    {
        if (differs (t.freq[k], rawF[k])) { rawF[k] = t.freq[k]; tf[k] = std::log (std::max (1.0f, rawF[k])); }
        if (differs (t.bw[k], rawB[k]))   { rawB[k] = t.bw[k];   tb[k] = std::log (std::max (1.0f, rawB[k])); }
    }

    static bool differs (float a, float b) noexcept { return std::abs (a - b) > 0.0f; }

    static bool step (double& s, double target, double alpha) noexcept
    {
        const double d = target - s;
        if (std::abs (d) <= kEps)
        {
            const bool changed = std::abs (d) > 0.0;
            s = target;
            return changed;
        }
        s += alpha * d;
        return true;
    }

    std::array<float, 5>  rawF { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f }, rawB { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f };
    std::array<double, 5> tf {}, tb {}, lf {}, lb {}, hz {}, bwHz {};
    bool primed = false, dirty = true;
};

} // namespace monolith::dsp
