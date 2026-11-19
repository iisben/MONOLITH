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
    xfadeScratchBuffer.setSize (2, preparedMaxBlock, false, true, true);

    dcL.prepare (sr);
    dcR.prepare (sr);

    const size_t cap = static_cast<size_t> (sr * 0.04) + 64;
    delayL.prepare (cap);
    delayR.prepare (cap);

    flutterNoise.seed (seed * 747796405u + 2891336453u);

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
    xfadeScratchBuffer.clear();

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
    smootherPrimed  = false;
    osXfade         = 1.0f;
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

    const float srF        = static_cast<float> (sr);
    const float targetDrive = std::clamp (drive01, 0.0f, 1.0f);
    const float targetTape  = std::clamp (tapeInstability01, 0.0f, 1.0f);
    const float targetOxide = std::clamp (oxideDamping01, 0.0f, 1.0f);

    if (! smootherPrimed)
    {
        smDrive        = targetDrive;
        smTape         = targetTape;
        smOxide        = targetOxide;
        lastHq4x       = highQuality4x;
        osXfade        = 1.0f;
        smootherPrimed = true;
    }

    // Trigger click-free 15 ms crossfade when toggling 2x <-> 4x oversampling live
    if (highQuality4x != lastHq4x)
    {
        if (highQuality4x && os4x) os4x->reset();
        if (! highQuality4x && os2x) os2x->reset();
        lastHq4x = highQuality4x;
        osXfade  = 0.0f;
    }

    const float paramCoeff = 1.0f - std::exp (-1.0f / (0.020f * srF));

    // Helper lambda to run asymmetric tube/tape drive inside an oversampling stage
    auto runOversampledDrive = [&] (juce::dsp::Oversampling<float>* os,
                                    juce::AudioBuffer<float>& buf,
                                    float startDrive) noexcept
    {
        if (os == nullptr)
            return startDrive;

        float* sL = buf.getWritePointer (0);
        float* sR = buf.getWritePointer (1);
        for (int i = 0; i < n; ++i)
        {
            sL[i] = left[i];
            sR[i] = right[i];
        }

        float* chans[2] = { sL, sR };
        juce::dsp::AudioBlock<float> block (chans, 2, static_cast<size_t> (n));
        auto osBlock = os->processSamplesUp (block);

        const size_t osLen = osBlock.getNumSamples();
        const float factor = static_cast<float> (os->getOversamplingFactor());
        const float osCoeff = 1.0f - std::exp (-1.0f / (0.020f * srF * factor));

        float curDrive = startDrive;
        if (curDrive > 1.0e-5f || targetDrive > 1.0e-5f)
        {
            float* dataL = osBlock.getChannelPointer (0);
            float* dataR = osBlock.getChannelPointer (1);
            for (size_t i = 0; i < osLen; ++i)
            {
                curDrive += osCoeff * (targetDrive - curDrive);
                const float wet = std::clamp (curDrive * 20.0f, 0.0f, 1.0f);
                if (wet > 1.0e-5f)
                {
                    const float g = 1.0f + curDrive * 7.5f;
                    const float b = curDrive * 0.22f;
                    const float biasOffset = fastTanh (b);
                    const float normScale  = 1.0f / (1.0f + 4.8f * curDrive);

                    const float xL = dataL[i];
                    const float xR = dataR[i];
                    const float yL = (fastTanh (g * xL + b) - biasOffset) * normScale;
                    const float yR = (fastTanh (g * xR + b) - biasOffset) * normScale;
                    dataL[i] = xL + wet * (yL - xL);
                    dataR[i] = xR + wet * (yR - xR);
                }
            }
        }

        os->processSamplesDown (block);
        return curDrive;
    };

    // ── 1. Per-Voice Pre-Sum Oversampled Asymmetric Tape/Tube Saturation ────
    auto* activeOs = (highQuality4x && os4x) ? os4x.get() : os2x.get();
    const float driveBefore = smDrive;
    smDrive = runOversampledDrive (activeOs, scratchBuffer, driveBefore);

    float* scratchL = scratchBuffer.getWritePointer (0);
    float* scratchR = scratchBuffer.getWritePointer (1);

    if (osXfade < 1.0f)
    {
        auto* prevOs = (highQuality4x && os2x) ? os2x.get() : os4x.get();
        (void) runOversampledDrive (prevOs, xfadeScratchBuffer, driveBefore);
        const float* prevL = xfadeScratchBuffer.getReadPointer (0);
        const float* prevR = xfadeScratchBuffer.getReadPointer (1);
        const float xfadeInc = 1.0f / std::max (0.015f * srF, 16.0f);

        for (int i = 0; i < n; ++i)
        {
            osXfade = std::min (1.0f, osXfade + xfadeInc);
            const float mixL = prevL[i] + osXfade * (scratchL[i] - prevL[i]);
            const float mixR = prevR[i] + osXfade * (scratchR[i] - prevR[i]);
            left[i]  = dcL.process (mixL);
            right[i] = dcR.process (mixR);
        }
    }
    else
    {
        for (int i = 0; i < n; ++i)
        {
            left[i]  = dcL.process (scratchL[i]);
            right[i] = dcR.process (scratchR[i]);
        }
    }

    // ── 2. Musical Tape Wow & Flutter (Smoothed Cubic Modulated Delay) ──────
    const float wowInc     = kTwoPi * wowRateHz / srF;
    const float flutterInc = kTwoPi * flutterRateHz / srF;
    // Slower smoother for tape delay position avoids pitch-jump clicks on rapid knob turns
    const float tapeDelayCoeff = 1.0f - std::exp (-1.0f / (0.045f * srF));

    if (smTape > 1.0e-5f || targetTape > 1.0e-5f)
    {
        for (int i = 0; i < n; ++i)
        {
            smTape += tapeDelayCoeff * (targetTape - smTape);

            const float inL = left[i];
            const float inR = right[i];
            delayL.push (inL);
            delayR.push (inR);

            wowPhase += wowInc;
            if (wowPhase >= kTwoPi) wowPhase -= kTwoPi;
            flutterPhase += flutterInc;
            if (flutterPhase >= kTwoPi) flutterPhase -= kTwoPi;

            smoothedFlutter += 0.015f * (flutterNoise.nextPink() - smoothedFlutter);

            const float tapeMix          = std::clamp (smTape * 18.0f, 0.0f, 1.0f);
            const float baseDelaySamples = 4.0f + 0.0065f * srF * smTape;
            const float wowDepthSamples  = 0.0038f * srF * smTape * smTape;
            const float flutDepthSamples = 0.00055f * srF * smTape;

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
        for (int i = 0; i < n; ++i)
        {
            delayL.push (left[i]);
            delayR.push (right[i]);
        }
    }

    // ── 3. Oxide Damping & Magnetic Head Bump (Per-Sample Smoothed) ─────────
    if (smOxide > 1.0e-5f || targetOxide > 1.0e-5f)
    {
        headBumpBpL.setParameters (240.0f, 0.90f, srF);
        headBumpBpR.setParameters (240.0f, 0.90f, srF);

        const int ctrlStep = std::max (8, static_cast<int> (srF / 3000.0f));

        for (int i = 0; i < n; ++i)
        {
            smOxide += paramCoeff * (targetOxide - smOxide);

            if ((i % ctrlStep) == 0)
            {
                const float cutoff = 19500.0f * std::pow (1600.0f / 19500.0f, smOxide);
                oxideLpL.setParameters (cutoff, 0.72f, srF);
                oxideLpR.setParameters (cutoff, 0.72f, srF);
            }

            const float bumpGain = 0.28f * smOxide;
            const float oxideWet = std::clamp (smOxide * 22.0f, 0.0f, 1.0f);

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
