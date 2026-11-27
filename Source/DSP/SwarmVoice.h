#pragma once

#include "DspMath.h"
#include "EntropyStage.h"
#include "HarmonicLayers.h"
#include "PhonemeField.h"
#include "VocalTract.h"
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
    float singersReference  = 1.0f;  // uncapped ensemble size per voice (loudness normalisation reference)
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
    float onsetDispersion01 = 0.68f; // ensemble entry looseness (SwarmEngine: effective spread)

    // Absolute control-grid flags supplied by SwarmEngine (block-size invariance)
    bool  controlCellStart  = true;  // chunk begins on an absolute control-cell boundary
    bool  controlCellEnd    = true;  // chunk ends on an absolute control-cell boundary
};

/**
    Single polyphonic voice synthesizing an ensemble of up to 8 independent
    physical-acoustic virtual singers, harmonic layers, and per-voice stereo
    oversampled analog tape entropy. Voice steals and forced fast releases use a
    sample-accurate 4 ms raised-cosine output ramp (a stolen voice starts its new
    note only once the old one has faded to exactly zero).
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
    [[nodiscard]] bool isReleasing() const noexcept      { return active && releasing && ! pendingStart; }
    [[nodiscard]] int  getMidiNote() const noexcept      { return pendingStart ? pendingNote : currentMidiNote; }
    [[nodiscard]] uint64_t getAge() const noexcept       { return noteAge; }
    [[nodiscard]] float getCurrentLevel() const noexcept { return currentEnvPeak; }
    [[nodiscard]] int  getActiveSingerCount() const noexcept { return activeSingerCount; }

    [[nodiscard]] int getLatencySamples (bool hq4x, bool driveEngaged) const noexcept
    {
        return entropy.getLatencySamples (hq4x, driveEngaged);
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
    static constexpr double kDeclickRampSec = 0.004; // raised-cosine steal / fast-release ramp

    struct SingerState
    {
        uint32_t seed          = 1u;
        float phase            = 0.0f;
        float vibratoPhase     = 0.0f;
        float vibratoRateHz    = 5.6f;   // 4.8 - 6.5 Hz
        float vibratoDelaySec  = 0.35f;  // staggered delayed onset
        float vibratoRampSec   = 0.35f;  // smoothstep onset length
        float vibratoDepthScale = 1.0f;
        float microTuneCents   = 0.0f;
        float vtlScale         = 1.0f;   // formant scale = L_ref / L_singer (vocal tract length)
        std::array<float, 5> shapeDev {}; // per-formant tract-shape deviation (F2..F5)
        float tensionOffset    = 0.0f;   // per-singer glottal tension offset
        float attackScale      = 1.0f;   // per-singer RC attack time scale
        float attackCoeff      = 0.0f;   // cached RC attack coefficient
        float onsetRank        = 0.0f;   // seeded entry slot within the onset-dispersion window [0, 1)
        float panOffset        = 0.0f;   // seeded seat offset
        float currentPanL      = 0.70710678f;
        float currentPanR      = 0.70710678f;
        float targetPanL       = 0.70710678f;
        float targetPanR       = 0.70710678f;
        float wanderState      = 0.0f;
        float activeGain       = 0.0f;
        float envLevel         = 0.0f;

        // Deterministic seeded 1/f micro-jitter (cents) & shimmer (gain), control-rate + per-sample ramp
        float jitterLp = 0.0f, jitterSlow = 0.0f, jitterCents = 0.0f, jitterStep = 0.0f;
        float shimmerLp = 0.0f, shimmerSlow = 0.0f, shimmer = 0.0f, shimmerStep = 0.0f;

        DCBlocker64  excDc;          // ~5 Hz 64-bit excitation DC blocker
        PinkNoiseGen breathNoise;
        PinkNoiseGen wanderNoise;
        PinkNoiseGen jitterNoise;
        PinkNoiseGen shimmerNoise;
        float tractSpread = -1.0f;        // spread the tract coefficients were built for (-1 = stale)
        Bandpass64 breathBp;              // 64-bit aspiration shaper
        Bandpass64 onsetBpA;              // 64-bit formant-tracking onset shapers
        Bandpass64 onsetBpB;
    };

    double sr                 = 48000.0;
    int    preparedMaxBlock   = 512;
    int    voiceIdx           = 0;

    bool     active            = false;
    bool     releasing         = false;
    int      currentMidiNote   = 60;
    float    targetVelocity    = 0.8f;
    float    smoothedVelocity  = 0.8f;
    uint64_t noteAge           = 0;
    int64_t  noteSamples       = 0;   // absolute samples since note start (no float time accumulation)
    float    currentEnvPeak    = 0.0f;
    float    cachedAttackSec   = -1.0f;
    int      activeSingerCount = 0;
    int      ctrlCounter       = 0;   // absolute per-voice control-rate counter
    int      silentSamples     = 0;   // consecutive silent released samples
    FormantTarget ctrlTarget {};      // persists across render calls

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
    FormantSmoother formantSmoother;        // log-domain, epsilon-settled formant/bandwidth glide
    NasalPoleZero   nasalL, nasalR;         // per-voice nasal pole-zero pair (identity at nasalDip = 0)
    float nasalCoeffDip       = -1.0f;

    // Raised-cosine de-click ramp: StealOut fades the old note to exactly 0, then the pending note starts
    // sample-accurately; FastOut (CC120 / legato hand-off) fades to 0 and holds the voice muted.
    enum class Ramp : uint8_t { None, StealOut, FastOut };
    Ramp  rampKind            = Ramp::None;
    int   rampPos             = 0;
    int   rampLen             = 192;
    bool  fadeHold            = false;
    bool  pendingStart        = false;
    bool  pendingRelease      = false;
    int   pendingNote         = 60;
    float pendingVelocity     = 0.8f;

    void beginNote (int midiNote, float velocity01, uint64_t triggerStamp) noexcept;
    void renderSynth (int offset, int count, const VoiceRenderParams& params,
                      std::array<float, 5>& formantAcc, float& breathAcc) noexcept;

    std::array<SingerState, kMaxSingersPerVoice> singers {};
    VocalTractBank tractBank;         // SoA SIMD 64-bit Klatt tracts, one lane per singer
    static_assert (VocalTractBank::kLanes == kMaxSingersPerVoice, "one tract lane per singer");
    HarmonicLayers harmonicLayers;
    EntropyStage entropy;

    std::vector<float> voiceBufL;
    std::vector<float> voiceBufR;
    std::vector<float> envBuf;
};

} // namespace monolith::dsp
