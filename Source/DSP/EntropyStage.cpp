#include "EntropyStage.h"
#include <algorithm>
#include <cmath>

namespace monolith::dsp
{

namespace
{
    // Outer 48<->96k stage: pass 0.40 fs, stop 0.50 fs (Nyquist-folded harmonics land in the stopband).
    constexpr int    kOuterUpTaps  = 112;   // Type II (even)
    constexpr int    kOuterDnTaps  = 129;   // Type I (odd)
    constexpr double kOuterFc      = 0.225; // cycles / 2x sample
    // Inner 96<->192k stage (4x only): pass 0.5 fs, stop 1.5 fs.
    constexpr int    kInnerUpTaps  = 30;
    constexpr int    kInnerDnTaps  = 29;
    constexpr double kInnerFc      = 0.25;  // cycles / 4x sample
    constexpr double kStopbandDb   = 100.0;
    constexpr double kAdaaDelay    = 0.5;   // ADAA1 group delay (OS samples), matched by midpoint dry path
} // namespace

void EntropyStage::prepare (double sampleRate, int maxBlockSize, uint32_t voiceSeed)
{
    sr = std::max (8000.0, sampleRate);
    preparedMaxBlock = std::max (32, maxBlockSize);
    seed = (voiceSeed != 0u) ? voiceSeed : 1u;

    for (int c = 0; c < 2; ++c)
    {
        route2x.outer[c].design (kOuterUpTaps, kOuterDnTaps, kOuterFc, kStopbandDb, preparedMaxBlock);
        route4x.outer[c].design (kOuterUpTaps, kOuterDnTaps, kOuterFc, kStopbandDb, preparedMaxBlock);
        route4x.inner[c].design (kInnerUpTaps, kInnerDnTaps, kInnerFc, kStopbandDb, 2 * preparedMaxBlock);
        osBufA[c].assign (static_cast<size_t> (2 * preparedMaxBlock), 0.0f);
        osBufB[c].assign (static_cast<size_t> (4 * preparedMaxBlock), 0.0f);
    }
    route2x.fourX = false;
    route4x.fourX = true;

    // Exact analytic latency: linear-phase FIR group delays + ADAA1 half-sample (all integer at base rate)
    const auto& o = route2x.outer[0];
    const auto& in4 = route4x.inner[0];
    const double lat2 = (o.upDelayOs() + kAdaaDelay + o.downDelayOs()) / 2.0;
    const double lat4 = (o.upDelayOs() + o.downDelayOs() + (in4.upDelayOs() + kAdaaDelay + in4.downDelayOs()) / 2.0) / 2.0;
    jassert (std::abs (lat2 - std::round (lat2)) < 1.0e-9 && std::abs (lat4 - std::round (lat4)) < 1.0e-9);
    latency2x = static_cast<int> (std::lround (lat2));
    latency4x = static_cast<int> (std::lround (lat4));

    scratchBuffer.setSize (2, preparedMaxBlock, false, true, true);
    xfadeScratchBuffer.setSize (2, preparedMaxBlock, false, true, true);

    dcL.prepare (sr, 15.0);
    dcR.prepare (sr, 15.0);
    dc2L.prepare (sr, 5.0);
    dc2R.prepare (sr, 5.0);

    const size_t cap = static_cast<size_t> (sr * 0.04) + 64;
    delayL.prepare (cap);
    delayR.prepare (cap);

    oxideLpL.setAnalogMix (std::min (19500.0, 0.44 * sr), 0.72, sr, 0.0, 0.0, 1.0);
    oxideLpR.setAnalogMix (std::min (19500.0, 0.44 * sr), 0.72, sr, 0.0, 0.0, 1.0);
    headBumpBpL.setAnalogMix (240.0, 0.90, sr, 0.0, 1.0 / 0.90, 0.0);
    headBumpBpR.setAnalogMix (240.0, 0.90, sr, 0.0, 1.0 / 0.90, 0.0);

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
    route2x.reset();
    route4x.reset();
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
            if (want == 2) route4x.reset();
            if (want == 1) route2x.reset();
            prevRouteMode = routeMode;
            routeMode     = want;
            // Pre-roll: hold the crossfade at 0 until the freshly reset FIR route has flushed its
            // start-up transient (> its group delay), then run the 5 ms raised-cosine crossfade.
            const float xfInc = 1.0f / std::max (0.005f * srF, 16.0f);
            osXfade = -static_cast<float> (std::max (latency2x, latency4x) + 32) * xfInc;
        }
        tapeActive  = smTape > 1.0e-5f || targetTape > 1.0e-5f;
        oxideActive = smOxide > 1.0e-5f || targetOxide > 1.0e-5f;
    }

    const float paramCoeff = 1.0f - std::exp (-1.0f / (0.020f * srF));

    // Runs one routing path (bypass / 2x / 4x linear-phase OS + 64-bit ADAA1 asymmetric drive) into buf
    auto runRoute = [&] (int route, juce::AudioBuffer<float>& buf, float startDrive) noexcept
    {
        float* s[2] = { buf.getWritePointer (0), buf.getWritePointer (1) };
        for (int i = 0; i < n; ++i)
        {
            s[0][i] = left[i];
            s[1][i] = right[i];
        }

        if (route == 0)
            return startDrive; // bypass: drive disengaged, zero latency

        DriveRoute& R = (route == 2) ? route4x : route2x;
        const int factor = R.fourX ? 4 : 2;
        const int osLen  = n * factor;
        const float osCoeff = 1.0f - std::exp (-1.0f / (0.020f * srF * static_cast<float> (factor)));

        float endDrive = startDrive;
        for (int c = 0; c < 2; ++c)
        {
            float* a = osBufA[c].data();
            R.outer[c].up (s[c], a, n);
            float* work = a;
            if (R.fourX)
            {
                R.inner[c].up (a, osBufB[c].data(), 2 * n);
                work = osBufB[c].data();
            }

            float  curDrive = startDrive;
            double u1 = R.u1[c], F1u1 = R.F1u1[c], x1 = R.x1[c];
            for (int i = 0; i < osLen; ++i)
            {
                curDrive += osCoeff * (targetDrive - curDrive);
                const double d   = static_cast<double> (curDrive);
                const double wet = std::clamp (d * 20.0, 0.0, 1.0);
                const double x   = static_cast<double> (work[i]);
                const double xm  = 0.5 * (x + x1); // dry path phase-matched to the ADAA1 half-sample delay
                const double g   = 1.0 + 7.5 * d;
                const double b   = 0.22 * d;
                const double u   = g * x + b;
                const double F1u = ThroatShaper::F1 (u);
                double out = xm;
                if (wet > 1.0e-5)
                {
                    const double ya = ThroatShaper::adaa1Cached (u, u1, F1u, F1u1);
                    const double y  = (ya - ThroatShaper::f (b)) / (1.0 + 4.8 * d);
                    out = xm + wet * (y - xm);
                }
                u1 = u; F1u1 = F1u; x1 = x;
                work[i] = static_cast<float> (out);
            }
            R.u1[c] = u1; R.F1u1[c] = F1u1; R.x1[c] = x1;

            if (R.fourX)
                R.inner[c].down (work, a, 2 * n);
            R.outer[c].down (a, s[c], n);
            endDrive = curDrive;
        }
        return endDrive;
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
        // 5 ms raised-cosine route crossfade (C1-smooth at both ends) on oversampling / bypass mode toggles
        const float xfadeInc = 1.0f / std::max (0.005f * srF, 16.0f);

        for (int i = 0; i < n; ++i)
        {
            osXfade = std::min (1.0f, osXfade + xfadeInc);
            const float w = (osXfade <= 0.0f) ? 0.0f : 0.5f - 0.5f * std::cos (kPi * osXfade);
            left[i]  = prevL[i] + w * (scratchL[i] - prevL[i]);
            right[i] = prevR[i] + w * (scratchR[i] - prevR[i]);
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

            const float wowL = fastSin (wowPhase);
            const float wowR = fastSin (wowPhase + 0.65f);
            const float fltL = fastSin (flutterPhase) + 0.45f * smoothedFlutter;
            const float fltR = fastCos (flutterPhase * 1.07f) - 0.45f * smoothedFlutter;

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
        const int ctrlStep = std::max (8, static_cast<int> (srF / 3000.0f));

        for (int i = 0; i < n; ++i)
        {
            smOxide += paramCoeff * (targetOxide - smOxide);

            if (oxideCtrlCounter == 0)
            {
                const double cutoff = std::min (19500.0 * std::pow (1600.0 / 19500.0, static_cast<double> (smOxide)), 0.44 * sr);
                oxideLpL.setAnalogMix (cutoff, 0.72, sr, 0.0, 0.0, 1.0);
                oxideLpR.setAnalogMix (cutoff, 0.72, sr, 0.0, 0.0, 1.0);
            }
            if (++oxideCtrlCounter >= ctrlStep)
                oxideCtrlCounter = 0;

            const float bumpGain = 0.28f * smOxide;
            const float oxideWet = std::clamp (smOxide * 22.0f, 0.0f, 1.0f);

            const float dryL = left[i];
            const float dryR = right[i];

            const float dampedL = static_cast<float> (oxideLpL.process (dryL + bumpGain * headBumpBpL.process (dryL)));
            const float dampedR = static_cast<float> (oxideLpR.process (dryR + bumpGain * headBumpBpR.process (dryR)));

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
