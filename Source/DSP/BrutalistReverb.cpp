#include "BrutalistReverb.h"
#include <algorithm>
#include <cmath>

namespace monolith::dsp
{

namespace
{
    constexpr double kPiD    = 3.14159265358979323846;
    constexpr double kTwoPiD = 6.28318530717958647692;

    constexpr std::array<double, BrutalistReverb::kNumFdnLines> kFdnDelaysMs {
        29.7, 37.1, 43.3, 53.9, 61.7, 71.3, 83.1, 97.4
    };

    constexpr std::array<double, BrutalistReverb::kNumAllpass> kAllpassMsL { 4.3, 7.1, 11.3, 16.9 };
    constexpr std::array<double, BrutalistReverb::kNumAllpass> kAllpassMsR { 4.7, 7.7, 12.1, 17.9 };

    constexpr std::array<double, BrutalistReverb::kNumFdnLines> kLfoRatesHz {
        0.23, 0.31, 0.37, 0.43, 0.53, 0.61, 0.67, 0.79
    };

    constexpr double kModGentle    = 0.6;   // gentle chorus: <= ~6 cents peak pitch deviation
    constexpr double kAirDbPerM    = 0.10;  // ISO 9613-1 air absorption near 8 kHz (20 C, 50 % RH)
    constexpr double kAirRefHz     = 8000.0;
    constexpr double kSoundSpeed   = 343.0;
    constexpr double kSafetyRms    = 0.35;  // slow RMS guard (replaces in-loop saturation)

    struct SpaceProfile
    {
        double sizeScale;
        double preDelayMs;
        double earlyBalance;
        double decayScale;
        double dampFloorHz;
        double modDepthMs;
        double shimmerStL;
        double shimmerStR;
        double shimmerCap;
    };

    constexpr std::array<SpaceProfile, 4> kSpaceProfiles {{
        // 0: Cathedral (Towering liturgical decay + airy +12st shimmer)
        { 1.18, 24.0, 0.25, 1.15, 1800.0, 0.52,  12.00,  12.00, 0.28 },
        // 1: Concrete Silo (Lorn cold dark reflections + sub-octave -12st weight)
        { 0.62,  9.0, 0.60, 0.72, 1250.0, 0.18, -12.00, -12.00, 0.24 },
        // 2: Obsidian Abyss (Massive modulated low-mid gloom + dark -12st/+7st blend)
        { 1.48, 34.0, 0.20, 1.35,  920.0, 0.85, -12.00,   7.00, 0.28 },
        // 3: Ghost Void (Haunted pitch-drifted dark shimmer)
        { 1.32, 28.0, 0.28, 1.25, 1550.0, 1.10,  11.86,  12.14, 0.32 }
    }};

    bool isPrime (long v) noexcept
    {
        if (v < 2) return false;
        if (v % 2 == 0) return v == 2;
        for (long d = 3; d * d <= v; d += 2)
            if (v % d == 0) return false;
        return true;
    }

    long nextPrimeAtLeast (long v) noexcept
    {
        v = std::max (2L, v);
        while (! isPrime (v)) ++v;
        return v;
    }

    /** Orthonormal (unitary) 8x8 fast Walsh-Hadamard transform, 64-bit. */
    inline void hadamard8 (std::array<double, 8>& v) noexcept
    {
        for (size_t i = 0; i < 8; i += 2)
        {
            const double a = v[i], b = v[i + 1];
            v[i] = a + b; v[i + 1] = a - b;
        }
        for (size_t i = 0; i < 8; i += 4)
            for (size_t j = 0; j < 2; ++j)
            {
                const double a = v[i + j], b = v[i + j + 2];
                v[i + j] = a + b; v[i + j + 2] = a - b;
            }
        for (size_t j = 0; j < 4; ++j)
        {
            const double a = v[j], b = v[j + 4];
            v[j] = a + b; v[j + 4] = a - b;
        }
        constexpr double kNorm = 0.35355339059327376220; // 1 / sqrt(8)
        for (double& x : v)
            x *= kNorm;
    }

    /** Unity-DC one-pole y = (1-p) x + p y1 whose magnitude at w equals amp (0 < amp <= 1). */
    double onePoleForMagnitude (double amp, double w) noexcept
    {
        const double a2 = std::clamp (amp * amp, 1.0e-6, 1.0 - 1.0e-12);
        const double c  = std::cos (w);
        const double A  = 1.0 - a2;
        const double B  = 1.0 - a2 * c;
        const double disc = std::max (0.0, B * B - A * A);
        return std::clamp ((B - std::sqrt (disc)) / A, 0.0, 0.999);
    }

