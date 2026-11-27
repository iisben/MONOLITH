#pragma once

#include "TextureAssetTable.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>

namespace monolith::v2
{

/** One pre-decoded, guard-padded asset. Layout per channel: [pre(1) | body(length) | post(3)].
    Looped assets carry wrap-around guard samples, one-shots carry zeros, so 4-point Hermite
    never branches or takes a modulo per sample. */
struct TextureSample
{
    std::array<std::vector<float>, 2> ch; // mono assets are duplicated into channel 1
    int length = 0;
    double sampleRate = 48000.0;
    bool looped = false;
    bool pitched = false;
    int rootNote = 60;
};

/** Immutable, process-wide bank: decoded once (message thread, first construction) and shared by every
    plug-in instance. Never touched by the audio thread except for read-only sample access. */
class TextureBank
{
public:
    static const TextureBank& get();
    [[nodiscard]] int size() const noexcept { return static_cast<int> (samples.size()); }
    [[nodiscard]] const TextureSample& operator[] (int i) const noexcept { return samples[static_cast<size_t> (i)]; }
    [[nodiscard]] size_t bytesResident() const noexcept;

private:
    TextureBank();
    std::vector<TextureSample> samples;
};

/**
    V2 Layer A — Organic Air, Breath & Choir Texture Engine.

    Isolated, additive, post-core: renders note-triggered texture voices from the pre-decoded bank and SUMS
    them into the engine output after SwarmEngine::processBlock. Never reads or writes engine state.

    Real-time contract: no allocation, locks, I/O or exceptions in process(). All buffers are sized in prepare().
    Bypass contract: while the mix target and the smoothed mix are both 0 and no voice is active, process()
    returns before touching the buffer, so output is bit-identical to V1 and latency is unchanged. The layer is
    delayed by the engine's own PDC latency so it stays phase-aligned with the voices, which adds no latency.
*/
class TextureLayer
{
public:
    static constexpr int kMaxVoices    = 12; // audible voices; extra slots absorb 3 ms steal fades
    static constexpr int kVoiceSlots   = 16;
    static constexpr int kAlignRingLen = 4096; // power of two, > max engine PDC at 192 kHz

    TextureLayer();

    using ParamResolver = std::function<std::atomic<float>* (const char* paramId)>;
    void bindParameters (juce::AudioProcessorValueTreeState& apvts, const ParamResolver& resolver = {});
    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    /** Audio thread. engineLatencySamples aligns the layer with the PDC-delayed engine output. */
    void process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, int engineLatencySamples) noexcept;

    [[nodiscard]] bool isActive() const noexcept { return active; }
    [[nodiscard]] int getActiveVoiceCount() const noexcept { return activeVoiceCount.load (std::memory_order_relaxed); }
    [[nodiscard]] static constexpr int getAddedLatencySamples() noexcept { return 0; }

private:
    enum class Stage : uint8_t { idle, attack, sustain, release, kill };

    struct Voice
    {
        Stage stage = Stage::idle;
        int note = -1;
        int asset = 0;
        bool held = false;
        double pos = 0.0;
        double inc = 1.0;
        float env = 0.0f;
        float attackStep = 1.0f;
        float killStep = 1.0f;
        float velGain = 1.0f;
        uint32_t age = 0;
    };

    struct Svf
    {
        float ic1 = 0.0f, ic2 = 0.0f;
        void reset() noexcept { ic1 = ic2 = 0.0f; }
    };

    void handleMidi (const juce::MidiMessage& m) noexcept;
    void noteOn (int note, float velocity01) noexcept;
    void noteOff (int note) noexcept;
    void allNotesOff (bool hard) noexcept;
    double voiceIncrement (const Voice& v) const noexcept;
    void renderVoices (int start, int num) noexcept;
    void finishChunk (juce::AudioBuffer<float>& buffer, int bufferOffset, int num, int alignDelay) noexcept;

    const TextureBank& bank;

    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pAsset = nullptr;
    std::atomic<float>* pTune = nullptr;
    std::atomic<float>* pKeytrack = nullptr;
    std::atomic<float>* pAttack = nullptr;
    std::atomic<float>* pRelease = nullptr;
    std::atomic<float>* pVelSens = nullptr;
    std::atomic<float>* pHighPass = nullptr;
    std::atomic<float>* pLowPass = nullptr;
    std::atomic<float>* pWidth = nullptr;
    std::atomic<float>* pCc64Freeze = nullptr; // V1 param, read-only: CC64 routing mirrors the engine

    double sr = 48000.0;
    int chunkCapacity = 0;
    std::array<Voice, kVoiceSlots> voices {};
    uint32_t ageCounter = 0;
    bool sustainPedal = false;
    bool active = false;

    std::vector<float> scratchL, scratchR;
    std::vector<float> ringL, ringR;
    int ringWrite = 0;

    float mixSmoothed = 0.0f;
    float mixCoef = 0.0f;
    float hpLog = 0.0f, lpLog = 0.0f; // smoothed log2 cutoffs
    float cutoffCoef = 0.0f;
    float widthSmoothed = 1.0f;
    float releaseCoef = 0.999f;
    std::array<Svf, 2> hp {}, lp {};

    std::atomic<int> activeVoiceCount { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TextureLayer)
};

} // namespace monolith::v2
