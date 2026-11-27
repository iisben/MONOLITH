#pragma once

#include "../DSP/SwarmVoice.h"
#include <array>
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>

namespace monolith::v2
{

/**
    V2 per-note expression hook (MPE lower zone: master channel 1, member channels 2-16).

    Control plane only. SwarmEngine::handleMidiMessage offers every message to consume() first (same audio
    thread, sample-accurate); per-note data is then folded into a per-voice COPY of the cell's
    VoiceRenderParams via applyToVoice(). The voice's synthesis math is untouched. Targets:
      per-note pitch bend (+/-48 st)  -> pitchBendSt
      pressure (MPE / poly AT), CC2   -> vocal effort (aftertouch01, tension, spectral tilt), vibrato depth
      slide (MPE CC74 / global CC74)  -> formant openness (phonemeY) + spectral tilt
    Neutral contract: while MPE is off and neither CC74 nor CC2 has been received, isEngaged() is false and the
    engine takes its original V1 path. With MPE on but no expression data, applyToVoice() is an identity.
*/
class MpeExpression
{
public:
    void bindParameters (juce::AudioProcessorValueTreeState& apvts);
    void reset() noexcept;

    /** Audio thread, once per block before the engine runs. */
    void beginBlock() noexcept;

    /** Audio thread. Returns true when the message is per-note expression the engine must not see globally. */
    bool consume (const juce::MidiMessage& msg) noexcept;

    [[nodiscard]] bool isEngaged() const noexcept { return mpeOn || timbreSeen || breathSeen; }
    [[nodiscard]] bool isMpeEnabled() const noexcept { return mpeOn; }

    void applyToVoice (int midiNote, dsp::VoiceRenderParams& vp) const noexcept;

    /** Strongest live pressure across MPE channels / poly notes, and the CC2 breath level (audio thread). */
    [[nodiscard]] float getMaxPressure() const noexcept;
    [[nodiscard]] float getBreath() const noexcept { return breathSeen ? breath : 0.0f; }

    /** UI telemetry (relaxed atomics, written at the end of each block by the processor). */
    std::atomic<bool>  uiMpeActive { false };
    std::atomic<float> uiPressure  { 0.0f };
    std::atomic<float> uiSlide     { 0.5f };

    void publishTelemetry() noexcept;

private:
    std::atomic<float>* pEnable   = nullptr;
    std::atomic<float>* pPbRange  = nullptr;
    std::atomic<float>* pVibrato  = nullptr;
    std::atomic<float>* pEffort   = nullptr;
    std::atomic<float>* pOpenness = nullptr;

    bool  mpeOn = false;
    float pbRange = 48.0f, vibratoDepth = 0.4f, effortDepth = 0.5f, opennessDepth = 0.5f;

    std::array<float, 17> chBendNorm {};  // [1..16], -1..+1
    std::array<float, 17> chPressure {};
    std::array<float, 17> chTimbre {};
    std::array<int8_t, 128> noteChannel {};
    std::array<float, 128> polyPressure {};
    float globalTimbre = 0.5f;
    bool  timbreSeen   = false;
    float breath       = 0.0f;
    bool  breathSeen   = false;
};

} // namespace monolith::v2
