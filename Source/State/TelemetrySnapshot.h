#pragma once

#include <array>
#include <atomic>

namespace monolith
{

/**
    Lock-free telemetry snapshot written once per audio block and read at 60 Hz
    by the UI thread using memory_order_relaxed.
*/
struct TelemetrySnapshot
{
    static_assert (std::atomic<float>::is_always_lock_free, "telemetry taps must be lock-free");
    static_assert (std::atomic<int>::is_always_lock_free, "telemetry taps must be lock-free");

    std::atomic<float> rmsL { 0.0f };
    std::atomic<float> rmsR { 0.0f };
    std::atomic<float> peakL { 0.0f };
    std::atomic<float> peakR { 0.0f };

    std::atomic<float> audioEnergy { 0.0f };
    std::atomic<float> breathDensity { 0.0f };
    std::atomic<float> swarmSpreadLive { 0.68f };
    std::atomic<float> tensionLive { 0.68f };

    std::atomic<float> effectivePhonemeX { 0.50f };
    std::atomic<float> effectivePhonemeY { 0.86f };
    std::atomic<float> liveExpression { 0.78f };
    std::atomic<float> freezeEnergy { 0.0f };

    std::atomic<int> activeVoices { 0 };
    std::atomic<int> activeSingers { 0 };

    std::array<std::atomic<float>, 5> formantEnergy {};

    void reset() noexcept
    {
        rmsL.store (0.0f, std::memory_order_relaxed);
        rmsR.store (0.0f, std::memory_order_relaxed);
        peakL.store (0.0f, std::memory_order_relaxed);
        peakR.store (0.0f, std::memory_order_relaxed);
        audioEnergy.store (0.0f, std::memory_order_relaxed);
        breathDensity.store (0.0f, std::memory_order_relaxed);
        freezeEnergy.store (0.0f, std::memory_order_relaxed);
        activeVoices.store (0, std::memory_order_relaxed);
        activeSingers.store (0, std::memory_order_relaxed);
        for (auto& f : formantEnergy)
            f.store (0.0f, std::memory_order_relaxed);
    }
};

} // namespace monolith