    /** C1 soft guard: identity for |x| <= 1, asymptote 2 (pathological overload only). */
    inline double guard (double x) noexcept
    {
        const double ax = std::abs (x);
        if (ax <= 1.0) return x;
        const double e = ax - 1.0;
        const double y = 1.0 + e / (1.0 + e);
        return x < 0.0 ? -y : y;
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
        earlyApL[i].prepare (static_cast<size_t> (sr * (kAllpassMsL[i] * 0.002)) + 32);
        earlyApR[i].prepare (static_cast<size_t> (sr * (kAllpassMsR[i] * 0.002)) + 32);
    }

    // Rate-scaled, mutually prime (distinct primes) FDN lengths per Space Model
    for (size_t s = 0; s < static_cast<size_t> (kNumSpaces); ++s)
    {
        long prev = 0;
        for (size_t i = 0; i < static_cast<size_t> (kNumFdnLines); ++i)
        {
            const long nominal = std::lround (kFdnDelaysMs[i] * 0.001 * kSpaceProfiles[s].sizeScale * sr);
            const long p = nextPrimeAtLeast (std::max (nominal, prev + 1));
            primeDelays[s][i] = static_cast<double> (p);
            prev = p;
        }
    }

    for (size_t i = 0; i < static_cast<size_t> (kNumFdnLines); ++i)
    {
        const size_t cap = static_cast<size_t> (sr * (kFdnDelaysMs[i] * 0.001 * 1.65)) + 128;
        fdnLines[i].prepare (cap);
        fdnDc[i].prepare (sr, 5.0);
        lfoRates[i] = kLfoRatesHz[i];
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
        wallIc1[i] = wallIc2[i] = 0.0;
        fdnDc[i].reset();
        fdnAirZ[i] = 0.0;
        const double ph = static_cast<double> (i) * (kTwoPiD / static_cast<double> (kNumFdnLines));
        lfoSin[i] = std::sin (ph);
        lfoCos[i] = std::cos (ph);
        lfoRotS[i] = std::sin (kTwoPiD * lfoRates[i] / sr);
        lfoRotC[i] = std::cos (kTwoPiD * lfoRates[i] / sr);
        lastAirDelay[i] = -1.0;
    }

    shimmerL.reset();
    shimmerR.reset();

    freezeBlend           = 0.0;
    frozenTargetRms       = 0.0;
    currentTankRms        = 0.0;
    safetyGain            = 1.0;
    freezeEnergyIndicator = 0.0f;
    wasFrozen             = false;
    smoothersPrimed       = false;
    dampCtrlCounter       = 0;
    lastWallCutHz         = -1.0;
    lastAirFreeze         = -1.0;
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

    const int spaceIdx = std::clamp (static_cast<int> (space), 0, kNumSpaces - 1);
    const auto& prof   = kSpaceProfiles[static_cast<size_t> (spaceIdx)];
    const auto& primes = primeDelays[static_cast<size_t> (spaceIdx)];

    const double invSr = 1.0 / sr;

    const double targetMix     = std::clamp (static_cast<double> (mix01), 0.0, 1.0);
    const double targetDecay   = std::clamp (static_cast<double> (decay01), 0.0, 1.0);
    const double targetDamp    = std::clamp (static_cast<double> (damp01), 0.0, 1.0);
    const double targetShimmer = std::clamp (static_cast<double> (shimmer01), 0.0, 1.0);

    if (freezeEnabled && ! wasFrozen)
        frozenTargetRms = std::max (currentTankRms, 0.0);
    wasFrozen = freezeEnabled;

    const double freezeCoeff  = 1.0 - std::exp (-1.0 / (0.040 * sr));
    const double freezeTarget = freezeEnabled ? 1.0 : 0.0;

    const double rt60Sec               = 0.38 * std::pow (42.0, targetDecay) * prof.decayScale;
    const double targetModDepthSamples = kModGentle * prof.modDepthMs * 0.001 * sr;
    const double targetPreDelaySamples = std::max (2.0, prof.preDelayMs * 0.001 * sr);

    const double effectiveDamp   = targetDamp * (1.0 - 0.90 * freezeBlend);
    const double targetDampCutHz = std::clamp (16000.0 * std::pow (prof.dampFloorHz / 16000.0, effectiveDamp),
                                               320.0,
                                               0.43 * sr);

    std::array<double, kNumFdnLines> targetFdnFeedback {};
    for (size_t i = 0; i < static_cast<size_t> (kNumFdnLines); ++i)
    {
        const double dSec    = primes[i] * invSr;
        targetFdnFeedback[i] = std::clamp (std::pow (10.0, -3.0 * dSec / std::max (rt60Sec, 0.15)), 0.10, 0.99995);
    }

