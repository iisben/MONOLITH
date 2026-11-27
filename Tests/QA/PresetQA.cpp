/*
    MONOLITH II — Stage 5 closed-loop flagship preset QA + audition export.

    Usage:
      MonolithPresetQA [--preset-dir <dir>] [--only <1-based list, e.g. 3,7>] [--no-audition] [--csv <path>] [--rate <Hz>]

    Presets: by default the 20 flagship presets embedded in the binary (assets/v2_presets via BinaryData).
    --preset-dir loads the same V2-schema *.mnlpreset files from disk (tuning loop, no rebuild); the runner then
    also reports whether the embedded copy is stale.

    --rate (V2 Stage 6) re-runs every phrase and LOCKED assertion at another host rate (e.g. 44100, 96000);
    audition export is 48 kHz only and is disabled at any other rate, and the CSV defaults to preset_qa_<rate>.csv.

    Phrases (48 kHz unless --rate, 512-sample blocks, fresh processor per render):
      susC3 / susC4      single sustained note, velocity 100
      chord              C3 G3 C4 Eb4 at velocity 110, held 4 s (sustain window 2-4 s, tail window 4-5.5 s)
      melody             C5 line (8 notes, legato overlaps), velocity 100
      macroN sweep       chord + MACRO N 0 -> 100 % (0.5 -> 3.5 s);  macroN max: chord at MACRO N = 100 %
      allMax             chord with all four macros at 100 %
      wheel sweep        chord + CC1 0 -> 127 (0.5 -> 3.5 s);  wheel0 / wheel127: chord at CC1 0 / 127
      pressure127        chord at channel pressure 127
      mpe / mpePressure  MPE on, chord on member channels 2-5, without / with per-note pressure + slide
      audition           4 s musical phrase chosen from the preset's tags (exported as 24-bit WAV)

    LOCKED assertions (namespace lock5; never relax — refine the preset instead):
      tp      true peak (4x oversampled) <= -0.5 dBTP on EVERY render above
      rms     chord RMS (sustain window) within [-19, -11] dBFS
      clean   zero NaN / Inf, DC offset < -80 dBFS on every render
      mono    chord and susC3: sub-150 Hz L/R correlation >= +0.4, mono-sum RMS drop <= 4.0 dB
      macro   each macro: delta(chord, macroN max) >= 1.5 dB
      expr    delta(wheel0, wheel127) >= 1.5 dB; delta(chord, pressure127) >= 1.0 dB; delta(mpe, mpePressure) >= 1.0 dB
      meta    V2 schema, 5 categories x 4, name/tags/description, 4 unique macro labels, no dead macro,
              CC1 + pressure routes present, non-zero MPE depths
      div     no near-duplicate pair: a pair is a duplicate when centroid, width, attack, release AND spectral
              shape are ALL within tolerance (score = max_i |d_i| / tol_i < 1)
    delta(a, b) = max over {sustain, tail} windows of max(|dRMS|, mid/side 1/3-octave band RMS distance).
*/

#include "PluginProcessor.h"
#include "State/ParameterIDs.h"
#include "V2/ModMatrix.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <algorithm>
#include <array>
#include <cstdlib>
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

namespace
{

namespace lock5
{
    constexpr double truePeakDbtp    = -0.5;
    constexpr double chordRmsLo      = -19.0;
    constexpr double chordRmsHi      = -11.0;
    constexpr double dcDbfs          = -80.0;
    constexpr double sub150Corr      = 0.4;
    constexpr double monoDropDb      = 4.0;
    constexpr double macroDeltaDb    = 1.5;
    constexpr double wheelDeltaDb    = 1.5;
    constexpr double pressureDeltaDb = 1.0;
    constexpr double mpeDeltaDb      = 1.0;
    // near-duplicate tolerances (a pair is a duplicate only if every dimension is inside its tolerance)
    constexpr double dupCentroidOct  = 0.10;  // ~7 % spectral centroid
    constexpr double dupWidth        = 0.05;  // side/mid RMS ratio
    constexpr double dupAttackOct    = 0.35;
    constexpr double dupReleaseOct   = 0.35;
    constexpr double dupShapeDb      = 2.0;   // normalised 1/3-octave spectral-shape RMS distance
}

double gSr = 48000.0; // QA rate; --rate overrides for the Stage 6 host-rate spot-check (thresholds unchanged)
constexpr int kBlock = 512;
constexpr double kPi = 3.14159265358979323846;

using Vec = std::vector<double>;
struct Stereo
{
    Vec L, R;
    [[nodiscard]] size_t size() const noexcept { return L.size(); }
};

size_t S (double sec) { return static_cast<size_t> (std::llround (sec * gSr)); }
double dbPow (double p) { return 10.0 * std::log10 (std::max (p, 1.0e-300)); }
double dbAmp (double a) { return 20.0 * std::log10 (std::max (std::abs (a), 1.0e-300)); }

std::string fmt (double v, int prec = 1)
{
    std::ostringstream o;
    o << std::fixed << std::setprecision (prec) << v;
    return o.str();
}

// ── analysis ────────────────────────────────────────────────────────────────────────────────────────────
double rmsDb (const Stereo& s, size_t a, size_t b)
{
    b = std::min (b, s.size());
    double acc = 0.0;
    for (size_t i = a; i < b; ++i) acc += 0.5 * (s.L[i] * s.L[i] + s.R[i] * s.R[i]);
    return dbPow (acc / std::max (1.0, static_cast<double> (b - a)));
}

bool allFinite (const Stereo& s)
{
    for (size_t i = 0; i < s.size(); ++i)
        if (! std::isfinite (s.L[i]) || ! std::isfinite (s.R[i])) return false;
    return true;
}

/** DC offset = the 0 Hz component, measured as a Hann-weighted mean over the whole render. A plain (rectangular)
    mean over a few seconds is dominated by leakage from sub-octave / LFO content and note-on/off asymmetry rather
    than by true DC; the Hann weighting suppresses that leakage by > 100 dB beyond ~1 Hz. */
double dcDb (const Stereo& s)
{
    double l = 0, r = 0, wsum = 0;
    const double n = static_cast<double> (s.size());
    for (size_t i = 0; i < s.size(); ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2.0 * kPi * (static_cast<double> (i) + 0.5) / n);
        l += w * s.L[i]; r += w * s.R[i]; wsum += w;
    }
    return dbAmp (std::max (std::abs (l / std::max (wsum, 1e-300)), std::abs (r / std::max (wsum, 1e-300))));
}

