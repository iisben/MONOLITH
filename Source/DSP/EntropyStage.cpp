#include "EntropyStage.h"
#include <algorithm>
#include <cmath>

namespace monolith::dsp
{

namespace
{
    /** Measures the integer impulse-peak latency of a JUCE oversampler configuration. */
    int measureOversamplerLatency (size_t stages)
    {
        juce::dsp::Oversampling<float> os (1, stages, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        constexpr size_t kLen = 256;
        os.initProcessing (kLen);
        std::vector<float> buf (kLen, 0.0f);
        buf[0] = 1.0f;
        float* ch[1] = { buf.data() };
        juce::dsp::AudioBlock<float> block (ch, 1, kLen);
        os.processSamplesUp (block);
        os.processSamplesDown (block);
        size_t peak = 0;
        for (size_t i = 1; i < kLen; ++i)
            if (std::abs (buf[i]) > std::abs (buf[peak]))
                peak = i;
        return static_cast<int> (peak);
    }
} // namespace

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

    latency2x = measureOversamplerLatency (1);
    latency4x = measureOversamplerLatency (2);

    scratchBuffer.setSize (2, preparedMaxBlock, false, true, true);
    xfadeScratchBuffer.setSize (2, preparedMaxBlock, false, true, true);

    dcL.prepare (sr, 15.0);
    dcR.prepare (sr, 15.0);
    dc2L.prepare (sr, 5.0);
    dc2R.prepare (sr, 5.0);

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
    dc2L.reset();
    dc2R.reset();
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
    smoothedFlutter  = 0.0f;
    smootherPrimed   = false;
    osXfade          = 1.0f;
    oxideCtrlCounter = 0;
}

int EntropyStage::getLatencySamples (bool highQuality4x, bool driveEngaged) const noexcept
{
    if (! driveEngaged)
        return 0;
    return highQuality4x ? latency4x : latency2x;
}

void EntropyStage::processBlock (float* left,
                                 float* right,
                                 int numSamples,
                                 float drive01,
                                 float tapeInstability01,
                                 float oxideDamping01,
                                 bool highQuality4x,
                                 bool allowModeSwitch) noexcept
{
    const int n = std::min (numSamples, preparedMaxBlock);
    if (n <= 0)
        return;

    const float srF         = static_cast<float> (sr);
    const float targetDrive = std::clamp (drive01, 0.0f, 1.0f);
    const float targetTape  = std::clamp (tapeInstability01, 0.0f, 1.0f);
    const float targetOxide = std::clamp (oxideDamping01, 0.0f, 1.0f);

    auto desiredRoute = [&]() noexcept
    {
        const bool engaged = targetDrive > 1.0e-5f || smDrive > 1.0e-5f;
        return engaged ? (highQuality4x ? 2 : 1) : 0;
    };

    if (! smootherPrimed)
    {
        smDrive        = targetDrive;
        smTape         = targetTape;
        smOxide        = targetOxide;
        routeMode      = desiredRoute();
        prevRouteMode  = routeMode;
        osXfade        = 1.0f;
        tapeActive     = smTape > 1.0e-5f || targetTape > 1.0e-5f;
        oxideActive    = smOxide > 1.0e-5f || targetOxide > 1.0e-5f;
        smootherPrimed = true;
    }
    else if (allowModeSwitch)
    {
        // Routing / gating decisions only on absolute control-cell boundaries (block-size invariant)
        const int want = desiredRoute();
        if (want != routeMode)
        {
            if (want == 2 && os4x) os4x->reset();
            if (want == 1 && os2x) os2x->reset();
            prevRouteMode = routeMode;
            routeMode     = want;
            osXfade       = 0.0f;
        }
        tapeActive  = smTape > 1.0e-5f || targetTape > 1.0e-5f;
        oxideActive = smOxide > 1.0e-5f || targetOxide > 1.0e-5f;
    }

    const float paramCoeff = 1.0f - std::exp (-1.0f / (0.020f * srF));

    // Runs one routing path (bypass / 2x / 4x asymmetric tube-tape drive) into buf
    auto runRoute = [&] (int route, juce::AudioBuffer<float>& buf, float startDrive) noexcept
    {
        float* sL = buf.getWritePointer (0);
        float* sR = buf.getWritePointer (1);
        for (int i = 0; i < n; ++i)
        {
            sL[i] = left[i];
            sR[i] = right[i];
        }

        auto* os = (route == 2) ? os4x.get() : (route == 1 ? os2x.get() : nullptr);
        if (os == nullptr)
            return startDrive; // bypass: drive disengaged, zero latency

        float* chans[2] = { sL, sR };
        juce::dsp::AudioBlock<float> block (chans, 2, static_cast<size_t> (n));
        auto osBlock = os->processSamplesUp (block);

        const size_t osLen = osBlock.getNumSamples();
        const float factor = static_cast<float> (os->getOversamplingFactor());
        const float osCoeff = 1.0f - std::exp (-1.0f / (0.020f * srF * factor));

        float curDrive = startDrive;
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

        os->processSamplesDown (block);
        return curDrive;
    };

    // ── 1. Per-Voice Pre-Sum Oversampled Asymmetric Tape/Tube Saturation ────
    const float driveBefore = smDrive;
    smDrive = runRoute (routeMode, scratchBuffer, driveBefore);

    float* scratchL = scratchBuffer.getWritePointer (0);
    float* scratchR = scratchBuffer.getWritePointer (1);

    if (osXfade < 1.0f)
    {
        (void) runRoute (prevRouteMode, xfadeScratchBuffer, driveBefore);
        const float* prevL = xfadeScratchBuffer.getReadPointer (0);
        const float* prevR = xfadeScratchBuffer.getReadPointer (1);
        const float xfadeInc = 1.0f / std::max (0.015f * srF, 16.0f);

        for (int i = 0; i < n; ++i)
        {
            osXfade = std::min (1.0f, osXfade + xfadeInc);
            left[i]  = prevL[i] + osXfade * (scratchL[i] - prevL[i]);
            right[i] = prevR[i] + osXfade * (scratchR[i] - prevR[i]);
        }
    }
    else
    {
        for (int i = 0; i < n; ++i)
        {
            left[i]  = scratchL[i];
            right[i] = scratchR[i];
        }
    }

    // ── 2. Musical Tape Wow & Flutter (Smoothed Cubic Modulated Delay) ──────
    const float wowInc     = kTwoPi * wowRateHz / srF;
    const float flutterInc = kTwoPi * flutterRateHz / srF;
    // Slower smoother for tape delay position avoids pitch-jump clicks on rapid knob turns
    const float tapeDelayCoeff = 1.0f - std::exp (-1.0f / (0.045f * srF));

    if (tapeActive)
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
    if (oxideActive)
    {
        headBumpBpL.setParameters (240.0f, 0.90f, srF);
        headBumpBpR.setParameters (240.0f, 0.90f, srF);

        const int ctrlStep = std::max (8, static_cast<int> (srF / 3000.0f));

        for (int i = 0; i < n; ++i)
        {
            smOxide += paramCoeff * (targetOxide - smOxide);

            if (oxideCtrlCounter == 0)
            {
                const float cutoff = 19500.0f * std::pow (1600.0f / 19500.0f, smOxide);
                oxideLpL.setParameters (cutoff, 0.72f, srF);
                oxideLpR.setParameters (cutoff, 0.72f, srF);
            }
            if (++oxideCtrlCounter >= ctrlStep)
                oxideCtrlCounter = 0;

            const float bumpGain = 0.28f * smOxide;
            const float oxideWet = std::clamp (smOxide * 22.0f, 0.0f, 1.0f);

            const float dryL = left[i];
            const float dryR = right[i];

            const float dampedL = oxideLpL.processLowpass (dryL + bumpGain * headBumpBpL.processBandpass (dryL));
            const float dampedR = oxideLpR.processLowpass (dryR + bumpGain * headBumpBpR.processBandpass (dryR));

            left[i]  = dryL + oxideWet * (dampedL - dryL);
            right[i] = dryR + oxideWet * (dampedR - dryR);
        }
    }

    // ── 4. End-of-chain 64-bit DC blocker (removes drive bias AND tape-flutter LF drift) ──
    for (int i = 0; i < n; ++i)
    {
        left[i]  = sanitize (static_cast<float> (dc2L.process (dcL.process (left[i]))));
        right[i] = sanitize (static_cast<float> (dc2R.process (dcR.process (right[i]))));
    }
}

} // namespace monolith::dsp
