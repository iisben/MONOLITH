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
    rampLen = std::max (8, static_cast<int> (std::lround (kDeclickRampSec * sr)));

    voiceBufL.assign (static_cast<size_t> (preparedMaxBlock), 0.0f);
    voiceBufR.assign (static_cast<size_t> (preparedMaxBlock), 0.0f);
    envBuf.assign (static_cast<size_t> (preparedMaxBlock), 0.0f);

    const float voiceMirror = (voiceIndex % 2 == 0) ? 1.0f : -1.0f;
    for (int i = 0; i < kMaxSingersPerVoice; ++i)
    {
        auto& sg = singers[static_cast<size_t> (i)];
        const uint32_t base = static_cast<uint32_t> ((voiceIndex + 1) * 7919 + (i + 1) * 104729);
        std::array<uint32_t, 14> h {};
        h[0] = hashSeed (base);
        for (size_t k = 1; k < h.size(); ++k)
            h[k] = hashSeed (h[k - 1]);
        const auto bip = [&] (size_t k) { return seedToUnit (h[k]) * 2.0f - 1.0f; };

        sg.seed = h[0];
        sg.vibratoRateHz     = 4.80f + 1.70f * seedToUnit (h[0]);   // 4.8 - 6.5 Hz
        sg.vibratoDelaySec   = 0.22f + 0.30f * seedToUnit (h[1]);   // staggered delayed onset
        sg.microTuneCents    = bip (2) * 13.5f;
        // Vocal tract length: formants AND bandwidths scale with L_ref / L_singer = 1 +/- 7.2 % (the singer's
        // existing seeded formant offset, so every preset keeps its ensemble "take")
        sg.vtlScale          = 1.0f + bip (3) * 0.072f;
        sg.shapeDev[0]       = 0.0f;
        for (size_t f = 1; f < 5; ++f)
            sg.shapeDev[f] = bip (4 + f) * 0.018f;                    // individual tract shape, F2..F5 +/- 1.8 %
        sg.vibratoRampSec    = 0.30f + 0.15f * seedToUnit (h[9]);
        sg.vibratoDepthScale = 0.85f + 0.30f * seedToUnit (h[10]);
        sg.tensionOffset     = bip (11) * 0.04f;
        sg.attackScale       = 0.88f + 0.24f * seedToUnit (h[12]);
        sg.onsetRank         = seedToUnit (h[4]);                       // entry slot within the dispersion window
        sg.panOffset         = voiceMirror * bip (13) * 0.12f;

        sg.excDc.prepare (sr, 5.0);
        sg.breathNoise.seed (h[1] ^ 0xA3C59AC3u);
        sg.wanderNoise.seed (h[2] ^ 0x517CC1B7u);
        sg.jitterNoise.seed (h[5] ^ 0x2C1B3C6Du);
        sg.shimmerNoise.seed (h[6] ^ 0x297A2D39u);
    }

    harmonicLayers.prepare (sr);
    entropy.prepare (sr, preparedMaxBlock, static_cast<uint32_t> (voiceIndex + 1) * 2654435761u);

    cachedAttackSec = -1.0f;
    reset();
}

