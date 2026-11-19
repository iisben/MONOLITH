#include "SwarmEngine.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace monolith::dsp
{

namespace
{
    [[nodiscard]] inline float loadParam (const std::atomic<float>* ptr, float fallback) noexcept
    {
        return (ptr != nullptr) ? ptr->load (std::memory_order_relaxed) : fallback;
    }

    [[nodiscard]] inline float dbToGain (float db) noexcept
    {
        return std::exp (db * 0.1151292546497022842f);
    }

    inline void smoothToward (float& state, float target, float alpha) noexcept
    {
        state += alpha * (target - state);
    }
} // namespace

void SwarmEngine::prepare (double sampleRate, int maxBlockSize, const juce::AudioProcessorValueTreeState& apvts)
{
    sr = std::max (8000.0, sampleRate);
    preparedMaxBlock = std::max (32, maxBlockSize);

    ptrs.phonemeX       = apvts.getRawParameterValue (params::phonemeX);
    ptrs.phonemeY       = apvts.getRawParameterValue (params::phonemeY);
    ptrs.orbitEnable    = apvts.getRawParameterValue (params::orbitEnable);
    ptrs.orbitRate      = apvts.getRawParameterValue (params::orbitRate);
    ptrs.orbitDepth     = apvts.getRawParameterValue (params::orbitDepth);
    ptrs.orbitSync      = apvts.getRawParameterValue (params::orbitSync);

    ptrs.registerMode   = apvts.getRawParameterValue (params::registerMode);
    ptrs.ensembleSize   = apvts.getRawParameterValue (params::ensembleSize);
    ptrs.ensembleSpread = apvts.getRawParameterValue (params::ensembleSpread);
    ptrs.vibratoDepth   = apvts.getRawParameterValue (params::vibratoDepth);
    ptrs.attackTime     = apvts.getRawParameterValue (params::attackTime);
    ptrs.releaseTime    = apvts.getRawParameterValue (params::releaseTime);
    ptrs.onsetMode      = apvts.getRawParameterValue (params::onsetMode);
    ptrs.onsetAmount    = apvts.getRawParameterValue (params::onsetAmount);

    ptrs.glottalTension = apvts.getRawParameterValue (params::glottalTension);
    ptrs.breathAmount   = apvts.getRawParameterValue (params::breathAmount);
    ptrs.throatShift    = apvts.getRawParameterValue (params::throatShift);
    ptrs.formantFocus   = apvts.getRawParameterValue (params::formantFocus);
    ptrs.subAbyss       = apvts.getRawParameterValue (params::subAbyss);
    ptrs.spectralCrown  = apvts.getRawParameterValue (params::spectralCrown);
    ptrs.masterGain     = apvts.getRawParameterValue (params::masterGain);

    ptrs.driveAmount    = apvts.getRawParameterValue (params::driveAmount);
    ptrs.tapeInstability= apvts.getRawParameterValue (params::tapeInstability);
    ptrs.oxideDamping   = apvts.getRawParameterValue (params::oxideDamping);
    ptrs.screwPitch     = apvts.getRawParameterValue (params::screwPitch);
    ptrs.screwDepth     = apvts.getRawParameterValue (params::screwDepth);
    ptrs.oversampleHq   = apvts.getRawParameterValue (params::oversampleHq);

    ptrs.spaceType      = apvts.getRawParameterValue (params::spaceType);
    ptrs.spaceMix       = apvts.getRawParameterValue (params::spaceMix);
    ptrs.spaceDecay     = apvts.getRawParameterValue (params::spaceDecay);
    ptrs.spaceDamp      = apvts.getRawParameterValue (params::spaceDamp);
    ptrs.spaceShimmer   = apvts.getRawParameterValue (params::spaceShimmer);
    ptrs.spaceFreeze    = apvts.getRawParameterValue (params::spaceFreeze);
    ptrs.cc64Freeze     = apvts.getRawParameterValue (params::cc64Freeze);

    ptrs.macroColossus  = apvts.getRawParameterValue (params::macroColossus);
    ptrs.macroSpecter   = apvts.getRawParameterValue (params::macroSpecter);
    ptrs.macroRot       = apvts.getRawParameterValue (params::macroRot);
    ptrs.macroAbyss     = apvts.getRawParameterValue (params::macroAbyss);
    ptrs.legatoEnable   = apvts.getRawParameterValue (params::legatoEnable);
    ptrs.glideTime      = apvts.getRawParameterValue (params::glideTime);
    ptrs.expressionLevel= apvts.getRawParameterValue (params::expressionLevel);

    // Pre-initialize shared read-only static tables outside processBlock()
    (void) LfGlottalTables::instance();
    (void) PhonemeField::evaluate (0.5f, 0.8f, VocalRegister::Tenor, 220.0f, 0.0f, 0.5f);

    for (int i = 0; i < kMaxVoices; ++i)
        voices[static_cast<size_t> (i)].prepare (sr, preparedMaxBlock, i);

    orbitEngine.prepare (sr);
    reverb.prepare (sr, preparedMaxBlock);

    busL.assign (static_cast<size_t> (preparedMaxBlock), 0.0f);
    busR.assign (static_cast<size_t> (preparedMaxBlock), 0.0f);

    smoothMasterGain.reset (sr, 0.025);
    const float initGain = dbToGain (loadParam (ptrs.masterGain, params::defaults::masterGain));
    smoothMasterGain.setCurrentAndTargetValue (initGain);

    const float initExpr = loadParam (ptrs.expressionLevel, params::defaults::expressionLevel);
    lastParamExpression = initExpr;
    midiModWheel = initExpr;
    uiExpression.store (initExpr, std::memory_order_relaxed);

    reset();
}

void SwarmEngine::reset() noexcept
{
    for (auto& v : voices)
        v.reset();

    orbitEngine.reset();
    reverb.reset();

    std::fill (busL.begin(), busL.end(), 0.0f);
    std::fill (busR.begin(), busR.end(), 0.0f);

    pitchBendSemitones = 0.0f;
    channelAftertouch  = 0.0f;
    sustainPedalDown   = false;
    cc64FreezeActive   = false;
    sustainedNotes.fill (false);
    heldNotes.fill (-1);
    heldVelocities.fill (0.0f);
    heldCount          = 0;
    lastTriggeredNote  = -1;
    sampleCounter      = 0;
    noteTriggerSeq     = 0;
    midiCc1Received    = false;
    smoothersPrimed    = false;
}

int SwarmEngine::getLatencySamples() const noexcept
{
    const bool hq4x = loadParam (ptrs.oversampleHq, 0.0f) >= 0.5f;
    return voices[0].getLatencySamples (hq4x);
}

void SwarmEngine::panicImmediate() noexcept
{
    for (auto& v : voices)
        v.reset();

    reverb.reset();
    sustainPedalDown = false;
    cc64FreezeActive = false;
    sustainedNotes.fill (false);
    heldNotes.fill (-1);
    heldCount = 0;
    lastTriggeredNote = -1;
}

int SwarmEngine::allocateVoiceIndex() noexcept
{
    // 1. Prefer completely free inactive voice
    for (int i = 0; i < kMaxVoices; ++i)
    {
        if (! voices[static_cast<size_t> (i)].isActive())
            return i;
    }

    // 2. Steal releasing voice with lowest amplitude
    int bestIdx = -1;
    float lowestScore = 1.0e9f;
    for (int i = 0; i < kMaxVoices; ++i)
    {
        if (voices[static_cast<size_t> (i)].isReleasing())
        {
            const float score = voices[static_cast<size_t> (i)].getCurrentLevel();
            if (score < lowestScore)
            {
                lowestScore = score;
                bestIdx = i;
            }
        }
    }
    if (bestIdx >= 0)
        return bestIdx;

    // 3. Steal chronologically oldest active voice (using monotonic noteTriggerSeq)
    uint64_t oldestAge = std::numeric_limits<uint64_t>::max();
    bestIdx = 0;
    for (int i = 0; i < kMaxVoices; ++i)
    {
        const uint64_t age = voices[static_cast<size_t> (i)].getAge();
        if (age < oldestAge)
        {
            oldestAge = age;
            bestIdx = i;
        }
    }
    return bestIdx;
}

void SwarmEngine::triggerNoteOn (int midiNote, float velocity01, bool legatoOn, float glideSec) noexcept
{
    const int note = std::clamp (midiNote, 0, 127);
    const float vel = std::clamp (velocity01, 0.05f, 1.0f);
    const uint64_t stamp = ++noteTriggerSeq;

    if (legatoOn)
    {
        int existingPos = -1;
        for (int i = 0; i < heldCount; ++i)
        {
            if (heldNotes[static_cast<size_t> (i)] == note)
            {
                existingPos = i;
                break;
            }
        }
        if (existingPos < 0 && heldCount < kMaxHeldNotes)
        {
            heldNotes[static_cast<size_t> (heldCount)] = note;
            heldVelocities[static_cast<size_t> (heldCount)] = vel;
            ++heldCount;
        }

        if (heldCount > 1 && voices[0].isActive())
        {
            voices[0].glideToNote (note, vel, glideSec);
            lastTriggeredNote = note;
            return;
        }

        for (int i = 1; i < kMaxVoices; ++i)
        {
            if (voices[static_cast<size_t> (i)].isActive())
                voices[static_cast<size_t> (i)].forceFastRelease();
        }
        voices[0].startNote (note, vel, glideSec, stamp);
        lastTriggeredNote = note;
        return;
    }

    sustainedNotes[static_cast<size_t> (note)] = false;

    // If the same note is currently held (duplicate NoteOn without NoteOff), retrigger that voice cleanly
    for (int i = 0; i < kMaxVoices; ++i)
    {
        if (voices[static_cast<size_t> (i)].isActive()
            && ! voices[static_cast<size_t> (i)].isReleasing()
            && voices[static_cast<size_t> (i)].getMidiNote() == note)
        {
            voices[static_cast<size_t> (i)].startNote (note, vel, glideSec, stamp);
            lastTriggeredNote = note;
            return;
        }
    }

    // Otherwise allocate a free voice first so releasing tails of the same note overlap organically
    const int idx = allocateVoiceIndex();
    voices[static_cast<size_t> (idx)].startNote (note, vel, glideSec, stamp);
    lastTriggeredNote = note;
}

void SwarmEngine::triggerNoteOff (int midiNote, bool legatoOn, float glideSec) noexcept
{
    const int note = std::clamp (midiNote, 0, 127);

    if (legatoOn)
    {
        for (int i = 0; i < heldCount; ++i)
        {
            if (heldNotes[static_cast<size_t> (i)] == note)
            {
                for (int j = i; j < heldCount - 1; ++j)
                {
                    heldNotes[static_cast<size_t> (j)] = heldNotes[static_cast<size_t> (j + 1)];
                    heldVelocities[static_cast<size_t> (j)] = heldVelocities[static_cast<size_t> (j + 1)];
                }
                --heldCount;
                break;
            }
        }

        if (heldCount > 0 && voices[0].isActive())
        {
            const int prevNote = heldNotes[static_cast<size_t> (heldCount - 1)];
            const float prevVel = heldVelocities[static_cast<size_t> (heldCount - 1)];
            voices[0].glideToNote (prevNote, prevVel, glideSec);
            lastTriggeredNote = prevNote;
        }
        else if (! sustainPedalDown)
        {
            voices[0].stopNote (true);
        }
        else
        {
            sustainedNotes[static_cast<size_t> (note)] = true;
        }
        return;
    }

    if (sustainPedalDown)
    {
        sustainedNotes[static_cast<size_t> (note)] = true;
        return;
    }

    for (auto& v : voices)
    {
        if (v.isActive() && ! v.isReleasing() && v.getMidiNote() == note)
            v.stopNote (true);
    }
}

void SwarmEngine::handleMidiMessage (const juce::MidiMessage& msg,
                                     bool legatoOn,
                                     float glideSec,
                                     bool cc64TriggersFreeze) noexcept
{
    if (msg.isNoteOn())
    {
        triggerNoteOn (msg.getNoteNumber(), msg.getFloatVelocity(), legatoOn, glideSec);
    }
    else if (msg.isNoteOff())
    {
        triggerNoteOff (msg.getNoteNumber(), legatoOn, glideSec);
    }
    else if (msg.isPitchWheel())
    {
        const float norm = (static_cast<float> (msg.getPitchWheelValue()) - 8192.0f) / 8192.0f;
        pitchBendSemitones = std::clamp (norm * 2.0f, -2.0f, 2.0f);
    }
    else if (msg.isAftertouch())
    {
        channelAftertouch = static_cast<float> (msg.getAfterTouchValue()) / 127.0f;
    }
    else if (msg.isChannelPressure())
    {
        channelAftertouch = static_cast<float> (msg.getChannelPressureValue()) / 127.0f;
    }
    else if (msg.isController())
    {
        const int cc  = msg.getControllerNumber();
        const int val = msg.getControllerValue();

        if (cc == 1)
        {
            midiModWheel = static_cast<float> (val) / 127.0f;
            midiCc1Received = true;
        }
        else if (cc == 64)
        {
            const bool down = val >= 64;
            if (cc64TriggersFreeze)
            {
                cc64FreezeActive = down;
                if (sustainPedalDown && ! down)
                    sustainPedalDown = false;
            }
            else
            {
                cc64FreezeActive = false;
                if (sustainPedalDown && ! down)
                {
                    sustainPedalDown = false;
                    for (int n = 0; n < 128; ++n)
                    {
                        if (sustainedNotes[static_cast<size_t> (n)])
                        {
                            sustainedNotes[static_cast<size_t> (n)] = false;
                            triggerNoteOff (n, legatoOn, glideSec);
                        }
                    }
                }
                else
                {
                    sustainPedalDown = down;
                }
            }
        }
        else if (cc == 120 || cc == 123)
        {
            for (auto& v : voices)
                v.stopNote (cc == 123);
            heldCount = 0;
            sustainPedalDown = false;
            sustainedNotes.fill (false);
        }
    }
}

void SwarmEngine::processBlock (juce::AudioBuffer<float>& buffer,
                                juce::MidiBuffer& midiMessages,
                                double hostBpm,
                                TelemetrySnapshot& telemetry) noexcept
{
    juce::ScopedNoDenormals noDenormals;

    const int totalSamples = buffer.getNumSamples();
    const int numChannels  = buffer.getNumChannels();
    if (totalSamples <= 0 || numChannels <= 0)
        return;

    if (panicRequested.exchange (false, std::memory_order_acq_rel))
        panicImmediate();

    for (int ch = 2; ch < numChannels; ++ch)
        buffer.clear (ch, 0, totalSamples);

    // Read base APVTS parameters
    const float basePhX        = loadParam (ptrs.phonemeX, params::defaults::phonemeX);
    const float basePhY        = loadParam (ptrs.phonemeY, params::defaults::phonemeY);
    const bool  orbitOn        = loadParam (ptrs.orbitEnable, 0.0f) >= 0.5f;
    const float orbitRateHz    = loadParam (ptrs.orbitRate, params::defaults::orbitRate);
    const float orbitDepth01   = loadParam (ptrs.orbitDepth, params::defaults::orbitDepth);
    const bool  orbitSyncOn    = loadParam (ptrs.orbitSync, 0.0f) >= 0.5f;

    const auto  regMode        = static_cast<VocalRegister> (
        std::clamp (static_cast<int> (std::lround (loadParam (ptrs.registerMode, 4.0f))), 0, 4));
    const float baseEnsSize    = loadParam (ptrs.ensembleSize, params::defaults::ensembleSize);
    const float baseSpread     = loadParam (ptrs.ensembleSpread, params::defaults::ensembleSpread);
    const float baseVibrato    = loadParam (ptrs.vibratoDepth, params::defaults::vibratoDepth);
    const float baseAttack     = loadParam (ptrs.attackTime, params::defaults::attackTime);
    const float baseRelease    = loadParam (ptrs.releaseTime, params::defaults::releaseTime);
    const auto  onsetModeVal   = static_cast<OnsetMode> (
        std::clamp (static_cast<int> (std::lround (loadParam (ptrs.onsetMode, 0.0f))), 0, 2));
    const float baseOnsetAmt   = loadParam (ptrs.onsetAmount, params::defaults::onsetAmount);

    const float baseTension    = loadParam (ptrs.glottalTension, params::defaults::glottalTension);
    const float baseBreath     = loadParam (ptrs.breathAmount, params::defaults::breathAmount);
    const float baseThroat     = loadParam (ptrs.throatShift, params::defaults::throatShift);
    const float baseFocus      = loadParam (ptrs.formantFocus, params::defaults::formantFocus);
    const float baseSub        = loadParam (ptrs.subAbyss, params::defaults::subAbyss);
    const float baseCrown      = loadParam (ptrs.spectralCrown, params::defaults::spectralCrown);
    const float masterGainDb   = loadParam (ptrs.masterGain, params::defaults::masterGain);

    const float baseDrive      = loadParam (ptrs.driveAmount, params::defaults::driveAmount);
    const float baseTape       = loadParam (ptrs.tapeInstability, params::defaults::tapeInstability);
    const float baseOxide      = loadParam (ptrs.oxideDamping, params::defaults::oxideDamping);
    const float baseScrewPitch = loadParam (ptrs.screwPitch, params::defaults::screwPitch);
    const float baseScrewDepth = loadParam (ptrs.screwDepth, params::defaults::screwDepth);
    const bool  os4x           = loadParam (ptrs.oversampleHq, 0.0f) >= 0.5f;

    const auto  spaceModel     = static_cast<SpaceType> (
        std::clamp (static_cast<int> (std::lround (loadParam (ptrs.spaceType, 0.0f))), 0, 3));
    const float baseSpaceMix   = loadParam (ptrs.spaceMix, params::defaults::spaceMix);
    const float baseSpaceDecay = loadParam (ptrs.spaceDecay, params::defaults::spaceDecay);
    const float baseSpaceDamp  = loadParam (ptrs.spaceDamp, params::defaults::spaceDamp);
    const float baseShimmer    = loadParam (ptrs.spaceShimmer, params::defaults::spaceShimmer);
    const bool  uiFreeze       = loadParam (ptrs.spaceFreeze, 0.0f) >= 0.5f;
    const bool  cc64FreezeOn   = loadParam (ptrs.cc64Freeze, 0.0f) >= 0.5f;

    const float colossusTarget = loadParam (ptrs.macroColossus, 0.0f);
    const float specterTarget  = loadParam (ptrs.macroSpecter, 0.0f);
    const float rotTarget      = loadParam (ptrs.macroRot, 0.0f);
    const float abyssTarget    = loadParam (ptrs.macroAbyss, 0.0f);

    const bool  legatoOn       = loadParam (ptrs.legatoEnable, 0.0f) >= 0.5f;
    const float glideSec       = loadParam (ptrs.glideTime, params::defaults::glideTime);
    const float paramExpr      = loadParam (ptrs.expressionLevel, params::defaults::expressionLevel);

    if (uiExprTouched.exchange (false, std::memory_order_acq_rel)
        || std::abs (paramExpr - lastParamExpression) > 1.0e-4f)
    {
        lastParamExpression = paramExpr;
        midiCc1Received = false;
    }

    if (! smoothersPrimed)
    {
        smPhX             = basePhX;
        smPhY             = basePhY;
        smEnsSize         = baseEnsSize;
        smSpread          = baseSpread;
        smVibrato         = baseVibrato;
        smTension         = baseTension;
        smBreath          = baseBreath;
        smThroat          = baseThroat;
        smFocus           = baseFocus;
        smSub             = baseSub;
        smCrown           = baseCrown;
        smDrive           = baseDrive;
        smTape            = baseTape;
        smOxide           = baseOxide;
        smScrewPitch      = baseScrewPitch;
        smScrewDepth      = baseScrewDepth;
        smPitchBend       = pitchBendSemitones;
        smAftertouch      = channelAftertouch;
        smExpr            = midiCc1Received ? midiModWheel : paramExpr;
        smColossus        = colossusTarget;
        smSpecter         = specterTarget;
        smRot             = rotTarget;
        smAbyss           = abyssTarget;
        smSingersPerVoice = 1.0f + (baseEnsSize - 1.0f) * (7.0f / 23.0f);
        smoothersPrimed   = true;
    }

    std::array<float, 5> formantAcc {};
    float breathAcc   = 0.0f;
    float sumSqL      = 0.0f;
    float sumSqR      = 0.0f;
    float peakL       = 0.0f;
    float peakR       = 0.0f;
    int   chunkCount  = 0;

    float lastEffPhX     = smPhX;
    float lastEffPhY     = smPhY;
    float lastEffSpread  = smSpread;
    float lastEffTension = smTension;
    float lastEffBreath  = smBreath;
    float lastLiveExpr   = smExpr;

    const int maxStep = std::min (preparedMaxBlock, kControlSubBlockMax);

    auto renderSpan = [&] (int startOffset, int spanSamples) noexcept
    {
        int pos = startOffset;
        int remaining = spanSamples;

        while (remaining > 0)
        {
            const int chunk = std::min (remaining, maxStep);
            ++chunkCount;

            // Sample-rate-independent sub-block exponential smoothing (22 ms controls, 35 ms singer budget)
            const float dtSec      = static_cast<float> (chunk) / static_cast<float> (sr);
            const float ctrlAlpha  = 1.0f - std::exp (-dtSec / 0.022f);
            const float exprAlpha  = 1.0f - std::exp (-dtSec / 0.030f);
            const float sizeAlpha  = 1.0f - std::exp (-dtSec / 0.045f);

            smoothToward (smPhX, basePhX, ctrlAlpha);
            smoothToward (smPhY, basePhY, ctrlAlpha);
            smoothToward (smEnsSize, baseEnsSize, sizeAlpha);
            smoothToward (smSpread, baseSpread, ctrlAlpha);
            smoothToward (smVibrato, baseVibrato, ctrlAlpha);
            smoothToward (smTension, baseTension, ctrlAlpha);
            smoothToward (smBreath, baseBreath, ctrlAlpha);
            smoothToward (smThroat, baseThroat, ctrlAlpha);
            smoothToward (smFocus, baseFocus, ctrlAlpha);
            smoothToward (smSub, baseSub, ctrlAlpha);
            smoothToward (smCrown, baseCrown, ctrlAlpha);
            smoothToward (smDrive, baseDrive, ctrlAlpha);
            smoothToward (smTape, baseTape, ctrlAlpha);
            smoothToward (smOxide, baseOxide, ctrlAlpha);
            smoothToward (smScrewPitch, baseScrewPitch, ctrlAlpha);
            smoothToward (smScrewDepth, baseScrewDepth, ctrlAlpha);
            smoothToward (smPitchBend, pitchBendSemitones, ctrlAlpha);
            smoothToward (smAftertouch, channelAftertouch, exprAlpha);
            smoothToward (smExpr, midiCc1Received ? midiModWheel : paramExpr, exprAlpha);
            smoothToward (smColossus, colossusTarget, ctrlAlpha);
            smoothToward (smSpecter, specterTarget, ctrlAlpha);
            smoothToward (smRot, rotTarget, ctrlAlpha);
            smoothToward (smAbyss, abyssTarget, ctrlAlpha);

            const float cCurve = smColossus * (0.65f + 0.35f * smColossus);
            const float sCurve = smSpecter  * (0.65f + 0.35f * smSpecter);
            const float rCurve = smRot      * (0.65f + 0.35f * smRot);
            const float aCurve = smAbyss    * (0.65f + 0.35f * smAbyss);

            const float effSize    = std::clamp (smEnsSize + 10.0f * cCurve, 1.0f, 24.0f);
            const float effSpread  = std::clamp (smSpread + 0.30f * cCurve, 0.0f, 1.0f);
            const float effSub     = std::clamp (smSub + 0.45f * cCurve + 0.16f * aCurve, 0.0f, 1.0f);
            const float effCrown   = std::clamp (smCrown + 0.35f * cCurve + 0.25f * sCurve, 0.0f, 1.0f);
            const float effBreath  = std::clamp (smBreath + 0.48f * sCurve, 0.0f, 1.0f);
            const float effTension = std::clamp (smTension + 0.22f * cCurve - 0.38f * sCurve, 0.0f, 1.0f);
            const float effThroat  = std::clamp (smThroat - 2.2f * sCurve - 1.8f * rCurve, -12.0f, 12.0f);
            const float effDrive   = std::clamp (smDrive + 0.52f * rCurve + 0.12f * cCurve, 0.0f, 1.0f);
            const float effTape    = std::clamp (smTape + 0.58f * rCurve + 0.10f * sCurve, 0.0f, 1.0f);
            const float effOxide   = std::clamp (smOxide + 0.48f * rCurve, 0.0f, 1.0f);

            const float effSpaceMix   = std::clamp (baseSpaceMix + 0.32f * aCurve + 0.14f * cCurve + 0.14f * sCurve, 0.0f, 1.0f);
            const float effSpaceDecay = std::clamp (baseSpaceDecay + 0.28f * aCurve + 0.12f * cCurve, 0.0f, 1.0f);
            const float effSpaceDamp  = std::clamp (baseSpaceDamp + 0.34f * aCurve, 0.0f, 1.0f);
            const float effShimmer    = std::clamp (baseShimmer + 0.42f * sCurve + 0.22f * aCurve, 0.0f, 1.0f);

            const float compDb = -1.8f * cCurve - 1.2f * rCurve;
            smoothMasterGain.setTargetValue (dbToGain (masterGainDb + compDb));

            orbitEngine.advance (chunk, orbitOn, orbitRateHz, orbitDepth01, orbitSyncOn, hostBpm, smPhX, smPhY);
            const float orbitX = orbitEngine.getEffectiveX();
            const float orbitY = orbitEngine.getEffectiveY();
            const float effPhX = std::clamp (orbitX + sCurve * (0.45f - orbitX) * 0.55f, 0.0f, 1.0f);
            const float effPhY = std::clamp (orbitY + sCurve * (0.20f - orbitY) * 0.65f, 0.0f, 1.0f);

            // Smooth dynamic 24-virtual-singer budgeting (zero step jumps when adding 4th+ chord notes)
            int activeCount = 0;
            for (const auto& v : voices)
            {
                if (v.isActive())
                    ++activeCount;
            }
            const float desiredPerVoice = 1.0f + (effSize - 1.0f) * (7.0f / 23.0f);
            float targetSingersPerVoice = desiredPerVoice;
            if (activeCount >= 4)
            {
                const float budgetCap = 24.0f / static_cast<float> (activeCount);
                const float minStereo = (effSize >= 2.0f) ? 2.0f : 1.0f;
                targetSingersPerVoice = std::clamp (std::min (desiredPerVoice, budgetCap), minStereo, 8.0f);
            }
            smoothToward (smSingersPerVoice, targetSingersPerVoice, sizeAlpha);

            VoiceRenderParams vParams;
            vParams.phonemeX          = effPhX;
            vParams.phonemeY          = effPhY;
            vParams.regMode           = regMode;
            vParams.singersTarget     = smSingersPerVoice;
            vParams.spread01          = effSpread;
            vParams.vibratoDepth01    = smVibrato;
            vParams.attackSec         = baseAttack;
            vParams.releaseSec        = baseRelease;
            vParams.onsetMode         = onsetModeVal;
            vParams.onsetAmount01     = baseOnsetAmt;
            vParams.glottalTension01  = effTension;
            vParams.breathAmount01    = effBreath;
            vParams.throatShiftSt     = effThroat;
            vParams.formantFocus01    = smFocus;
            vParams.subAbyss01        = effSub;
            vParams.spectralCrown01   = effCrown;
            vParams.drive01           = effDrive;
            vParams.tapeInstability01 = effTape;
            vParams.oxideDamping01    = effOxide;
            vParams.screwPitchSt      = smScrewPitch;
            vParams.screwDepth01      = smScrewDepth;
            vParams.oversample4x      = os4x;
            vParams.pitchBendSt       = smPitchBend;
            vParams.expression01      = smExpr;
            vParams.aftertouch01      = smAftertouch;
            vParams.glideTimeSec      = glideSec;

            const bool freezeOn = uiFreeze || (cc64FreezeOn && cc64FreezeActive);

            std::fill (busL.begin(), busL.begin() + chunk, 0.0f);
            std::fill (busR.begin(), busR.begin() + chunk, 0.0f);

            for (auto& v : voices)
            {
                if (v.isActive())
                    v.renderBlock (busL.data(), busR.data(), chunk, vParams, formantAcc, breathAcc);
            }

            reverb.processBlock (busL.data(),
                                 busR.data(),
                                 chunk,
                                 spaceModel,
                                 effSpaceMix,
                                 effSpaceDecay,
                                 effSpaceDamp,
                                 effShimmer,
                                 freezeOn);

            float* outL = buffer.getWritePointer (0) + pos;
            float* outR = (numChannels > 1) ? (buffer.getWritePointer (1) + pos) : nullptr;

            for (int i = 0; i < chunk; ++i)
            {
                const float g = smoothMasterGain.getNextValue();
                const float l = softSafetyLimit (busL[static_cast<size_t> (i)] * g);
                const float r = softSafetyLimit (busR[static_cast<size_t> (i)] * g);

                if (outR != nullptr)
                {
                    outL[i] = l;
                    outR[i] = r;
                }
                else
                {
                    outL[i] = 0.5f * (l + r);
                }

                sumSqL += l * l;
                sumSqR += r * r;
                peakL = std::max (peakL, std::abs (l));
                peakR = std::max (peakR, std::abs (r));
            }

            lastEffPhX     = effPhX;
            lastEffPhY     = effPhY;
            lastEffSpread  = effSpread;
            lastEffTension = effTension;
            lastEffBreath  = effBreath;
            lastLiveExpr   = smExpr;

            sampleCounter += static_cast<uint64_t> (chunk);
            pos       += chunk;
            remaining -= chunk;
        }
    };

    // Sample-accurate MIDI dispatch interleaved with smooth sub-block audio rendering
    int cursor = 0;
    for (const auto metadata : midiMessages)
    {
        const int eventSample = std::clamp (metadata.samplePosition, 0, totalSamples);
        if (eventSample > cursor)
        {
            renderSpan (cursor, eventSample - cursor);
            cursor = eventSample;
        }
        handleMidiMessage (metadata.getMessage(), legatoOn, glideSec, cc64FreezeOn);
    }

    if (cursor < totalSamples)
        renderSpan (cursor, totalSamples - cursor);

    // Update lock-free UI telemetry
    int liveVoices  = 0;
    int liveSingers = 0;
    for (const auto& v : voices)
    {
        if (v.isActive())
        {
            ++liveVoices;
            liveSingers += v.getActiveSingerCount();
        }
    }

    const float invTotal  = 1.0f / static_cast<float> (totalSamples);
    const float invChunks = 1.0f / static_cast<float> (std::max (chunkCount, 1));
    const float rmsL = std::sqrt (sumSqL * invTotal);
    const float rmsR = std::sqrt (sumSqR * invTotal);

    telemetry.rmsL.store (rmsL, std::memory_order_relaxed);
    telemetry.rmsR.store (rmsR, std::memory_order_relaxed);
    telemetry.peakL.store (peakL, std::memory_order_relaxed);
    telemetry.peakR.store (peakR, std::memory_order_relaxed);
    telemetry.audioEnergy.store (std::clamp ((rmsL + rmsR) * 1.4f, 0.0f, 1.0f), std::memory_order_relaxed);
    telemetry.breathDensity.store (std::clamp ((breathAcc * invChunks) * 0.45f + lastEffBreath * 0.45f, 0.0f, 1.0f),
                                   std::memory_order_relaxed);
    telemetry.swarmSpreadLive.store (lastEffSpread, std::memory_order_relaxed);
    telemetry.tensionLive.store (lastEffTension, std::memory_order_relaxed);
    telemetry.effectivePhonemeX.store (lastEffPhX, std::memory_order_relaxed);
    telemetry.effectivePhonemeY.store (lastEffPhY, std::memory_order_relaxed);
    telemetry.liveExpression.store (lastLiveExpr, std::memory_order_relaxed);
    telemetry.freezeEnergy.store (reverb.getFreezeEnergy(), std::memory_order_relaxed);
    telemetry.activeVoices.store (liveVoices, std::memory_order_relaxed);
    telemetry.activeSingers.store (std::min (liveSingers, 24), std::memory_order_relaxed);

    const float normFormant = invChunks / static_cast<float> (std::max (liveVoices, 1));
    for (size_t f = 0; f < 5; ++f)
        telemetry.formantEnergy[f].store (std::clamp (formantAcc[f] * normFormant * 2.2f, 0.0f, 1.0f),
                                          std::memory_order_relaxed);
}

} // namespace monolith::dsp
