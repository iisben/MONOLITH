#pragma once

#include "DspMath.h"
#include "EntropyStage.h"
#include "HarmonicLayers.h"
#include "PhonemeField.h"
#include <array>
#include <cstdint>
#include <vector>

namespace monolith::dsp
{

enum class OnsetMode : int
{
    SoftBreath    = 0,
    ClosedHum     = 1,
    GhostSibilant = 2
};

struct VoiceRenderParams
{
    float phonemeX          = 0.50f;
    float phonemeY          = 0.86f;
    VocalRegister regMode   = VocalRegister::SatbAuto;
    float singersTarget     = 6.0f;
    float spread01          = 0.68f;
    float vibratoDepth01    = 0.36f;
    float attackSec         = 0.38f;
    float releaseSec        = 1.50f;
    OnsetMode onsetMode     = OnsetMode::SoftBreath;
    float onsetAmount01     = 0.28f;

    float glottalTension01  = 0.68f;
    float breathAmount01    = 0.20f;
    float throatShiftSt     = 0.0f;
    float formantFocus01    = 0.60f;
    float subAbyss01        = 0.42f;
    float spectralCrown01   = 0.36f;

    float drive01           = 0.24f;
    float tapeInstability01 = 0.14f;
    float oxideDamping01    = 0.12f;
    float screwPitchSt      = 0.0f;
    float screwDepth01      = 0.0f;
    bool  oversample4x      = false;

    float pitchBendSt       = 0.0f;
    float expression01      = 0.78f;
    float aftertouch01      = 0.0f;
    float glideTimeSec      = 0.14f;
};

/**
    Single polyphonic voice synthesizing an ensemble of up to 8 independent
    physical-acoustic virtual singers, harmonic layers, and per-voice stereo
    oversampled analog tape entropy with built-in C0-continuous de-click bridging.
*/
class SwarmVoice
{
public:
    static constexpr int kMaxSingersPerVoice = 8;

    void prepare (double sampleRate, int maxBlockSize, int voiceIndex);
    void reset() noexcept;

    void startNote (int midiNote, float velocity01, float currentGlideTimeSec, uint64_t triggerStamp) noexcept;
    void glideToNote (int midiNote, float velocity01, float glideTimeSec) noexcept;
    void stopNote (bool allowTailOff) noexcept;
    void forceFastRelease() noexcept;

    [[nodiscard]] bool isActive() const noexcept         { return active; }
    [[nodiscard]] bool isReleasing() const noexcept      { return active && releasing; }
    [[nodiscard]] int  getMidiNote() const noexcept      { return currentMidiNote; }
    [[nodiscard]] uint64_t getAge() const noexcept       { return noteAge; }
    [[nodiscard]] float getCurrentLevel() const noexcept { return currentEnvPeak; }
    [[nodiscard]] int  getActiveSingerCount() const noexcept { return activeSingerCount; }

    [[nodiscard]] int getLatencySamples (bool hq4x) const noexcept
    {
        return entropy.getLatencySamples (hq4x);
    }

    /**
        Renders numSamples into the provided stereo bus (additive into outL/outR).
        Updates formantEnergyOut[0..4] and breathEnergyOut for telemetry.
    */
    void renderBlock (float* outL,
                      float* outR,
                      int numSamples,
                      const VoiceRenderParams& params,
                      std::array<float, 5>& formantEnergyOut,
                      float& breathEnergyOut) noexcept;

private:
    struct SingerState
    {
        uint32_t seed          = 1u;
        double phase           = 0.0;
        QuadratureLfo vibrato;
        float vibratoRateHz    = 5.2f;
        float vibratoDelaySec  = 0.35f;
        float microTuneCents   = 0.0f;
        float formantJitter    = 1.0f;
        float currentPanL      = 0.70710678f;
        float currentPanR      = 0.70710678f;
        float targetPanL       = 0.70710678f;
        float targetPanR       = 0.70710678f;
        float staggerSec       = 0.0f;
        float wanderState      = 0.0f;
        float activeGain       = 0.0f;
        float envLevel         = 0.0f;

        PinkNoiseGen breathNoise;
        PinkNoiseGen wanderNoise;
        std::array<ZdfSvf, 5> formants;
        ZdfSvf breathBp;
        ZdfSvf onsetBpA;
        ZdfSvf onsetBpB;
    };

    double sr                 = 48000.0;
    int    preparedMaxBlock   = 512;
    int    voiceIdx           = 0;

    bool     active            = false;
    bool     releasing         = false;
    bool     fastSteal         = false;
    int      currentMidiNote   = 60;
    float    targetVelocity    = 0.8f;
    float    smoothedVelocity  = 0.8f;
    uint64_t noteAge           = 0;
    float    noteElapsedSec    = 0.0f;
    float    releaseElapsedSec = 0.0f;
    float    silentTailSec     = 0.0f;
    float    currentEnvPeak    = 0.0f;
    int      activeSingerCount = 0;

    // Legato portamento & smooth bell-shaped pitch scoop state
    float currentNoteF        = 60.0f;
    float targetNoteF         = 60.0f;
    float glideRatePerSample  = 0.0f;
    float scoopTargetSt       = 0.0f;
    float scoopCurrentSt      = 0.0f;
    float scoopDecayPerSample = 0.0f;
    float screwSagSt          = 0.0f;

    // Per-sample smoothed formant gains, ensemble normalization & modulation controls
    bool  formantSmoothPrimed = false;
    float smoothedSingerNorm  = 1.0f;
    float smPitchModSt        = 0.0f;
    float smExpr              = 0.78f;
    float smSubAmt            = 0.0f;
    float smCrownAmt          = 0.0f;
    float smTension           = 0.5f;
    float smBreath            = 0.2f;
    std::array<float, 5> smFormantFreq {};
    std::array<float, 5> smFormantBw {};
    std::array<float, 5> smFormantGain {};
    float smVoicedMix         = 1.0f;
    float smBreathBoost       = 0.0f;
    float smNasalDip          = 0.0f;

    // Anti-click voice steal / retrigger bridge (guarantees C0 continuity)
    float lastPostOutL        = 0.0f;
    float lastPostOutR        = 0.0f;
    float declickL            = 0.0f;
    float declickR            = 0.0f;
    float declickDecay        = 0.997f;

    std::array<SingerState, kMaxSingersPerVoice> singers {};
    HarmonicLayers harmonicLayers;
    EntropyStage entropy;

    std::vector<float> voiceBufL;
    std::vector<float> voiceBufR;
};

} // namespace monolith::dsp
