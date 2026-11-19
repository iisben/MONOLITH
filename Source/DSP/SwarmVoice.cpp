#include "SwarmVoice.h"
#include <algorithm>
#include <cmath>

namespace monolith::dsp
{

namespace
{
    [[nodiscard]] inline uint32_t hashSeed (uint32_t x) noexcept
    {
        x ^= x >> 16;
        x *= 0x7FEB352Du;
        x ^= x >> 15;
        x *= 0x846CA68Bu;
        x ^= x >> 16;
        return (x != 0u) ? x : 1u;
    }

    [[nodiscard]] inline float seedToUnit (uint32_t s) noexcept
    {
        return static_cast<float> (s & 0x00FFFFFFu) / static_cast<float> (0x01000000u);
    }
} // namespace

void SwarmVoice::prepare (double sampleRate, int maxBlockSize, int voiceIndex)
{
    sr = std::max (8000.0, sampleRate);
    preparedMaxBlock = std::max (32, maxBlockSize);
    voiceIdx = voiceIndex;
    declickDecay = static_cast<float> (std::exp (-1.0 / (0.008 * sr)));

    voiceBufL.assign (static_cast<size_t> (preparedMaxBlock), 0.0f);
    voiceBufR.assign (static_cast<size_t> (preparedMaxBlock), 0.0f);

    for (int i = 0; i < kMaxSingersPerVoice; ++i)
    {
        auto& sg = singers[static_cast<size_t> (i)];
        const uint32_t base = static_cast<uint32_t> ((voiceIndex + 1) * 7919 + (i + 1) * 104729);
        const uint32_t h0 = hashSeed (base);
        const uint32_t h1 = hashSeed (h0);
        const uint32_t h2 = hashSeed (h1);
        const uint32_t h3 = hashSeed (h2);
        const uint32_t h4 = hashSeed (h3);

        sg.seed = h0;
        sg.vibratoRateHz   = 4.50f + 1.70f * seedToUnit (h0); // 4.5 - 6.2 Hz
        sg.vibratoDelaySec = 0.22f + 0.30f * seedToUnit (h1);
        sg.microTuneCents  = (seedToUnit (h2) * 2.0f - 1.0f) * 13.5f;
        sg.formantJitter   = 1.0f + (seedToUnit (h3) * 2.0f - 1.0f) * 0.072f;
        sg.staggerSec      = seedToUnit (h4) * 0.085f;

        sg.breathNoise.seed (h1 ^ 0xA3C59AC3u);
        sg.wanderNoise.seed (h2 ^ 0x517CC1B7u);
    }

    harmonicLayers.prepare (sr);
    entropy.prepare (sr, preparedMaxBlock, static_cast<uint32_t> (voiceIndex + 1) * 2654435761u);

    reset();
}

void SwarmVoice::reset() noexcept
{
    active            = false;
    releasing         = false;
    fastSteal         = false;
    currentMidiNote   = 60;
    targetVelocity    = 0.8f;
    smoothedVelocity  = 0.8f;
    noteAge           = 0;
    noteElapsedSec    = 0.0f;
    releaseElapsedSec = 0.0f;
    silentTailSec     = 0.0f;
    currentEnvPeak    = 0.0f;
    activeSingerCount = 0;

    currentNoteF        = 60.0f;
    targetNoteF         = 60.0f;
    glideRatePerSample  = 0.0f;
    scoopTargetSt       = 0.0f;
    scoopCurrentSt      = 0.0f;
    scoopDecayPerSample = 0.0f;
    screwSagSt          = 0.0f;

    formantSmoothPrimed = false;
    smoothedSingerNorm  = 1.0f;
    smFormantFreq.fill (500.0f);
    smFormantBw.fill (80.0f);
    smFormantGain.fill (0.0f);
    smVoicedMix         = 1.0f;
    smBreathBoost       = 0.0f;
    smNasalDip          = 0.0f;

    lastPostOutL        = 0.0f;
    lastPostOutR        = 0.0f;
    declickL            = 0.0f;
    declickR            = 0.0f;

    std::fill (voiceBufL.begin(), voiceBufL.end(), 0.0f);
    std::fill (voiceBufR.begin(), voiceBufR.end(), 0.0f);

    for (int i = 0; i < kMaxSingersPerVoice; ++i)
    {
        auto& sg = singers[static_cast<size_t> (i)];
        sg.phase        = seedToUnit (hashSeed (sg.seed + 17u));
        sg.vibratoPhase = seedToUnit (hashSeed (sg.seed + 53u)) * kTwoPi;
        sg.wanderState  = 0.0f;
        sg.activeGain   = 0.0f;
        sg.envLevel     = 0.0f;
        sg.currentPanL  = 0.70710678f;
        sg.currentPanR  = 0.70710678f;
        sg.targetPanL   = 0.70710678f;
        sg.targetPanR   = 0.70710678f;
        sg.breathNoise.resetFilter();
        sg.wanderNoise.resetFilter();
        for (auto& svf : sg.formants)
        {
            svf.reset();
            svf.invalidateCache();
        }
        sg.breathBp.reset();
        sg.onsetBpA.reset();
        sg.onsetBpB.reset();
        sg.breathBp.invalidateCache();
        sg.onsetBpA.invalidateCache();
        sg.onsetBpB.invalidateCache();
    }

    harmonicLayers.reset();
    entropy.reset();
}

void SwarmVoice::startNote (int midiNote, float velocity01, float currentGlideTimeSec, uint64_t triggerStamp) noexcept
{
    (void) currentGlideTimeSec;

    // If stealing/retriggering an active voice, bridge exact residual post-entropy sample smoothly over 8 ms
    if (active)
    {
        declickL = sanitize (lastPostOutL);
        declickR = sanitize (lastPostOutR);
        entropy.reset();
    }
    else
    {
        declickL = 0.0f;
        declickR = 0.0f;
        lastPostOutL = 0.0f;
        lastPostOutR = 0.0f;
    }

    active            = true;
    releasing         = false;
    fastSteal         = false;
    currentMidiNote   = std::clamp (midiNote, 0, 127);
    targetVelocity    = std::clamp (velocity01, 0.05f, 1.0f);
    smoothedVelocity  = targetVelocity;
    noteAge           = triggerStamp;
    noteElapsedSec    = 0.0f;
    releaseElapsedSec = 0.0f;
    silentTailSec     = 0.0f;
    currentEnvPeak    = 0.0f;

    currentNoteF        = static_cast<float> (currentMidiNote);
    targetNoteF         = currentNoteF;
    glideRatePerSample  = 0.0f;
    scoopTargetSt       = 0.0f;
    scoopCurrentSt      = 0.0f;
    scoopDecayPerSample = 0.0f;
    screwSagSt          = -0.45f;
    formantSmoothPrimed = false;

    for (int i = 0; i < kMaxSingersPerVoice; ++i)
    {
        auto& sg = singers[static_cast<size_t> (i)];
        sg.phase        = seedToUnit (hashSeed (sg.seed + static_cast<uint32_t> (midiNote * 131 + i)));
        sg.vibratoPhase = seedToUnit (hashSeed (sg.seed + 97u)) * kTwoPi;
        sg.wanderState  = 0.0f;
        sg.envLevel     = 0.0f;
        sg.activeGain   = 0.0f;
        for (auto& svf : sg.formants)
            svf.reset();
        sg.breathBp.reset();
        sg.onsetBpA.reset();
        sg.onsetBpB.reset();
    }

    harmonicLayers.reset();
}

void SwarmVoice::glideToNote (int midiNote, float velocity01, float glideTimeSec) noexcept
{
    const int clampedNote = std::clamp (midiNote, 0, 127);
    const float newTarget = static_cast<float> (clampedNote);
    const float interval  = newTarget - currentNoteF;

    currentMidiNote = clampedNote;
    targetNoteF     = newTarget;
    targetVelocity  = 0.5f * targetVelocity + 0.5f * std::clamp (velocity01, 0.05f, 1.0f);
    releasing       = false;
    fastSteal       = false;
    silentTailSec   = 0.0f;

    const float srF = static_cast<float> (sr);
    const float gSec = std::max (glideTimeSec, 0.01f);
    glideRatePerSample = 1.0f - std::exp (-3.5f / (gSec * srF));

    // Smooth bell-shaped vocal pitch-scoop on upward legato leaps >= 1.5 semitones
    if (interval >= 1.5f)
    {
        scoopTargetSt       = -0.36f;
        scoopDecayPerSample = std::exp (-3.0f / (std::max (0.045f, gSec * 0.42f) * srF));
    }
}

void SwarmVoice::stopNote (bool allowTailOff) noexcept
{
    if (! active)
        return;

    if (allowTailOff)
    {
        releasing         = true;
        releaseElapsedSec = 0.0f;
    }
    else
    {
        // 12 ms anti-click fast fade instead of hard 1-sample cut on DAW CC120 All Sound Off
        forceFastRelease();
    }
}

void SwarmVoice::forceFastRelease() noexcept
{
    if (! active)
        return;
    releasing = true;
    fastSteal = true;
}

void SwarmVoice::renderBlock (float* outL,
                              float* outR,
                              int numSamples,
                              const VoiceRenderParams& params,
                              std::array<float, 5>& formantEnergyOut,
                              float& breathEnergyOut) noexcept
{
    if (! active)
        return;

    const int n = std::min (numSamples, preparedMaxBlock);
    if (n <= 0)
        return;

    const float srF    = static_cast<float> (sr);
    const float invSrF = 1.0f / srF;

    const float spread        = std::clamp (params.spread01, 0.0f, 1.0f);
    const float targetExpr    = std::clamp (params.expression01, 0.0f, 1.0f);
    const float atouch        = std::clamp (params.aftertouch01, 0.0f, 1.0f);
    const float targetTension = std::clamp (params.glottalTension01 * (0.45f + 0.55f * targetExpr) + 0.25f * atouch, 0.0f, 1.0f);
    const float targetBreath  = std::clamp (params.breathAmount01 * (1.25f - 0.35f * targetExpr) + 0.15f * atouch, 0.0f, 1.0f);
    const float targetPitchModSt = params.pitchBendSt + params.screwPitchSt;
    const float targetSubAmt     = std::clamp (params.subAbyss01, 0.0f, 1.0f);
    const float targetCrownAmt   = std::clamp (params.spectralCrown01, 0.0f, 1.0f);

    if (! formantSmoothPrimed)
    {
        smPitchModSt = targetPitchModSt;
        smExpr       = targetExpr;
        smSubAmt     = targetSubAmt;
        smCrownAmt   = targetCrownAmt;
        smTension    = targetTension;
        smBreath     = targetBreath;
    }

    // Compute per-singer target weights and stereo pan across ALL 8 slots (smooth fade out on budget reduction)
    const float clampedTarget = std::clamp (params.singersTarget, 1.0f, static_cast<float> (kMaxSingersPerVoice));
    const int   targetCount   = std::clamp (static_cast<int> (std::ceil (clampedTarget)), 1, kMaxSingersPerVoice);

    std::array<float, kMaxSingersPerVoice> targetSingerWeights {};
    float targetWeightSum = 0.0f;
    for (int s = 0; s < kMaxSingersPerVoice; ++s)
    {
        float w = 0.0f;
        if (static_cast<float> (s) < clampedTarget)
            w = std::min (1.0f, clampedTarget - static_cast<float> (s));
        targetSingerWeights[static_cast<size_t> (s)] = w;
        targetWeightSum += w;

        float pan = 0.0f;
        if (s > 0 || targetCount > 1)
        {
            const float sign = (s % 2 == 0) ? -1.0f : 1.0f;
            const float rank = static_cast<float> ((s / 2) + 1) / static_cast<float> ((kMaxSingersPerVoice / 2));
            pan = sign * rank * spread;
        }
        const float angle = (pan + 1.0f) * 0.25f * kPi;
        auto& sg = singers[static_cast<size_t> (s)];
        sg.targetPanL = std::cos (angle);
        sg.targetPanR = std::sin (angle);
        if (! formantSmoothPrimed)
        {
            sg.currentPanL = sg.targetPanL;
            sg.currentPanR = sg.targetPanR;
        }
    }

    const float targetSingerNorm = 1.0f / std::sqrt (std::max (targetWeightSum, 1.0f));
    if (! formantSmoothPrimed)
        smoothedSingerNorm = targetSingerNorm;

    // Envelope & per-sample smoothing rates
    const float atkTime           = std::max (params.attackSec, 0.005f);
    const float relTime           = fastSteal ? 0.012f : std::max (params.releaseSec, 0.020f);
    const float atkCoeff          = 1.0f - std::exp (-3.0f / (atkTime * srF));
    const float relCoeff          = std::exp (-6.907755f / (relTime * srF));
    const float sagDecay          = std::exp (-1.0f / (0.28f * srF));
    const float wanderCoeff       = 1.0f - std::exp (-kTwoPi * 1.4f / srF);
    const float weightSmoothCoeff = 1.0f - std::exp (-1.0f / (0.025f * srF));
    const float fastParamCoeff    = 1.0f - std::exp (-1.0f / (0.008f * srF));
    const float scoopAtkCoeff     = 1.0f - std::exp (-1.0f / (0.010f * srF));

    const float onsetDurSec = 0.042f + 0.065f * (1.0f - 0.55f * targetVelocity);
    const float onsetDrive  = params.onsetAmount01 * (0.35f + 0.65f * targetVelocity);

    const VocalRegister resolvedReg = PhonemeField::resolveRegister (params.regMode, currentMidiNote);
    const float totalThroatShift = params.throatShiftSt - 8.5f * std::clamp (params.screwDepth01, 0.0f, 1.0f);

    const auto& glottalLut = LfGlottalTables::instance();

    const int ctrlStep = std::max (8, static_cast<int> (srF / 3000.0f));

    std::fill (voiceBufL.begin(), voiceBufL.begin() + n, 0.0f);
    std::fill (voiceBufR.begin(), voiceBufR.begin() + n, 0.0f);

    float blockMaxEnv = 0.0f;
    float breathAcc   = 0.0f;
    std::array<float, 5> formantAcc {};

    FormantTarget baseTarget {};

    for (int i = 0; i < n; ++i)
    {
        if (std::abs (targetNoteF - currentNoteF) > 1.0e-4f)
            currentNoteF += glideRatePerSample * (targetNoteF - currentNoteF);
        else
            currentNoteF = targetNoteF;

        scoopCurrentSt += scoopAtkCoeff * (scoopTargetSt - scoopCurrentSt);
        scoopTargetSt  *= scoopDecayPerSample;
        screwSagSt     *= sagDecay;

        smoothedVelocity   += weightSmoothCoeff * (targetVelocity - smoothedVelocity);
        smoothedSingerNorm += weightSmoothCoeff * (targetSingerNorm - smoothedSingerNorm);
        smPitchModSt       += fastParamCoeff * (targetPitchModSt - smPitchModSt);
        smExpr             += fastParamCoeff * (targetExpr - smExpr);
        smSubAmt           += fastParamCoeff * (targetSubAmt - smSubAmt);
        smCrownAmt         += fastParamCoeff * (targetCrownAmt - smCrownAmt);
        smTension          += fastParamCoeff * (targetTension - smTension);
        smBreath           += fastParamCoeff * (targetBreath - smBreath);

        const float voiceGainScale = 0.70f * (0.40f + 0.60f * smoothedVelocity);
        const float screwPitchOffset = smPitchModSt + screwSagSt * params.screwDepth01;
        const float baseMidi = currentNoteF + scoopCurrentSt + screwPitchOffset;
        const float baseF0   = std::clamp (midiToFreq (baseMidi), 18.0f, 0.42f * srF);

        if ((i % ctrlStep) == 0)
        {
            baseTarget = PhonemeField::evaluate (params.phonemeX,
                                                 params.phonemeY,
                                                 resolvedReg,
                                                 baseF0,
                                                 totalThroatShift,
                                                 params.formantFocus01);

            if (! formantSmoothPrimed)
            {
                smFormantFreq       = baseTarget.freq;
                smFormantBw         = baseTarget.bw;
                smFormantGain       = baseTarget.gain;
                smVoicedMix         = baseTarget.voicedMix;
                smBreathBoost       = baseTarget.breathBoost;
                smNasalDip          = baseTarget.nasalDip;
                formantSmoothPrimed = true;
            }
            else
            {
                // 12 ms formant frequency/bandwidth smoothing eliminates pops on Register / Preset switches
                const float ctrlDt    = static_cast<float> (ctrlStep) * invSrF;
                const float fmtAlpha  = 1.0f - std::exp (-ctrlDt / 0.012f);
                for (size_t f = 0; f < 5; ++f)
                {
                    smFormantFreq[f] += fmtAlpha * (baseTarget.freq[f] - smFormantFreq[f]);
                    smFormantBw[f]   += fmtAlpha * (baseTarget.bw[f]   - smFormantBw[f]);
                }
            }

            harmonicLayers.updateFilters (baseF0, smTension);

            for (int s = 0; s < kMaxSingersPerVoice; ++s)
            {
                auto& sg = singers[static_cast<size_t> (s)];
                if (sg.activeGain < 1.0e-5f && targetSingerWeights[static_cast<size_t> (s)] < 1.0e-5f)
                    continue;

                const float jitter = 1.0f + (sg.formantJitter - 1.0f) * spread;
                for (size_t f = 0; f < 5; ++f)
                {
                    const float fc = smFormantFreq[f] * jitter;
                    const float bw = smFormantBw[f];
                    const float q  = std::clamp (fc / std::max (bw, 20.0f), 0.5f, 20.0f);
                    sg.formants[f].setParameters (fc, q, srF);
                }

                const float breathFc = std::clamp (1700.0f + 950.0f * baseTarget.breathBoost + 850.0f * smTension, 600.0f, 5200.0f);
                sg.breathBp.setParameters (breathFc, 0.80f, srF);

                if (params.onsetMode == OnsetMode::SoftBreath)
                {
                    sg.onsetBpA.setParameters (smFormantFreq[1], 2.2f, srF);
                    sg.onsetBpB.setParameters (smFormantFreq[2], 2.6f, srF);
                }
                else
                {
                    sg.onsetBpA.setParameters (2750.0f, 3.2f, srF);
                    sg.onsetBpB.setParameters (5300.0f, 3.5f, srF);
                }
            }
        }

        // Smooth formant gains and vocal mode weights per sample to prevent ctrlStep zippering
        for (size_t f = 0; f < 5; ++f)
            smFormantGain[f] += fastParamCoeff * (baseTarget.gain[f] - smFormantGain[f]);
        smVoicedMix   += fastParamCoeff * (baseTarget.voicedMix - smVoicedMix);
        smBreathBoost += fastParamCoeff * (baseTarget.breathBoost - smBreathBoost);
        smNasalDip    += fastParamCoeff * (baseTarget.nasalDip - smNasalDip);

        float onsetEnv = 0.0f;
        if (! releasing && noteElapsedSec < onsetDurSec)
        {
            const float u = noteElapsedSec / onsetDurSec;
            onsetEnv = std::sin (kPi * u) * onsetDrive;
        }

        float sumL = 0.0f;
        float sumR = 0.0f;
        float sampleMaxEnv = 0.0f;

        // Always iterate all 8 singer slots so singers fading out on budget changes finish smoothly
        for (int s = 0; s < kMaxSingersPerVoice; ++s)
        {
            auto& sg = singers[static_cast<size_t> (s)];
            const float targetW = targetSingerWeights[static_cast<size_t> (s)];

            sg.activeGain  += weightSmoothCoeff * (targetW - sg.activeGain);
            sg.currentPanL += weightSmoothCoeff * (sg.targetPanL - sg.currentPanL);
            sg.currentPanR += weightSmoothCoeff * (sg.targetPanR - sg.currentPanR);

            if (sg.activeGain < 1.0e-5f && targetW < 1.0e-5f)
            {
                if (sg.activeGain > 0.0f)
                {
                    sg.activeGain = 0.0f;
                    for (auto& svf : sg.formants)
                        svf.reset();
                }
                continue;
            }

            if (! releasing)
            {
                const float staggerThreshold = sg.staggerSec * spread;
                if (noteElapsedSec >= staggerThreshold)
                    sg.envLevel += atkCoeff * (1.0f - sg.envLevel);
            }
            else
            {
                sg.envLevel *= relCoeff;
            }

            if (sg.envLevel > sampleMaxEnv)
                sampleMaxEnv = sg.envLevel;

            if (sg.envLevel < 1.0e-6f)
                continue;

            sg.vibratoPhase += kTwoPi * sg.vibratoRateHz * invSrF;
            if (sg.vibratoPhase >= kTwoPi) sg.vibratoPhase -= kTwoPi;

            const float vibOnsetRamp = std::clamp ((noteElapsedSec - sg.vibratoDelaySec) / 0.35f, 0.0f, 1.0f);
            const float vibCents = std::sin (sg.vibratoPhase)
                                 * (params.vibratoDepth01 * 28.0f)
                                 * vibOnsetRamp;

            sg.wanderState += wanderCoeff * (sg.wanderNoise.nextPink() - sg.wanderState);
            const float wanderCents = sg.wanderState * (10.0f + 16.0f * spread);
            const float detuneCents = sg.microTuneCents * spread;

            const float singerFreq = std::clamp (
                baseF0 * fastCentsToRatio (detuneCents + vibCents + wanderCents),
                18.0f,
                0.42f * srF);

            sg.phase += singerFreq * invSrF;
            if (sg.phase >= 1.0f) sg.phase -= std::floor (sg.phase);

            const float glottal  = glottalLut.sample (sg.phase, smTension, singerFreq, srF);
            const float openGate = glottalLut.glottalOpenGate (sg.phase, smTension);

            const float rawPink    = sg.breathNoise.nextPink();
            const float breathFilt = sg.breathBp.processBandpass (rawPink);
            const float breathAmt  = std::clamp (smBreath * 0.65f + smBreathBoost * 1.15f, 0.0f, 1.35f);
            const float breathSig  = breathFilt * (0.35f + 0.65f * openGate) * breathAmt;

            float onsetAdd = 0.0f;
            float nasalSuppress = 1.0f;
            if (onsetEnv > 1.0e-4f)
            {
                if (params.onsetMode == OnsetMode::SoftBreath)
                {
                    onsetAdd = (sg.onsetBpA.processBandpass (rawPink) * 0.6f
                              + sg.onsetBpB.processBandpass (rawPink) * 0.4f) * onsetEnv * 1.1f;
                }
                else if (params.onsetMode == OnsetMode::ClosedHum)
                {
                    nasalSuppress = 1.0f - 0.85f * onsetEnv;
                }
                else
                {
                    onsetAdd = (sg.onsetBpA.processBandpass (rawPink) * 0.55f
                              + sg.onsetBpB.processBandpass (rawPink) * 0.65f) * onsetEnv * 0.85f;
                }
            }

            const float exprScale = 0.28f + 0.72f * smExpr;
            const float voicedExc = glottal * smVoicedMix * exprScale;
            const float breathExc = breathSig * (0.35f + 0.65f * smExpr);
            const float totalExc  = voicedExc + breathExc + onsetAdd;

            breathAcc += std::abs (breathExc) * sg.activeGain * sg.envLevel;

            const float f1 = sg.formants[0].processBandpass (totalExc) * smFormantGain[0];
            const float f2 = sg.formants[1].processBandpass (totalExc) * smFormantGain[1] * nasalSuppress;
            const float f3 = sg.formants[2].processBandpass (totalExc) * smFormantGain[2] * nasalSuppress;
            const float f4 = sg.formants[3].processBandpass (totalExc) * smFormantGain[3] * nasalSuppress;
            const float f5 = sg.formants[4].processBandpass (totalExc) * smFormantGain[4] * nasalSuppress;

            const float nasalMul = 1.0f - 0.35f * smNasalDip;
            const float tractOut = (f1 + (-f2 + f3 - f4 + f5) * nasalMul) * 4.2f;

            const float singerOut = tractOut * sg.envLevel * sg.activeGain;
            sumL += singerOut * sg.currentPanL;
            sumR += singerOut * sg.currentPanR;

            if (s == 0)
            {
                const float envScale = sg.envLevel;
                formantAcc[0] += std::abs (f1) * envScale;
                formantAcc[1] += std::abs (f2) * envScale;
                formantAcc[2] += std::abs (f3) * envScale;
                formantAcc[3] += std::abs (f4) * envScale;
                formantAcc[4] += std::abs (f5) * envScale;
            }
        }

        if (sampleMaxEnv > blockMaxEnv)
            blockMaxEnv = sampleMaxEnv;

        // Per-sample envelope for Sub-Abyss & Spectral Crown (zero block-boundary staircase steps!)
        const float envForLayers = std::clamp (sampleMaxEnv, 0.0f, 1.0f);
        float outSampleL = sumL * smoothedSingerNorm * voiceGainScale;
        float outSampleR = sumR * smoothedSingerNorm * voiceGainScale;

        float layerL = 0.0f;
        float layerR = 0.0f;
        harmonicLayers.processSample (baseF0,
                                      smSubAmt,
                                      smCrownAmt,
                                      smExpr,
                                      layerL,
                                      layerR);

        outSampleL += layerL * envForLayers * voiceGainScale;
        outSampleR += layerR * envForLayers * voiceGainScale;

        voiceBufL[static_cast<size_t> (i)] = sanitize (outSampleL);
        voiceBufR[static_cast<size_t> (i)] = sanitize (outSampleR);

        noteElapsedSec += invSrF;
        if (releasing)
            releaseElapsedSec += invSrF;
    }

    // Count currently audible singers for telemetry
    int liveSingers = 0;
    for (int s = 0; s < kMaxSingersPerVoice; ++s)
    {
        if (singers[static_cast<size_t> (s)].activeGain > 0.01f)
            ++liveSingers;
    }
    activeSingerCount = std::max (liveSingers, 1);
    currentEnvPeak    = blockMaxEnv;

    // ALWAYS run per-voice EntropyStage BEFORE checking voice deactivation so tape delay tails drain cleanly
    entropy.processBlock (voiceBufL.data(),
                          voiceBufR.data(),
                          n,
                          params.drive01,
                          params.tapeInstability01,
                          params.oxideDamping01,
                          params.oversample4x);

    float postPeak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float sL = sanitize (voiceBufL[static_cast<size_t> (i)] + declickL);
        const float sR = sanitize (voiceBufR[static_cast<size_t> (i)] + declickR);
        declickL *= declickDecay;
        declickR *= declickDecay;

        outL[i] += sL;
        outR[i] += sR;

        lastPostOutL = sL;
        lastPostOutR = sR;
        postPeak = std::max (postPeak, std::max (std::abs (sL), std::abs (sR)));
    }

    const float invN = 1.0f / static_cast<float> (n);
    for (size_t f = 0; f < 5; ++f)
        formantEnergyOut[f] += formantAcc[f] * invN;
    breathEnergyOut += breathAcc * invN * smoothedSingerNorm;

    // Deactivate voice ONLY after envelope, de-click bridge, AND EntropyStage tape delay drain to silence
    if (releasing && blockMaxEnv < 1.0e-5f && postPeak < 1.0e-5f)
    {
        silentTailSec += static_cast<float> (n) * invSrF;
        if (silentTailSec >= 0.025f)
            reset();
    }
    else
    {
        silentTailSec = 0.0f;
    }
}

} // namespace monolith::dsp
