#pragma once

#include "MidiLearn.h"
#include "ModMatrix.h"
#include <array>
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>

namespace monolith::v2
{

/**
    V2 control plane: Performance Macros routing matrix, XY Sculptor, expressive MIDI sources, velocity curve,
    MIDI-Learn capture and click-free state transitions.

    Mechanism: every modulatable parameter gets a lock-free SHADOW atomic. SwarmEngine and TextureLayer bind to
    the shadow (via the resolver passed to prepare / bindParameters) instead of the raw APVTS atomic, and
    process() rewrites each shadow once per block before the engine runs:
        shadow = base                                              (no active modulation: exact copy)
        shadow = from0to1 (clamp (to0to1 (base) + sum offsets))    (macro / XY / MIDI routes)
    The engine reads parameters exactly once per block and smooths them on its own 64-sample control grid, so the
    DSP math is untouched and an idle control plane is bit-identical to V1.

    Threads: config setters, labels and toValueTree() are message-thread only; the routing POD reaches the audio
    thread through a TripleBuffer. process() is audio-thread only: no allocation, locks or I/O.
*/
class ControlPlane
{
public:
    ControlPlane();

    void bind (juce::AudioProcessorValueTreeState& apvts);
    void prepare (double sampleRate) noexcept;

    /** Resolver for SwarmEngine::prepare / TextureLayer::bindParameters. nullptr = not shadowed. */
    [[nodiscard]] std::atomic<float>* resolve (const char* paramId) noexcept;

    // ── message thread: routing configuration ──────────────────────────────
    void resetToFactory (const std::array<juce::String, kNumMacros>& labels);
    bool setRoute (int source, int slot, const juce::String& destId, float depth, bool bipolar);
    void clearRoutes (int source);
    void setLegacyMacroCurve (int macro, bool enabled);
    void setMacroLabel (int macro, const juce::String& label);
    [[nodiscard]] juce::String getMacroLabel (int macro) const;
    [[nodiscard]] std::array<juce::String, kNumMacros> getMacroLabels() const { return labels; }
    [[nodiscard]] const ModConfig& getConfig() const noexcept { return master; }
    [[nodiscard]] uint32_t getRevision() const noexcept { return revision; }

    /** <V2Control><Source id="macro1" label=".." legacy="1"><Route dest=".." depth=".." bipolar=".."/></Source>.. */
    [[nodiscard]] juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree& tree); // invalid / missing tree -> factory (V1) routing

    /** Glide every shadowed parameter from its last value to the new APVTS value over ms (A/B, undo, redo). */
    void requestTransition (float ms) noexcept;

    // ── audio thread ───────────────────────────────────────────────────────
    /** Applies the velocity curve in place, captures MIDI-Learn CCs and expressive sources, rewrites shadows. */
    void process (juce::MidiBuffer& midi, int numSamples, MidiLearn* learn) noexcept;

    // ── UI telemetry (relaxed atomics) ─────────────────────────────────────
    [[nodiscard]] float getSourceValue (int source) const noexcept
    {
        return (source >= 0 && source < kNumModSources) ? uiSource[static_cast<size_t> (source)].load (std::memory_order_relaxed) : 0.0f;
    }
    /** Live modulated value of a destination (real units), e.g. for modulation-ring overlays. */
    [[nodiscard]] float getShadowValue (int destIndex) const noexcept
    {
        return (destIndex >= 0 && destIndex < kNumShadows) ? shadow[static_cast<size_t> (destIndex)].load (std::memory_order_relaxed) : 0.0f;
    }

    /** Velocity curve shared by the processor pre-pass and tests. curve: 0 Linear .. 4 Fixed. */
    [[nodiscard]] static int applyVelocityCurve (int velocity, int curve) noexcept;

private:
    void publish();
    void computeShadows (int numSamples, bool primeAll) noexcept;

    // Parameter bindings
    std::array<std::atomic<float>*, kNumShadows> basePtr {};
    std::array<juce::NormalisableRange<float>, kNumShadows> range {};
    std::array<std::atomic<float>, kNumShadows> shadow;
    std::array<std::atomic<float>*, kNumMacros> macroRaw {};
    std::atomic<float>* pXyX = nullptr;
    std::atomic<float>* pXyY = nullptr;
    std::atomic<float>* pXyMode = nullptr;
    std::atomic<float>* pVelCurve = nullptr;
    std::atomic<float>* pSwell = nullptr;
    int texMixShadow = -1;
    std::array<int, 2> xyVowelDest {}; // built-in XY routes resolved once in bind() (no String work on audio thread)
    std::array<int, 6> xyLightDest {};

    // Message-thread master config
    ModConfig master;
    std::array<juce::String, kNumMacros> labels;
    uint32_t revision = 0;
    TripleBuffer<ModConfig> configBuffer;

    // Audio-thread state
    double sr = 48000.0;
    bool primed = false;
    std::array<float, kNumModSources> srcSmoothed {};
    float modWheel = 0.0f, breath = 0.0f;
    std::array<float, 16> chPressure {};
    std::array<float, 128> polyPressure {};
    std::array<float, kNumShadows> lastBase {};
    std::array<float, kNumShadows> glideFrom {};
    std::array<float, kNumModDestinations> offsets {};
    int64_t glidePos = 0, glideLen = 0;
    std::atomic<float> transitionRequestMs { 0.0f };

    std::array<std::atomic<float>, kNumModSources> uiSource;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControlPlane)
};

} // namespace monolith::v2
