#include "OutputAnalyser.h"
#include <algorithm>
#include <cmath>

namespace monolith::ui
{

namespace
{
    [[nodiscard]] inline float toDb (double ms) noexcept
    {
        return ms > 1.0e-12 ? static_cast<float> (10.0 * std::log10 (ms)) : OutputAnalyser::kFloorDb;
    }
}

OutputAnalyser::OutputAnalyser()
{
    spectrum.fill (kFloorDb);
    envelope.fill (kFloorDb);
    prepare (48000.0);
}

float OutputAnalyser::bandHz (int band) noexcept
{
    const float t = static_cast<float> (band) / static_cast<float> (kNumBands - 1);
    return kMinHz * std::pow (kMaxHz / kMinHz, t);
}

float OutputAnalyser::hzToNorm (float hz) noexcept
{
    return std::log (std::max (hz, 1.0f) / kMinHz) / std::log (kMaxHz / kMinHz);
}

void OutputAnalyser::prepare (double sampleRate)
{
    sr = sampleRate > 1000.0 ? sampleRate : 48000.0;
    scratchL.assign (static_cast<size_t> (v2::AudioTap::kCapacity), 0.0f);
    scratchR.assign (static_cast<size_t> (v2::AudioTap::kCapacity), 0.0f);
    history.assign (static_cast<size_t> (kFftSize), 0.0f);
    historyPos = 0;
    framesSeen = 0;
    window.resize (static_cast<size_t> (kFftSize));
    for (int i = 0; i < kFftSize; ++i)
        window[static_cast<size_t> (i)] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * static_cast<float> (i) / static_cast<float> (kFftSize - 1));
    fftData.assign (static_cast<size_t> (kFftSize * 2), 0.0f);

    // Band -> FFT bin spans (each band covers at least its nearest bin)
    const double binHz = sr / kFftSize;
    for (int b = 0; b < kNumBands; ++b)
    {
        const double lo = bandHz (b) * std::pow (2.0, -0.5 / 18.6);
        const double hi = bandHz (b) * std::pow (2.0, 0.5 / 18.6);
        int b0 = static_cast<int> (std::floor (lo / binHz));
        int b1 = static_cast<int> (std::ceil (hi / binHz));
        b0 = std::clamp (b0, 1, kFftSize / 2 - 1);
        b1 = std::clamp (b1, b0, kFftSize / 2 - 1);
        bandBins[static_cast<size_t> (b)] = { b0, b1 };
    }

    // ITU-R BS.1770 K-weighting at the actual rate (pre-filter shelf + RLB high-pass)
    {
        const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
        const double K = std::tan (juce::MathConstants<double>::pi * f0 / sr);
        const double Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;
        Biquad s;
        s.b0 = (Vh + Vb * K / Q + K * K) / a0;
        s.b1 = 2.0 * (K * K - Vh) / a0;
        s.b2 = (Vh - Vb * K / Q + K * K) / a0;
        s.a1 = 2.0 * (K * K - 1.0) / a0;
        s.a2 = (1.0 - K / Q + K * K) / a0;
        kShelf = { s, s };
    }
    {
        const double f0 = 38.13547087602444, Q = 0.5003270373238773;
        const double K = std::tan (juce::MathConstants<double>::pi * f0 / sr);
        const double a0 = 1.0 + K / Q + K * K;
        Biquad h;
        h.b0 = 1.0; h.b1 = -2.0; h.b2 = 1.0;
        h.a1 = 2.0 * (K * K - 1.0) / a0;
        h.a2 = (1.0 - K / Q + K * K) / a0;
        kHighPass = { h, h };
    }
    blockLen = std::max (1, static_cast<int> (std::lround (sr * 0.1)));
    blockAccum = 0.0;
    blockCount = 0;
    blockMs.fill (0.0);
    blockWrite = blocksFilled = 0;
}

void OutputAnalyser::process (v2::AudioTap& tap, double dt)
{
    dt = std::clamp (dt, 0.0, 0.25);
    const int n = tap.pull (scratchL.data(), scratchR.data(), static_cast<int> (scratchL.size()));
    freshAudio = n > 0;

    // ── Peaks / clip ────────────────────────────────────────────────────
    std::array<float, 2> pk { 0.0f, 0.0f };
    for (int i = 0; i < n; ++i)
    {
        const float l = scratchL[static_cast<size_t> (i)], r = scratchR[static_cast<size_t> (i)];
        pk[0] = std::max (pk[0], std::abs (l));
        pk[1] = std::max (pk[1], std::abs (r));

        // Loudness (K-weighted mean square, 100 ms blocks)
        const double kl = kHighPass[0].process (kShelf[0].process (l));
        const double kr = kHighPass[1].process (kShelf[1].process (r));
        blockAccum += kl * kl + kr * kr;
        if (++blockCount >= blockLen)
        {
            blockMs[static_cast<size_t> (blockWrite)] = blockAccum / blockLen;
            blockWrite = (blockWrite + 1) % static_cast<int> (blockMs.size());
            blocksFilled = std::min (blocksFilled + 1, static_cast<int> (blockMs.size()));
            blockAccum = 0.0;
            blockCount = 0;
        }

        history[static_cast<size_t> (historyPos)] = 0.5f * (l + r);
        historyPos = (historyPos + 1) & (kFftSize - 1);
    }
    framesSeen += n;

    for (size_t c = 0; c < 2; ++c)
    {
        const float db = pk[c] > 1.0e-6f ? 20.0f * std::log10 (pk[c]) : kFloorDb;
        if (pk[c] >= 0.99943f) // -0.005 dBFS
            clipLatched = true;
        barDb[c] = db >= barDb[c] ? db : std::max (db, barDb[c] - static_cast<float> (26.0 * dt));
        if (db >= holdDb[c]) { holdDb[c] = db; holdTimer[c] = 1.2; }
        else if ((holdTimer[c] -= dt) <= 0.0) holdDb[c] = std::max (kFloorDb, holdDb[c] - static_cast<float> (14.0 * dt));
        maxPeakDb = std::max (maxPeakDb, db);
    }

    auto meanBlocks = [this] (int count) {
        const int c = std::min (count, blocksFilled);
        if (c <= 0) return 0.0;
        double s = 0.0;
        for (int i = 1; i <= c; ++i)
            s += blockMs[static_cast<size_t> ((blockWrite - i + static_cast<int> (blockMs.size())) % static_cast<int> (blockMs.size()))];
        return s / c;
    };
    momentary = std::max (kFloorDb, -0.691f + toDb (meanBlocks (4)));
    shortTerm = std::max (kFloorDb, -0.691f + toDb (meanBlocks (30)));

    const bool loud = std::max (pk[0], pk[1]) > 1.0e-4f; // -80 dBFS
    silentFor = loud ? 0.0 : silentFor + dt;
    signalPresent = silentFor < 0.6;

    analyseSpectrum (dt);
}

