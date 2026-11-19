#include "BrutalistReverb.h"
#include <algorithm>
#include <cmath>

namespace monolith::dsp
{

namespace
{
    constexpr std::array<float, BrutalistReverb::kNumFdnLines> kFdnDelaysMs {
        29.7f, 37.1f, 43.3f, 53.9f, 61.7f, 71.3f, 83.1f, 97.4f
    };

    constexpr std::array<float, BrutalistReverb::kNumAllpass> kAllpassMsL {
        4.3f, 7.1f, 11.3f, 16.9f
    };

    constexpr std::array<float, BrutalistReverb::kNumAllpass> kAllpassMsR {
        4.7f, 7.7f, 12.1f, 17.9f
    };

    constexpr std::array<float, BrutalistReverb::kNumFdnLines> kLfoRatesHz {
        0.23f, 0.31f, 0.37f, 0.43f, 0.53f, 0.61f, 0.67f, 0.79f
    };

    struct SpaceProfile
    {
        float sizeScale;
        float preDelayMs;
        float earlyBalance;
        float decayScale;
        float dampFloorHz;
        float modDepthMs;
        float shimmerStL;
        float shimmerStR;
        float shimmerCap;
    };

    constexpr std::array<SpaceProfile, 4> kSpaceProfiles {{
        // 0: Cathedral (Towering liturgical decay + airy +12st shimmer)
        { 1.18f, 24.0f, 0.25f, 1.15f, 1800.0f, 0.52f,  12.00f,  12.00f, 0.28f },
        // 1: Concrete Silo (Lorn cold dark reflections + sub-octave -12st weight)
        { 0.62f,  9.0f, 0.60f, 0.72f, 1250.0f, 0.18f, -12.00f, -12.00f, 0.24f },
        // 2: Obsidian Abyss (Massive modulated low-mid gloom + dark -12st/+7st blend)
        { 1.48f, 34.0f, 0.20f, 1.35f,  920.0f, 0.85f, -12.00f,   7.00f, 0.28f },
        // 3: Ghost Void (Haunted pitch-drifted dark shimmer)
        { 1.32f, 28.0f, 0.28f, 1.25f, 1550.0f, 1.10f,  11.86f,  12.14f, 0.32f }
    }};

