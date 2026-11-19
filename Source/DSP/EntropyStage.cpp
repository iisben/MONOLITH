#include "EntropyStage.h"
#include <algorithm>
#include <cmath>

namespace monolith::dsp
{

void EntropyStage::prepare (double sampleRate, int maxBlockSize, uint32_t voiceSeed)
{
    sr = std::max (8000.0, sampleRate);
    preparedMaxBlock = std::max (32, maxBlockSize);
    seed = (voiceSeed != 0u) ? voiceSeed : 1u;

    os2x = std::make_unique<juce::dsp::Oversampling<float>> (
        2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    os4x = std::make_unique<juce::dsp::Oversampling<float>> (
        2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);

    os2x->initProcessing (static_cast<size_t> (preparedMaxBlock));
    os4x->initProcessing (static_cast<size_t> (preparedMaxBlock));

    scratchBuffer.setSize (2, preparedMaxBlock, false, true, true);

    dcL.prepare (sr);
    dcR.prepare (sr);

    // Reserve +/- 28 ms tape delay modulation capacity cleanly across 44.1-192 kHz
    const size_t cap = static_cast<size_t> (sr * 0.04) + 64;
    delayL.prepare (cap);
    delayR.prepare (cap);

    flutterNoise.seed (seed * 747796405u + 2891336453u);

    // Deterministic unique wow/flutter frequencies per voice so chords drift organically
    const float u0 = static_cast<float> ((seed * 1664525u + 1013904223u) & 0xFFFFu) / 65535.0f;
    const float u1 = static_cast<float> ((seed * 22695477u + 1u) & 0xFFFFu) / 65535.0f;
    wowRateHz     = 0.42f + 0.36f * u0;
    flutterRateHz = 6.20f + 4.30f * u1;

    wowPhase     = u0 * kTwoPi;
    flutterPhase = u1 * kTwoPi;

    reset();
}

void EntropyStage::reset() noexcept
{
    if (os2x) os2x->reset();
    if (os4x) os4x->reset();
    scratchBuffer.clear();

    dcL.reset();
    dcR.reset();
    delayL.reset();
    delayR.reset();
    flutterNoise.resetFilter();

    oxideLpL.reset();
    oxideLpR.reset();
    headBumpBpL.reset();
    headBumpBpR.reset();
    oxideLpL.invalidateCache();
    oxideLpR.invalidateCache();
    headBumpBpL.invalidateCache();
    headBumpBpR.invalidateCache();
    smoothedFlutter = 0.0f;
}

int EntropyStage::getLatencySamples (bool highQuality4x) const noexcept
{
    if (highQuality4x && os4x)
        return static_cast<int> (std::lround (os4x->getLatencyInSamples()));
    if (os2x)
        return static_cast<int> (std::lround (os2x->getLatencyInSamples()));
    return 0;
}

void EntropyStage::processBlock (float* left,
                                 float* right,
                                 int numSamples,
                                 float drive01,
                                 float tapeInstability01,
                                 float oxideDamping01,
                                 bool highQuality4x) noexcept
{
    const int n = std::min (numSamples, preparedMaxBlock);
    if (n <= 0)
        return;

    const float drive = std::clamp (drive01, 0.0f, 1.0f);
    const float tape  = std::clamp (tapeInstability01, 0.0f, 1.0f);
    const float oxide = std::clamp (oxideDamping01, 0.0f, 1.0f);

    // ── 1. Per-Voice Pre-Sum Oversampled Asymmetric Tape/Tube Saturation ────
    // Always process through the active oversampler so plugin latency remains constant across drive=0..1.
    {
        float* scratchL = scratchBuffer.getWritePointer (0);
        float* scratchR = scratchBuffer.getWritePointer (1);
        for (int i = 0; i < n; ++i)
        {
            scratchL[i] = left[i];
            scratchR[i] = right[i];
        }

        float* chans[2] = { scratchL, scratchR };
        juce::dsp::AudioBlock<float> block (chans, 2, static_cast<size_t> (n));
        auto* os = (highQuality4x && os4x) ? os4x.get() : os2x.get();

        if (os != nullptr)
        {
            auto osBlock = os->processSamplesUp (block);

            if (drive > 1.0e-4f)
            {
                const size_t osLen = osBlock.getNumSamples();
                const float g = 1.0f + drive * 7.5f;
                const float b = drive * 0.22f;
                const float biasOffset = std::tanh (b);
                const float normScale = 1.0f / (1.0f + 4.8f * drive);
                const float wet = std::clamp (drive * 20.0f, 0.0f, 1.0f);

                for (size_t ch = 0; ch < 2; ++ch)
                {
                    float* data = osBlock.getChannelPointer (ch);
                    for (size_t i = 0; i < osLen; ++i)
                    {
                        const float x = data[i];
                        const float shaped = (fastTanh (g * x + b) - biasOffset) * normScale;
                        data[i] = x + wet * (shaped - x);
                    }
                }
            }

            os->processSamplesDown (block);
        }

        for (int i = 0; i < n; ++i)
        {
            left[i]  = dcL.process (scratchL[i]);
            right[i] = dcR.process (scratchR[i]);
        }
    }

    // ── 2. Musical Tape Wow & Flutter (Cubic Interpolated Modulated Delay) ──
    const float srF = static_cast<float> (sr);
    if (tape > 1.0e-4f)
    {
        const float wowInc     = kTwoPi * wowRateHz / srF;
        const float flutterInc = kTwoPi * flutterRateHz / srF;

        // Smooth blend ensures tape == 0 is clean and rising from 0 never clicks
        const float tapeMix = std::clamp (tape * 18.0f, 0.0f, 1.0f);
        const float baseDelaySamples = 4.0f + 0.0065f * srF * tape;
        const float wowDepthSamples  = 0.0038f * srF * tape * tape;
        const float flutDepthSamples = 0.00055f * srF * tape;

        for (int i = 0; i < n; ++i)
        {
            const float inL = left[i];
            const float inR = right[i];
            delayL.push (inL);
            delayR.push (inR);

            wowPhase += wowInc;
            if (wowPhase >= kTwoPi) wowPhase -= kTwoPi;
            flutterPhase += flutterInc;
            if (flutterPhase >= kTwoPi) flutterPhase -= kTwoPi;

            // Perturb flutter slightly with smoothed pink noise for organic cassette warble
            smoothedFlutter += 0.015f * (flutterNoise.nextPink() - smoothedFlutter);

            const float wowL = std::sin (wowPhase);
            const float wowR = std::sin (wowPhase + 0.65f);
            const float fltL = std::sin (flutterPhase) + 0.45f * smoothedFlutter;
            const float fltR = std::cos (flutterPhase * 1.07f) - 0.45f * smoothedFlutter;

            const float dL = baseDelaySamples + wowDepthSamples * wowL + flutDepthSamples * fltL;
            const float dR = baseDelaySamples + wowDepthSamples * wowR + flutDepthSamples * fltR;

            const float wetL = delayL.readCubic (dL);
            const float wetR = delayR.readCubic (dR);

            left[i]  = inL + tapeMix * (wetL - inL);
            right[i] = inR + tapeMix * (wetR - inR);
        }
    }
    else
    {
        // Keep delay history warm so enabling Tape Drift mid-note never pops
        for (int i = 0; i < n; ++i)
        {
            delayL.push (left[i]);
            delayR.push (right[i]);
        }
    }

    // ── 3. Oxide Damping & Magnetic Head Bump ───────────────────────────────
    if (oxide > 1.0e-4f)
    {
        // Logarithmic sweep 19.5 kHz -> 1.6 kHz
        const float cutoff = 19500.0f * std::pow (1600.0f / 19500.0f, oxide);
        oxideLpL.setParameters (cutoff, 0.72f, srF);
        oxideLpR.setParameters (cutoff, 0.72f, srF);

        headBumpBpL.setParameters (240.0f, 0.90f, srF);
        headBumpBpR.setParameters (240.0f, 0.90f, srF);

        const float bumpGain = 0.28f * oxide;
        const float oxideWet = std::clamp (oxide * 22.0f, 0.0f, 1.0f);

        for (int i = 0; i < n; ++i)
        {
            const float dryL = left[i];
            const float dryR = right[i];

            const float dampedL = oxideLpL.processLowpass (dryL + bumpGain * headBumpBpL.processBandpass (dryL));
            const float dampedR = oxideLpR.processLowpass (dryR + bumpGain * headBumpBpR.processBandpass (dryR));

            left[i]  = sanitize (dryL + oxideWet * (dampedL - dryL));
            right[i] = sanitize (dryR + oxideWet * (dampedR - dryR));
        }
    }
}

} // namespace monolith::dsp
