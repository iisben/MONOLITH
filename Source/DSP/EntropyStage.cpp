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
    flutterNoise.prepare (sr);

    const float u0 = static_cast<float> ((seed * 1664525u + 1013904223u) & 0xFFFFu) / 65535.0f;
    const float u1 = static_cast<float> ((seed * 22695477u + 1u) & 0xFFFFu) / 65535.0f;
    wowRateHz     = 0.42f + 0.36f * u0;
    flutterRateHz = 6.20f + 4.30f * u1;

    wowLfo.setHz (static_cast<double> (wowRateHz), sr);
    wowLfo.setPhaseRad (static_cast<double> (u0) * kTwoPiD);
    flutterLfo.setHz (static_cast<double> (flutterRateHz), sr);
    flutterLfo.setPhaseRad (static_cast<double> (u1) * kTwoPiD);
    // Independent 1.07× oscillator. Multiplying a wrapped phase by 1.07 jumps
    // once per cycle; a second quadrature does not.
    flutterSideLfo.setHz (static_cast<double> (flutterRateHz) * 1.07, sr);
    flutterSideLfo.setPhaseRad (static_cast<double> (u1) * kTwoPiD * 1.07);

    // Match the legacy one-pole (pole 0.985 at 48 kHz) at every sample rate.
    const double flutterPole = std::exp (std::log (0.985) * (48000.0 / sr));
    flutterSmooth = static_cast<float> (1.0 - flutterPole);

    const int lat2 = (os2x != nullptr) ? static_cast<int> (std::lround (os2x->getLatencyInSamples())) : 0;
    const int lat4 = (os4x != nullptr) ? static_cast<int> (std::lround (os4x->getLatencyInSamples())) : 0;
    const int maxLat = std::max (lat2, lat4);
    align2.assign (maxLat - lat2);
    align4.assign (maxLat - lat4);
    reportedLatency = maxLat;

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
    emph2.reset();
    emph4.reset();
    align2.reset();
    align4.reset();
    smoothedFlutter = 0.0f;
    smootherPrimed  = false;
    osXfade         = 1.0f;
}