    std::array<double, kNumAllpass> targetApLenL {};
    std::array<double, kNumAllpass> targetApLenR {};
    const double apScale = 0.55 + 0.45 * prof.sizeScale;
    for (size_t a = 0; a < static_cast<size_t> (kNumAllpass); ++a)
    {
        targetApLenL[a] = std::max (8.0, kAllpassMsL[a] * 0.001 * apScale * sr);
        targetApLenR[a] = std::max (8.0, kAllpassMsR[a] * 0.001 * apScale * sr);
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
        smFdnDelay        = primes;
        smFdnFeedback     = targetFdnFeedback;
        smoothersPrimed   = true;
    }

    const double fastCoeff     = 1.0 - std::exp (-1.0 / (0.025 * sr));
    const double geomCoeff     = 1.0 - std::exp (-1.0 / (0.060 * sr));
    const double rmsTrackAlpha = 1.0 - std::exp (-1.0 / (0.045 * sr));
    const double safetyCoeff   = 1.0 - std::exp (-1.0 / (0.050 * sr));
    const int    ctrlStep      = std::max (8, static_cast<int> (sr / 3000.0));
    const double airW          = kTwoPiD * std::min (kAirRefHz, 0.40 * sr) * invSr;

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

        if (dampCtrlCounter == 0)
        {
            // Coefficients are refreshed only when their drivers moved (idle tails cost no transcendentals)
            if (std::abs (smDampCutHz - lastWallCutHz) > 1.0e-4 * smDampCutHz)
            {
                lastWallCutHz = smDampCutHz;
                const double g = std::tan (kPiD * std::clamp (smDampCutHz, 1.0, 0.49 * sr) * invSr);
                constexpr double k = 1.0 / 0.707;
                wallM1 = 1.0 / (1.0 + g * (g + k));
                wallM2 = g * wallM1;
                wallM3 = g * wallM2;
            }
            const bool freezeMoved = std::abs (freezeBlend - lastAirFreeze) > 1.0e-4;
            for (size_t i = 0; i < static_cast<size_t> (kNumFdnLines); ++i)
            {
                if (! freezeMoved && std::abs (smFdnDelay[i] - lastAirDelay[i]) < 0.25)
                    continue;
                lastAirDelay[i] = smFdnDelay[i];
                // Air absorption: dB loss per pass proportional to the acoustic path length of this line
                const double pathM  = kSoundSpeed * smFdnDelay[i] * invSr;
                const double lossDb = kAirDbPerM * pathM * (1.0 - 0.90 * freezeBlend);
                fdnAirP[i] = onePoleForMagnitude (std::pow (10.0, -lossDb / 20.0), airW);
            }
            if (freezeMoved)
                lastAirFreeze = freezeBlend;
            shimmerL.setPitch (smShimmerStL);
            shimmerR.setPitch (smShimmerStR);
        }
        if (++dampCtrlCounter >= ctrlStep)
            dampCtrlCounter = 0;

        const double dryL = static_cast<double> (sanitize (left[n]));
        const double dryR = static_cast<double> (sanitize (right[n]));

        // Pre-delay + cubic 4-stage allpass diffuser
        preDelayL.push (dryL);
        preDelayR.push (dryR);

        double earlyL = preDelayL.readCubic (smPreDelaySamples);
        double earlyR = preDelayR.readCubic (smPreDelaySamples * 1.07);

        for (size_t a = 0; a < static_cast<size_t> (kNumAllpass); ++a)
        {
            smApLenL[a] += geomCoeff * (targetApLenL[a] - smApLenL[a]);
            smApLenR[a] += geomCoeff * (targetApLenR[a] - smApLenR[a]);
            earlyL = earlyApL[a].process (earlyL, smApLenL[a], 0.58);
            earlyR = earlyApR[a].process (earlyR, smApLenR[a], 0.58);
        }

        const double injectScale = 1.0 - freezeBlend;
        const double injectL = earlyL * injectScale;
        const double injectR = earlyR * injectScale;

        // Read 8 gently modulated FDN lines (smoothly morphed prime base delays)
        std::array<double, kNumFdnLines> tap {};
        double energySumSq = 0.0;
        const double modScale = 1.0 - 0.35 * freezeBlend;

