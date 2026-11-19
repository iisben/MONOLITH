#pragma once

#include <array>
#include <cstddef>

namespace monolith::dsp
{

enum class VocalRegister : int
{
    BassoProfundo = 0,
    Tenor         = 1,
    Alto          = 2,
    Soprano       = 3,
    SatbAuto      = 4
};

enum class PhonemeState : int
{
    Ah = 0,
    Oh,
    Oo,
    Eh,
    Ee,
    Hum,
    Whisper,
    NumStates
};

struct PhonemeAnchor
{
    const char* code;
    const char* name;
    float x;
    float y;
};

struct FormantTarget
{
    std::array<float, 5> freq {};
    std::array<float, 5> bw {};
    std::array<float, 5> gain {};
    float voicedMix   = 1.0f;
    float breathBoost = 0.0f;
    float nasalDip    = 0.0f;
};

/**
    Evaluates the 2D Phoneme & Timbre Vector Field across 7 curated acoustic
    vocal states and 4 physiological vocal registers.
*/
class PhonemeField
{
public:
    static constexpr size_t kNumStates = static_cast<size_t> (PhonemeState::NumStates);

    static const std::array<PhonemeAnchor, kNumStates>& getAnchors() noexcept;

    /** Maps a MIDI note (or explicit register choice) to one of the 4 physical registers. */
    [[nodiscard]] static VocalRegister resolveRegister (VocalRegister mode, int midiNote) noexcept;

    /**
        Evaluates smooth, click-free interpolated F1-F5 frequencies, bandwidths,
        and gains at (x, y) in [0, 1]^2 using modified-Shepard Gaussian RBF weights
        in log-frequency and log-bandwidth domains, plus throat shift and Soprano jaw-tracking.
    */
    [[nodiscard]] static FormantTarget evaluate (float x, float y,
                                                 VocalRegister reg,
                                                 float f0Hz,
                                                 float throatShiftSemitones,
                                                 float formantFocus01) noexcept;
};

/**
    Smooth incommensurate Lissajous / golden-ratio Organic Orbit Engine.
    Displaces (x, y) smoothly during sustained chords, free-running or host BPM synced.
*/
class OrganicOrbitEngine
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void advance (int numSamples,
                  bool enabled,
                  float rateHz,
                  float depth01,
                  bool tempoSync,
                  double hostBpm,
                  float baseX,
                  float baseY) noexcept;

    [[nodiscard]] float getEffectiveX() const noexcept { return effectiveX; }
    [[nodiscard]] float getEffectiveY() const noexcept { return effectiveY; }

private:
    double sr          = 48000.0;
    double phaseA      = 0.0;
    double phaseB      = 1.17;
    double phaseC      = 2.63;
    float  effectiveX  = 0.50f;
    float  effectiveY  = 0.86f;
    float  smoothDepth = 0.0f;
};

} // namespace monolith::dsp
