/*
    MONOLITH — Locked 8-Gate Headless Flagship QA Harness (Phase 2)

    Usage:
      MonolithQA [--phase 3.1|3.2|3.3|3.4|3.5|4] [--active <ids>] [--only <ids>] [--write-baseline]

    Sub-gate ids : 1 2 3morph 3toggle 4adaa 4alias 5 6block 6rate 6pdc 7dc 7steal 7dry 7fdn 8snap 8cpu
    Group ids    : 3 4 6 7 8 all, 7tract (=7dry), 7stealdry (=7steal,7dry)

    Status semantics:
      PASS    — meets its LOCKED threshold.
      FAIL    — active sub-gate misses its threshold (process exits 1).
      PENDING — inactive sub-gate that misses (baseline only), or mechanism not implemented yet.

    THRESHOLDS IN namespace `lock` ARE LOCKED. Never relax them. Only the ADAPTERS marked
    "ADAPTER" may be re-pointed when a Phase 3.x subsystem replaces the probed component.
*/

#include "PluginProcessor.h"
#include "DSP/DspMath.h"
#include "DSP/EntropyStage.h"
#include "DSP/PhonemeField.h"
#include "DSP/VocalTract.h"
#include "State/ParameterIDs.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#if __has_include("DSP/ThroatDrive.h")
 #include "DSP/ThroatDrive.h"
 #define MONOLITH_QA_HAS_ADAA 1
#else
 #define MONOLITH_QA_HAS_ADAA 0
#endif

namespace
{
// ============================================================================
// LOCKED THRESHOLDS (Phase 2). Do not edit except baselineCpuPct (set once in Phase 2).
namespace lock
{
    constexpr double g1AliasC5C6Dbc   = -80.0;
    constexpr double g1AliasC7Dbc     = -75.0;
    constexpr double g1DcDbfs         = -90.0;
    constexpr double g1ExtentFloorDbc = -90.0;   // bright-effort C4 harmonic at min(0.35 fs, 16 kHz) must exceed this
    constexpr double g1ExclBins       = 4.5;     // ±4 bins around each integer harmonic (BH4 main lobe)

    constexpr double g2FreqPct        = 1.0;     // |peak - analytical| and cross-rate tracking
    constexpr double g2LevelDb        = 0.5;     // cross-rate peak level tracking vs 48 kHz
    constexpr double g2NotchDb        = 12.0;    // max dip below normalized analytical reference
    constexpr double g2VowelRmsDb     = 3.0;     // vowel-space RMS deviation from mean (tract, source-neutral; see RE-SCOPE)
    // RE-SCOPE (user decision 2026-11-26, Phase 3.2): the full-processor sustained-vowel RMS spread is set by
    // glottal-harmonic / F1 coupling and cannot be levelled without moving factory presets beyond g8RmsDb
    // (P01/P23 sit on the /a/ anchor, P14 on /u/). g2VowelRmsDb now applies to the formant engine itself
    // (TractProbe IR energy = flat-source RMS, every rate); the full-processor spread is a non-regression guard.
    constexpr double g2VowelSpreadDb  = 7.86;    // full-processor C3/C4 vowel RMS dev must not exceed Phase 2 baseline

    constexpr double g3D2Ratio        = 5.0;     // morph max |d2| vs smooth static reference
    constexpr double g3ToggleD2At48k  = 0.02;    // * (48000/fs)^2
    constexpr double g3EnergyDb       = 6.0;     // morph 50 ms window RMS over static reference max

    constexpr double g4DerivTol       = 1.0e-5;
    constexpr double g4AliasDbc       = -80.0;
    constexpr double g4AliasImproveDb = 20.0;
    constexpr double g4DcDbfs         = -90.0;

    constexpr double g5CombDb         = -12.0;   // deepest onset comb notch vs median ensemble/solo ratio
    constexpr double g5SubCorr        = 0.5;
    constexpr double g5MonoDropDb     = 3.5;

    constexpr double g6ResidualDbfs   = -100.0;
    constexpr int    g6LatencyTol     = 0;

    constexpr double g7D2Ratio        = 5.0;
    constexpr double g7DcDbfs         = -90.0;
    constexpr double g7TailDbfs       = -120.0;
    constexpr double g7IdleCpuPct     = 0.5;
    constexpr double g7FdnRiseDb      = 3.0;     // max 250 ms window-to-window RMS rise in tail
    constexpr double g7FdnCpuRatio    = 1.5;     // late-tail block time vs silent pre-roll

    constexpr double g8RmsDb          = 1.5;
    constexpr double g8PeakDbfs       = -0.1;
    constexpr double g8CpuPct         = 15.0;
    constexpr double g8CpuRatio       = 1.35;
    constexpr double baselineCpuPct   = 7.78;    // PHASE2_BASELINE_CPU (Phase 2 baseline, Apple arm64 Release -O3)
}

constexpr double kPi = 3.14159265358979323846;
using Vec = std::vector<double>;

struct Stereo
{
    Vec L, R;
    [[nodiscard]] size_t size() const noexcept { return L.size(); }
};

double dbPow (double p) { return 10.0 * std::log10 (std::max (p, 1.0e-300)); }
double dbAmp (double a) { return 20.0 * std::log10 (std::max (std::abs (a), 1.0e-300)); }

std::string fmt (double v, int prec = 1)
{
    std::ostringstream o;
    if (! std::isfinite (v)) o << "nan";
    else o << std::fixed << std::setprecision (prec) << v;
    return o.str();
}

// ============================================================================
// Double-precision radix-2 FFT + windows
void fftInPlace (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; (j & bit) != 0; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const size_t half = len / 2;
        for (size_t k = 0; k < half; ++k)
        {
            const double ang = -2.0 * kPi * static_cast<double> (k) / static_cast<double> (len);
            const std::complex<double> w (std::cos (ang), std::sin (ang));
            for (size_t i = 0; i < n; i += len)
            {
                const auto u = a[i + k];
                const auto v = a[i + k + half] * w;
                a[i + k]        = u + v;
                a[i + k + half] = u - v;
            }
        }
    }
}

enum class Win { None, Hann, BH4 };

Vec makeWindow (size_t n, Win type)
{
    Vec w (n, 1.0);
    for (size_t i = 0; i < n; ++i)
    {
        const double t = 2.0 * kPi * static_cast<double> (i) / static_cast<double> (n);
        if (type == Win::Hann) w[i] = 0.5 - 0.5 * std::cos (t);
        if (type == Win::BH4)  w[i] = 0.35875 - 0.48829 * std::cos (t) + 0.14128 * std::cos (2 * t) - 0.01168 * std::cos (3 * t);
    }
    return w;
}

Vec powerSpectrum (const Vec& x, size_t start, size_t n, Win type)
{
    const Vec w = makeWindow (n, type);
    std::vector<std::complex<double>> a (n);
    for (size_t i = 0; i < n; ++i)
        a[i] = (start + i < x.size()) ? x[start + i] * w[i] : 0.0;
    fftInPlace (a);
    Vec p (n / 2 + 1);
    for (size_t k = 0; k <= n / 2; ++k) p[k] = std::norm (a[k]);
    return p;
}

