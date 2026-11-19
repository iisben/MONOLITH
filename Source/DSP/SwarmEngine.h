#pragma once

#include "../State/ParameterIDs.h"
#include "../State/TelemetrySnapshot.h"
#include "BrutalistReverb.h"
#include "PhonemeField.h"
#include "SwarmVoice.h"
#include <array>
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

namespace monolith::dsp
{

/**
    Top-level 12-voice / 24-virtual-singer choral swarm engine.
    Provides sample-accurate MIDI sub-block dispatch, 64-sample smooth control
    sub-blocking across all continuous parameters, dynamic 24-singer budgeting,
    4 curved Master Macros, legato stacking, architectural reverb, and lock-free
    UI telemetry output.
*/
class SwarmEngine
{
public:
    static constexpr int kMaxVoices          = 12;
    static constexpr int kMaxHeldNotes       = 16;
    static constexpr int kControlSubBlockMax = 64;

    void prepare (double sampleRate, int maxBlockSize, const juce::AudioProcessorValueTreeState& apvts);
    void reset() noexcept;

    [[nodiscard]] int getLatencySamples() const noexcept;

    /**
        100% real-time-safe block processor with sample-accurate MIDI timing
        and automatic sub-block chunking <= min(preparedMaxBlockSize, 64).
    */
    void processBlock (juce::AudioBuffer<float>& buffer,
                       juce::MidiBuffer& midiMessages,
                       double hostBpm,
                       TelemetrySnapshot& telemetry) noexcept;

    /** Thread-safe panic trigger (sets atomic consumed on audio thread). */
    void requestPanic() noexcept { panicRequested.store (true, std::memory_order_release); }

    /** Allows direct UI drag of Expression bar while merging smoothly with MIDI CC1. */
    void setUiExpressionOverride (float val01) noexcept
    {
        uiExpression.store (std::clamp (val01, 0.0f, 1.0f), std::memory_order_relaxed);
        uiExprTouched.store (true, std::memory_order_release);
    }

private:
    struct CachedParamPtrs
    {
        std::atomic<float>* phonemeX       = nullptr;
        std::atomic<float>* phonemeY       = nullptr;
        std::atomic<float>* orbitEnable    = nullptr;
        std::atomic<float>* orbitRate      = nullptr;
        std::atomic<float>* orbitDepth     = nullptr;
        std::atomic<float>* orbitSync      = nullptr;

        std::atomic<float>* registerMode   = nullptr;
        std::atomic<float>* ensembleSize   = nullptr;
        std::atomic<float>* ensembleSpread = nullptr;
        std::atomic<float>* vibratoDepth   = nullptr;
        std::atomic<float>* attackTime     = nullptr;
        std::atomic<float>* releaseTime    = nullptr;
        std::atomic<float>* onsetMode      = nullptr;
        std::atomic<float>* onsetAmount    = nullptr;

        std::atomic<float>* glottalTension = nullptr;
        std::atomic<float>* breathAmount   = nullptr;
        std::atomic<float>* throatShift    = nullptr;
        std::atomic<float>* formantFocus   = nullptr;
        std::atomic<float>* subAbyss       = nullptr;
        std::atomic<float>* spectralCrown  = nullptr;
        std::atomic<float>* masterGain     = nullptr;

        std::atomic<float>* driveAmount    = nullptr;
        std::atomic<float>* tapeInstability= nullptr;
        std::atomic<float>* oxideDamping   = nullptr;
        std::atomic<float>* screwPitch     = nullptr;
        std::atomic<float>* screwDepth     = nullptr;
        std::atomic<float>* oversampleHq   = nullptr;

        std::atomic<float>* spaceType      = nullptr;
        std::atomic<float>* spaceMix       = nullptr;
        std::atomic<float>* spaceDecay     = nullptr;
        std::atomic<float>* spaceDamp      = nullptr;
        std::atomic<float>* spaceShimmer   = nullptr;
        std::atomic<float>* spaceFreeze    = nullptr;
        std::atomic<float>* cc64Freeze     = nullptr;

        std::atomic<float>* macroColossus  = nullptr;
        std::atomic<float>* macroSpecter   = nullptr;
        std::atomic<float>* macroRot       = nullptr;
        std::atomic<float>* macroAbyss     = nullptr;
        std::atomic<float>* legatoEnable   = nullptr;
        std::atomic<float>* glideTime      = nullptr;
        std::atomic<float>* expressionLevel= nullptr;
    };

    void handleMidiMessage (const juce::MidiMessage& msg,
                            bool legatoOn,
                            float glideSec,
                            bool cc64TriggersFreeze) noexcept;
    void triggerNoteOn (int midiNote, float velocity01, bool legatoOn, float glideSec) noexcept;
    void triggerNoteOff (int midiNote, bool legatoOn, float glideSec) noexcept;
    void panicImmediate() noexcept;
    [[nodiscard]] int allocateVoiceIndex() noexcept;

    CachedParamPtrs ptrs;
    double   sr               = 48000.0;
    int      preparedMaxBlock = 512;
    uint64_t sampleCounter    = 0;
    uint64_t noteTriggerSeq   = 0;

    std::array<SwarmVoice, kMaxVoices> voices;
    OrganicOrbitEngine orbitEngine;
    BrutalistReverb reverb;

    std::vector<float> busL;
    std::vector<float> busR;

    // MIDI & expression state
    float pitchBendSemitones  = 0.0f;
    float midiModWheel        = 0.78f;
    float channelAftertouch   = 0.0f;
    bool  sustainPedalDown    = false;
    bool  cc64FreezeActive    = false;
    std::array<bool, 128> sustainedNotes {};

    // Legato held-note stack (zero allocation)
    std::array<int, kMaxHeldNotes> heldNotes {};
    std::array<float, kMaxHeldNotes> heldVelocities {};
    int heldCount         = 0;
    int lastTriggeredNote = -1;

    // Smoothers (all continuous controls smoothed across sub-blocks; masterGain per-sample)
    bool  smoothersPrimed     = false;
    float smPhX               = params::defaults::phonemeX;
    float smPhY               = params::defaults::phonemeY;
    float smEnsSize           = params::defaults::ensembleSize;
    float smSpread            = params::defaults::ensembleSpread;
    float smVibrato           = params::defaults::vibratoDepth;
    float smTension           = params::defaults::glottalTension;
    float smBreath            = params::defaults::breathAmount;
    float smThroat            = params::defaults::throatShift;
    float smFocus             = params::defaults::formantFocus;
    float smSub               = params::defaults::subAbyss;
    float smCrown             = params::defaults::spectralCrown;
    float smDrive             = params::defaults::driveAmount;
    float smTape              = params::defaults::tapeInstability;
    float smOxide             = params::defaults::oxideDamping;
    float smScrewPitch        = params::defaults::screwPitch;
    float smScrewDepth        = params::defaults::screwDepth;
    float smPitchBend         = 0.0f;
    float smAftertouch        = 0.0f;
    float smExpr              = params::defaults::expressionLevel;
    float smSingersPerVoice   = 6.0f;
    float smColossus          = 0.0f;
    float smSpecter           = 0.0f;
    float smRot               = 0.0f;
    float smAbyss             = 0.0f;

    juce::SmoothedValue<float> smoothMasterGain;

    std::atomic<bool>  panicRequested { false };
    std::atomic<bool>  uiExprTouched  { false };
    std::atomic<float> uiExpression   { 0.78f };
    float lastParamExpression         = 0.78f;
    bool  midiCc1Received             = false;
};

} // namespace monolith::dsp