void SwarmVoice::reset() noexcept
{
    active            = false;
    releasing         = false;
    currentMidiNote   = 60;
    targetVelocity    = 0.8f;
    smoothedVelocity  = 0.8f;
    noteAge           = 0;
    noteSamples       = 0;
    currentEnvPeak    = 0.0f;
    activeSingerCount = 0;
    ctrlCounter       = 0;
    silentSamples     = 0;
    ctrlTarget        = FormantTarget {};

    currentNoteF        = 60.0f;
    targetNoteF         = 60.0f;
    glideRatePerSample  = 0.0f;
    scoopTargetSt       = 0.0f;
    scoopCurrentSt      = 0.0f;
    scoopDecayPerSample = 0.0f;
    screwSagSt          = 0.0f;

    formantSmoothPrimed = false;
    formantSmoother.invalidate();
    smoothedSingerNorm  = 1.0f;
    smFormantFreq.fill (500.0f);
    smFormantBw.fill (80.0f);
    smFormantGain.fill (0.0f);
    smVoicedMix         = 1.0f;
    smBreathBoost       = 0.0f;
    smNasalDip          = 0.0f;
    nasalL.reset();
    nasalR.reset();
    nasalCoeffDip       = -1.0f;

    rampKind       = Ramp::None;
    rampPos        = 0;
    fadeHold       = false;
    pendingStart   = false;
    pendingRelease = false;

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
        sg.jitterLp = sg.jitterSlow = sg.jitterCents = sg.jitterStep = 0.0f;
        sg.shimmerLp = sg.shimmerSlow = sg.shimmer = sg.shimmerStep = 0.0f;
        sg.currentPanL  = 0.70710678f;
        sg.currentPanR  = 0.70710678f;
        sg.targetPanL   = 0.70710678f;
        sg.targetPanR   = 0.70710678f;
        sg.excDc.reset();
        sg.breathNoise.resetFilter();
        sg.wanderNoise.resetFilter();
        sg.jitterNoise.resetFilter();
        sg.shimmerNoise.resetFilter();
        tractBank.reset (i);
        sg.tractSpread = -1.0f;
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

    if (active && ! fadeHold)
    {
        // Voice steal / retrigger: fade the sounding note out with a raised-cosine ramp (continuing an
        // in-flight fast-release ramp from its current gain), then start the new note sample-accurately
        // at the ramp end. EntropyStage keeps running, so its oversampler / tape states drain smoothly.
        if (rampKind == Ramp::None)
            rampPos = 0;
        rampKind        = Ramp::StealOut;
        pendingStart    = true;
        pendingRelease  = false;
        pendingNote     = std::clamp (midiNote, 0, 127);
        pendingVelocity = velocity01;
        noteAge         = triggerStamp;
        silentSamples   = 0;
        return;
    }

    beginNote (midiNote, velocity01, triggerStamp);
}

void SwarmVoice::beginNote (int midiNote, float velocity01, uint64_t triggerStamp) noexcept
{
    active            = true;
    releasing         = false;
    rampKind          = Ramp::None;
    rampPos           = 0;
    fadeHold          = false;
    pendingStart      = false;
    pendingRelease    = false;
    currentMidiNote   = std::clamp (midiNote, 0, 127);
    targetVelocity    = std::clamp (velocity01, 0.05f, 1.0f);
    smoothedVelocity  = targetVelocity;
    noteAge           = triggerStamp;
    noteSamples       = 0;
    currentEnvPeak    = 0.0f;
    ctrlCounter       = 0;
    silentSamples     = 0;

    currentNoteF        = static_cast<float> (currentMidiNote);
    targetNoteF         = currentNoteF;
    glideRatePerSample  = 0.0f;
    scoopTargetSt       = 0.0f;
    scoopCurrentSt      = 0.0f;
    scoopDecayPerSample = 0.0f;
    screwSagSt          = -0.45f;
    formantSmoothPrimed = false;
    formantSmoother.invalidate();
    nasalL.reset();
    nasalR.reset();

    for (int i = 0; i < kMaxSingersPerVoice; ++i)
    {
        auto& sg = singers[static_cast<size_t> (i)];
        sg.phase        = seedToUnit (hashSeed (sg.seed + static_cast<uint32_t> (currentMidiNote * 131 + i)));
        sg.vibratoPhase = seedToUnit (hashSeed (sg.seed + 97u)) * kTwoPi;
        sg.wanderState  = 0.0f;
        sg.envLevel     = 0.0f;
        sg.activeGain   = 0.0f;
        // Deterministic per-note humanization: reseed the 1/f jitter / shimmer sources from (singer, note)
        const uint32_t noteSeed = hashSeed (sg.seed ^ (static_cast<uint32_t> (currentMidiNote + 1) * 0x9E3779B9u));
        sg.jitterNoise.seed (noteSeed);
        sg.shimmerNoise.seed (hashSeed (noteSeed));
        sg.jitterLp = sg.jitterSlow = sg.jitterCents = sg.jitterStep = 0.0f;
        sg.shimmerLp = sg.shimmerSlow = sg.shimmer = sg.shimmerStep = 0.0f;
        sg.excDc.reset();
        tractBank.reset (i);
        sg.breathBp.reset();
        sg.onsetBpA.reset();
        sg.onsetBpB.reset();
    }

    harmonicLayers.reset();
}

