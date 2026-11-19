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
    velocity          = 0.8f;
    noteAge           = 0;
    noteElapsedSec    = 0.0f;
    releaseElapsedSec = 0.0f;
    currentEnvPeak    = 0.0f;
    activeSingerCount = 0;

    currentNoteF        = 60.0f;
    targetNoteF         = 60.0f;
    glideRatePerSample  = 0.0f;
    scoopAmountSt       = 0.0f;
    scoopDecayPerSample = 0.0f;
    screwSagSt          = 0.0f;

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

void SwarmVoice::startNote (int midiNote, float velocity01, float currentGlideTimeSec) noexcept
{
    active            = true;
    releasing         = false;
    fastSteal         = false;
    currentMidiNote   = std::clamp (midiNote, 0, 127);
    velocity          = std::clamp (velocity01, 0.05f, 1.0f);
    noteElapsedSec    = 0.0f;
    releaseElapsedSec = 0.0f;
    currentEnvPeak    = 0.0f;

    currentNoteF       = static_cast<float> (currentMidiNote);
    targetNoteF        = currentNoteF;
    glideRatePerSample = 0.0f;
    scoopAmountSt      = 0.0f;
    screwSagSt         = -0.45f; // Initial tape-droop onset impulse when Screw Shadow > 0
    (void) currentGlideTimeSec;

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
    velocity        = 0.5f * velocity + 0.5f * std::clamp (velocity01, 0.05f, 1.0f);
    releasing       = false;
    fastSteal       = false;

    const float srF = static_cast<float> (sr);
    const float gSec = std::max (glideTimeSec, 0.01f);
    glideRatePerSample = 1.0f - std::exp (-3.5f / (gSec * srF));

    // Natural subtle vocal pitch-scoop on upward legato leaps >= 1.5 semitones
    if (interval >= 1.5f)
    {
        scoopAmountSt = -0.36f;
        scoopDecayPerSample = std::exp (-3.0f / (std::max (0.04f, gSec * 0.42f) * srF));
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
        reset();
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
                              uint64_t globalSampleCounter,
                              std::array<float, 5>& formantEnergyOut,
                              float& breathEnergyOut) noexcept
{
    if (! active)
        return;

    const int n = std::min (numSamples, preparedMaxBlock);
    if (n <= 0)
        return;

    noteAge = globalSampleCounter;

    const float srF    = static_cast<float> (sr);
    const float invSrF = 1.0f / srF;

    const float spread    = std::clamp (params.spread01, 0.0f, 1.0f);
    const float expr      = std::clamp (params.expression01, 0.0f, 1.0f);
    const float atouch    = std::clamp (params.aftertouch01, 0.0f, 1.0f);
    const float effTension = std::clamp (params.glottalTension01 * (0.45f + 0.55f * expr) + 0.25f * atouch, 0.0f, 1.0f);
    const float effBreath  = std::clamp (params.breathAmount01 * (1.25f - 0.35f * expr) + 0.15f * atouch, 0.0f, 1.0f);

    // Singer budgeting for this voice (1..8 fractional target -> smooth fade on highest singer)
    const float clampedTarget = std::clamp (params.singersTarget, 1.0f, static_cast<float> (kMaxSingersPerVoice));
    const int   numActive     = std::clamp (static_cast<int> (std::ceil (clampedTarget)), 1, kMaxSingersPerVoice);
    activeSingerCount = numActive;

    // Update stereo pan & target activation weights per singer
    std::array<float, kMaxSingersPerVoice> targetSingerWeights {};
    float activeWeightSum = 0.0f;
    for (int s = 0; s < kMaxSingersPerVoice; ++s)
    {
        float w = 0.0f;
        if (static_cast<float> (s) < clampedTarget)
        {
            const float rem = clampedTarget - static_cast<float> (s);
            w = std::min (1.0f, rem);
        }
        targetSingerWeights[static_cast<size_t> (s)] = w;
        activeWeightSum += w;

        // Deterministic symmetric stereo spread around center
        float pan = 0.0f;
        if (s > 0 || numActive > 1)
        {
            const float sign = (s % 2 == 0) ? -1.0f : 1.0f;
            const float rank = static_cast<float> ((s / 2) + 1) / static_cast<float> ((kMaxSingersPerVoice / 2));
            pan = sign * rank * spread;
        }
        const float angle = (pan + 1.0f) * 0.25f * kPi;
        singers[static_cast<size_t> (s)].panL = std::cos (angle);
        singers[static_cast<size_t> (s)].panR = std::sin (angle);
    }

    const float singerNorm = 1.0f / std::sqrt (std::max (activeWeightSum, 1.0f));
    const float voiceGainScale = 0.70f * (0.40f + 0.60f * velocity);

    // Envelope rates
    const float atkTime  = std::max (params.attackSec, 0.005f);
    const float relTime  = fastSteal ? 0.012f : std::max (params.releaseSec, 0.020f);
    const float atkCoeff = 1.0f - std::exp (-3.0f / (atkTime * srF));
    const float relCoeff = std::exp (-6.907755f / (relTime * srF));
    const float sagDecay = std::exp (-1.0f / (0.28f * srF));
    const float wanderCoeff = 1.0f - std::exp (-kTwoPi * 1.4f / srF);
    const float weightSmoothCoeff = 1.0f - std::exp (-1.0f / (0.015f * srF));

    const float onsetDurSec = 0.042f + 0.065f * (1.0f - 0.55f * velocity);
    const float onsetDrive  = params.onsetAmount01 * (0.35f + 0.65f * velocity);

    const VocalRegister resolvedReg = PhonemeField::resolveRegister (params.regMode, currentMidiNote);
    const float totalThroatShift = params.throatShiftSt - 8.5f * std::clamp (params.screwDepth01, 0.0f, 1.0f);

    const auto& glottalLut = LfGlottalTables::instance();

    // Update formant filter parameters at ~3 kHz control rate (sample-rate independent)
    const int ctrlStep = std::max (8, static_cast<int> (srF / 3000.0f));

    std::fill (voiceBufL.begin(), voiceBufL.begin() + n, 0.0f);
    std::fill (voiceBufR.begin(), voiceBufR.begin() + n, 0.0f);

    float maxBlockEnv = 0.0f;
    float breathAcc   = 0.0f;
    std::array<float, 5> formantAcc {};

    FormantTarget baseTarget {};

    for (int i = 0; i < n; ++i)
    {
        // Advance pitch glide, scoop & screw droop
        if (std::abs (targetNoteF - currentNoteF) > 1.0e-4f)
            currentNoteF += glideRatePerSample * (targetNoteF - currentNoteF);
        else
            currentNoteF = targetNoteF;

        scoopAmountSt *= scoopDecayPerSample;
        screwSagSt    *= sagDecay;

        const float screwPitchOffset = params.screwPitchSt + screwSagSt * params.screwDepth01;
        const float baseMidi = currentNoteF + scoopAmountSt + params.pitchBendSt + screwPitchOffset;
        const float baseF0   = std::clamp (midiToFreq (baseMidi), 18.0f, 0.42f * srF);

        if ((i % ctrlStep) == 0)
        {
            baseTarget = PhonemeField::evaluate (params.phonemeX,
                                                 params.phonemeY,
                                                 resolvedReg,
                                                 baseF0,
                                                 totalThroatShift,
                                                 params.formantFocus01);

            for (int s = 0; s < numActive; ++s)
            {
                auto& sg = singers[static_cast<size_t> (s)];
                const float jitter = 1.0f + (sg.formantJitter - 1.0f) * spread;
                for (size_t f = 0; f < 5; ++f)
                {
                    const float fc = baseTarget.freq[f] * jitter;
                    const float bw = baseTarget.bw[f];
                    const float q  = std::clamp (fc / std::max (bw, 20.0f), 0.5f, 20.0f);
                    sg.formants[f].setParameters (fc, q, srF);
                }

                const float breathFc = std::clamp (1700.0f + 950.0f * baseTarget.breathBoost + 850.0f * effTension, 600.0f, 5200.0f);
                sg.breathBp.setParameters (breathFc, 0.80f, srF);

                if (params.onsetMode == OnsetMode::SoftBreath)
                {
                    sg.onsetBpA.setParameters (baseTarget.freq[1], 2.2f, srF);
                    sg.onsetBpB.setParameters (baseTarget.freq[2], 2.6f, srF);
                }
                else
                {
                    sg.onsetBpA.setParameters (2750.0f, 3.2f, srF);
                    sg.onsetBpB.setParameters (5300.0f, 3.5f, srF);
                }
            }
        }

        // Onset articulator envelope (active during initial note attack window)
        float onsetEnv = 0.0f;
        if (! releasing && noteElapsedSec < onsetDurSec)
        {
            const float u = noteElapsedSec / onsetDurSec;
            onsetEnv = std::sin (kPi * u) * onsetDrive;
        }

        float sumL = 0.0f;
        float sumR = 0.0f;

        for (int s = 0; s < numActive; ++s)
        {
            auto& sg = singers[static_cast<size_t> (s)];

            // Smooth singer budget crossfade
            sg.activeGain += weightSmoothCoeff * (targetSingerWeights[static_cast<size_t> (s)] - sg.activeGain);
            if (sg.activeGain < 1.0e-5f && targetSingerWeights[static_cast<size_t> (s)] < 1.0e-5f)
                continue;

            // Lung envelope with per-singer entrance stagger
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

            if (sg.envLevel > maxBlockEnv)
                maxBlockEnv = sg.envLevel;

            if (sg.envLevel < 1.0e-5f)
                continue;

            // Delayed vibrato + 1/f pink pitch wander + deterministic micro-detune
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
                baseF0 * semitonesToRatio ((detuneCents + vibCents + wanderCents) * 0.01f),
                18.0f,
                0.42f * srF);

            sg.phase += singerFreq * invSrF;
            if (sg.phase >= 1.0f) sg.phase -= std::floor (sg.phase);

            // Band-limited LF glottal pulse + glottal-phase-modulated aspirated breath
            const float glottal  = glottalLut.sample (sg.phase, effTension, singerFreq, srF);
            const float openGate = glottalLut.glottalOpenGate (sg.phase, effTension);

            const float rawPink    = sg.breathNoise.nextPink();
            const float breathFilt = sg.breathBp.processBandpass (rawPink);
            const float breathAmt  = std::clamp (effBreath * 0.65f + baseTarget.breathBoost * 1.15f, 0.0f, 1.35f);
            const float breathSig  = breathFilt * (0.35f + 0.65f * openGate) * breathAmt;

            // Consonant Onset Articulator
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
                    // Closed-mouth 'M'/'N' attack suppresses upper formants during initial closure
                    nasalSuppress = 1.0f - 0.85f * onsetEnv;
                }
                else
                {
                    onsetAdd = (sg.onsetBpA.processBandpass (rawPink) * 0.55f
                              + sg.onsetBpB.processBandpass (rawPink) * 0.65f) * onsetEnv * 0.85f;
                }
            }

            const float exprScale = 0.28f + 0.72f * expr;
            const float voicedExc = glottal * baseTarget.voicedMix * exprScale;
            const float breathExc = breathSig * (0.35f + 0.65f * expr);
            const float totalExc  = voicedExc + breathExc + onsetAdd;

            breathAcc += std::abs (breathExc) * sg.activeGain * sg.envLevel;

            // 5-Formant Parallel Vocal Tract Bank (+F1 -F2 +F3 -F4 +F5)
            const float f1 = sg.formants[0].processBandpass (totalExc) * baseTarget.gain[0];
            const float f2 = sg.formants[1].processBandpass (totalExc) * baseTarget.gain[1] * nasalSuppress;
            const float f3 = sg.formants[2].processBandpass (totalExc) * baseTarget.gain[2] * nasalSuppress;
            const float f4 = sg.formants[3].processBandpass (totalExc) * baseTarget.gain[3] * nasalSuppress;
            const float f5 = sg.formants[4].processBandpass (totalExc) * baseTarget.gain[4] * nasalSuppress;

            // Nasal dip on closed 'Hum' state gently softens F2/F3 resonance
            const float nasalMul = 1.0f - 0.35f * baseTarget.nasalDip;
            const float tractOut = (f1 + (-f2 + f3 - f4 + f5) * nasalMul) * 4.2f;

            const float singerOut = tractOut * sg.envLevel * sg.activeGain;
            sumL += singerOut * sg.panL;
            sumR += singerOut * sg.panR;

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

        const float envForLayers = std::clamp (maxBlockEnv, 0.0f, 1.0f);
        float outSampleL = sumL * singerNorm * voiceGainScale;
        float outSampleR = sumR * singerNorm * voiceGainScale;

        // Add Sub-Abyss and Spectral Crown layers
        float layerL = 0.0f;
        float layerR = 0.0f;
        harmonicLayers.processSample (baseF0,
                                      params.subAbyss01,
                                      params.spectralCrown01,
                                      effTension,
                                      expr,
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

    currentEnvPeak = maxBlockEnv;

    if (releasing && maxBlockEnv < 1.0e-4f)
    {
        reset();
        return;
    }

    // Per-Voice Pre-Sum Oversampled Analog Saturation, Tape Drift & Oxide Damping
    entropy.processBlock (voiceBufL.data(),
                          voiceBufR.data(),
                          n,
                          params.drive01,
                          params.tapeInstability01,
                          params.oxideDamping01,
                          params.oversample4x);

    for (int i = 0; i < n; ++i)
    {
        outL[i] += voiceBufL[static_cast<size_t> (i)];
        outR[i] += voiceBufR[static_cast<size_t> (i)];
    }

    const float invN = 1.0f / static_cast<float> (n);
    for (size_t f = 0; f < 5; ++f)
        formantEnergyOut[f] += formantAcc[f] * invN;
    breathEnergyOut += breathAcc * invN * singerNorm;
}

} // namespace monolith::dsp