        for (size_t i = 0; i < static_cast<size_t> (kNumFdnLines); ++i)
        {
            smFdnDelay[i]    += geomCoeff * (primes[i] - smFdnDelay[i]);
            smFdnFeedback[i] += fastCoeff * (targetFdnFeedback[i] - smFdnFeedback[i]);

            const double ls = lfoSin[i] * lfoRotC[i] + lfoCos[i] * lfoRotS[i];
            const double lc = lfoCos[i] * lfoRotC[i] - lfoSin[i] * lfoRotS[i];
            lfoSin[i] = ls;
            lfoCos[i] = lc;

            const double d = smFdnDelay[i] + smModDepthSamples * modScale * ls;
            const double rawTap = fdnLines[i].readCubic (d);

            // Wall absorption (TPT 2-pole LP, shared coefficients)
            const double v0 = fdnDc[i].process (rawTap);
            const double v3 = v0 - wallIc2[i];
            const double v1 = wallM1 * wallIc1[i] + wallM2 * v3;
            const double v2 = wallIc2[i] + wallM2 * wallIc1[i] + wallM3 * v3;
            wallIc1[i] = 2.0 * v1 - wallIc1[i];
            wallIc2[i] = 2.0 * v2 - wallIc2[i];
            const double wall = v2;
            fdnAirZ[i] = wall + fdnAirP[i] * (fdnAirZ[i] - wall);
            const double damped = fdnAirZ[i];
            tap[i] = damped;
            energySumSq += damped * damped;
        }

        const double instRms = std::sqrt (energySumSq * (1.0 / static_cast<double> (kNumFdnLines)));
        currentTankRms += rmsTrackAlpha * (instRms - currentTankRms);

        // Slow RMS safety gain (no per-sample saturation inside the loop)
        const double safetyTarget = (currentTankRms > kSafetyRms) ? kSafetyRms / currentTankRms : 1.0;
        safetyGain += safetyCoeff * (safetyTarget - safetyGain);

        // Unitary 8x8 Hadamard scattering
        std::array<double, kNumFdnLines> mixed = tap;
        hadamard8 (mixed);

        // Spectral Freeze smooth RMS cloud stabilizer (tracks smooth currentTankRms -> zero AM flutter)
        double freezeLoopGain = 1.0;
        if (freezeBlend > 1.0e-3 && frozenTargetRms > 1.0e-5 && currentTankRms > 1.0e-6)
        {
            const double ratio = frozenTargetRms / currentTankRms;
            freezeLoopGain = std::clamp (1.0 + 0.008 * (ratio - 1.0), 0.994, 1.006);
        }

        // Stereo tank sum for output and pitch-shimmer feedback
        const double tankOutL = (tap[0] + tap[2] + tap[4] + tap[6]) * 0.25;
        const double tankOutR = (tap[1] + tap[3] + tap[5] + tap[7]) * 0.25;

        const double shimL = shimmerL.process (tankOutL);
        const double shimR = shimmerR.process (tankOutR);

        const double shimGain  = smShimmer * smShimmerCap * (1.0 - 0.85 * freezeBlend);
        const double fbInjectL = injectL * 0.34 + shimL * shimGain;
        const double fbInjectR = injectR * 0.34 + shimR * shimGain;

        for (size_t i = 0; i < static_cast<size_t> (kNumFdnLines); ++i)
        {
            const double normalGain = smFdnFeedback[i];
            const double g   = (normalGain + freezeBlend * (freezeLoopGain - normalGain)) * safetyGain;
            const double inj = ((i & 1u) == 0u) ? fbInjectL : fbInjectR;
            fdnLines[i].push (guard (mixed[i] * g + inj));
        }

        const double wetL = earlyL * smEarlyBalance + tankOutL * (1.0 - 0.35 * smEarlyBalance);
        const double wetR = earlyR * smEarlyBalance + tankOutR * (1.0 - 0.35 * smEarlyBalance);

        const double effMix  = std::max (smMix, freezeBlend * 0.55);
        const double dryGain = std::max (std::cos (smMix * 0.5 * kPiD), 0.78 * freezeBlend);
        const double wetGain = std::sin (effMix * 0.5 * kPiD) * 0.92;

        left[n]  = sanitize (static_cast<float> (dryL * dryGain + wetL * wetGain));
        right[n] = sanitize (static_cast<float> (dryR * dryGain + wetR * wetGain));
    }

    for (size_t i = 0; i < static_cast<size_t> (kNumFdnLines); ++i)
    {
        const double r = 1.0 / std::sqrt (lfoSin[i] * lfoSin[i] + lfoCos[i] * lfoCos[i]);
        lfoSin[i] *= r;
        lfoCos[i] *= r;
    }

    freezeEnergyIndicator = static_cast<float> (freezeBlend * std::clamp (currentTankRms * 2.5, 0.0, 1.0));
}

} // namespace monolith::dsp