void SwarmVoice::glideToNote (int midiNote, float velocity01, float glideTimeSec) noexcept
{
    const int clampedNote = std::clamp (midiNote, 0, 127);

    if (pendingStart)
    {
        // Steal ramp still running: the deferred note simply becomes the glide target
        pendingNote     = clampedNote;
        pendingVelocity = velocity01;
        pendingRelease  = false;
        return;
    }
    if (rampKind == Ramp::FastOut || fadeHold)
    {
        startNote (clampedNote, velocity01, glideTimeSec, noteAge);
        return;
    }

    const float newTarget = static_cast<float> (clampedNote);
    const float interval  = newTarget - currentNoteF;

    currentMidiNote = clampedNote;
    targetNoteF     = newTarget;
    targetVelocity  = 0.5f * targetVelocity + 0.5f * std::clamp (velocity01, 0.05f, 1.0f);
    releasing       = false;
    silentSamples   = 0;

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
        if (pendingStart)
            pendingRelease = true; // note-off for the deferred note: start it, then release at once
        else
            releasing = true;
    }
    else
    {
        // CC120 All Sound Off: 4 ms raised-cosine fade instead of a hard 1-sample cut
        forceFastRelease();
    }
}

void SwarmVoice::forceFastRelease() noexcept
{
    if (! active)
        return;
    releasing      = true;
    pendingStart   = false; // a deferred steal start is cancelled; the running ramp keeps its gain
    pendingRelease = false;
    if (fadeHold)
        return;
    if (rampKind == Ramp::None)
        rampPos = 0;
    rampKind = Ramp::FastOut;
}