    /** Orthonormal 8x8 Fast Walsh-Hadamard mixing matrix (1 / sqrt(8) = 0.35355339f). */
    inline void hadamard8 (std::array<float, 8>& v) noexcept
    {
        for (int i = 0; i < 8; i += 2)
        {
            const float a = v[static_cast<size_t> (i)];
            const float b = v[static_cast<size_t> (i + 1)];
            v[static_cast<size_t> (i)]     = a + b;
            v[static_cast<size_t> (i + 1)] = a - b;
        }
        for (int i = 0; i < 8; i += 4)
        {
            for (int j = 0; j < 2; ++j)
            {
                const float a = v[static_cast<size_t> (i + j)];
                const float b = v[static_cast<size_t> (i + j + 2)];
                v[static_cast<size_t> (i + j)]     = a + b;
                v[static_cast<size_t> (i + j + 2)] = a - b;
            }
        }
        for (int j = 0; j < 4; ++j)
        {
            const float a = v[static_cast<size_t> (j)];
            const float b = v[static_cast<size_t> (j + 4)];
            v[static_cast<size_t> (j)]     = a + b;
            v[static_cast<size_t> (j + 4)] = a - b;
        }
        constexpr float kNorm = 0.35355339059327376f;
        for (float& x : v)
            x *= kNorm;
    }
} // namespace

void BrutalistReverb::prepare (double sampleRate, int maxBlockSize)
{
    (void) maxBlockSize;
    sr = std::max (8000.0, sampleRate);

    preDelayL.prepare (static_cast<size_t> (sr * 0.065) + 32);
    preDelayR.prepare (static_cast<size_t> (sr * 0.065) + 32);

    for (size_t i = 0; i < static_cast<size_t> (kNumAllpass); ++i)
    {
        earlyApL[i].prepare (static_cast<size_t> (sr * (kAllpassMsL[i] * 0.002f)) + 32);
        earlyApR[i].prepare (static_cast<size_t> (sr * (kAllpassMsR[i] * 0.002f)) + 32);
    }

    for (size_t i = 0; i < static_cast<size_t> (kNumFdnLines); ++i)
    {
        const size_t cap = static_cast<size_t> (sr * (kFdnDelaysMs[i] * 0.001f * 1.65f)) + 128;
        fdnLines[i].prepare (cap);
        fdnDc[i].prepare (sr);
        lfoPhases[i] = static_cast<float> (i) * (kTwoPi / static_cast<float> (kNumFdnLines));
        lfoRates[i]  = kLfoRatesHz[i];
    }

    shimmerL.prepare (sr);
    shimmerR.prepare (sr);

    reset();
}

void BrutalistReverb::reset() noexcept
{
    preDelayL.reset();
    preDelayR.reset();

    for (auto& ap : earlyApL) ap.clear();
    for (auto& ap : earlyApR) ap.clear();

    for (size_t i = 0; i < static_cast<size_t> (kNumFdnLines); ++i)
    {
        fdnLines[i].reset();
        fdnDampLp[i].reset();
        fdnDampLp[i].invalidateCache();
        fdnDc[i].reset();
    }

    shimmerL.reset();
    shimmerR.reset();

    freezeBlend           = 0.0f;
    frozenTargetRms       = 0.0f;
    currentTankRms        = 0.0f;
    freezeEnergyIndicator = 0.0f;
    wasFrozen             = false;
    smoothersPrimed       = false;
}

void BrutalistReverb::processBlock (float* left,
                                    float* right,
                                    int numSamples,
                                    SpaceType space,
                                    float mix01,
                                    float decay01,
                                    float damp01,
                                    float shimmer01,
                                    bool freezeEnabled) noexcept
{
    if (numSamples <= 0)
        return;

    const int spaceIdx = std::clamp (static_cast<int> (space), 0, 3);
    const auto& prof   = kSpaceProfiles[static_cast<size_t> (spaceIdx)];

    const float srF    = static_cast<float> (sr);
    const float invSrF = 1.0f / srF;

    const float targetMix     = std::clamp (mix01, 0.0f, 1.0f);
    const float targetDecay   = std::clamp (decay01, 0.0f, 1.0f);
    const float targetDamp    = std::clamp (damp01, 0.0f, 1.0f);
    const float targetShimmer = std::clamp (shimmer01, 0.0f, 1.0f);

    if (freezeEnabled && ! wasFrozen)
        frozenTargetRms = std::max (currentTankRms, 0.0f);
    wasFrozen = freezeEnabled;

    const float freezeCoeff  = 1.0f - std::exp (-1.0f / (0.040f * srF));
    const float freezeTarget = freezeEnabled ? 1.0f : 0.0f;

    const float rt60Sec               = 0.38f * std::pow (42.0f, targetDecay) * prof.decayScale;
    const float targetModDepthSamples = prof.modDepthMs * 0.001f * srF;
    const float targetPreDelaySamples = std::max (2.0f, prof.preDelayMs * 0.001f * srF);

    const float effectiveDamp = targetDamp * (1.0f - 0.90f * freezeBlend);
    const float targetDampCutHz = std::clamp (16000.0f * std::pow (prof.dampFloorHz / 16000.0f, effectiveDamp),
                                              320.0f,
                                              0.43f * srF);

    std::array<float, kNumFdnLines> targetFdnDelay {};
    std::array<float, kNumFdnLines> targetFdnFeedback {};
    for (size_t i = 0; i < static_cast<size_t> (kNumFdnLines); ++i)
    {
        const float dSec     = kFdnDelaysMs[i] * 0.001f * prof.sizeScale;
        targetFdnDelay[i]    = dSec * srF;
        targetFdnFeedback[i] = std::clamp (std::pow (10.0f, -3.0f * dSec / std::max (rt60Sec, 0.15f)), 0.10f, 0.9990f);
    }

    std::array<float, kNumAllpass> targetApLenL {};
    std::array<float, kNumAllpass> targetApLenR {};
    const float apScale = 0.55f + 0.45f * prof.sizeScale;
    for (size_t a = 0; a < static_cast<size_t> (kNumAllpass); ++a)
    {
        targetApLenL[a] = std::max (8.0f, kAllpassMsL[a] * 0.001f * apScale * srF);
        targetApLenR[a] = std::max (8.0f, kAllpassMsR[a] * 0.001f * apScale * srF);
    }

    if (! smoothersPrimed)
    {
        smMix             = targetMix;
        smShimmer         = targetShimmer;
        smDampCutHz       = targetDampCutHz;
        smPreDelaySamples = targetPreDelaySamples;
        smEarlyBalance    = prof.earlyBalance;
        smModDepthSamples = targetModDepthSamples;
        smShimmerCap      = prof.shimmerCap;
        smShimmerStL      = prof.shimmerStL;
        smShimmerStR      = prof.shimmerStR;
        smApLenL          = targetApLenL;
        smApLenR          = targetApLenR;
        smFdnDelay        = targetFdnDelay;
        smFdnFeedback     = targetFdnFeedback;
        for (size_t i = 0; i < static_cast<size_t> (kNumFdnLines); ++i)
            fdnDampLp[i].setParameters (smDampCutHz, 0.707f, srF);
        smoothersPrimed   = true;
    }

    const float fastCoeff     = 1.0f - std::exp (-1.0f / (0.025f * srF));
    const float geomCoeff     = 1.0f - std::exp (-1.0f / (0.060f * srF));
    const float rmsTrackAlpha = 1.0f - std::exp (-1.0f / (0.045f * srF));
    const int   ctrlStep      = std::max (8, static_cast<int> (srF / 3000.0f));

    for (int n = 0; n < numSamples; ++n)
    {
        freezeBlend       += freezeCoeff * (freezeTarget - freezeBlend);
        smMix             += fastCoeff * (targetMix - smMix);
        smShimmer         += fastCoeff * (targetShimmer - smShimmer);
        smDampCutHz       += fastCoeff * (targetDampCutHz - smDampCutHz);
        smPreDelaySamples += geomCoeff * (targetPreDelaySamples - smPreDelaySamples);
        smEarlyBalance    += fastCoeff * (prof.earlyBalance - smEarlyBalance);
        smModDepthSamples += fastCoeff * (targetModDepthSamples - smModDepthSamples);
        smShimmerCap      += fastCoeff * (prof.shimmerCap - smShimmerCap);
        smShimmerStL      += geomCoeff * (prof.shimmerStL - smShimmerStL);
        smShimmerStR      += geomCoeff * (prof.shimmerStR - smShimmerStR);

        if ((n % ctrlStep) == 0)
        {
            for (size_t i = 0; i < static_cast<size_t> (kNumFdnLines); ++i)
                fdnDampLp[i].setParameters (smDampCutHz, 0.707f, srF);
        }

        const float dryL = sanitize (left[n]);
        const float dryR = sanitize (right[n]);

        // Pre-delay + Fractional 4-Stage Allpass Diffuser
        preDelayL.push (dryL);
        preDelayR.push (dryR);

        float earlyL = preDelayL.readLinear (smPreDelaySamples);
        float earlyR = preDelayR.readLinear (smPreDelaySamples * 1.07f);

        for (size_t a = 0; a < static_cast<size_t> (kNumAllpass); ++a)
        {
            smApLenL[a] += geomCoeff * (targetApLenL[a] - smApLenL[a]);
            smApLenR[a] += geomCoeff * (targetApLenR[a] - smApLenR[a]);
            earlyL = earlyApL[a].process (earlyL, smApLenL[a], 0.58f);
            earlyR = earlyApR[a].process (earlyR, smApLenR[a], 0.58f);
        }

        const float injectScale = 1.0f - freezeBlend;
        const float injectL = earlyL * injectScale;
        const float injectR = earlyR * injectScale;

        // Read 8 modulated FDN lines with smoothly morphed base delays
        std::array<float, kNumFdnLines> tap {};
        float energySumSq = 0.0f;

        for (size_t i = 0; i < static_cast<size_t> (kNumFdnLines); ++i)
        {
            smFdnDelay[i]    += geomCoeff * (targetFdnDelay[i] - smFdnDelay[i]);
            smFdnFeedback[i] += fastCoeff * (targetFdnFeedback[i] - smFdnFeedback[i]);

            lfoPhases[i] += kTwoPi * lfoRates[i] * invSrF;
            if (lfoPhases[i] >= kTwoPi) lfoPhases[i] -= kTwoPi;

            const float modScale = 1.0f - 0.35f * freezeBlend;
            const float d = smFdnDelay[i] + smModDepthSamples * modScale * std::sin (lfoPhases[i]);
            const float rawTap = fdnLines[i].readCubic (d);

            const float damped = fdnDampLp[i].processLowpass (fdnDc[i].process (rawTap));
            tap[i] = damped;
            energySumSq += damped * damped;
        }

        const float instRms = std::sqrt (energySumSq * (1.0f / static_cast<float> (kNumFdnLines)));
        currentTankRms += rmsTrackAlpha * (instRms - currentTankRms);

        // Orthogonal 8x8 Hadamard scattering
        std::array<float, kNumFdnLines> mixed = tap;
        hadamard8 (mixed);

        // Spectral Freeze smooth RMS cloud stabilizer (tracks smooth currentTankRms -> zero AM flutter)
        float freezeLoopGain = 1.0f;
        if (freezeBlend > 1.0e-3f && frozenTargetRms > 1.0e-5f && currentTankRms > 1.0e-6f)
        {
            const float ratio = frozenTargetRms / currentTankRms;
            freezeLoopGain = std::clamp (1.0f + 0.008f * (ratio - 1.0f), 0.994f, 1.006f);
        }

        // Extract stereo tank sum for output and pitch-shimmer feedback
        const float tankOutL = (tap[0] + tap[2] + tap[4] + tap[6]) * 0.25f;
        const float tankOutR = (tap[1] + tap[3] + tap[5] + tap[7]) * 0.25f;

        const float shimL = shimmerL.process (tankOutL, smShimmerStL);
        const float shimR = shimmerR.process (tankOutR, smShimmerStR);

        const float shimGain  = smShimmer * smShimmerCap * (1.0f - 0.85f * freezeBlend);
        const float fbInjectL = injectL * 0.34f + shimL * shimGain;
        const float fbInjectR = injectR * 0.34f + shimR * shimGain;

        for (size_t i = 0; i < static_cast<size_t> (kNumFdnLines); ++i)
        {
            const float normalGain = smFdnFeedback[i];
            const float g   = normalGain + freezeBlend * (freezeLoopGain - normalGain);
            const float inj = ((i & 1u) == 0u) ? fbInjectL : fbInjectR;
            const float nextVal = fastTanh ((mixed[i] * g + inj) * 0.85f) * 1.17647f;
            fdnLines[i].push (nextVal);
        }

        const float wetL = earlyL * smEarlyBalance + tankOutL * (1.0f - 0.35f * smEarlyBalance);
        const float wetR = earlyR * smEarlyBalance + tankOutR * (1.0f - 0.35f * smEarlyBalance);

        const float effMix  = std::max (smMix, freezeBlend * 0.55f);
        const float dryGain = std::max (std::cos (smMix * 0.5f * kPi), 0.78f * freezeBlend);
        const float wetGain = std::sin (effMix * 0.5f * kPi) * 0.92f;

        left[n]  = sanitize (dryL * dryGain + wetL * wetGain);
        right[n] = sanitize (dryR * dryGain + wetR * wetGain);
    }

    freezeEnergyIndicator = freezeBlend * std::clamp (currentTankRms * 2.5f, 0.0f, 1.0f);
}

} // namespace monolith::dsp
