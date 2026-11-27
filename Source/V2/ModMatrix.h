#pragma once

#include "../State/ParameterIDs.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <juce_audio_processors/juce_audio_processors.h>

namespace monolith::v2
{

// ── Modulation sources (control plane only; every source is neutral at its default) ─────────────────────────
enum class ModSource : int
{
    macro1 = 0, macro2, macro3, macro4, // V1 macro params reused as MACRO 1-4 (COLOSSUS/SPECTER/ROT/ABYSS)
    xyX, xyY,                           // XY Sculptor axes (always bipolar around the 0.5 centre)
    modWheel,                           // CC1 (0 until received)
    pressure,                           // channel / poly / MPE pressure (max)
    breath,                             // CC2 (0 until received)
    count
};

inline constexpr int kNumModSources    = static_cast<int> (ModSource::count);
inline constexpr int kRoutesPerSource  = 8;
inline constexpr int kNumMacros        = 4;

inline constexpr std::array<const char*, kNumModSources> kModSourceIds {
    "macro1", "macro2", "macro3", "macro4", "xy_x", "xy_y", "mod_wheel", "pressure", "breath"
};

inline constexpr std::array<const char*, kNumMacros> kMacroParamIds {
    params::macroColossus, params::macroSpecter, params::macroRot, params::macroAbyss
};

inline constexpr std::array<const char*, kNumMacros> kDefaultMacroLabels { "COLOSSUS", "SPECTER", "ROT", "ABYSS" };

/** Continuous parameters a route may target. Offsets are applied in each parameter's normalised domain and
    feed the existing smoothed targets (the engine and texture layer read the shadow value instead of the raw
    APVTS value). expression_level, glide, attack/release and every choice/bool parameter are excluded. */
inline constexpr std::array<const char*, 31> kModDestinations {
    params::phonemeX, params::phonemeY, params::orbitRate, params::orbitDepth,
    params::ensembleSize, params::ensembleSpread, params::vibratoDepth, params::onsetAmount,
    params::glottalTension, params::breathAmount, params::throatShift, params::formantFocus,
    params::subAbyss, params::spectralCrown, params::masterGain,
    params::driveAmount, params::tapeInstability, params::oxideDamping, params::screwPitch, params::screwDepth,
    params::spaceMix, params::spaceDecay, params::spaceDamp, params::spaceShimmer,
    params::v2TexMix, params::v2TexTune, params::v2TexHighPass, params::v2TexLowPass, params::v2TexWidth,
    params::v2TexAttack, params::v2TexRelease
};
inline constexpr int kNumModDestinations = static_cast<int> (kModDestinations.size());

/** Shadow slots: every destination plus the 4 macro params (legacy-curve gating). */
inline constexpr int kNumShadows = kNumModDestinations + kNumMacros;

[[nodiscard]] inline int findDestination (const juce::String& id) noexcept
{
    for (int i = 0; i < kNumModDestinations; ++i)
        if (id == kModDestinations[static_cast<size_t> (i)])
            return i;
    return -1;
}

[[nodiscard]] inline int findSource (const juce::String& id) noexcept
{
    for (int i = 0; i < kNumModSources; ++i)
        if (id == kModSourceIds[static_cast<size_t> (i)])
            return i;
    return -1;
}

struct ModRoute
{
    int16_t dest    = -1;    // index into kModDestinations, -1 = empty slot
    float   depth   = 0.0f;  // -1 .. +1 of the destination's normalised range
    bool    bipolar = false; // bipolar: offset = depth * (2v - 1) (neutral at v = 0.5); unipolar: depth * v
};

/** POD routing snapshot published to the audio thread through a TripleBuffer. */
struct ModConfig
{
    std::array<std::array<ModRoute, kRoutesPerSource>, kNumModSources> routes {};
    std::array<bool, kNumMacros> legacyMacroCurve { true, true, true, true }; // V1 hard-wired macro curves
};

/** Lock-free single-writer / single-reader triple buffer (message thread writes, audio thread reads). */
template <typename T>
class TripleBuffer
{
public:
    TripleBuffer() = default;

    /** Writer: fill the back buffer, then publish. */
    T& back() noexcept { return slots[static_cast<size_t> (backIdx)]; }
    void publish() noexcept
    {
        const int prev = middle.exchange (backIdx | kDirty, std::memory_order_acq_rel);
        backIdx = prev & kIndexMask;
    }

    /** Reader: returns the most recently published value (never blocks, never allocates). */
    const T& read() noexcept
    {
        if ((middle.load (std::memory_order_relaxed) & kDirty) != 0)
        {
            const int prev = middle.exchange (frontIdx, std::memory_order_acq_rel);
            frontIdx = prev & kIndexMask;
        }
        return slots[static_cast<size_t> (frontIdx)];
    }

private:
    static constexpr int kDirty = 4, kIndexMask = 3;
    std::array<T, 3> slots {};
    int backIdx = 0, frontIdx = 1;
    std::atomic<int> middle { 2 };
    static_assert (std::atomic<int>::is_always_lock_free);
};

} // namespace monolith::v2