void SwarmVoice::renderSynth (int offset,
                              int count,
                              const VoiceRenderParams& params,
                              std::array<float, 5>& formantAcc,
                              float& breathAcc) noexcept
{
    const float  srF    = static_cast<float> (sr);
    const float  invSrF = 1.0f / srF;
    const double invSrD = 1.0 / sr;

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

        // Haas-safe placement: equal-power level panning only (no inter-channel delay, so mono fold-down has
        // no comb), alternating seats mirrored per voice plus a seeded per-singer seat offset.
        auto& sg = singers[static_cast<size_t> (s)];
        float pan = 0.0f;
        if (s > 0 || targetCount > 1)
        {
            const float sign = ((s % 2 == 0) ? -1.0f : 1.0f) * ((voiceIdx % 2 == 0) ? 1.0f : -1.0f);
            const float rank = static_cast<float> ((s / 2) + 1) / static_cast<float> ((kMaxSingersPerVoice / 2));
            pan = std::clamp ((sign * rank + sg.panOffset) * spread, -1.0f, 1.0f);
        }
        const float angle = (pan + 1.0f) * 0.25f * kPi;
        sg.targetPanL = std::cos (angle);
        sg.targetPanR = std::sin (angle);
        if (! formantSmoothPrimed)
        {
            sg.currentPanL = sg.targetPanL;
            sg.currentPanR = sg.targetPanR;
        }
    }

    // Budget-conserving ensemble loudness: normalise to the ensemble size asked for, not the budget-capped
    // count, so a 24-singer choir split across many notes keeps its total energy (no extra +6 dB that would
    // drive the bus into the safety limiter). Identical to the per-voice unit normalisation when uncapped.
    const float targetSingerNorm = 1.0f / std::sqrt (std::max ({ targetWeightSum, params.singersReference, 1.0f }));
    if (! formantSmoothPrimed)
        smoothedSingerNorm = targetSingerNorm;

    // RC envelopes (exact one-pole charge / discharge); per-singer attack humanization, cached per attack time
    const float atkTime           = std::max (params.attackSec, 0.005f);
    const float relTime           = std::max (params.releaseSec, 0.020f);
    if (std::abs (atkTime - cachedAttackSec) > 0.0f)
    {
        cachedAttackSec = atkTime;
        for (auto& sg : singers)
            sg.attackCoeff = 1.0f - std::exp (-3.0f / (atkTime * sg.attackScale * srF));
    }
    const float relCoeff          = std::exp (-6.907755f / (relTime * srF));
    const float sagDecay          = std::exp (-1.0f / (0.28f * srF));
    const float wanderCoeff       = 1.0f - std::exp (-kTwoPi * 1.4f / srF);
    const float weightSmoothCoeff = 1.0f - std::exp (-1.0f / (0.025f * srF));
    const float fastParamCoeff    = 1.0f - std::exp (-1.0f / (0.008f * srF));
    const float scoopAtkCoeff     = 1.0f - std::exp (-1.0f / (0.010f * srF));

    const float onsetDurSec = 0.042f + 0.065f * (1.0f - 0.55f * targetVelocity);
    const float onsetDrive  = params.onsetAmount01 * (0.35f + 0.65f * targetVelocity);
    // Controllable onset dispersion: each singer enters at its seeded slot within this window
    const float dispersionSec = 0.085f * std::clamp (params.onsetDispersion01, 0.0f, 1.0f);

    const VocalRegister resolvedReg = PhonemeField::resolveRegister (params.regMode, currentMidiNote);
    const float totalThroatShift = params.throatShiftSt - 8.5f * std::clamp (params.screwDepth01, 0.0f, 1.0f);

    const auto& glottalLut = LfGlottalTables::instance();
    const float log2Sr = std::log2 (std::max (srF, 8000.0f));

    const int ctrlStep = std::max (8, static_cast<int> (srF / 3000.0f));

    // Seeded 1/f micro-jitter (~1.0-1.7 cents rms) & shimmer (~1.6 % rms) at control rate, per-sample ramped
    const float ctrlDtSec      = static_cast<float> (ctrlStep) * invSrF;
    const float invCtrlStep    = 1.0f / static_cast<float> (ctrlStep);
    const float jitterLpCoeff  = 1.0f - std::exp (-kTwoPi * 30.0f * ctrlDtSec);
    const float shimmerLpCoeff = 1.0f - std::exp (-kTwoPi * 20.0f * ctrlDtSec);
    const float jitterDepth    = 9.0f * (0.6f + 0.4f * spread);
    const float shimmerDepth   = 0.093f;
    const float driftHpCoeff   = 1.0f - std::exp (-kTwoPi * 3.0f * ctrlDtSec);

    FormantTarget& baseTarget = ctrlTarget;

    for (int i = 0; i < count; ++i)
    {
        const float noteSec = static_cast<float> (static_cast<double> (noteSamples) * invSrD);

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
        const float rawF0    = midiToFreq (baseMidi);
        const float baseF0   = std::clamp (rawF0, 18.0f, 0.42f * srF);
        // Exact log2 f0 from the MIDI pitch (log2 440 + (m - 69) / 12): no per-singer log2 in the glottal mip pick
        const bool  f0InRange  = rawF0 >= 18.0f && rawF0 <= 0.42f * srF;
        const float log2BaseF0 = f0InRange ? 8.7813597f + (baseMidi - 69.0f) * (1.0f / 12.0f)
                                                   : std::log2 (baseF0);

        const bool ctrlTick = (ctrlCounter == 0);
        if (++ctrlCounter >= ctrlStep)
            ctrlCounter = 0;

        if (ctrlTick)
        {
            baseTarget = PhonemeField::evaluate (params.phonemeX,
                                                 params.phonemeY,
                                                 resolvedReg,
                                                 baseF0,
                                                 totalThroatShift,
                                                 params.formantFocus01);

            if (! formantSmoothPrimed)
            {
                formantSmoother.prime (baseTarget);
                smFormantGain       = baseTarget.gain;
                smVoicedMix         = baseTarget.voicedMix;
                smBreathBoost       = baseTarget.breathBoost;
                smNasalDip          = baseTarget.nasalDip;
                formantSmoothPrimed = true;
            }
            else
            {
                // 12 ms log-domain formant frequency/bandwidth glide (epsilon-settled: steady vowels skip all
                // coefficient work) eliminates pops on Register / Preset switches
                const double ctrlDt = static_cast<double> (ctrlStep) / sr;
                formantSmoother.advance (baseTarget, 1.0 - std::exp (-ctrlDt / 0.012));
            }

            const bool tractDirty = formantSmoother.isDirty();
            if (tractDirty)
                for (size_t f = 0; f < 5; ++f)
                {
                    smFormantFreq[f] = static_cast<float> (formantSmoother.freq (f));
                    smFormantBw[f]   = static_cast<float> (formantSmoother.bw (f));
                }

            if (std::abs (smNasalDip - nasalCoeffDip) > 1.0e-6f)
            {
                nasalCoeffDip = smNasalDip;
                nasalL.set (nasalCoeffDip, sr);
                nasalR.set (nasalCoeffDip, sr);
            }
            if (! (nasalL.isFinite() && nasalR.isFinite()))
            {
                nasalL.reset();
                nasalR.reset();
            }

            harmonicLayers.updateFilters (baseF0, smTension);

            for (int s = 0; s < kMaxSingersPerVoice; ++s)
            {
                auto& sg = singers[static_cast<size_t> (s)];
                if (sg.activeGain < 1.0e-5f && targetSingerWeights[static_cast<size_t> (s)] < 1.0e-5f)
                {
                    sg.tractSpread = -1.0f; // coefficients go stale while parked
                    continue;
                }

                // 1/f jitter (3-30 Hz) and shimmer (3-20 Hz): slow drift stays with the existing wander, so these
                // perturb pitch / level without re-rolling the ensemble's slow beating or feeding sub-Hz AM into drive
                sg.jitterLp  += jitterLpCoeff * (sg.jitterNoise.nextPink() - sg.jitterLp);
                sg.jitterSlow  += driftHpCoeff * (sg.jitterLp - sg.jitterSlow);
                sg.shimmerLp += shimmerLpCoeff * (sg.shimmerNoise.nextPink() - sg.shimmerLp);
                sg.shimmerSlow += driftHpCoeff * (sg.shimmerLp - sg.shimmerSlow);
                sg.jitterStep  = ((sg.jitterLp - sg.jitterSlow) * jitterDepth - sg.jitterCents) * invCtrlStep;
                sg.shimmerStep = ((sg.shimmerLp - sg.shimmerSlow) * shimmerDepth - sg.shimmer) * invCtrlStep;

                if (tractDirty || std::abs (spread - sg.tractSpread) > 0.0f)
                {
                    // Per-singer vocal tract length scales every formant AND bandwidth (constant Q), plus a small
                    // individual tract-shape deviation on F2..F5; both widen with ensemble spread.
                    sg.tractSpread = spread;
                    const double vtl = 1.0 + (static_cast<double> (sg.vtlScale) - 1.0) * static_cast<double> (spread);
                    for (int f = 0; f < VocalTract::kNumFormants; ++f)
                    {
                        const auto fi = static_cast<size_t> (f);
                        const double scale = vtl * (1.0 + static_cast<double> (sg.shapeDev[fi] * spread));
                        const double fc = formantSmoother.freq (fi) * scale;
                        tractBank.setFormant (s, f, fc, VocalTract::qFor (fc, formantSmoother.bw (fi) * scale), sr);
                    }
                }
                if (! tractBank.isFinite (s))
                    tractBank.reset (s);

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
        if (! releasing && noteSec < onsetDurSec)
        {
            const float u = noteSec / onsetDurSec;
            onsetEnv = sinTurns (0.5f * u) * onsetDrive;
        }

        float sumL = 0.0f;
        float sumR = 0.0f;
        float sampleMaxEnv = 0.0f;

        // Closed-hum onset suppression of F2..F5 is voice-wide (depends only on the onset envelope)
        const float nasalSuppress = (onsetEnv > 1.0e-4f && params.onsetMode == OnsetMode::ClosedHum)
                                        ? 1.0f - 0.85f * onsetEnv : 1.0f;

        // SoA lanes for the SIMD tract bank: pass 1 builds per-singer excitation, pass 2 runs all
        // tracts in lock-step, pass 3 mixes (same summation order as the former scalar loop).
        alignas (64) double laneExc[kMaxSingersPerVoice] = {};
        alignas (64) double laneOut[kMaxSingersPerVoice] = {};
        bool laneOn[kMaxSingersPerVoice] = {};
        int  laneCount = 0;

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
                    tractBank.reset (s);
                }
                continue;
            }

            if (! releasing)
            {
                if (noteSec >= sg.onsetRank * dispersionSec)
                    sg.envLevel += sg.attackCoeff * (1.0f - sg.envLevel);
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

            // Staggered delayed-onset vibrato with a C1-smooth (smoothstep) per-singer onset ramp
            float vibOnsetRamp = 0.0f;
            if (noteSec > sg.vibratoDelaySec)
            {
                const float u = std::min ((noteSec - sg.vibratoDelaySec) / sg.vibratoRampSec, 1.0f);
                vibOnsetRamp = u * u * (3.0f - 2.0f * u);
            }
            const float vibCents = fastSin (sg.vibratoPhase)
                                 * (params.vibratoDepth01 * 28.0f * sg.vibratoDepthScale)
                                 * vibOnsetRamp;

            sg.jitterCents += sg.jitterStep;
            sg.shimmer     += sg.shimmerStep;

            sg.wanderState += wanderCoeff * (sg.wanderNoise.nextPink() - sg.wanderState);
            const float wanderCents = sg.wanderState * (10.0f + 16.0f * spread);
            const float detuneCents = sg.microTuneCents * spread;

            const float totalCents = detuneCents + vibCents + wanderCents + sg.jitterCents;
            const float rawSingerF = baseF0 * fastCentsToRatio (totalCents);
            const float singerFreq = std::clamp (rawSingerF, 18.0f, 0.42f * srF);
            const bool  sgInRange   = rawSingerF >= 18.0f && rawSingerF <= 0.42f * srF;
            const float log2SingerF = sgInRange ? log2BaseF0 + totalCents * (1.0f / 1200.0f)
                                                                 : std::log2 (singerFreq);

            sg.phase += singerFreq * invSrF;
            if (sg.phase >= 1.0f) sg.phase -= std::floor (sg.phase);

            // Per-singer glottal tension offset, limited by the headroom on both sides so the mean stays put
            const float tensionRoom = std::clamp (std::min (smTension, 1.0f - smTension) * 16.6667f, 0.0f, 1.0f);
            const float sgTension = smTension + sg.tensionOffset * spread * tensionRoom;
            const float glottal   = glottalLut.sampleLog2 (sg.phase, sgTension, log2SingerF, log2Sr);
            const float openGate  = glottalLut.glottalOpenGate (sg.phase, sgTension);

            const float rawPink    = sg.breathNoise.nextPink();
            const float breathFilt = sg.breathBp.processBandpass (rawPink);
            const float breathAmt  = std::clamp (smBreath * 0.65f + smBreathBoost * 1.15f, 0.0f, 1.35f);
            // Aspiration turbulence follows open-phase glottal airflow (small leakage floor when closed)
            const float breathSig  = breathFilt * (0.25f + 0.75f * openGate) * breathAmt;

            float onsetAdd = 0.0f;
            if (onsetEnv > 1.0e-4f)
            {
                if (params.onsetMode == OnsetMode::SoftBreath)
                {
                    onsetAdd = (sg.onsetBpA.processBandpass (rawPink) * 0.6f
                              + sg.onsetBpB.processBandpass (rawPink) * 0.4f) * onsetEnv * 1.1f;
                }
                else if (params.onsetMode == OnsetMode::ClosedHum)
                {
                    // F2..F5 suppression applied voice-wide via nasalSuppress
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
            const float totalExc  = static_cast<float> (sg.excDc.process (static_cast<double> (voicedExc) + breathExc)) + onsetAdd;

            breathAcc += std::abs (breathExc) * sg.activeGain * sg.envLevel;

            laneExc[s] = static_cast<double> (totalExc);
            laneOn[s]  = true;
            laneCount  = s + 1;
        }
        // (bank rounds laneCount up to a 2 / 4 / 8 SIMD width; parked lanes inside stay frozen)

        // 64-bit notch-free Klatt parallel tracts, all singers at once (nasal pole-zero acts on the voice sum)
        const double tractGain[VocalTract::kNumFormants] = {
            static_cast<double> (smFormantGain[0]),
            static_cast<double> (smFormantGain[1] * nasalSuppress),
            static_cast<double> (smFormantGain[2] * nasalSuppress),
            static_cast<double> (smFormantGain[3] * nasalSuppress),
            static_cast<double> (smFormantGain[4] * nasalSuppress) };
        double lane0Branch[VocalTract::kNumFormants] = {};
        tractBank.process (laneExc, tractGain, laneOn, laneOut, lane0Branch, laneCount);

        for (int s = 0; s < laneCount; ++s)
        {
            if (! laneOn[s])
                continue;
            const auto& sg = singers[static_cast<size_t> (s)];
            const float tractOut  = static_cast<float> (laneOut[s]);
            const float singerOut = tractOut * sg.envLevel * sg.activeGain * (1.0f + sg.shimmer);
            sumL += singerOut * sg.currentPanL;
            sumR += singerOut * sg.currentPanR;

            if (s == 0)
            {
                const float envScale = sg.envLevel;
                for (size_t k = 0; k < static_cast<size_t> (VocalTract::kNumFormants); ++k)
                    formantAcc[k] += std::abs (static_cast<float> (lane0Branch[k])) * envScale;
            }
        }

        envBuf[static_cast<size_t> (offset + i)] = sampleMaxEnv;

        // Per-sample envelope for Sub-Abyss & Spectral Crown (zero block-boundary staircase steps!)
        const float envForLayers = std::clamp (sampleMaxEnv, 0.0f, 1.0f);
        sumL = static_cast<float> (nasalL.process (static_cast<double> (sumL)));
        sumR = static_cast<float> (nasalR.process (static_cast<double> (sumR)));

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

        // Raised-cosine de-click ramp (steal / fast release): reaches exactly 0 on its last sample
        float rampGain = 1.0f;
        if (fadeHold)
        {
            rampGain = 0.0f;
        }
        else if (rampKind != Ramp::None)
        {
            ++rampPos;
            rampGain = 0.5f + 0.5f * fastCos (kPi * static_cast<float> (rampPos) / static_cast<float> (rampLen));
        }

        voiceBufL[static_cast<size_t> (offset + i)] = sanitize (outSampleL * rampGain);
        voiceBufR[static_cast<size_t> (offset + i)] = sanitize (outSampleR * rampGain);

        ++noteSamples;
    }
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

    float breathAcc = 0.0f;
    std::array<float, 5> formantAcc {};

    // Synthesis in segments split exactly at ramp completion, so a deferred (stolen) note starts on the
    // same absolute sample regardless of host block partitioning.
    int countFrom = 0; // samples before this index belong to a steal ramp and never count as tail silence
    const auto completeRamp = [&] (int at) noexcept {
        if (rampKind == Ramp::StealOut)
        {
            const bool releaseAtOnce = pendingRelease;
            beginNote (pendingNote, pendingVelocity, noteAge);
            releasing = releaseAtOnce;
            countFrom = at;
        }
        else
        {
            rampKind = Ramp::None;
            fadeHold = true;
            for (auto& sg : singers)
                sg.envLevel = 0.0f;
        }
    };

    int pos = 0;
    while (pos < n)
    {
        if (rampKind != Ramp::None && rampPos >= rampLen)
            completeRamp (pos);
        int seg = n - pos;
        if (rampKind != Ramp::None)
            seg = std::min (seg, rampLen - rampPos);
        renderSynth (pos, seg, params, formantAcc, breathAcc);
        pos += seg;
    }
    if (rampKind != Ramp::None && rampPos >= rampLen)
        completeRamp (n);
    if (pendingStart)
        countFrom = n;

    // Count currently audible singers for telemetry
    int liveSingers = 0;
    for (int s = 0; s < kMaxSingersPerVoice; ++s)
    {
        if (singers[static_cast<size_t> (s)].activeGain > 0.01f)
            ++liveSingers;
    }
    activeSingerCount = std::max (liveSingers, 1);
    currentEnvPeak    = envBuf[static_cast<size_t> (n - 1)]; // partition-invariant (last-sample envelope)

    // ALWAYS run per-voice EntropyStage BEFORE checking voice deactivation so tape delay tails drain cleanly
    entropy.processBlock (voiceBufL.data(),
                          voiceBufR.data(),
                          n,
                          params.drive01,
                          params.tapeInstability01,
                          params.oxideDamping01,
                          params.oversample4x,
                          params.controlCellStart);

    for (int i = 0; i < n; ++i)
    {
        const float sL = sanitize (voiceBufL[static_cast<size_t> (i)]);
        const float sR = sanitize (voiceBufR[static_cast<size_t> (i)]);

        outL[i] += sL;
        outR[i] += sR;

        if (i >= countFrom && releasing && envBuf[static_cast<size_t> (i)] < 1.0e-5f && std::abs (sL) < 1.0e-5f && std::abs (sR) < 1.0e-5f)
            ++silentSamples;
        else
            silentSamples = 0;
    }

    const float invN = 1.0f / static_cast<float> (n);
    for (size_t f = 0; f < 5; ++f)
        formantEnergyOut[f] += formantAcc[f] * invN;
    breathEnergyOut += breathAcc * invN * smoothedSingerNorm;

    // Deactivate voice ONLY after 25 ms of per-sample silence (envelope, de-click ramp AND
    // EntropyStage tape drain), and only on an absolute control-cell boundary (block-size invariant).
    if (params.controlCellEnd && ! pendingStart && silentSamples >= static_cast<int> (0.025 * sr))
        reset();
}

} // namespace monolith::dsp