double dcRectDb (const Stereo& s) // diagnostic only (plain mean)
{
    double l = 0, r = 0;
    for (size_t i = 0; i < s.size(); ++i) { l += s.L[i]; r += s.R[i]; }
    const double n = std::max<double> (1.0, static_cast<double> (s.size()));
    return dbAmp (std::max (std::abs (l / n), std::abs (r / n)));
}

/** ITU-R BS.1770-style true peak: 4x oversampling with a 32-tap-per-phase Kaiser-windowed sinc. */
class TruePeak
{
public:
    TruePeak()
    {
        auto i0 = [] (double x) { double s = 1, t = 1; for (int k = 1; k < 30; ++k) { t *= (x / (2.0 * k)) * (x / (2.0 * k)); s += t; } return s; };
        constexpr double beta = 8.0, half = kTaps / 2.0;
        for (int p = 1; p < 4; ++p)
        {
            double sum = 0.0;
            for (int k = 0; k < kTaps; ++k)
            {
                const int n = k - kTaps / 2 + 1;                // tap offset relative to x[i]
                const double t = static_cast<double> (p) / 4.0 - n; // distance to the interpolation point
                const double sinc = std::abs (t) < 1e-12 ? 1.0 : std::sin (kPi * t) / (kPi * t);
                const double u = t / half;
                const double w = std::abs (u) >= 1.0 ? 0.0 : i0 (beta * std::sqrt (1.0 - u * u)) / i0 (beta);
                h[static_cast<size_t> (p - 1)][static_cast<size_t> (k)] = sinc * w;
                sum += sinc * w;
            }
            for (auto& c : h[static_cast<size_t> (p - 1)]) c /= sum;
        }
    }

    [[nodiscard]] double peakDb (const Stereo& s) const { return dbAmp (std::max (peak (s.L), peak (s.R))); }

private:
    static constexpr int kTaps = 32;
    std::array<std::array<double, kTaps>, 3> h {};

    [[nodiscard]] double peak (const Vec& x) const
    {
        double m = 0.0;
        for (const double v : x) m = std::max (m, std::abs (v));
        const size_t lo = kTaps / 2 - 1, n = x.size();
        for (size_t i = lo; i + kTaps / 2 + 1 < n; ++i)
        {
            const double* base = x.data() + (i - lo);
            for (const auto& ph : h)
            {
                double y = 0.0;
                for (int k = 0; k < kTaps; ++k) y += base[k] * ph[static_cast<size_t> (k)];
                m = std::max (m, std::abs (y));
            }
        }
        return m;
    }
};