int EntropyStage::getLatencySamples (bool) const noexcept
{
    return reportedLatency;
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

    // Trigger click-free 15 ms crossfade when toggling 2x <-> 4x oversampling live.
    // Only the path being engaged is cleared; its align delay starts empty so the
    // fade-in does not replay stale samples.
    if (highQuality4x != lastHq4x)
    {
        if (highQuality4x)
        {
            if (os4x) os4x->reset();
            emph4.reset();
            align4.reset();
        }
        else
        {
            if (os2x) os2x->reset();
            emph2.reset();
            align2.reset();
        }
        lastHq4x = highQuality4x;
        osXfade  = 0.0f;
    }

    const float paramCoeff = 1.0f - std::exp (-1.0f / (0.020f * srF));

    // HF shelf: H = 1 + k (1 - L), L a unity-DC one-pole at kEmphHz.
    // Post coefficient kInv = -k / (1 + β k), β = 2p/(1+p), inverts H when the
    // waveshaper is linear, so small-signal level is unchanged.
    constexpr double kEmph   = 0.18;
    constexpr double kEmphHz = 7200.0;

    auto runOversampledDrive = [&] (juce::dsp::Oversampling<float>* os,
                                    juce::AudioBuffer<float>& buf,
                                    Emphasis& emph,
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
        const double osSr = sr * static_cast<double> (factor);
        const double pole = std::exp (-kTwoPiD * kEmphHz / osSr);
        const double beta = (2.0 * pole) / (1.0 + pole);
        const double kInv = -kEmph / (1.0 + beta * kEmph);
        const double envA = 1.0 - std::exp (-1.0 / (0.012 * osSr));
        const double oneMinusPole = 1.0 - pole;

        float curDrive = startDrive;
        if (curDrive > 1.0e-5f || targetDrive > 1.0e-5f)
        {
            float* dataL = osBlock.getChannelPointer (0);
            float* dataR = osBlock.getChannelPointer (1);

            auto shape = [&] (float x, double& preS, double& postS, double& env,
                              float g, float b, float normScale) noexcept -> float
            {
                const double xd = static_cast<double> (x);
                preS = pole * preS + oneMinusPole * xd;
                const double pre = xd + kEmph * (xd - preS);

                env += envA * (std::abs (pre) - env);
                if (! std::isfinite (env))
                    env = 0.0;

                const float bDyn = b * (1.0f + 0.20f * std::min (1.0f, static_cast<float> (env)));
                const float y = (fastTanh (g * static_cast<float> (pre) + bDyn) - fastTanh (bDyn)) * normScale;
                const double yd = static_cast<double> (y);
                postS = pole * postS + oneMinusPole * yd;
                const double post = yd + kInv * (yd - postS);
                preS = finiteOrZero (preS);
                postS = finiteOrZero (postS);
                return static_cast<float> (finiteOrZero (post));
            };

            for (size_t i = 0; i < osLen; ++i)
            {
                curDrive += osCoeff * (targetDrive - curDrive);
                const float wet = std::clamp (curDrive * 20.0f, 0.0f, 1.0f);
                const float g = 1.0f + curDrive * 7.5f;
                const float b = curDrive * 0.22f;
                const float normScale = 1.0f / (1.0f + 4.8f * curDrive);

                const float xL = dataL[i];
                const float xR = dataR[i];
                const float yL = shape (xL, emph.preL, emph.postL, emph.envL, g, b, normScale);
                const float yR = shape (xR, emph.preR, emph.postR, emph.envR, g, b, normScale);
                dataL[i] = sanitize (xL + wet * (yL - xL));
                dataR[i] = sanitize (xR + wet * (yR - xR));
            }
        }
        else
        {
            emph.reset();
        }

        os->processSamplesDown (block);
        return curDrive;
    };

    // ── 1. Per-Voice Pre-Sum Oversampled Asymmetric Tape/Tube Saturation ────
    const bool activeIs4x = highQuality4x && os4x;
    auto* activeOs = activeIs4x ? os4x.get() : os2x.get();
    Emphasis& activeEmph = activeIs4x ? emph4 : emph2;
    const float driveBefore = smDrive;
    smDrive = runOversampledDrive (activeOs, scratchBuffer, activeEmph, driveBefore);

    float* scratchL = scratchBuffer.getWritePointer (0);
    float* scratchR = scratchBuffer.getWritePointer (1);
    (activeIs4x ? align4 : align2).process (scratchL, scratchR, n);

    if (osXfade < 1.0f)
    {
        const bool prevIs4x = ! activeIs4x;
        auto* prevOs = prevIs4x ? os4x.get() : os2x.get();
        Emphasis& prevEmph = prevIs4x ? emph4 : emph2;
        (void) runOversampledDrive (prevOs, xfadeScratchBuffer, prevEmph, driveBefore);
        float* prevL = xfadeScratchBuffer.getWritePointer (0);
        float* prevR = xfadeScratchBuffer.getWritePointer (1);
        (prevIs4x ? align4 : align2).process (prevL, prevR, n);

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
    // Slower smoother for tape delay position avoids pitch-jump clicks on rapid knob turns
    const float tapeDelayCoeff = 1.0f - std::exp (-1.0f / (0.045f * srF));
    // sin(θ + 0.65) = sin θ · cos 0.65 + cos θ · sin 0.65
    constexpr float kWowOffSin = 0.6051864057f;
    constexpr float kWowOffCos = 0.7960837985f;

    if (smTape > 1.0e-5f || targetTape > 1.0e-5f)
    {
        for (int i = 0; i < n; ++i)
        {
            smTape += tapeDelayCoeff * (targetTape - smTape);

            const float inL = left[i];
            const float inR = right[i];
            delayL.push (inL);
            delayR.push (inR);

            wowLfo.tick();
            flutterLfo.tick();
            flutterSideLfo.tick();

            smoothedFlutter += flutterSmooth * (flutterNoise.nextPink() - smoothedFlutter);

            const float tapeMix          = std::clamp (smTape * 18.0f, 0.0f, 1.0f);
            const float baseDelaySamples = 4.0f + 0.0065f * srF * smTape;
            const float wowDepthSamples  = 0.0038f * srF * smTape * smTape;
            const float flutDepthSamples = 0.00055f * srF * smTape;

            const float wowL = wowLfo.sinOut();
            const float wowR = wowL * kWowOffCos + wowLfo.cosOut() * kWowOffSin;
            const float fltL = flutterLfo.sinOut() + 0.45f * smoothedFlutter;
            const float fltR = flutterSideLfo.cosOut() - 0.45f * smoothedFlutter;

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
        // 19500 * (1600/19500)^x = 19500 * exp(x * ln(1600/19500)); log is invariant.
        static const float kOxideLog = std::log (1600.0f / 19500.0f);

        for (int i = 0; i < n; ++i)
        {
            smOxide += paramCoeff * (targetOxide - smOxide);

            if ((i % ctrlStep) == 0)
            {
                const float cutoff = 19500.0f * std::exp (kOxideLog * smOxide);
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
