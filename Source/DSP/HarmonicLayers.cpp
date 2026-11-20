#include "HarmonicLayers.h"
#include <algorithm>
#include <cmath>

namespace monolith::dsp
{

void HarmonicLayers::prepare (double sampleRate) noexcept
{
    sr = std::max (8000.0, sampleRate);
    reset();
}

void HarmonicLayers::reset() noexcept
{
    subPhase    = 0.0f;
    crownPhaseL = 0.0f;
    crownPhaseR = 0.37f;
    subLp.reset();
    subHp.reset();
    crownBpL.reset();
    crownBpR.reset();
    subLp.invalidateCache();
    subHp.invalidateCache();
    crownBpL.invalidateCache();
    crownBpR.invalidateCache();
    updateFilters (220.0f, 0.5f);
}

void HarmonicLayers::updateFilters (float f0Hz, float tension01) noexcept
{
    const float srF = static_cast<float> (sr);
    subHp.setParameters (28.0f, 0.707f, srF);
    const float lpCut = std::clamp (f0Hz * 1.85f, 110.0f, 340.0f);
    subLp.setParameters (lpCut, 0.85f, srF);

    const float bpFc = std::clamp (2200.0f + 1850.0f * tension01, 1400.0f, 0.42f * srF);
    crownBpL.setParameters (bpFc, 1.05f, srF);
    crownBpR.setParameters (bpFc * 1.04f, 1.05f, srF);
}

void HarmonicLayers::processSample (float f0Hz,
                                    float subAmount01,
                                    float crownAmount01,
                                    float expression01,
                                    float& outL,
                                    float& outR) noexcept
{
    const float srF   = static_cast<float> (sr);
    const float invSr = 1.0f / srF;

    float addL = 0.0f;
    float addR = 0.0f;

    // ── Sub-Abyss: Phase-Coherent Throat / Chest Resonance Sub-Octave ───────
    const float subAmt = std::clamp (subAmount01, 0.0f, 1.0f);
    if (subAmt > 1.0e-5f)
    {
        const float subFreq = std::clamp (0.5f * f0Hz, 18.0f, 0.25f * srF);
        subPhase += subFreq * invSr;
        if (subPhase >= 1.0f) subPhase -= std::floor (subPhase);

        const float p = kTwoPi * subPhase;
        const float rawSub = 0.78f * std::sin (p)
                           + 0.16f * std::sin (2.0f * p + 0.30f)
                           + 0.06f * std::sin (3.0f * p);

        const float sat = fastTanh (1.65f * rawSub + 0.16f) - 0.1586f;
        const float sculpted = subLp.processLowpass (subHp.processHighpass (sat));
        const float subGain = subAmt * subAmt * 0.25f * (0.25f + 0.75f * expression01);
        addL += sculpted * subGain;
        addR += sculpted * subGain;
    }

    // ── Spectral Crown: Airy Upper-Octave Falsetto Sheen ──────────────────────
    const float crownAmt = std::clamp (crownAmount01, 0.0f, 1.0f);
    if (crownAmt > 1.0e-5f)
    {
        const float crownBaseFreq = 2.0f * f0Hz;
        const float nyquistFade = std::clamp ((0.42f * srF - crownBaseFreq) / (0.08f * srF), 0.0f, 1.0f);
        if (nyquistFade > 0.0f)
        {
            const float fL = crownBaseFreq * 0.9974f;
            const float fR = crownBaseFreq * 1.0026f;

            crownPhaseL += fL * invSr;
            crownPhaseR += fR * invSr;
            if (crownPhaseL >= 1.0f) crownPhaseL -= std::floor (crownPhaseL);
            if (crownPhaseR >= 1.0f) crownPhaseR -= std::floor (crownPhaseR);

            const float wL = 0.76f * std::sin (kTwoPi * crownPhaseL)
                           + 0.24f * std::sin (kTwoPi * 2.0f * crownPhaseL);
            const float wR = 0.76f * std::sin (kTwoPi * crownPhaseR)
                           + 0.24f * std::sin (kTwoPi * 2.0f * crownPhaseR);

            const float swell = 0.25f + 0.75f * (expression01 * expression01);
            // The crown is a near-sine sheen at 2*f0. On high notes it lands squarely inside the
            // 2-4 kHz presence band and reads as a whistle, so fade it out as 2*f0 rises.
            const float pitchFade = std::clamp ((3200.0f - crownBaseFreq) / 2000.0f, 0.18f, 1.0f);
            const float crownGain = crownAmt * 0.48f * swell * nyquistFade * pitchFade;

            addL += crownBpL.processBandpass (wL) * crownGain;
            addR += crownBpR.processBandpass (wR) * crownGain;
        }
    }

    outL += sanitize (addL);
    outR += sanitize (addR);
}

} // namespace monolith::dsp