Vec avgPowerSpectrum (const Vec& x, size_t start, size_t end, size_t n, size_t hop)
{
    Vec acc (n / 2 + 1, 0.0);
    int frames = 0;
    for (size_t s = start; s + n <= end; s += hop, ++frames)
    {
        const Vec p = powerSpectrum (x, s, n, Win::Hann);
        for (size_t k = 0; k < acc.size(); ++k) acc[k] += p[k];
    }
    if (frames > 0) for (auto& v : acc) v /= frames;
    return acc;
}

/** BH4-weighted mean (immune to partial-period bias). */
double windowedMeanDbfs (const Vec& x, size_t start, size_t n)
{
    const Vec w = makeWindow (n, Win::BH4);
    double s = 0.0, ws = 0.0;
    for (size_t i = 0; i < n && start + i < x.size(); ++i) { s += w[i] * x[start + i]; ws += w[i]; }
    return dbAmp (s / std::max (ws, 1.0e-30));
}

struct AliasStats
{
    double aliasDbc = 0.0;
    std::vector<double> harmDbc; // index h-1
};

AliasStats aliasAnalysis (const Vec& x, size_t start, size_t n, double f0, double sr)
{
    const Vec p = powerSpectrum (x, start, n, Win::BH4);
    const double binHz = sr / static_cast<double> (n);
    const size_t half = n / 2;
    std::vector<char> excl (half + 1, 0);
    for (size_t k = 0; k <= 4; ++k) excl[k] = 1;

    int H = static_cast<int> (std::floor ((0.5 * sr) / f0));
    if (H * f0 >= 0.5 * sr) --H;

    AliasStats st;
    Vec harmP;
    double carrier = 1.0e-300;
    for (int h = 1; h <= H; ++h)
    {
        const double c = h * f0 / binHz;
        const long lo = std::max (0L, static_cast<long> (std::ceil (c - lock::g1ExclBins)));
        const long hi = std::min (static_cast<long> (half), static_cast<long> (std::floor (c + lock::g1ExclBins)));
        double pk = 0.0;
        for (long k = lo; k <= hi; ++k) { excl[static_cast<size_t> (k)] = 1; pk = std::max (pk, p[static_cast<size_t> (k)]); }
        harmP.push_back (pk);
        carrier = std::max (carrier, pk);
    }
    double res = 1.0e-300;
    for (size_t k = 5; k < half; ++k)
        if (! excl[k]) res = std::max (res, p[k]);

    st.aliasDbc = dbPow (res / carrier);
    for (double hp : harmP) st.harmDbc.push_back (dbPow (hp / carrier));
    return st;
}

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void lowpass (double fc, double q, double sr)
    {
        const double w = 2.0 * kPi * fc / sr, c = std::cos (w), s = std::sin (w), al = s / (2.0 * q), a0 = 1.0 + al;
        b0 = (1.0 - c) * 0.5 / a0; b1 = (1.0 - c) / a0; b2 = b0; a1 = -2.0 * c / a0; a2 = (1.0 - al) / a0;
    }
    double process (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

Vec lowpass4 (const Vec& x, double fc, double sr)
{
    Biquad a, b;
    a.lowpass (fc, 0.54119610, sr);
    b.lowpass (fc, 1.30656296, sr);
    Vec y (x.size());
    for (size_t i = 0; i < x.size(); ++i) y[i] = b.process (a.process (x[i]));
    return y;
}

double pearson (const Vec& a, const Vec& b, size_t from, size_t to)
{
    double ma = 0, mb = 0;
    const double n = static_cast<double> (to - from);
    for (size_t i = from; i < to; ++i) { ma += a[i]; mb += b[i]; }
    ma /= n; mb /= n;
    double sab = 0, saa = 0, sbb = 0;
    for (size_t i = from; i < to; ++i)
    {
        const double da = a[i] - ma, dbv = b[i] - mb;
        sab += da * dbv; saa += da * da; sbb += dbv * dbv;
    }
    return sab / std::sqrt (std::max (saa * sbb, 1.0e-300));
}

double maxD2 (const Stereo& s, size_t from, size_t to)
{
    double m = 0.0;
    to = std::min (to, s.size());
    for (size_t i = std::max<size_t> (from, 2); i < to; ++i)
    {
        m = std::max (m, std::abs (s.L[i] - 2 * s.L[i - 1] + s.L[i - 2]));
        m = std::max (m, std::abs (s.R[i] - 2 * s.R[i - 1] + s.R[i - 2]));
    }
    return m;
}

double rmsDb (const Stereo& s, size_t a, size_t b)
{
    double acc = 0.0;
    b = std::min (b, s.size());
    for (size_t i = a; i < b; ++i) acc += 0.5 * (s.L[i] * s.L[i] + s.R[i] * s.R[i]);
    return dbPow (acc / std::max<double> (1.0, static_cast<double> (b - a)));
}

double peakAbs (const Stereo& s, size_t a, size_t b)
{
    double m = 0.0;
    b = std::min (b, s.size());
    for (size_t i = a; i < b; ++i) m = std::max ({ m, std::abs (s.L[i]), std::abs (s.R[i]) });
    return m;
}

double maxWindowRmsDb (const Stereo& s, size_t a, size_t b, size_t win)
{
    double m = -400.0;
    for (size_t i = a; i + win <= std::min (b, s.size()); i += win / 2)
        m = std::max (m, rmsDb (s, i, i + win));
    return m;
}

bool allFinite (const Stereo& s)
{
    for (size_t i = 0; i < s.size(); ++i)
        if (! std::isfinite (s.L[i]) || ! std::isfinite (s.R[i])) return false;
    return true;
}

Vec mid (const Stereo& s)
{
    Vec m (s.size());
    for (size_t i = 0; i < s.size(); ++i) m[i] = 0.5 * (s.L[i] + s.R[i]);
    return m;
}

double median (Vec v)
{
    if (v.empty()) return 0.0;
    std::sort (v.begin(), v.end());
    return v[v.size() / 2];
}

double midiHz (int n) { return 440.0 * std::pow (2.0, (n - 69) / 12.0); }

// ============================================================================
// Deterministic QA PRNG (resettable)
struct QaRng
{
    uint32_t s = 0xC0FFEEu;
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
};

// ============================================================================
// Full-processor headless driver
struct Event
{
    int64_t pos;
    juce::MidiMessage msg;
};

Event noteOn (int64_t p, int n, float v = 0.78f) { return { p, juce::MidiMessage::noteOn (1, n, v) }; }
Event noteOff (int64_t p, int n)                 { return { p, juce::MidiMessage::noteOff (1, n) }; }

using BlockFn = std::function<int()>;
BlockFn fixedBlocks (int n) { return [n] { return n; }; }

class QaSynth
{
public:
    QaSynth() : proc (std::make_unique<monolith::MonolithAudioProcessor>()) {}

    void set (const char* id, float v)
    {
        if (auto* p = proc->getAPVTS().getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (v));
    }

    void dry() { set (monolith::params::spaceMix, 0.0f); }

    void prepare (double rate, int maxBlk)
    {
        sr = rate;
        maxBlock = maxBlk;
        proc->setRateAndBufferSizeDetails (rate, maxBlk);
        proc->prepareToPlay (rate, maxBlk);
    }

    [[nodiscard]] int64_t S (double sec) const { return static_cast<int64_t> (std::llround (sec * sr)); }

    Stereo render (int64_t total, std::vector<Event> ev, const BlockFn& nextBlock,
                   const std::function<void (int64_t)>& autom = nullptr, Vec* blockUs = nullptr)
    {
        std::stable_sort (ev.begin(), ev.end(), [] (const Event& a, const Event& b) { return a.pos < b.pos; });
        Stereo out;
        out.L.assign (static_cast<size_t> (total), 0.0);
        out.R.assign (static_cast<size_t> (total), 0.0);
        std::vector<float> bl (static_cast<size_t> (maxBlock)), br (static_cast<size_t> (maxBlock));
        juce::MidiBuffer midi;
        size_t ei = 0;
        int64_t pos = 0;
        while (pos < total)
        {
            int bs = static_cast<int> (std::min<int64_t> (nextBlock(), total - pos));
            bs = std::clamp (bs, 0, maxBlock);
            if (autom) autom (pos);
            midi.clear();
            while (ei < ev.size() && ev[ei].pos < pos + bs)
            {
                midi.addEvent (ev[ei].msg, static_cast<int> (std::max<int64_t> (0, ev[ei].pos - pos)));
                ++ei;
            }
            std::fill (bl.begin(), bl.end(), 0.0f);
            std::fill (br.begin(), br.end(), 0.0f);
            float* ch[2] = { bl.data(), br.data() };
            juce::AudioBuffer<float> buf (ch, 2, bs);
            const auto t0 = std::chrono::steady_clock::now();
            proc->processBlock (buf, midi);
            const auto t1 = std::chrono::steady_clock::now();
            if (blockUs != nullptr && bs > 0)
                blockUs->push_back (std::chrono::duration<double, std::micro> (t1 - t0).count());
            for (int i = 0; i < bs; ++i)
            {
                out.L[static_cast<size_t> (pos + i)] = bl[static_cast<size_t> (i)];
                out.R[static_cast<size_t> (pos + i)] = br[static_cast<size_t> (i)];
            }
            pos += bs;
        }
        return out;
    }

    std::unique_ptr<monolith::MonolithAudioProcessor> proc;
    double sr = 48000.0;
    int maxBlock = 512;
};

// ============================================================================
// Results
struct Result
{
    std::string id;
    bool pass = false;
    bool implemented = true;
    std::string detail;
};

std::vector<Result> gResults;
std::ostringstream gLog; // verbose details -> build/monolith_qa/qa_report.txt

void record (const std::string& id, bool pass, const std::string& detail, bool implemented = true)
{
    gResults.push_back ({ id, pass, implemented, detail });
}

const std::array<std::pair<const char*, std::array<float, 2>>, 3> kVowels {{
    { "/a/", { 0.50f, 0.88f } }, { "/i/", { 0.88f, 0.36f } }, { "/u/", { 0.12f, 0.36f } }
}};

const std::array<double, 4> kRates { 44100.0, 48000.0, 96000.0, 192000.0 };

// ============================================================================
// ADAPTER (Gate 1): raw glottal excitation, isolated from the tract.
Vec renderGlottal (double f0, double tension, double sr, size_t n)
{
    const auto& lut = monolith::dsp::LfGlottalTables::instance();
    Vec y (n);
    double ph = 0.0;
    const double inc = f0 / sr;
    for (size_t i = 0; i < n; ++i)
    {
        y[i] = lut.sample (static_cast<float> (ph), static_cast<float> (tension), static_cast<float> (f0), static_cast<float> (sr));
        ph += inc;
        if (ph >= 1.0) ph -= 1.0;
    }
    return y;
}

void gate1()
{
    const double sr = 48000.0;
    const size_t n = 65536;
    const std::array<int, 4> notes { 60, 72, 84, 96 };
    const std::array<double, 3> tensions { 0.0, 0.68, 1.0 };
    std::array<double, 4> worstAlias { -400, -400, -400, -400 };
    double worstDc = -400.0;
    for (size_t ni = 0; ni < notes.size(); ++ni)
        for (double t : tensions)
        {
            const double f0 = midiHz (notes[ni]);
            const Vec y = renderGlottal (f0, t, sr, n);
            const auto st = aliasAnalysis (y, 0, n, f0, sr);
            worstAlias[ni] = std::max (worstAlias[ni], st.aliasDbc);
            worstDc = std::max (worstDc, windowedMeanDbfs (y, 0, n));
            gLog << "G1 note " << notes[ni] << " tension " << t << " alias " << fmt (st.aliasDbc) << " dBc\n";
        }

    // Bright-effort C4 harmonic extent
    const double f0 = midiHz (60);
    const double limit = std::min (0.35 * sr, 16000.0);
    const auto st = aliasAnalysis (renderGlottal (f0, 1.0, sr, n), 0, n, f0, sr);
    const int hTop = static_cast<int> (std::floor (limit / f0));
    const double extDbc = (hTop >= 1 && static_cast<size_t> (hTop) <= st.harmDbc.size()) ? st.harmDbc[static_cast<size_t> (hTop - 1)] : -400.0;

    const bool pass = worstAlias[1] < lock::g1AliasC5C6Dbc && worstAlias[2] < lock::g1AliasC5C6Dbc
                   && worstAlias[3] < lock::g1AliasC7Dbc && worstDc < lock::g1DcDbfs && extDbc > lock::g1ExtentFloorDbc;
    record ("1", pass, "alias C4 " + fmt (worstAlias[0]) + " C5 " + fmt (worstAlias[1]) + " C6 " + fmt (worstAlias[2])
                       + " C7 " + fmt (worstAlias[3]) + " dBc | DC " + fmt (worstDc) + " dBFS | h" + std::to_string (hTop)
                       + "@" + fmt (hTop * f0, 0) + "Hz " + fmt (extDbc) + " dBc");
}

// ============================================================================
// ADAPTER (Gate 2): voiced vocal tract, mirrors SwarmVoice::renderBlock — Phase 3.2: 64-bit VocalTract + nasal pair.
struct TractProbe
{
    monolith::dsp::VocalTract tract;
    monolith::dsp::NasalPoleZero nasal;
    monolith::dsp::FormantTarget t;

    void setup (const monolith::dsp::FormantTarget& tgt, double sr)
    {
        t = tgt;
        tract.reset();
        nasal.reset();
        for (int k = 0; k < monolith::dsp::VocalTract::kNumFormants; ++k)
        {
            const double fc = t.freq[static_cast<size_t> (k)];
            tract.setFormant (k, fc, monolith::dsp::VocalTract::qFor (fc, t.bw[static_cast<size_t> (k)]), sr);
        }
        nasal.set (t.nasalDip, sr);
    }

    double process (double x) { return nasal.process (tract.process (x, t.gain)); }
};

monolith::dsp::FormantTarget vowelTarget (size_t v)
{
    return monolith::dsp::PhonemeField::evaluate (kVowels[v].second[0], kVowels[v].second[1],
                                                  monolith::dsp::VocalRegister::Tenor,
                                                  static_cast<float> (midiHz (48)), 0.0f,
                                                  monolith::params::defaults::formantFocus);
}

/** Analytical (continuous-time) incoherent-sum reference magnitude — no inter-formant cancellation. */
double analyticalRef (const monolith::dsp::FormantTarget& t, double f)
{
    double acc = 0.0;
    for (size_t k = 0; k < 5; ++k)
    {
        const double q = std::max (0.5, static_cast<double> (t.freq[k]) / std::max (20.0, static_cast<double> (t.bw[k])));
        const double u = f / t.freq[k];
        const double h = (u / q) / std::sqrt ((1 - u * u) * (1 - u * u) + (u / q) * (u / q));
        acc += (t.gain[k] * h) * (t.gain[k] * h);
    }
    return std::sqrt (acc);
}

void gate2()
{
    std::map<double, std::array<double, 3>> tractRmsDb; // flat-source tract RMS per rate / vowel
    double worstFreqPct = 0.0, worstTrackPct = 0.0, worstLevelDb = 0.0, worstNotch = 0.0;
    int missingPeaks = 0;
    for (size_t v = 0; v < kVowels.size(); ++v)
    {
        const auto tgt = vowelTarget (v);
        std::array<double, 5> f48 {}, l48 {};
        for (int pass = 0; pass < 2; ++pass) // pass 0 measures 48k reference first
            for (double sr : kRates)
            {
                if ((pass == 0) != (sr == 48000.0)) continue;
                const size_t n = static_cast<size_t> (65536 * std::max (1.0, std::round (sr / 48000.0)));
                TractProbe probe;
                probe.setup (tgt, sr);
                Vec ir (n);
                for (size_t i = 0; i < n; ++i) ir[i] = probe.process (i == 0 ? 1.0 : 0.0);
                const Vec p = powerSpectrum (ir, 0, n, Win::None);
                {
                    double e = 0.0;
                    for (double x : ir) e += x * x;
                    tractRmsDb[sr][v] = 10.0 * std::log10 (std::max (e, 1.0e-300));
                }
                const double binHz = sr / static_cast<double> (n);
                auto magDb = [&] (double fq) {
                    const double b = fq / binHz;
                    const size_t k = std::min (static_cast<size_t> (b), p.size() - 2);
                    const double fr = b - static_cast<double> (k);
                    return dbAmp (std::sqrt (p[k]) * (1 - fr) + std::sqrt (p[k + 1]) * fr);
                };

                for (size_t k = 0; k < 5; ++k)
                {
                    const double fc = tgt.freq[k];
                    const size_t lo = static_cast<size_t> (std::ceil (0.85 * fc / binHz));
                    const size_t hi = std::min (p.size() - 2, static_cast<size_t> (std::floor (1.15 * fc / binHz)));
                    size_t best = 0;
                    for (size_t b = lo + 1; b < hi; ++b)
                        if (p[b] > p[b - 1] && p[b] >= p[b + 1] && (best == 0 || p[b] > p[best])) best = b;
                    if (best == 0) { ++missingPeaks; gLog << "G2 " << kVowels[v].first << " F" << k + 1 << " @" << sr << " no peak\n"; continue; }
                    const double a = dbPow (p[best - 1]), bb = dbPow (p[best]), c = dbPow (p[best + 1]);
                    const double den = a - 2 * bb + c;
                    const double off = (std::abs (den) > 1e-12) ? 0.5 * (a - c) / den : 0.0;
                    const double fm = (static_cast<double> (best) + off) * binHz;
                    const double lm = 0.5 * (bb - 0.25 * (a - c) * off); // dB amplitude
                    worstFreqPct = std::max (worstFreqPct, std::abs (fm - fc) / fc * 100.0);
                    if (sr == 48000.0) { f48[k] = fm; l48[k] = lm; }
                    else if (f48[k] > 0.0)
                    {
                        worstTrackPct = std::max (worstTrackPct, std::abs (fm - f48[k]) / f48[k] * 100.0);
                        worstLevelDb  = std::max (worstLevelDb, std::abs (lm - l48[k]));
                    }
                    gLog << "G2 " << kVowels[v].first << " F" << k + 1 << " @" << sr << " ref " << fmt (fc) << " meas " << fmt (fm, 2) << " lvl " << fmt (lm, 2) << "\n";
                }

                // Normalized notch check vs analytical incoherent sum
                const double fLo = 0.5 * tgt.freq[0];
                const double fHi = std::min (1.15 * tgt.freq[4], 0.45 * sr);
                Vec diff;
                for (int g = 0; g < 600; ++g)
                {
                    const double fq = fLo * std::pow (fHi / fLo, g / 599.0);
                    diff.push_back (magDb (fq) - dbAmp (analyticalRef (tgt, fq)));
                }
                const double med = median (diff);
                for (double d : diff) worstNotch = std::min (worstNotch, d - med);
            }
    }

    // Sustained C3/C4 vowel RMS (full processor, dry)
    double worstVowelDev = 0.0;
    std::string vowelStr;
    for (int note : { 48, 60 })
    {
        Vec r;
        for (size_t v = 0; v < kVowels.size(); ++v)
        {
            QaSynth q;
            q.dry();
            q.set (monolith::params::phonemeX, kVowels[v].second[0]);
            q.set (monolith::params::phonemeY, kVowels[v].second[1]);
            q.prepare (48000.0, 512);
            const auto out = q.render (q.S (2.5), { noteOn (0, note) }, fixedBlocks (512));
            r.push_back (rmsDb (out, static_cast<size_t> (q.S (1.0)), static_cast<size_t> (q.S (2.5))));
        }
        double mean = 0.0;
        for (double x : r) mean += x / static_cast<double> (r.size());
        for (double x : r) worstVowelDev = std::max (worstVowelDev, std::abs (x - mean));
        vowelStr += " " + std::string (note == 48 ? "C3" : "C4") + "[" + fmt (r[0]) + "," + fmt (r[1]) + "," + fmt (r[2]) + "]";
    }

    double worstTractDev = 0.0;
    std::string tractStr;
    for (const auto& [rate, e] : tractRmsDb)
    {
        const double mean = (e[0] + e[1] + e[2]) / 3.0;
        for (double x : e) worstTractDev = std::max (worstTractDev, std::abs (x - mean));
        if (rate == 48000.0) tractStr = "[" + fmt (e[0] - mean) + "," + fmt (e[1] - mean) + "," + fmt (e[2] - mean) + "]";
    }

    const bool pass = missingPeaks == 0 && worstFreqPct <= lock::g2FreqPct && worstTrackPct <= lock::g2FreqPct
                   && worstLevelDb <= lock::g2LevelDb && worstNotch >= -lock::g2NotchDb && worstTractDev <= lock::g2VowelRmsDb
                   && worstVowelDev <= lock::g2VowelSpreadDb;
    record ("2", pass, "Ferr " + fmt (worstFreqPct, 2) + "% track " + fmt (worstTrackPct, 2) + "% lvl " + fmt (worstLevelDb, 2)
                       + "dB missing " + std::to_string (missingPeaks) + " | notch " + fmt (worstNotch) + "dB | tract vowelRMS dev "
                       + fmt (worstTractDev, 2) + "dB" + tractStr + " | full-proc spread " + fmt (worstVowelDev, 2) + "dB" + vowelStr);
}

// ============================================================================
void gate3()
{
    // ---- 3morph
    bool finite = true, bounded = true;
    double worstRatio = 0.0, worstEnergy = -400.0;
    for (double sr : kRates)
    {
        double refD2 = 0.0, refWin = -400.0;
        for (int note : { 48, 72 })
            for (size_t v = 0; v < kVowels.size(); ++v)
            {
                QaSynth q;
                q.dry();
                q.set (monolith::params::vibratoDepth, 0.8f);
                q.set (monolith::params::phonemeX, kVowels[v].second[0]);
                q.set (monolith::params::phonemeY, kVowels[v].second[1]);
                q.prepare (sr, 512);
                const auto out = q.render (q.S (1.2), { noteOn (0, note) }, fixedBlocks (64));
                refD2  = std::max (refD2, maxD2 (out, static_cast<size_t> (q.S (0.3)), out.size()));
                refWin = std::max (refWin, maxWindowRmsDb (out, static_cast<size_t> (q.S (0.3)), out.size(), static_cast<size_t> (q.S (0.05))));
            }

        QaSynth q;
        q.dry();
        q.set (monolith::params::vibratoDepth, 0.8f);
        q.set (monolith::params::legatoEnable, 1.0f);
        q.set (monolith::params::glideTime, 0.3f);
        q.set (monolith::params::phonemeX, kVowels[0].second[0]);
        q.set (monolith::params::phonemeY, kVowels[0].second[1]);
        q.prepare (sr, 512);
        auto morph = [&] (int64_t pos) {
            const double t = static_cast<double> (pos) / sr;
            const double u = std::clamp ((t - 0.8) / 0.3, 0.0, 1.0);
            const auto& A = kVowels[0].second; const auto& I = kVowels[1].second; const auto& U = kVowels[2].second;
            double x, y;
            if (u < 0.5) { const double w = 2 * u;     x = A[0] + w * (I[0] - A[0]); y = A[1] + w * (I[1] - A[1]); }
            else         { const double w = 2 * u - 1; x = I[0] + w * (U[0] - I[0]); y = I[1] + w * (U[1] - I[1]); }
            q.set (monolith::params::phonemeX, static_cast<float> (x));
            q.set (monolith::params::phonemeY, static_cast<float> (y));
        };
        const auto out = q.render (q.S (2.0), { noteOn (0, 48), noteOn (q.S (0.8), 72) }, fixedBlocks (64), morph);
        finite  = finite && allFinite (out);
        bounded = bounded && peakAbs (out, 0, out.size()) <= 1.0;
        const double d2 = maxD2 (out, static_cast<size_t> (q.S (0.3)), out.size());
        worstRatio  = std::max (worstRatio, d2 / std::max (refD2, 1.0e-12));
        worstEnergy = std::max (worstEnergy, maxWindowRmsDb (out, static_cast<size_t> (q.S (0.3)), out.size(), static_cast<size_t> (q.S (0.05))) - refWin);
        gLog << "G3morph @" << sr << " d2 " << d2 << " ref " << refD2 << "\n";
    }
    record ("3morph", finite && bounded && worstRatio <= lock::g3D2Ratio && worstEnergy <= lock::g3EnergyDb,
            "finite " + std::to_string (finite) + " | d2/ref " + fmt (worstRatio, 2) + "x | energy +" + fmt (worstEnergy) + "dB");

    // ---- 3toggle: mid-note mode / bypass toggles on a dark sustained note
    double worstNorm = 0.0, worstStaticNorm = 0.0;
    for (double sr : kRates)
    {
        const double bound = lock::g3ToggleD2At48k * (48000.0 / sr) * (48000.0 / sr);
        for (int withToggles = 0; withToggles < 2; ++withToggles)
        {
            QaSynth q;
            q.dry();
            q.set (monolith::params::registerMode, 0.0f);
            q.set (monolith::params::phonemeX, kVowels[2].second[0]);
            q.set (monolith::params::phonemeY, kVowels[2].second[1]);
            q.prepare (sr, 512);
            using namespace monolith::params;
            std::vector<std::tuple<double, const char*, float>> sched {
                { 0.6, oversampleHq, 1.0f }, { 0.9, oversampleHq, 0.0f },
                { 1.2, driveAmount, 0.0f },  { 1.5, driveAmount, defaults::driveAmount },
                { 1.8, tapeInstability, 0.0f }, { 2.1, tapeInstability, defaults::tapeInstability },
                { 2.4, registerMode, 1.0f }, { 2.7, onsetMode, 2.0f } };
            size_t si = 0;
            auto autom = [&] (int64_t pos) {
                while (withToggles && si < sched.size() && q.S (std::get<0> (sched[si])) <= pos)
                {
                    q.set (std::get<1> (sched[si]), std::get<2> (sched[si]));
                    ++si;
                }
            };
            const auto out = q.render (q.S (3.0), { noteOn (0, 36) }, fixedBlocks (64), autom);
            const double d2 = maxD2 (out, static_cast<size_t> (q.S (0.5)), out.size());
            if (withToggles) worstNorm = std::max (worstNorm, d2 / bound);
            else             worstStaticNorm = std::max (worstStaticNorm, d2 / bound);
            gLog << "G3toggle @" << sr << (withToggles ? " toggled" : " static") << " d2 " << d2 << " bound " << bound << "\n";
        }
    }
    record ("3toggle", worstNorm < 1.0, "max d2/bound " + fmt (worstNorm, 2) + "x (static " + fmt (worstStaticNorm, 2) + "x)");
}

// ============================================================================
// ADAPTER (Gate 4): naive 1x reference of the stage's own nonlinearity at drive d.
double naiveShaper (double x, double d)
{
    const float g = 1.0f + static_cast<float> (d) * 7.5f;
    const float b = static_cast<float> (d) * 0.22f;
    return (monolith::dsp::fastTanh (g * static_cast<float> (x) + b) - monolith::dsp::fastTanh (b)) / (1.0f + 4.8f * static_cast<float> (d));
}

void gate4()
{
#if MONOLITH_QA_HAS_ADAA
    {
        using TS = monolith::dsp::ThroatShaper; // contract: static double f(x), F1(x), F2(x), adaa1(x, x1)
        double wd1 = 0.0, wd2 = 0.0, wc = 0.0;
        const double h = 1.0e-4;
        for (double x = -4.0; x <= 4.0; x += 0.01)
        {
            wd1 = std::max (wd1, std::abs ((TS::F1 (x + h) - TS::F1 (x - h)) / (2 * h) - TS::f (x)));
            wd2 = std::max (wd2, std::abs ((TS::F2 (x + h) - TS::F2 (x - h)) / (2 * h) - TS::F1 (x)));
        }
        for (double x1 = -4.0; x1 <= 4.0; x1 += 0.05)
            for (double dx = 1.0e-3; dx >= 1.0e-14; dx *= 0.1)
                wc = std::max (wc, std::abs (TS::adaa1 (x1 + dx, x1) - TS::f (x1 + 0.5 * dx)));
        record ("4adaa", wd1 <= lock::g4DerivTol && wd2 <= lock::g4DerivTol && wc <= lock::g4DerivTol,
                "F1'-f " + fmt (wd1 * 1e6, 3) + "e-6 F2'-F1 " + fmt (wd2 * 1e6, 3) + "e-6 cont " + fmt (wc * 1e6, 3) + "e-6");
    }
#else
    record ("4adaa", false, "ThroatShaper ADAA (DSP/ThroatDrive.h) not implemented", false);
#endif

    const double sr = 48000.0;
    const size_t n = 65536, total = 98304, start = total - n;
    double worstStage = -400.0, worstImprove = 400.0, worstDc = -400.0;
    bool pass = true;
    std::string per;
    for (double ftone : { 6000.0, 8000.0, 10000.0 })
        for (double drive : { 0.5, 1.0 })
        {
            monolith::dsp::EntropyStage es;
            es.prepare (sr, 512, 1u);
            std::vector<float> L (total), R (total);
            Vec naive (total);
            for (size_t i = 0; i < total; ++i)
            {
                const double x = 0.5 * std::sin (2 * kPi * ftone * static_cast<double> (i) / sr);
                L[i] = R[i] = static_cast<float> (x);
                naive[i] = naiveShaper (x, drive);
            }
            for (size_t off = 0; off < total; off += 512)
                es.processBlock (L.data() + off, R.data() + off, static_cast<int> (std::min<size_t> (512, total - off)),
                                 static_cast<float> (drive), 0.0f, 0.0f, false);
            const Vec y (L.begin(), L.end());
            const double a = aliasAnalysis (y, start, n, ftone, sr).aliasDbc;
            const double an = aliasAnalysis (naive, start, n, ftone, sr).aliasDbc;
            const double dc = windowedMeanDbfs (y, start, n);
            worstStage = std::max (worstStage, a);
            worstImprove = std::min (worstImprove, an - a);
            worstDc = std::max (worstDc, dc);
            pass = pass && (a < lock::g4AliasDbc || an - a >= lock::g4AliasImproveDb) && dc < lock::g4DcDbfs;
            gLog << "G4 tone " << ftone << " drive " << drive << " stage " << fmt (a) << " naive " << fmt (an) << " dc " << fmt (dc) << "\n";
        }
    record ("4alias", pass, "worst alias " + fmt (worstStage) + " dBc | min gain vs 1x " + fmt (worstImprove) + " dB | DC " + fmt (worstDc) + " dBFS");
}

// ============================================================================
void gate5()
{
    const double sr = 48000.0;
    double worstComb = 0.0, worstCorr = 1.0, worstDrop = 0.0;
    std::string per;
    for (int note : { 48, 60 })
    {
        auto run = [&] (bool solo) {
            QaSynth q;
            q.dry();
            if (solo) { q.set (monolith::params::ensembleSize, 1.0f); q.set (monolith::params::ensembleSpread, 0.0f); }
            q.prepare (sr, 512);
            return q.render (q.S (3.0), { noteOn (0, note) }, fixedBlocks (512));
        };
        const auto ens = run (false);
        const auto solo = run (true);
        const double f0 = midiHz (note);

        const size_t a = static_cast<size_t> (0.15 * sr), b = static_cast<size_t> (0.45 * sr), nfft = 8192;
        const Vec pe = avgPowerSpectrum (mid (ens), a, b, nfft, 1024);
        const Vec ps = avgPowerSpectrum (mid (solo), a, b, nfft, 1024);
        const double binHz = sr / nfft;
        const int H = std::min (32, static_cast<int> (5000.0 / f0));
        Vec D;
        for (int h = 1; h <= H; ++h)
        {
            const double c = h * f0 / binHz;
            double se = 0, ss = 0;
            for (long k = static_cast<long> (std::ceil (c - 3)); k <= static_cast<long> (std::floor (c + 3)); ++k)
            { se += pe[static_cast<size_t> (k)]; ss += ps[static_cast<size_t> (k)]; }
            D.push_back (dbPow (se / std::max (ss, 1.0e-300)));
        }
        const double med = median (D);
        double comb = 0.0;
        for (double d : D) comb = std::min (comb, d - med);
        worstComb = std::min (worstComb, comb);

        const size_t s0 = static_cast<size_t> (1.0 * sr), s1 = static_cast<size_t> (3.0 * sr);
        const double corr = pearson (lowpass4 (ens.L, 150.0, sr), lowpass4 (ens.R, 150.0, sr), s0, s1);
        worstCorr = std::min (worstCorr, corr);

        Stereo m;
        m.L = mid (ens);
        m.R = m.L;
        const double drop = rmsDb (ens, s0, s1) - rmsDb (m, s0, s1);
        worstDrop = std::max (worstDrop, drop);
        per += " " + std::string (note == 48 ? "C3" : "C4") + "[comb " + fmt (comb) + " corr " + fmt (corr, 2) + " drop " + fmt (drop, 2) + "]";
    }
    record ("5", worstComb > lock::g5CombDb && worstCorr >= lock::g5SubCorr && worstDrop <= lock::g5MonoDropDb, per.substr (1));
}

// ============================================================================
std::vector<Event> invarianceSeq (const QaSynth& q)
{
    return { noteOn (q.S (0.01), 48), noteOn (q.S (0.01), 52), noteOn (q.S (0.01), 55), noteOn (q.S (0.5), 59),
             { q.S (0.7), juce::MidiMessage::pitchWheel (1, 10000) },
             { q.S (0.8), juce::MidiMessage::controllerEvent (1, 1, 100) },
             noteOff (q.S (1.2), 48), noteOff (q.S (1.2), 52), noteOff (q.S (1.2), 55), noteOff (q.S (1.2), 59) };
}

double residualDbfs (const Stereo& a, const Stereo& b)
{
    double m = 0.0;
    for (size_t i = 0; i < std::min (a.size(), b.size()); ++i)
        m = std::max ({ m, std::abs (a.L[i] - b.L[i]), std::abs (a.R[i] - b.R[i]) });
    return dbAmp (m);
}

/** ADAPTER (Gate 6c): impulse peak through the latency-bearing stage (per-voice EntropyStage). */
int measuredStageLatency (float drive, bool hq)
{
    monolith::dsp::EntropyStage es;
    es.prepare (48000.0, 512, 1u);
    std::vector<float> L (4096, 0.0f), R (4096, 0.0f);
    L[0] = R[0] = 0.25f;
    for (size_t off = 0; off < L.size(); off += 512)
        es.processBlock (L.data() + off, R.data() + off, 512, drive, 0.0f, 0.0f, hq);
    size_t pk = 0;
    for (size_t i = 0; i < L.size(); ++i) if (std::abs (L[i]) > std::abs (L[pk])) pk = i;
    return static_cast<int> (pk);
}

void gate6()
{
    // 6block
    {
        QaSynth a, b;
        a.prepare (48000.0, 2048);
        b.prepare (48000.0, 2048);
        const auto ra = a.render (a.S (2.5), invarianceSeq (a), fixedBlocks (512));
        QaRng rng;
        const std::array<int, 7> sizes { 0, 1, 7, 13, 64, 137, 2048 };
        const auto rb = b.render (b.S (2.5), invarianceSeq (b), [&] { return sizes[rng.next() % sizes.size()]; });
        const double r = residualDbfs (ra, rb);
        record ("6block", r < lock::g6ResidualDbfs, "512 vs odd/prime residual " + fmt (r) + " dBFS");
    }
    // 6rate
    {
        QaSynth sw, fresh;
        for (double sr : { 44100.0, 192000.0, 48000.0 })
        {
            sw.prepare (sr, 512);
            (void) sw.render (sw.S (0.3), invarianceSeq (sw), fixedBlocks (512));
        }
        sw.prepare (96000.0, 512);
        fresh.prepare (96000.0, 512);
        const auto ra = sw.render (sw.S (2.0), invarianceSeq (sw), fixedBlocks (512));
        const auto rb = fresh.render (fresh.S (2.0), invarianceSeq (fresh), fixedBlocks (512));
        const double r = residualDbfs (ra, rb);
        record ("6rate", r < lock::g6ResidualDbfs, "44.1>192>48>96k vs fresh 96k residual " + fmt (r) + " dBFS");
    }
    // 6pdc
    {
        bool pass = true;
        std::string s;
        struct Cfg { float drive; bool hq; const char* name; };
        for (const Cfg& c : { Cfg { monolith::params::defaults::driveAmount, false, "2x" }, Cfg { monolith::params::defaults::driveAmount, true, "4x" },
                              Cfg { 0.0f, false, "bypass" } })
        {
            QaSynth q;
            q.set (monolith::params::driveAmount, c.drive);
            q.set (monolith::params::oversampleHq, c.hq ? 1.0f : 0.0f);
            q.prepare (48000.0, 512);
            const int reported = q.proc->getLatencySamples();
            const int measured = measuredStageLatency (c.drive, c.hq);
            const bool ok = std::abs (reported - measured) <= lock::g6LatencyTol && (c.drive > 0.0f || (reported == 0 && measured == 0));
            pass = pass && ok;
            s += std::string (s.empty() ? "" : " ") + c.name + " rep " + std::to_string (reported) + "/meas " + std::to_string (measured);
        }
        record ("6pdc", pass, s);
    }
}

// ============================================================================
void gate7()
{
    const double sr = 48000.0;
    // 7dc
    {
        QaSynth q;
        q.dry();
        q.prepare (sr, 512);
        const auto out = q.render (q.S (3.0), { noteOn (0, 48), noteOn (0, 51), noteOn (0, 55) }, fixedBlocks (512));
        const size_t a = static_cast<size_t> (q.S (1.0)), n = 65536;
        const double dc = std::max (windowedMeanDbfs (out.L, a, n), windowedMeanDbfs (out.R, a, n));
        record ("7dc", dc < lock::g7DcDbfs, "held-chord DC " + fmt (dc) + " dBFS");
    }
    // 7steal + 7dry
    {
        auto stealRun = [&] (int count, double totalSec, QaSynth& q) {
            q.dry();
            q.set (monolith::params::releaseTime, 0.05f);
            q.prepare (sr, 512);
            std::vector<Event> ev;
            for (int i = 0; i < count; ++i) ev.push_back (noteOn (q.S (0.05 + 0.04 * i), 36 + i));
            for (int i = 0; i < count; ++i) ev.push_back (noteOff (q.S (1.5), 36 + i));
            return q.render (q.S (totalSec), ev, fixedBlocks (512));
        };
        QaSynth qa, qb;
        const auto steal = stealRun (24, 11.6, qa);
        const auto ref   = stealRun (12, 1.6, qb);
        const double d2s = maxD2 (steal, 0, static_cast<size_t> (qa.S (1.6)));
        const double d2r = maxD2 (ref, 0, ref.size());
        record ("7steal", allFinite (steal) && d2s <= lock::g7D2Ratio * d2r,
                "24-note steal d2 " + fmt (d2s, 4) + " vs 12-note ref " + fmt (d2r, 4) + " (" + fmt (d2s / std::max (d2r, 1e-12), 2) + "x)");

        const double tail = dbAmp (peakAbs (steal, static_cast<size_t> (qa.S (11.5)), steal.size()));
        const int voices = qa.proc->getTelemetry().activeVoices.load();
        Vec us;
        (void) qa.render (512 * 400, {}, fixedBlocks (512), nullptr, &us);
        const double idlePct = median (us) / (512.0 / sr * 1.0e6) * 100.0;
        record ("7dry", tail < lock::g7TailDbfs && voices == 0 && idlePct < lock::g7IdleCpuPct,
                "10s tail " + fmt (tail) + " dBFS | voices " + std::to_string (voices) + " | idle " + fmt (idlePct, 3) + "% RT");
    }
    // 7fdn
    {
        QaSynth q;
        q.prepare (sr, 512);
        Vec us;
        const auto out = q.render (q.S (18.0), { noteOn (q.S (2.0), 48), noteOn (q.S (2.0), 52), noteOn (q.S (2.0), 55),
                                                 noteOff (q.S (3.0), 48), noteOff (q.S (3.0), 52), noteOff (q.S (3.0), 55) },
                                   fixedBlocks (512), nullptr, &us);
        const size_t win = static_cast<size_t> (q.S (0.25));
        double maxRise = -400.0, prev = 0.0;
        bool first = true;
        for (size_t i = static_cast<size_t> (q.S (3.5)); i + win <= out.size(); i += win)
        {
            const double r = rmsDb (out, i, i + win);
            if (! first && prev > -150.0) maxRise = std::max (maxRise, r - prev);
            prev = r;
            first = false;
        }
        const size_t bps = static_cast<size_t> (sr / 512.0);
        const Vec pre (us.begin() + static_cast<long> (bps / 2), us.begin() + static_cast<long> (2 * bps));
        const Vec late (us.end() - static_cast<long> (2 * bps), us.end());
        const double ratio = median (late) / std::max (median (pre), 1e-9);
        record ("7fdn", allFinite (out) && maxRise <= lock::g7FdnRiseDb && ratio <= lock::g7FdnCpuRatio,
                "15s tail max rise " + fmt (maxRise, 2) + " dB | end " + fmt (prev) + " dBFS | late/pre CPU " + fmt (ratio, 2) + "x");
    }
}

// ============================================================================
struct Snapshot { int idx; std::string name; double rms, peak, centroid, corr; };

std::string qaDir() { return std::string (MONOLITH_SOURCE_DIR) + "/build/monolith_qa"; }
std::string baselineCsv() { return std::string (MONOLITH_SOURCE_DIR) + "/Tests/QA/preset_baseline.csv"; }

void writeCsv (const std::string& path, const std::vector<Snapshot>& snaps)
{
    std::ofstream f (path);
    f << "index,name,rms_db,peak_db,centroid_hz,sub150_corr\n";
    for (const auto& s : snaps)
        f << s.idx << ',' << s.name << ',' << fmt (s.rms, 3) << ',' << fmt (s.peak, 3) << ',' << fmt (s.centroid, 1) << ',' << fmt (s.corr, 4) << '\n';
}

std::map<int, double> readBaselineRms()
{
    std::map<int, double> m;
    std::ifstream f (baselineCsv());
    std::string line;
    std::getline (f, line);
    while (std::getline (f, line))
    {
        std::vector<std::string> cols;
        std::stringstream ss (line);
        std::string c;
        while (std::getline (ss, c, ',')) cols.push_back (c);
        if (cols.size() >= 3) m[std::stoi (cols[0])] = std::stod (cols[2]);
    }
    return m;
}

double gCpuPct = 0.0;

void gate8 (bool writeBaseline)
{
    const double sr = 48000.0;
    std::vector<Snapshot> snaps;
    const int numPresets = QaSynth().proc->getNumPrograms();
    for (int i = 0; i < numPresets; ++i)
    {
        QaSynth q;
        q.proc->applyFactoryPreset (i);
        q.prepare (sr, 512);
        const auto out = q.render (q.S (3.0), { noteOn (0, 48), noteOn (0, 51), noteOn (0, 55) }, fixedBlocks (512));
        const size_t a = static_cast<size_t> (q.S (1.0)), b = out.size();
        const Vec m = mid (out);
        const Vec ps = avgPowerSpectrum (m, a, b, 4096, 2048);
        double num = 0, den = 0;
        for (size_t k = 1; k < ps.size(); ++k)
        {
            const double fq = k * sr / 4096.0;
            if (fq < 20.0 || fq > 20000.0) continue;
            num += fq * ps[k]; den += ps[k];
        }
        snaps.push_back ({ i, q.proc->getProgramName (i).toStdString(), rmsDb (out, a, b), dbAmp (peakAbs (out, a, b)),
                           num / std::max (den, 1e-300), pearson (lowpass4 (out.L, 150, sr), lowpass4 (out.R, 150, sr), a, b) });
    }
    writeCsv (qaDir() + "/presets_current.csv", snaps);
    if (writeBaseline) writeCsv (baselineCsv(), snaps);

    const auto base = readBaselineRms();
    double worstDelta = 0.0, worstPeak = -400.0;
    int worstIdx = -1, missing = 0;
    for (const auto& s : snaps)
    {
        worstPeak = std::max (worstPeak, s.peak);
        const auto it = base.find (s.idx);
        if (it == base.end()) { ++missing; continue; }
        const double d = std::abs (s.rms - it->second);
        if (d > worstDelta) { worstDelta = d; worstIdx = s.idx; }
    }
    record ("8snap", missing == 0 && worstDelta <= lock::g8RmsDb && worstPeak <= lock::g8PeakDbfs,
            std::to_string (snaps.size()) + " presets | max |dRMS| " + fmt (worstDelta, 2) + " dB (#" + std::to_string (worstIdx)
                + ") | max peak " + fmt (worstPeak, 2) + " dBFS" + (missing ? " | missing baseline " + std::to_string (missing) : ""));

    // 8cpu: 8-voice chord + full 24-singer swarm + acoustic space, 512-sample stereo blocks @48k
    QaSynth q;
    q.set (monolith::params::ensembleSize, 24.0f);
    q.prepare (sr, 512);
    std::vector<Event> ev;
    for (int n : { 48, 52, 55, 59, 62, 65, 69, 72 }) ev.push_back (noteOn (0, n));
    Vec us;
    (void) q.render (512 * 400, ev, fixedBlocks (512), nullptr, &us);
    const Vec steady (us.begin() + 100, us.end());
    const double medUs = median (steady);
    gCpuPct = medUs / 10666.7 * 100.0;
    const double baseCpu = lock::baselineCpuPct > 0.0 ? lock::baselineCpuPct : gCpuPct;
    record ("8cpu", gCpuPct < lock::g8CpuPct || gCpuPct <= lock::g8CpuRatio * baseCpu,
            "median " + fmt (medUs, 1) + " us/blk = " + fmt (gCpuPct, 2) + "% RT (baseline " + fmt (baseCpu, 2) + "%, "
                + std::to_string (steady.size()) + " blks)");
}

// ============================================================================
const std::vector<std::string> kAllIds { "1", "2", "3morph", "3toggle", "4adaa", "4alias", "5", "6block", "6rate", "6pdc",
                                         "7dc", "7steal", "7dry", "7fdn", "8snap", "8cpu" };

std::set<std::string> expandIds (const std::string& csv)
{
    static const std::map<std::string, std::vector<std::string>> groups {
        { "3", { "3morph", "3toggle" } }, { "4", { "4adaa", "4alias" } }, { "6", { "6block", "6rate", "6pdc" } },
        { "7", { "7dc", "7steal", "7dry", "7fdn" } }, { "8", { "8snap", "8cpu" } }, { "7tract", { "7dry" } },
        { "7stealdry", { "7steal", "7dry" } }, { "all", kAllIds } };
    std::set<std::string> out;
    std::stringstream ss (csv);
    std::string tok;
    while (std::getline (ss, tok, ','))
    {
        if (tok.empty()) continue;
        if (auto it = groups.find (tok); it != groups.end()) out.insert (it->second.begin(), it->second.end());
        else out.insert (tok);
    }
    return out;
}

/** Cumulative active sets per phase (zero regressions on prior gates). */
std::string phaseActive (const std::string& ph)
{
    const std::string p31 = "1,6,7dc,8";
    const std::string p32 = p31 + ",2,3morph,7tract";
    const std::string p33 = p32 + ",3,5,7stealdry";
    const std::string p34 = p33 + ",4,7fdn";
    if (ph == "3.1") return p31;
    if (ph == "3.2") return p32;
    if (ph == "3.3") return p33;
    if (ph == "3.4") return p34;
    if (ph == "3.5" || ph == "4") return "all";
    return "";
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit; // MessageManager only; no windows / devices / OpenGL

    std::set<std::string> active, only;
    bool writeBaseline = false;
    std::string phase = "baseline";
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--phase" && i + 1 < argc)  { phase = argv[++i]; active = expandIds (phaseActive (phase)); }
        else if (a == "--active" && i + 1 < argc) active = expandIds (argv[++i]);
        else if (a == "--only" && i + 1 < argc)   only = expandIds (argv[++i]);
        else if (a == "--write-baseline")         writeBaseline = true;
    }
    auto want = [&] (const char* g) {
        if (only.empty()) return true;
        for (const auto& id : only) if (id.rfind (g, 0) == 0) return true;
        return false;
    };

    juce::File (qaDir()).createDirectory();
    const auto t0 = std::chrono::steady_clock::now();
    if (want ("1")) gate1();
    if (want ("2")) gate2();
    if (want ("3")) gate3();
    if (want ("4")) gate4();
    if (want ("5")) gate5();
    if (want ("6")) gate6();
    if (want ("7")) gate7();
    if (want ("8")) gate8 (writeBaseline);
    const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();

    int fails = 0, pending = 0, passes = 0;
    std::ostringstream table;
    table << "MONOLITH QA | phase " << phase << " | " << fmt (secs, 1) << " s\n";
    for (const auto& r : gResults)
    {
        const bool isActive = active.count (r.id) > 0;
        std::string st;
        if (r.pass && r.implemented) { st = "PASS"; ++passes; }
        else if (isActive)           { st = "FAIL"; ++fails; }
        else                         { st = "PENDING"; ++pending; }
        table << "[" << std::setw (7) << st << "] " << std::left << std::setw (8) << r.id << std::right << (isActive ? "* " : "  ") << r.detail << "\n";
    }
    table << "TOTAL pass " << passes << " fail " << fails << " pending " << pending << "\n";

    std::cout << table.str();
    std::ofstream (qaDir() + "/qa_report.txt") << table.str() << "\n--- details ---\n" << gLog.str();
    return fails == 0 ? 0 : 1;
}
