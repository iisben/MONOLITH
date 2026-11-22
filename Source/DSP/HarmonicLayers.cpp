#include "HarmonicLayers.h"
#include <algorithm>
#include <cmath>

namespace monolith::dsp
{

void HarmonicLayers::prepare (double sampleRate) noexcept
{
    sr = std::max (8000.0, sampleRate);
    (void) FastTrig::instance();
    reset();
}

void HarmonicLayers::reset() noexcept
{
    subPhase    = 0.0;
    crownPhaseL = 0.0;
    crownPhaseR = 0.37;
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
    const float srF    = static_cast<float> (sr);
    const double invSr = 1.0 / sr;
    const auto& trig   = FastTrig::instance();

    float addL = 0.0f;
    float addR = 0.0f;

    // ── Sub-Abyss: Phase-Coherent Throat / Chest Resonance Sub-Octave ───────
    const float subAmt = std::clamp (subAmount01, 0.0f, 1.0f);
    if (subAmt > 1.0e-5f)
    {
        const float subFreq = std::clamp (0.5f * f0Hz, 18.0f, 0.25f * srF);
        subPhase += static_cast<double> (subFreq) * invSr;
        if (subPhase >= 1.0) subPhase -= std::floor (subPhase);

        // 0.30 rad = 0.30 / 2π cycles. Partials above ~0.34·fs fade out (no foldover).
        const float ph  = static_cast<float> (subPhase);
        const float g2  = partialNyquistGain (2.0f * subFreq, srF);
        const float g3  = partialNyquistGain (3.0f * subFreq, srF);
        const float rawSub = 0.78f * trig.sinTurns (ph)
                           + 0.16f * g2 * trig.sinTurns (2.0f * ph + 0.0477464829f)
                           + 0.06f * g3 * trig.sinTurns (3.0f * ph);

        const float sat = fastTanh (1.65f * rawSub + 0.16f) - 0.1586f;
        const float sculpted = subLp.processLowpass (subHp.processHighpass (sat));
        const float subGain = subAmt * subAmt * 0.25f * (0.45f + 0.55f * expression01);
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

            crownPhaseL += static_cast<double> (fL) * invSr;
            crownPhaseR += static_cast<double> (fR) * invSr;
            if (crownPhaseL >= 1.0) crownPhaseL -= std::floor (crownPhaseL);
            if (crownPhaseR >= 1.0) crownPhaseR -= std::floor (crownPhaseR);

            const float h2L = partialNyquistGain (2.0f * fL, srF);
            const float h2R = partialNyquistGain (2.0f * fR, srF);
            const float phL = static_cast<float> (crownPhaseL);
            const float phR = static_cast<float> (crownPhaseR);
            const float wL = 0.76f * trig.sinTurns (phL)
                           + 0.24f * h2L * trig.sinTurns (2.0f * phL);
            const float wR = 0.76f * trig.sinTurns (phR)
                           + 0.24f * h2R * trig.sinTurns (2.0f * phR);

            const float swell = 0.25f + 0.75f * (expression01 * expression01);
            const float crownGain = crownAmt * 0.48f * swell * nyquistFade;

            addL += crownBpL.processBandpass (wL) * crownGain;
            addR += crownBpR.processBandpass (wR) * crownGain;
        }
    }

    outL += sanitize (addL);
    outR += sanitize (addR);
}

} // namespace monolith::dsp
