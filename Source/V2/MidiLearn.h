#pragma once

#include <array>
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>

namespace monolith::v2
{

/**
    Thread-safe MIDI CC Learn.

    Audio thread: handleCc() completes an armed learn (binds the CC) and pushes bound CC values into a fixed
    SPSC FIFO. It never allocates, locks or calls into the host. Message thread: drain() applies queued values
    with setValueNotifyingHost inside a gesture, so the host records the automation and attachments update.
    Bindings are stored per session and per preset (scope = preset), or in a global user file (scope = global).
*/
class MidiLearn
{
public:
    static constexpr int kNone = -1;
    static constexpr int kFifoSize = 512;

    MidiLearn();

    // ── message thread ─────────────────────────────────────────────────────
    void armLearn (int paramIndex) noexcept   { armed.store (paramIndex, std::memory_order_release); }
    void cancelLearn() noexcept               { armed.store (kNone, std::memory_order_release); }
    [[nodiscard]] int getArmedParam() const noexcept { return armed.load (std::memory_order_acquire); }

    void bind (int cc, int paramIndex) noexcept; // one CC -> one param, one param <- one CC
    void clearParam (int paramIndex) noexcept;
    void clearAll() noexcept;
    [[nodiscard]] int getCcForParam (int paramIndex) const noexcept;
    [[nodiscard]] int getParamForCc (int cc) const noexcept;
    [[nodiscard]] int getNumBindings() const noexcept;

    /** Applies queued CC values to the processor's parameters. Returns the number applied. */
    int drain (juce::AudioProcessor& processor);

    /** True once after an armed learn completed on the audio thread. */
    [[nodiscard]] bool consumeLearnCompleted() noexcept { return learnCompleted.exchange (false, std::memory_order_acq_rel); }

    enum class Scope { preset = 0, global = 1 };
    void setScope (Scope s) noexcept { scope.store (static_cast<int> (s), std::memory_order_relaxed); }
    [[nodiscard]] Scope getScope() const noexcept { return static_cast<Scope> (scope.load (std::memory_order_relaxed)); }

    /** <MidiMap scope=".."><Bind cc=".." param="paramID"/></MidiMap>; param IDs keep maps valid across versions. */
    [[nodiscard]] juce::ValueTree toValueTree (const juce::AudioProcessor& processor) const;
    void fromValueTree (const juce::ValueTree& tree, const juce::AudioProcessor& processor, bool applyScope = true);

    bool saveGlobalMap (const juce::File& file, const juce::AudioProcessor& processor) const;
    bool loadGlobalMap (const juce::File& file, const juce::AudioProcessor& processor);
    [[nodiscard]] static juce::File defaultGlobalMapFile();

    // ── audio thread ───────────────────────────────────────────────────────
    void handleCc (int cc, int value) noexcept;

private:
    struct Event { int16_t param; float value01; };

    std::array<std::atomic<int16_t>, 128> ccToParam;
    std::atomic<int> armed { kNone };
    std::atomic<bool> learnCompleted { false };
    std::atomic<int> scope { 0 };

    juce::AbstractFifo fifo { kFifoSize };
    std::array<Event, kFifoSize> events {};
    std::array<bool, 4096> touched {};    // message-thread scratch for gesture bracketing (param index < 4096)
    std::array<float, 4096> coalesced {}; // message-thread scratch: last queued value per parameter

    static_assert (std::atomic<int16_t>::is_always_lock_free);
};

} // namespace monolith::v2
