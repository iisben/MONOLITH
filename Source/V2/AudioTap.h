#pragma once

#include <algorithm>
#include <atomic>
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

namespace monolith::v2
{

/**
    V2 Stage 4: post-bus stereo output tap for the editor (spectral / formant visualiser, peak + loudness meter).

    Single producer (audio thread) / single consumer (message thread) over juce::AbstractFifo with storage
    allocated once in the constructor. The producer is a read-only observer of the final output buffer: it never
    touches the samples it copies, never blocks, never allocates. It returns after one relaxed atomic load while no
    editor is open (setActive (false)). When the consumer falls behind, the producer drops the frames that do not
    fit and the consumer reads them as a gap (the visualiser only needs recent audio).
*/
class AudioTap
{
public:
    static constexpr int kCapacity = 1 << 15; // frames (~0.68 s at 48 kHz), per channel

    AudioTap() : fifo (kCapacity), left (static_cast<size_t> (kCapacity), 0.0f), right (static_cast<size_t> (kCapacity), 0.0f) {}

    /** Message thread: the editor arms the tap while it is visible. */
    void setActive (bool shouldBeActive) noexcept { active.store (shouldBeActive, std::memory_order_release); }
    [[nodiscard]] bool isActive() const noexcept { return active.load (std::memory_order_acquire); }

    /** Audio thread, after the final post-core bus sum. Mono buffers are mirrored to both sides. */
    void push (const juce::AudioBuffer<float>& buffer) noexcept
    {
        if (! active.load (std::memory_order_relaxed))
            return;
        const int n = buffer.getNumSamples();
        const int chans = buffer.getNumChannels();
        if (n <= 0 || chans <= 0)
            return;

        int s1 = 0, n1 = 0, s2 = 0, n2 = 0;
        fifo.prepareToWrite (n, s1, n1, s2, n2);
        const int written = n1 + n2;
        if (written < n)
            dropped.fetch_add (n - written, std::memory_order_relaxed);
        if (written <= 0)
            return;

        const float* l = buffer.getReadPointer (0);
        const float* r = buffer.getReadPointer (chans > 1 ? 1 : 0);
        std::copy (l, l + n1, left.data() + s1);
        std::copy (r, r + n1, right.data() + s1);
        if (n2 > 0)
        {
            std::copy (l + n1, l + n1 + n2, left.data() + s2);
            std::copy (r + n1, r + n1 + n2, right.data() + s2);
        }
        fifo.finishedWrite (written);
    }

    /** Message thread: copies up to maxFrames of the oldest unread frames; returns the number copied. */
    int pull (float* outL, float* outR, int maxFrames) noexcept
    {
        int s1 = 0, n1 = 0, s2 = 0, n2 = 0;
        fifo.prepareToRead (std::max (0, maxFrames), s1, n1, s2, n2);
        std::copy (left.data() + s1, left.data() + s1 + n1, outL);
        std::copy (right.data() + s1, right.data() + s1 + n1, outR);
        if (n2 > 0)
        {
            std::copy (left.data() + s2, left.data() + s2 + n2, outL + n1);
            std::copy (right.data() + s2, right.data() + s2 + n2, outR + n1);
        }
        fifo.finishedRead (n1 + n2);
        return n1 + n2;
    }

    [[nodiscard]] int getNumReady() const noexcept { return fifo.getNumReady(); }

    /** Message thread (consumer side): discards everything queued, e.g. when the editor reopens. */
    void discardPending() noexcept { fifo.finishedRead (fifo.getNumReady()); }

    [[nodiscard]] int64_t getDroppedFrames() const noexcept { return dropped.load (std::memory_order_relaxed); }

private:
    juce::AbstractFifo fifo;
    std::vector<float> left, right;
    std::atomic<bool> active { false };
    std::atomic<int64_t> dropped { 0 };
    static_assert (std::atomic<bool>::is_always_lock_free && std::atomic<int64_t>::is_always_lock_free);
};

} // namespace monolith::v2