void fftInPlace (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * kPi / static_cast<double> (len);
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0, 0.0);
            for (size_t k = 0; k < len / 2; ++k)
            {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

constexpr size_t kFft = 4096;

Vec avgPower (const Vec& x, size_t a, size_t b)
{
    Vec acc (kFft / 2, 0.0);
    int frames = 0;
    std::vector<std::complex<double>> buf (kFft);
    for (size_t s = a; s + kFft <= std::min (b, x.size()); s += kFft / 2, ++frames)
    {
        for (size_t i = 0; i < kFft; ++i)
            buf[i] = x[s + i] * (0.5 - 0.5 * std::cos (2.0 * kPi * static_cast<double> (i) / static_cast<double> (kFft)));
        fftInPlace (buf);
        for (size_t k = 0; k < kFft / 2; ++k) acc[k] += std::norm (buf[k]);
    }
    for (auto& v : acc) v /= std::max (1, frames);
    return acc;
}

double centroidHz (const Vec& ps)
{
    double num = 0, den = 0;
    for (size_t k = 1; k < ps.size(); ++k)
    {
        const double f = static_cast<double> (k) * gSr / static_cast<double> (kFft);
        if (f < 20.0 || f > 20000.0) continue;
        num += f * ps[k]; den += ps[k];
    }
    return num / std::max (den, 1e-300);
}

constexpr int kBands = 27; // 1/3 octave, 40 Hz .. 16 kHz
std::array<double, kBands> bandsDb (const Vec& ps)
{
    std::array<double, kBands> out {};
    for (int b = 0; b < kBands; ++b)
    {
        const double fc = 40.0 * std::pow (2.0, b / 3.0), lo = fc / std::pow (2.0, 1.0 / 6.0), hi = fc * std::pow (2.0, 1.0 / 6.0);
        double e = 0.0;
        for (size_t k = 1; k < ps.size(); ++k)
        {
            const double f = static_cast<double> (k) * gSr / static_cast<double> (kFft);
            if (f >= lo && f < hi) e += ps[k];
        }
        out[static_cast<size_t> (b)] = dbPow (e);
    }
    return out;
}

Vec midOf (const Stereo& s)  { Vec m (s.size()); for (size_t i = 0; i < s.size(); ++i) m[i] = 0.5 * (s.L[i] + s.R[i]); return m; }
Vec sideOf (const Stereo& s) { Vec m (s.size()); for (size_t i = 0; i < s.size(); ++i) m[i] = 0.5 * (s.L[i] - s.R[i]); return m; }

double meanSq (const Vec& x, size_t a, size_t b)
{
    b = std::min (b, x.size());
    double acc = 0.0;
    for (size_t i = a; i < b; ++i) acc += x[i] * x[i];
    return acc / std::max (1.0, static_cast<double> (b - a));
}

/** RMS distance (dB) between two band sets over the bands within 60 dB of the loudest band in either set. */
double bandDistance (const std::vector<std::array<double, kBands>>& a, const std::vector<std::array<double, kBands>>& b)
{
    double top = -400.0;
    for (const auto& set : { &a, &b })
        for (const auto& bands : *set)
            for (const double v : bands) top = std::max (top, v);
    double acc = 0.0;
    int n = 0;
    for (size_t s = 0; s < a.size(); ++s)
        for (int k = 0; k < kBands; ++k)
        {
            const double x = a[s][static_cast<size_t> (k)], y = b[s][static_cast<size_t> (k)];
            if (std::max (x, y) < top - 60.0) continue;
            const double d = std::max (x, top - 60.0) - std::max (y, top - 60.0);
            acc += d * d; ++n;
        }
    return n > 0 ? std::sqrt (acc / n) : 0.0;
}

double windowDelta (const Stereo& a, const Stereo& b, size_t from, size_t to)
{
    const double dRms = std::abs (rmsDb (a, from, to) - rmsDb (b, from, to));
    const Vec ma = midOf (a), mb = midOf (b), sa = sideOf (a), sb = sideOf (b);
    const double bd = bandDistance ({ bandsDb (avgPower (ma, from, to)), bandsDb (avgPower (sa, from, to)) },
                                    { bandsDb (avgPower (mb, from, to)), bandsDb (avgPower (sb, from, to)) });
    return std::max (dRms, bd);
}

/** Chord-shaped renders: sustain 2-4 s and tail 4-5.5 s. */
double chordDelta (const Stereo& a, const Stereo& b)
{
    return std::max (windowDelta (a, b, S (2.0), S (4.0)), windowDelta (a, b, S (4.0), S (5.5)));
}

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void lowpass (double fc, double q)
    {
        const double w = 2.0 * kPi * fc / gSr, c = std::cos (w), s = std::sin (w), al = s / (2.0 * q), a0 = 1.0 + al;
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

Vec lowpass4 (const Vec& x, double fc)
{
    Biquad a, b;
    a.lowpass (fc, 0.54119610);
    b.lowpass (fc, 1.30656296);
    Vec y (x.size());
    for (size_t i = 0; i < x.size(); ++i) y[i] = b.process (a.process (x[i]));
    return y;
}

double pearson (const Vec& a, const Vec& b, size_t from, size_t to)
{
    to = std::min (to, std::min (a.size(), b.size()));
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
    return sab / std::sqrt (std::max (saa * sbb, 1e-300));
}

struct Mono { double corr150, dropDb; };
Mono monoCheck (const Stereo& s, size_t a, size_t b)
{
    const double corr = pearson (lowpass4 (s.L, 150.0), lowpass4 (s.R, 150.0), a, b);
    return { corr, rmsDb (s, a, b) - dbPow (meanSq (midOf (s), a, b)) };
}

// ── rendering ───────────────────────────────────────────────────────────────────────────────────────────
struct Event
{
    double t;
    juce::MidiMessage msg;
};

using Phrase = std::vector<Event>;

void addNote (Phrase& p, double on, double off, int note, int vel, int ch = 1)
{
    p.push_back ({ on, juce::MidiMessage::noteOn (ch, note, static_cast<juce::uint8> (vel)) });
    p.push_back ({ off, juce::MidiMessage::noteOff (ch, note) });
}

void addCcRamp (Phrase& p, int cc, double t0, double t1, int v0, int v1, int ch = 1)
{
    p.push_back ({ 0.0, juce::MidiMessage::controllerEvent (ch, cc, v0) });
    for (double t = t0; t <= t1 + 1e-9; t += 0.01)
    {
        const double u = std::clamp ((t - t0) / std::max (1e-9, t1 - t0), 0.0, 1.0);
        p.push_back ({ t, juce::MidiMessage::controllerEvent (ch, cc, static_cast<int> (std::lround (v0 + (v1 - v0) * u))) });
    }
}

const std::array<int, 4> kChord { 48, 55, 60, 63 }; // C3 G3 C4 Eb4

Phrase chordPhrase (int vel = 110, double off = 4.0)
{
    Phrase p;
    for (int n : kChord) addNote (p, 0.0, off, n, vel);
    return p;
}

class Rig
{
public:
    explicit Rig (const monolith::PresetRecord& rec) : proc (std::make_unique<monolith::MonolithAudioProcessor>())
    {
        proc->loadPresetRecord (rec);
    }

    void set (const char* id, float v)
    {
        if (auto* p = proc->getAPVTS().getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (v));
    }

    Stereo render (double seconds, Phrase ev, const std::function<void (double)>& autom = nullptr)
    {
        proc->setRateAndBufferSizeDetails (gSr, kBlock);
        proc->prepareToPlay (gSr, kBlock);
        std::stable_sort (ev.begin(), ev.end(), [] (const Event& a, const Event& b) { return a.t < b.t; });
        const size_t total = S (seconds);
        Stereo out;
        out.L.assign (total, 0.0);
        out.R.assign (total, 0.0);
        juce::AudioBuffer<float> buf (2, kBlock);
        juce::MidiBuffer midi;
        size_t ei = 0, pos = 0;
        while (pos < total)
        {
            const int bs = static_cast<int> (std::min<size_t> (kBlock, total - pos));
            if (autom) autom (static_cast<double> (pos) / gSr);
            midi.clear();
            while (ei < ev.size() && S (ev[ei].t) < pos + static_cast<size_t> (bs))
            {
                midi.addEvent (ev[ei].msg, static_cast<int> (S (ev[ei].t) > pos ? S (ev[ei].t) - pos : 0));
                ++ei;
            }
            buf.setSize (2, bs, false, false, true);
            buf.clear();
            proc->processBlock (buf, midi);
            for (int i = 0; i < bs; ++i)
            {
                out.L[pos + static_cast<size_t> (i)] = buf.getSample (0, i);
                out.R[pos + static_cast<size_t> (i)] = buf.getSample (1, i);
            }
            pos += static_cast<size_t> (bs);
        }
        return out;
    }

    std::unique_ptr<monolith::MonolithAudioProcessor> proc;
};

float ramp01 (double t, double t0, double t1) { return static_cast<float> (std::clamp ((t - t0) / (t1 - t0), 0.0, 1.0)); }

// ── audition phrases (4 s) ──────────────────────────────────────────────────────────────────────────────
float paramValue (const monolith::PresetRecord& r, const char* id, float fallback)
{
    for (const auto& [pid, v] : r.params)
        if (pid == id) return v;
    return fallback;
}

std::string auditionKind (const monolith::PresetRecord& r)
{
    auto has = [&] (const char* t) { return r.tags.contains (t, true); };
    if (has ("solo") || has ("lead") || has ("hook")) return "melody";
    if (has ("stab")) return "stabs";
    if (has ("evolving")) return "evolving";
    if (has ("drone")) return "drone";
    return "progression";
}

Phrase auditionPhrase (const monolith::PresetRecord& r, const std::string& kind)
{
    Phrase p;
    if (kind == "melody")
    {
        const int reg = static_cast<int> (std::lround (paramValue (r, monolith::params::registerMode, 4.0f)));
        const int tr = reg == 0 ? -24 : reg == 1 ? -12 : 0;
        const std::array<int, 5> notes { 67, 72, 75, 74, 72 };          // G4 C5 Eb5 D5 C5
        const std::array<double, 5> dur { 0.55, 0.65, 0.95, 0.55, 0.95 };
        double t = 0.0;
        for (size_t i = 0; i < notes.size(); ++i)
        {
            const double off = std::min (3.55, t + dur[i] + (i + 1 < notes.size() ? 0.03 : 0.0)); // legato overlap
            addNote (p, t, off, notes[i] + tr, i == 2 ? 112 : 96);
            t += dur[i];
        }
        if (r.tags.contains ("solo", true))
        {
            addCcRamp (p, 1, 0.0, 2.6, 70, 120);                         // breath swell -> operatic bloom
            for (double tt = 2.7; tt <= 3.5; tt += 0.02)
                p.push_back ({ tt, juce::MidiMessage::channelPressureChange (1, static_cast<int> (std::lround (127.0 * ramp01 (tt, 2.7, 3.4)))) });
        }
    }
    else if (kind == "stabs")
    {
        const double beat = 60.0 / 124.0;
        for (int i = 0; i < 8; ++i)
        {
            const double t = (i + 0.5) * beat * 0.98;
            const auto& ch = i < 4 ? std::array<int, 4> { 48, 58, 63, 67 } : std::array<int, 4> { 53, 56, 60, 65 }; // Cm7 -> Fm
            for (int n : ch) addNote (p, t, t + 0.20, n, 108);
        }
    }
    else if (kind == "drone")
    {
        addNote (p, 0.0, 3.4, 36, 100);
        addNote (p, 0.0, 3.4, 43, 96);
        addNote (p, 0.6, 3.4, 48, 92);
        addNote (p, 1.4, 3.4, 49, 88); // the minor second that makes it a ritual
    }
    else if (kind == "evolving")
    {
        for (int n : { 48, 55, 60, 62 }) addNote (p, 0.0, 3.5, n, 100); // Csus2, MACRO 1 blooms across the clip
    }
    else
    {
        for (int n : { 48, 55, 60, 63 }) addNote (p, 0.0, 1.95, n, 100); // Cm
        for (int n : { 44, 51, 60, 63 }) addNote (p, 2.0, 3.5, n, 100);  // Ab(add C)
    }
    return p;
}

bool writeWav (const juce::File& file, const Stereo& s)
{
    (void) file.deleteFile();
    juce::AudioBuffer<float> buf (2, static_cast<int> (s.size()));
    const size_t fade = S (0.25);
    for (size_t i = 0; i < s.size(); ++i)
    {
        const double g = i + fade >= s.size() ? 0.5 + 0.5 * std::cos (kPi * static_cast<double> (i + fade - s.size()) / static_cast<double> (fade)) : 1.0;
        buf.setSample (0, static_cast<int> (i), static_cast<float> (s.L[i] * g));
        buf.setSample (1, static_cast<int> (i), static_cast<float> (s.R[i] * g));
    }
    std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::FileOutputStream> (file);
    if (! static_cast<juce::FileOutputStream*> (os.get())->openedOk())
        return false;
    juce::WavAudioFormat wav;
    auto writer = wav.createWriterFor (os, juce::AudioFormatWriterOptions {}.withSampleRate (gSr).withNumChannels (2).withBitsPerSample (24));
    return writer != nullptr && writer->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
}

// ── per-preset report ───────────────────────────────────────────────────────────────────────────────────
struct Features
{
    double centroid = 0, width = 0, attack = 0, release = 0;
    std::array<double, kBands> shape {};
};

struct Report
{
    int idx = 0;
    std::string name, category, audition;
    double worstTp = -400, chordRms = 0, worstDc = -400, chordCorr = 0, chordDrop = 0, c3Corr = 0, c3Drop = 0;
    std::string worstTpPhrase, worstDcPhrase;
    std::array<double, 4> macroDelta {};
    double wheelDelta = 0, pressureDelta = 0, mpeDelta = 0;
    bool finite = true, metaOk = true;
    std::string metaWhy;
    Features f;
    double cpuSec = 0;
};

Features features (const Stereo& chord)
{
    Features f;
    const size_t a = S (2.0), b = S (4.0);
    const Vec m = midOf (chord), sd = sideOf (chord);
    f.centroid = centroidHz (avgPower (m, a, b));
    f.width = std::sqrt (meanSq (sd, a, b) / std::max (meanSq (m, a, b), 1e-300));
    const auto bands = bandsDb (avgPower (m, a, b));
    const double top = *std::max_element (bands.begin(), bands.end());
    double mean = 0;
    for (size_t k = 0; k < bands.size(); ++k) { f.shape[k] = std::max (bands[k], top - 60.0); mean += f.shape[k]; }
    mean /= kBands;
    for (auto& v : f.shape) v -= mean;

    // 10 ms stereo power envelope
    const size_t hop = S (0.01);
    Vec env;
    for (size_t i = 0; i + hop <= chord.size(); i += hop) env.push_back (rmsDb (chord, i, i + hop));
    const double sus = rmsDb (chord, a, b);
    f.attack = 4.0;
    for (size_t i = 0; i < env.size(); ++i)
        if (env[i] >= sus - 3.0) { f.attack = static_cast<double> (i) * 0.01; break; }
    f.release = 1.5;
    for (size_t i = static_cast<size_t> (400); i < env.size(); ++i)
        if (env[i] <= sus - 30.0) { f.release = static_cast<double> (i) * 0.01 - 4.0; break; }
    return f;
}

double dupScore (const Features& a, const Features& b)
{
    double shape = 0.0;
    for (size_t k = 0; k < a.shape.size(); ++k) shape += (a.shape[k] - b.shape[k]) * (a.shape[k] - b.shape[k]);
    shape = std::sqrt (shape / kBands);
    return std::max ({ std::abs (std::log2 (a.centroid / b.centroid)) / lock5::dupCentroidOct,
                       std::abs (a.width - b.width) / lock5::dupWidth,
                       std::abs (std::log2 ((a.attack + 0.02) / (b.attack + 0.02))) / lock5::dupAttackOct,
                       std::abs (std::log2 ((a.release + 0.05) / (b.release + 0.05))) / lock5::dupReleaseOct,
                       shape / lock5::dupShapeDb });
}

void checkMeta (const monolith::PresetRecord& r, Report& rep)
{
    std::vector<std::string> why;
    if (r.sourceVersion != 2) why.push_back ("schema");
    if (r.name.isEmpty() || r.description.length() < 24 || r.tags.size() < 4 || r.author.isEmpty()) why.push_back ("metadata");
    std::set<juce::String> labels;
    for (const auto& l : r.macroLabels) if (l.isNotEmpty()) labels.insert (l);
    if (labels.size() != 4) why.push_back ("macro labels");
    // V1 curves are slot-bound (slot 1 COLOSSUS .. slot 4 ABYSS). A V1 label is only allowed on its own slot with
    // the V1 curve on, so the knob does exactly what its name says.
    for (int m = 0; m < 4; ++m)
        for (int d = 0; d < 4; ++d)
            if (r.macroLabels[static_cast<size_t> (m)] == monolith::v2::kDefaultMacroLabels[static_cast<size_t> (d)])
            {
                bool legacyOn = false;
                for (const auto& src : r.control)
                    if (src.getProperty ("id").toString() == "macro" + juce::String (m + 1))
                        legacyOn = static_cast<bool> (src.getProperty ("legacy", true));
                if (m != d || ! legacyOn)
                    why.push_back ("V1 label " + r.macroLabels[static_cast<size_t> (m)].toStdString() + " misplaced / without V1 curve");
            }

    std::map<juce::String, int> routes;
    std::map<juce::String, bool> legacy;
    if (! r.control.isValid()) why.push_back ("no V2Control");
    for (const auto& src : r.control)
    {
        const auto id = src.getProperty ("id").toString();
        routes[id] = src.getNumChildren();
        legacy[id] = static_cast<bool> (src.getProperty ("legacy", true));
    }
    for (int m = 1; m <= 4; ++m)
    {
        const juce::String id = "macro" + juce::String (m);
        if (routes[id] == 0 && ! legacy[id]) why.push_back ("dead macro " + id.toStdString());
    }
    if (routes["mod_wheel"] == 0) why.push_back ("no CC1 routes");
    if (routes["pressure"] == 0) why.push_back ("no pressure routes");
    for (const char* id : { monolith::params::v2ExprVibrato, monolith::params::v2ExprEffort, monolith::params::v2ExprOpenness })
        if (paramValue (r, id, 0.0f) <= 0.0f) why.push_back (std::string ("zero ") + id);
    for (const auto& [pid, v] : r.params)
        if (monolith::params::isGlobalPerformanceParam (pid)) why.push_back ("stores global " + pid.toStdString());

    rep.metaOk = why.empty();
    for (const auto& w : why) rep.metaWhy += (rep.metaWhy.empty() ? "" : "; ") + w;
}

Report runPreset (int idx, const monolith::PresetRecord& rec, const TruePeak& tp, const juce::File& auditionDir, bool audition)
{
    using namespace monolith::params;
    const auto t0 = std::chrono::steady_clock::now();
    Report rep;
    rep.idx = idx;
    rep.name = rec.name.toStdString();
    rep.category = rec.category.toStdString();
    checkMeta (rec, rep);

    auto track = [&] (const std::string& phrase, const Stereo& s) {
        rep.finite = rep.finite && allFinite (s);
        if (const double dc = dcDb (s); dc > rep.worstDc) { rep.worstDc = dc; rep.worstDcPhrase = phrase; }
        if (std::getenv ("MONOLITH_QA_DC_DIAG") != nullptr)
            std::cerr << rec.name << " " << phrase << " dc hann " << fmt (dcDb (s), 1) << " rect " << fmt (dcRectDb (s), 1)
                      << " | LP2Hz rms " << fmt (dbPow (meanSq (lowpass4 (midOf (s), 2.0), S (0.5), s.size())), 1) << "\n";
        const double p = tp.peakDb (s);
        if (p > rep.worstTp) { rep.worstTp = p; rep.worstTpPhrase = phrase; }
    };

    const std::array<const char*, 4> macroIds { macroColossus, macroSpecter, macroRot, macroAbyss };

    // sustained notes
    for (int note : { 48, 60 })
    {
        Rig r (rec);
        Phrase p;
        addNote (p, 0.0, 3.0, note, 100);
        const auto out = r.render (4.5, p);
        track (note == 48 ? "susC3" : "susC4", out);
        if (note == 48)
        {
            const auto m = monoCheck (out, S (1.5), S (3.0));
            rep.c3Corr = m.corr150;
            rep.c3Drop = m.dropDb;
        }
    }

    // chord (reference for gain staging, mono, diversity and every delta)
    Stereo chord;
    {
        Rig r (rec);
        chord = r.render (5.5, chordPhrase());
        track ("chord", chord);
        rep.chordRms = rmsDb (chord, S (2.0), S (4.0));
        const auto m = monoCheck (chord, S (2.0), S (4.0));
        rep.chordCorr = m.corr150;
        rep.chordDrop = m.dropDb;
        rep.f = features (chord);
    }

    // C5 melody
    {
        Rig r (rec);
        Phrase p;
        const std::array<int, 8> mel { 72, 74, 75, 79, 77, 75, 74, 72 };
        for (size_t i = 0; i < mel.size(); ++i)
            addNote (p, 0.4 * static_cast<double> (i), 0.4 * static_cast<double> (i + 1) + 0.02, mel[i], 100);
        track ("melody", r.render (4.5, p));
    }

    // macros: sweep (peak / clicks) and static 100 % (delta + peak)
    for (int m = 0; m < 4; ++m)
    {
        {
            Rig r (rec);
            const char* id = macroIds[static_cast<size_t> (m)];
            track ("macro" + std::to_string (m + 1) + "Sweep",
                   r.render (6.0, chordPhrase (110, 4.5), [&] (double t) { r.set (id, ramp01 (t, 0.5, 3.5)); }));
        }
        Rig r (rec);
        r.set (macroIds[static_cast<size_t> (m)], 1.0f);
        const auto out = r.render (5.5, chordPhrase());
        track ("macro" + std::to_string (m + 1) + "Max", out);
        rep.macroDelta[static_cast<size_t> (m)] = chordDelta (chord, out);
    }
    {
        Rig r (rec);
        for (const char* id : macroIds) r.set (id, 1.0f);
        track ("allMacrosMax", r.render (5.5, chordPhrase()));
    }

    // mod wheel: sweep + static extremes
    {
        Rig r (rec);
        auto p = chordPhrase (110, 4.5);
        addCcRamp (p, 1, 0.5, 3.5, 0, 127);
        track ("wheelSweep", r.render (6.0, p));
    }
    {
        Rig r0 (rec), r1 (rec);
        auto p0 = chordPhrase(), p1 = chordPhrase();
        p0.push_back ({ 0.0, juce::MidiMessage::controllerEvent (1, 1, 0) });
        p1.push_back ({ 0.0, juce::MidiMessage::controllerEvent (1, 1, 127) });
        const auto w0 = r0.render (5.5, p0), w1 = r1.render (5.5, p1);
        track ("wheel0", w0);
        track ("wheel127", w1);
        rep.wheelDelta = chordDelta (w0, w1);
    }

    // channel pressure
    {
        Rig r (rec);
        auto p = chordPhrase();
        p.push_back ({ 0.0, juce::MidiMessage::channelPressureChange (1, 127) });
        const auto out = r.render (5.5, p);
        track ("pressure127", out);
        rep.pressureDelta = chordDelta (chord, out);
    }

    // MPE: member channels 2-5, per-note pressure + slide vs none
    {
        auto mpePhrase = [] (bool expressive) {
            Phrase p;
            for (size_t i = 0; i < kChord.size(); ++i)
            {
                const int ch = 2 + static_cast<int> (i);
                if (expressive)
                {
                    p.push_back ({ 0.0, juce::MidiMessage::channelPressureChange (ch, 0) });
                    p.push_back ({ 0.0, juce::MidiMessage::controllerEvent (ch, 74, 64) });
                    for (double t = 0.3; t <= 1.5; t += 0.02)
                    {
                        const auto u = ramp01 (t, 0.3, 1.5);
                        p.push_back ({ t, juce::MidiMessage::channelPressureChange (ch, static_cast<int> (std::lround (127.0 * u))) });
                        p.push_back ({ t, juce::MidiMessage::controllerEvent (ch, 74, static_cast<int> (std::lround (64.0 + 63.0 * u))) });
                    }
                }
                addNote (p, 0.0, 4.0, kChord[i], 110, ch);
            }
            return p;
        };
        Rig r0 (rec), r1 (rec);
        r0.set (v2MpeEnable, 1.0f);
        r1.set (v2MpeEnable, 1.0f);
        const auto m0 = r0.render (5.5, mpePhrase (false)), m1 = r1.render (5.5, mpePhrase (true));
        track ("mpe", m0);
        track ("mpePressure", m1);
        rep.mpeDelta = chordDelta (m0, m1);
    }

    // audition clip
    {
        Rig r (rec);
        rep.audition = auditionKind (rec);
        std::function<void (double)> autom;
        if (rep.audition == "evolving")
            autom = [&] (double t) { r.set (macroColossus, 0.75f * ramp01 (t, 0.5, 3.4)); };
        const auto out = r.render (4.0, auditionPhrase (rec, rep.audition), autom);
        track ("audition", out);
        if (audition)
        {
            const auto safe = juce::File::createLegalFileName (rec.name).replaceCharacter (' ', '_');
            const auto file = auditionDir.getChildFile (juce::String::formatted ("%02d_", idx + 1) + safe + ".wav");
            if (! writeWav (file, out))
                std::cerr << "audition write failed: " << file.getFullPathName() << "\n";
        }
    }

    rep.cpuSec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    return rep;
}

std::set<int> parseList (const std::string& csv)
{
    std::set<int> out;
    std::stringstream ss (csv);
    std::string tok;
    while (std::getline (ss, tok, ','))
        if (! tok.empty()) out.insert (std::stoi (tok));
    return out;
}

bool sameRecords (const std::vector<monolith::PresetRecord>& a, const std::vector<monolith::PresetRecord>& b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
    {
        auto xa = monolith::preset_schema::toXml (a[i]), xb = monolith::preset_schema::toXml (b[i]);
        if (! xa->isEquivalentTo (xb.get(), false)) return false;
    }
    return true;
}

} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit; // MessageManager only; no windows / devices

    juce::File presetDir;
    std::set<int> only;
    bool audition = true;
    std::string csvPath = std::string (MONOLITH_SOURCE_DIR) + "/build/v2_qa/preset_qa.csv";
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--preset-dir" && i + 1 < argc) presetDir = juce::File (juce::String (argv[++i]));
        else if (a == "--only" && i + 1 < argc)  only = parseList (argv[++i]);
        else if (a == "--no-audition")           audition = false;
        else if (a == "--csv" && i + 1 < argc)   csvPath = argv[++i];
        else if (a == "--rate" && i + 1 < argc)  gSr = std::stod (argv[++i]);
    }
    if (gSr != 48000.0)
    {
        audition = false; // the audition set is the 48 kHz / 24-bit deliverable; never overwrite it
        if (csvPath == std::string (MONOLITH_SOURCE_DIR) + "/build/v2_qa/preset_qa.csv")
            csvPath = std::string (MONOLITH_SOURCE_DIR) + "/build/v2_qa/preset_qa_" + std::to_string (juce::roundToInt (gSr)) + ".csv";
    }

    const auto embedded = monolith::preset_schema::loadFlagshipPresets();
    const auto presets = presetDir == juce::File() ? embedded : monolith::preset_schema::loadFlagshipPresets (presetDir);
    const bool stale = presetDir != juce::File() && ! sameRecords (embedded, presets);

    const auto auditionDir = juce::File (juce::String (MONOLITH_SOURCE_DIR)).getChildFile ("build/v2_qa/audition");
    if (audition) (void) auditionDir.createDirectory();

    // library-level metadata: 20 presets, 5 categories x 4, unique names, host programs 30..49
    std::map<std::string, int> perCat;
    std::set<std::string> names;
    for (const auto& r : presets) { ++perCat[r.category.toStdString()]; names.insert (r.name.toStdString()); }
    bool libOk = presets.size() == 20 && perCat.size() == 5 && names.size() == 20;
    for (const auto& [c, n] : perCat) libOk = libOk && n == 4;
    {
        monolith::MonolithAudioProcessor probe;
        const int v1 = probe.getPresetManager().getNumPresets();
        libOk = libOk && probe.getNumPrograms() == v1 + static_cast<int> (embedded.size());
        if (! embedded.empty())
        {
            probe.setCurrentProgram (v1 + 3);
            libOk = libOk && probe.getCurrentProgram() == v1 + 3 && probe.getProgramName (v1 + 3) == embedded[3].name
                 && probe.getCurrentPresetMeta().isFlagship();
            probe.setCurrentProgram (0);
            libOk = libOk && probe.getCurrentProgram() == 0 && ! probe.getCurrentPresetMeta().isFlagship();
        }
    }

    const TruePeak tp;
    std::vector<Report> reps;
    const auto t0 = std::chrono::steady_clock::now();
    for (size_t i = 0; i < presets.size(); ++i)
    {
        if (! only.empty() && only.count (static_cast<int> (i) + 1) == 0) continue;
        reps.push_back (runPreset (static_cast<int> (i), presets[i], tp, auditionDir, audition));
        std::cerr << "." << std::flush;
    }
    std::cerr << "\n";

    // diversity (only meaningful over the full set)
    double minDup = 1e9;
    std::string dupPair;
    for (size_t a = 0; a < reps.size(); ++a)
        for (size_t b = a + 1; b < reps.size(); ++b)
        {
            const double s = dupScore (reps[a].f, reps[b].f);
            if (s < minDup) { minDup = s; dupPair = std::to_string (reps[a].idx + 1) + "~" + std::to_string (reps[b].idx + 1); }
        }
    const bool divOk = reps.size() < 2 || minDup >= 1.0;

    // table
    std::ostringstream t;
    t << "MONOLITH II PRESET QA | " << fmt (gSr / 1000.0, 1) << " kHz | " << reps.size() << " presets | " << (presetDir == juce::File() ? "embedded" : "dir")
      << (stale ? " (EMBEDDED COPY STALE: rebuild)" : "") << " | "
      << fmt (std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count(), 1) << " s\n";
    t << " #  name                     TPmax  (phrase)        RMS    corC  drC  corC3 drC3   M1   M2   M3   M4  whl  prs  mpe  cent   wid  att  rel  aud        result\n";
    int fails = 0;
    std::ofstream csv (csvPath);
    csv << "index,name,category,true_peak_dbtp,worst_phrase,chord_rms_db,chord_sub150_corr,chord_mono_drop_db,c3_sub150_corr,c3_mono_drop_db,"
           "macro1_db,macro2_db,macro3_db,macro4_db,wheel_db,pressure_db,mpe_db,dc_db,centroid_hz,width,attack_s,release_s,audition,pass\n";
    for (const auto& r : reps)
    {
        std::vector<std::string> why;
        if (r.worstTp > lock5::truePeakDbtp) why.push_back ("tp");
        if (r.chordRms < lock5::chordRmsLo || r.chordRms > lock5::chordRmsHi) why.push_back ("rms");
        if (! r.finite || r.worstDc > lock5::dcDbfs) why.push_back ("clean(dc " + fmt (r.worstDc, 1) + " " + r.worstDcPhrase + ")");
        if (r.chordCorr < lock5::sub150Corr || r.c3Corr < lock5::sub150Corr || r.chordDrop > lock5::monoDropDb || r.c3Drop > lock5::monoDropDb)
            why.push_back ("mono");
        for (int m = 0; m < 4; ++m)
            if (r.macroDelta[static_cast<size_t> (m)] < lock5::macroDeltaDb) why.push_back ("macro" + std::to_string (m + 1));
        if (r.wheelDelta < lock5::wheelDeltaDb) why.push_back ("wheel");
        if (r.pressureDelta < lock5::pressureDeltaDb) why.push_back ("pressure");
        if (r.mpeDelta < lock5::mpeDeltaDb) why.push_back ("mpe");
        if (! r.metaOk) why.push_back ("meta(" + r.metaWhy + ")");
        const bool pass = why.empty();
        fails += pass ? 0 : 1;
        std::string res = pass ? "PASS" : "FAIL";
        for (const auto& w : why) res += " " + w;

        auto name = r.name.substr (0, 23);
        t << std::setw (2) << r.idx + 1 << "  " << std::left << std::setw (23) << name << std::right
          << std::setw (6) << fmt (r.worstTp, 2) << "  " << std::left << std::setw (14) << ("(" + r.worstTpPhrase + ")") << std::right
          << std::setw (6) << fmt (r.chordRms, 1) << std::setw (7) << fmt (r.chordCorr, 2) << std::setw (5) << fmt (r.chordDrop, 1)
          << std::setw (6) << fmt (r.c3Corr, 2) << std::setw (5) << fmt (r.c3Drop, 1);
        for (const double d : r.macroDelta) t << std::setw (5) << fmt (d, 1);
        t << std::setw (5) << fmt (r.wheelDelta, 1) << std::setw (5) << fmt (r.pressureDelta, 1) << std::setw (5) << fmt (r.mpeDelta, 1)
          << std::setw (6) << fmt (r.f.centroid, 0) << std::setw (6) << fmt (r.f.width, 2) << std::setw (5) << fmt (r.f.attack, 2)
          << std::setw (5) << fmt (r.f.release, 2) << "  " << std::left << std::setw (11) << r.audition << std::right << res << "\n";

        csv << r.idx + 1 << ",\"" << r.name << "\",\"" << r.category << "\"," << fmt (r.worstTp, 3) << ',' << r.worstTpPhrase << ','
            << fmt (r.chordRms, 3) << ',' << fmt (r.chordCorr, 4) << ',' << fmt (r.chordDrop, 3) << ',' << fmt (r.c3Corr, 4) << ','
            << fmt (r.c3Drop, 3);
        for (const double d : r.macroDelta) csv << ',' << fmt (d, 3);
        csv << ',' << fmt (r.wheelDelta, 3) << ',' << fmt (r.pressureDelta, 3) << ',' << fmt (r.mpeDelta, 3) << ',' << fmt (r.worstDc, 1)
            << ',' << fmt (r.f.centroid, 1) << ',' << fmt (r.f.width, 4) << ',' << fmt (r.f.attack, 3) << ',' << fmt (r.f.release, 3)
            << ',' << r.audition << ',' << (pass ? 1 : 0) << '\n';
    }
    const int passes = static_cast<int> (reps.size()) - fails;
    t << "diversity: min near-duplicate score " << fmt (minDup, 2) << " (" << dupPair << ", needs >= 1.00) " << (divOk ? "PASS" : "FAIL") << "\n";
    t << "library: 20 presets, 5 categories x 4, programs 30..49 " << (libOk ? "PASS" : "FAIL") << "\n";
    t << "TOTAL presets pass " << passes << " / " << reps.size() << " | diversity " << (divOk ? "PASS" : "FAIL")
      << " | library " << (libOk ? "PASS" : "FAIL") << (audition ? " | audition -> build/v2_qa/audition/" : "") << "\n";
    std::cout << t.str();
    return (fails == 0 && divOk && libOk && ! stale) ? 0 : 1;
}
