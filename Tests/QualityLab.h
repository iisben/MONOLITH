#pragma once
// Component-level aliasing & distortion measurements.
// Every DSP block that can create new frequencies (glottal oscillator, tape/tube drive,
// sub-octave saturator, master bus dynamics) is driven with a clean stimulus and its
// spectrum is split into "musical" bins (harmonics of the stimulus) and everything else.
// Anything that is not a harmonic is aliasing, interpolation noise or modulation junk.

#include "../Source/DSP/DspMath.h"
#include "../Source/DSP/EntropyStage.h"
#include "../Source/DSP/HarmonicLayers.h"
#include "../Source/DSP/MasterBus.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

namespace qlab
{

struct SpurReport
{
    double sfdrDb   = 0.0;   // strongest non-harmonic spur relative to the strongest harmonic (dBc, negative = good)
    double inharmDb = 0.0;   // total non-harmonic power relative to total harmonic power (dB)
    double thdDb    = 0.0;   // harmonics 2..N relative to fundamental (dB)
    double spurHz   = 0.0;
    double audibleSpurDb = 0.0; // strongest non-harmonic spur below 18 kHz (dBc)
};

/** Snap a test frequency onto an exact FFT bin (coherent sampling: zero window leakage). */
inline double coherent (double f, double sr, int order = 16)
{
    const double binHz = sr / (double) (1 << order);
    return std::round (f / binHz) * binHz;
}

/** Single long Blackman-Harris (4-term, -92 dB sidelobes) power spectrum. */
inline std::vector<double> powerSpectrum (const std::vector<float>& x, int order)
{
    const int size = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> work (2 * (size_t) size, 0.0f);
    const size_t start = x.size() > (size_t) size ? x.size() - (size_t) size : 0;
    for (int i = 0; i < size && start + (size_t) i < x.size(); ++i)
    {
        const double t = (double) i / (double) (size - 1);
        const double w = 0.35875 - 0.48829 * std::cos (2 * M_PI * t) + 0.14128 * std::cos (4 * M_PI * t) - 0.01168 * std::cos (6 * M_PI * t);
        work[(size_t) i] = (float) (x[start + (size_t) i] * w);
    }
    fft.performFrequencyOnlyForwardTransform (work.data());
    std::vector<double> p ((size_t) size / 2);
    for (int b = 0; b < size / 2; ++b) p[(size_t) b] = (double) work[(size_t) b] * work[(size_t) b];
    return p;
}

/** Splits spectrum into harmonics of f0 (+/- guard bins) and the rest. */
inline SpurReport analyse (const std::vector<float>& x, double sr, double f0, int order = 16,
                           double loHz = 20.0, double hiHz = -1.0)
{
    const auto p = powerSpectrum (x, order);
    const int size = 1 << order;
    const double binHz = sr / size;
    if (hiHz <= 0.0) hiHz = 0.5 * sr * 0.999;
    constexpr int guard = 6;

    std::vector<char> isHarm (p.size(), 0);
    double fundP = 0.0, harmP = 0.0, overP = 0.0, maxHarm = 0.0;
    for (int k = 1; k * f0 < 0.5 * sr; ++k)
    {
        const int c = (int) std::lround (k * f0 / binHz);
        double hp = 0.0;
        for (int b = std::max (0, c - guard); b <= std::min ((int) p.size() - 1, c + guard); ++b)
        {
            isHarm[(size_t) b] = 1;
            hp += p[(size_t) b];
        }
        harmP += hp;
        maxHarm = std::max (maxHarm, hp);
        if (k == 1) fundP = hp; else overP += hp;
    }

    SpurReport r;
    double inP = 0.0, worst = 0.0;
    for (size_t b = (size_t) std::max (1.0, loHz / binHz); b < p.size() && (double) b * binHz < hiHz; ++b)
    {
        if (isHarm[b]) continue;
        inP += p[b];
        // integrate a spur over +/-3 bins (window main lobe) for a fair dBc figure
        double s = 0.0;
        for (size_t j = (b > 3 ? b - 3 : 0); j <= std::min (p.size() - 1, b + 3); ++j) if (! isHarm[j]) s += p[j];
        if (s > worst) { worst = s; r.spurHz = (double) b * binHz; }
    }
    double worstAud = 0.0;
    for (size_t b = (size_t) std::max (1.0, loHz / binHz); b < p.size() && (double) b * binHz < std::min (hiHz, 18000.0); ++b)
    {
        if (isHarm[b]) continue;
        double s = 0.0;
        for (size_t j = (b > 3 ? b - 3 : 0); j <= std::min (p.size() - 1, b + 3); ++j) if (! isHarm[j]) s += p[j];
        worstAud = std::max (worstAud, s);
    }
    r.audibleSpurDb = 10.0 * std::log10 (std::max (worstAud, 1e-30) / std::max (maxHarm, 1e-30));
    r.sfdrDb   = 10.0 * std::log10 (std::max (worst, 1e-30) / std::max (maxHarm, 1e-30));
    r.inharmDb = 10.0 * std::log10 (std::max (inP, 1e-30) / std::max (harmP, 1e-30));
    r.thdDb    = 10.0 * std::log10 (std::max (overP, 1e-30) / std::max (fundP, 1e-30));
    return r;
}

inline void print (const std::string& name, const SpurReport& r)
{
    std::cout << "  " << std::left << std::setw (44) << name << std::right << std::fixed << std::setprecision (1)
              << " spur " << std::setw (7) << r.sfdrDb << " dBc"
              << " | inharm " << std::setw (7) << r.inharmDb << " dB"
              << " | <18k " << std::setw (7) << r.audibleSpurDb << " dBc"
              << " | THD " << std::setw (7) << r.thdDb << " dB"
              << "  (worst @ " << (int) r.spurHz << " Hz)\n";
}

// ---------------------------------------------------------------------------
inline SpurReport glottalOsc (double sr, double f0, float tension, int seconds = 2)
{
    f0 = coherent (f0, sr);
    const auto& lut = monolith::dsp::LfGlottalTables::instance();
    std::vector<float> x ((size_t) (sr * seconds));
    double ph = 0.0;
    for (auto& v : x)
    {
        v = lut.sample ((float) ph, tension, (float) f0, (float) sr);
        ph += f0 / sr;
        ph -= std::floor (ph);
    }
    return analyse (x, sr, f0);
}

inline SpurReport entropyDrive (double sr, double f0, float amp, float drive, bool hq4x)
{
    f0 = coherent (f0, sr);
    monolith::dsp::EntropyStage st;
    const int block = 256;
    st.prepare (sr, block, 7u);
    const int total = (int) (sr * 2.0);
    std::vector<float> outv;
    outv.reserve ((size_t) total);
    std::vector<float> l ((size_t) block), r ((size_t) block);
    double ph = 0.0;
    for (int pos = 0; pos < total; pos += block)
    {
        for (int i = 0; i < block; ++i)
        {
            l[(size_t) i] = r[(size_t) i] = (float) (amp * std::sin (2 * M_PI * ph));
            ph += f0 / sr;
            ph -= std::floor (ph);
        }
        st.processBlock (l.data(), r.data(), block, drive, 0.0f, 0.0f, hq4x);
        outv.insert (outv.end(), l.begin(), l.end());
    }
    return analyse (outv, sr, f0);
}

inline SpurReport subLayer (double sr, double f0)
{
    f0 = 2.0 * coherent (0.5 * f0, sr);
    monolith::dsp::HarmonicLayers h;
    h.prepare (sr);
    std::vector<float> x ((size_t) (sr * 2.0));
    for (size_t i = 0; i < x.size(); ++i)
    {
        if ((i % 16) == 0) h.updateFilters ((float) f0, 0.5f);
        float l = 0.0f, r = 0.0f;
        h.processSample ((float) f0, 1.0f, 0.0f, 1.0f, l, r);
        x[i] = l;
    }
    return analyse (x, sr, 0.5 * f0);
}

inline SpurReport masterBus (double sr, double f0, float ampDb)
{
    f0 = coherent (f0, sr);
    monolith::dsp::MasterBus mb;
    mb.prepare (sr);
    const float a = std::pow (10.0f, ampDb / 20.0f);
    std::vector<float> x ((size_t) (sr * 3.0));
    double ph = 0.0;
    for (auto& v : x)
    {
        float l = (float) (a * std::sin (2 * M_PI * ph)), r = l;
        mb.process (l, r);
        v = l;
        ph += f0 / sr;
        ph -= std::floor (ph);
    }
    return analyse (x, sr, f0);
}

/** Output true peak (dBTP) via 16x, 128-tap windowed-sinc reconstruction. */
inline double truePeakDb (const std::vector<float>& x)
{
    constexpr int over = 16, half = 64;
    double pk = 0.0;
    for (size_t n = (size_t) half; n + (size_t) half < x.size(); ++n)
        for (int ph = 0; ph < over; ++ph)
        {
            const double f = (double) ph / over;
            double acc = 0.0;
            for (int j = -half + 1; j <= half; ++j)
            {
                const double t = (double) j - f;
                const double s = std::abs (t) < 1e-12 ? 1.0 : std::sin (M_PI * t) / (M_PI * t);
                const double u = t / (half + 1);
                const double w = 0.42 + 0.5 * std::cos (M_PI * u) + 0.08 * std::cos (2 * M_PI * u);
                acc += x[(size_t) ((long) n + j)] * s * w;
            }
            pk = std::max (pk, std::abs (acc));
        }
    return 20.0 * std::log10 (std::max (pk, 1e-12));
}

/** Drives the master bus 6 dB over with a near-fs/4 tone (worst case for inter-sample overs). */
inline double masterBusTruePeak (double sr)
{
    monolith::dsp::MasterBus mb;
    mb.prepare (sr);
    std::vector<float> y ((size_t) (sr * 0.5));
    const double f = sr * 0.25; // samples land 45 deg off the crests: sample peak is 3 dB under true peak
    for (size_t i = 0; i < y.size(); ++i)
    {
        float l = (float) (2.0 * std::sin (2 * M_PI * f * (double) i / sr + 0.7853981634)), r = l;
        mb.process (l, r);
        y[i] = l;
    }
    std::vector<float> tail (y.end() - (long) (sr * 0.1), y.end());
    return truePeakDb (tail);
}

/** Runs the full measurement sweep. Returns worst figures for regression gating. */
struct LabSummary
{
    double worstGlottalSpur = -200.0;
    double worstDriveSpur   = -200.0;
    double worstBusThd      = -200.0;
    double worstSubSpur     = -200.0;
    double worstGlottalInharm = -200.0;
    double worstDriveAudible  = -200.0;
    double worstLimiterThd    = -200.0;
    double worstTruePeakDb    = -200.0;
};

inline LabSummary runAll (bool verbose)
{
    LabSummary s;
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        if (verbose) std::cout << " -- " << (int) sr << " Hz --\n";
        for (double f0 : { 65.41, 130.81, 261.63, 523.25, 1046.5, 1567.98 })
            for (float t : { 0.0f, 0.5f, 1.0f })
            {
                const auto r = glottalOsc (sr, f0, t);
                s.worstGlottalSpur = std::max (s.worstGlottalSpur, r.sfdrDb);
                s.worstGlottalInharm = std::max (s.worstGlottalInharm, r.inharmDb);
                if (verbose) print ("glottal f0=" + std::to_string ((int) f0) + " tension=" + std::to_string (t).substr (0, 3), r);
            }
        for (double f0 : { 220.0, 1013.0, 3127.0, 5987.0, 8311.0 })
            for (float drive : { 0.25f, 0.6f, 1.0f })
                for (bool hq : { false, true })
                {
                    const auto r = entropyDrive (sr, f0, 0.35f, drive, hq);
                    s.worstDriveSpur = std::max (s.worstDriveSpur, r.sfdrDb);
                    s.worstDriveAudible = std::max (s.worstDriveAudible, r.audibleSpurDb);
                    if (verbose) print ("drive f=" + std::to_string ((int) f0) + " d=" + std::to_string (drive).substr (0, 4) + (hq ? " hq8x" : " std4x"), r);
                }
        for (double f0 : { 110.0, 440.0, 1760.0 })
        {
            const auto r = subLayer (sr, f0);
            s.worstSubSpur = std::max (s.worstSubSpur, r.sfdrDb);
            if (verbose) print ("sub-abyss f0=" + std::to_string ((int) f0), r);
        }
        for (double f0 : { 55.0, 440.0, 3000.0 })
            for (float db : { -24.0f, -10.0f, -3.0f, 3.0f, 6.0f })
            {
                const auto r = masterBus (sr, f0, db);
                if (db < -5.0f) s.worstBusThd = std::max (s.worstBusThd, r.thdDb);
                if (db > 0.0f) s.worstLimiterThd = std::max (s.worstLimiterThd, r.thdDb);
                if (verbose) print ("masterbus f=" + std::to_string ((int) f0) + " lvl=" + std::to_string ((int) db) + "dBFS", r);
            }
        const double tp = masterBusTruePeak (sr);
        s.worstTruePeakDb = std::max (s.worstTruePeakDb, tp);
        if (verbose) std::cout << "  masterbus true peak (fs/4 tone, +6 dB over): " << tp << " dBTP (ceiling -1.0)\n";
    }
    return s;
}

} // namespace qlab