void OutputAnalyser::analyseSpectrum (double dt)
{
    if (freshAudio && framesSeen >= kFftSize / 2)
    {
        for (int i = 0; i < kFftSize; ++i)
            fftData[static_cast<size_t> (i)] = history[static_cast<size_t> ((historyPos + i) & (kFftSize - 1))] * window[static_cast<size_t> (i)];
        std::fill (fftData.begin() + kFftSize, fftData.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

        const float norm = 2.0f / (0.5f * static_cast<float> (kFftSize)); // Hann coherent gain
        for (int b = 0; b < kNumBands; ++b)
        {
            const auto [b0, b1] = bandBins[static_cast<size_t> (b)];
            float m = 0.0f;
            for (int k = b0; k <= b1; ++k)
                m = std::max (m, fftData[static_cast<size_t> (k)]);
            const float db = std::max (kFloorDb, 20.0f * std::log10 (std::max (1.0e-9f, m * norm)));
            float& s = spectrum[static_cast<size_t> (b)];
            s = db > s ? s + (db - s) * 0.65f : std::max (db, s - static_cast<float> (40.0 * dt));
        }
    }
    else
    {
        for (auto& s : spectrum)
            s = std::max (kFloorDb, s - static_cast<float> (40.0 * dt));
    }

    // 1/3-octave spectral envelope (raised-cosine kernel across +/- 6 bands)
    constexpr int kHalf = 6;
    for (int b = 0; b < kNumBands; ++b)
    {
        double acc = 0.0, wsum = 0.0;
        for (int o = -kHalf; o <= kHalf; ++o)
        {
            const int k = std::clamp (b + o, 0, kNumBands - 1);
            const double w = 0.5 + 0.5 * std::cos (juce::MathConstants<double>::pi * o / (kHalf + 1));
            acc += w * std::pow (10.0, spectrum[static_cast<size_t> (k)] / 20.0);
            wsum += w;
        }
        envelope[static_cast<size_t> (b)] = std::max (kFloorDb, static_cast<float> (20.0 * std::log10 (std::max (1.0e-9, acc / wsum))));
    }
    findFormants();
}

void OutputAnalyser::findFormants()
{
    numFormants = 0;
    if (! signalPresent)
        return;

    float top = kFloorDb;
    for (float e : envelope) top = std::max (top, e);

    struct Peak { int band; float db; };
    std::array<Peak, 16> peaks {};
    int np = 0;
    for (int b = 2; b < kNumBands - 2 && np < static_cast<int> (peaks.size()); ++b)
    {
        const float hz = bandHz (b);
        if (hz < 180.0f || hz > 5500.0f)
            continue;
        const float e = envelope[static_cast<size_t> (b)];
        if (e > envelope[static_cast<size_t> (b - 1)] && e >= envelope[static_cast<size_t> (b + 1)]
            && e > envelope[static_cast<size_t> (b - 2)] && e >= envelope[static_cast<size_t> (b + 2)] && e > top - 30.0f)
            peaks[static_cast<size_t> (np++)] = { b, e };
    }
    std::sort (peaks.begin(), peaks.begin() + np, [] (const Peak& a, const Peak& b) { return a.db > b.db; });

    std::array<Peak, 4> chosen {};
    int nc = 0;
    for (int i = 0; i < np && nc < 4; ++i)
    {
        bool clear = true;
        for (int j = 0; j < nc; ++j)
            clear = clear && std::abs (chosen[static_cast<size_t> (j)].band - peaks[static_cast<size_t> (i)].band) >= 6; // >= 1/3 octave apart
        if (clear)
            chosen[static_cast<size_t> (nc++)] = peaks[static_cast<size_t> (i)];
    }
    std::sort (chosen.begin(), chosen.begin() + nc, [] (const Peak& a, const Peak& b) { return a.band < b.band; });
    for (int i = 0; i < nc; ++i)
        formants[static_cast<size_t> (i)] = { bandHz (chosen[static_cast<size_t> (i)].band), chosen[static_cast<size_t> (i)].db };
    numFormants = nc;
}

} // namespace monolith::ui
