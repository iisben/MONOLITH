#pragma once

#include "DSP/SwarmEngine.h"
#include "Presets/PresetLibrary.h"
#include "Presets/PresetManager.h"
#include "State/ParameterIDs.h"
#include "State/TelemetrySnapshot.h"
#include "V2/ControlPlane.h"
#include "V2/MidiLearn.h"
#include "V2/MpeExpression.h"
#include "V2/TextureLayer.h"
#include "V2/UndoHistory.h"
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>

namespace monolith
{

class MonolithAudioProcessor final : public juce::AudioProcessor,
                                     private juce::Timer
{
public:
    MonolithAudioProcessor();
    ~MonolithAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    const TelemetrySnapshot& getTelemetry() const noexcept { return telemetry; }
    PresetManager& getPresetManager() noexcept { return presetManager; }
    const v2::TextureLayer& getTextureLayer() const noexcept { return textureLayer; }

    void triggerPanic() noexcept { engine.requestPanic(); }
    void setUiExpression (float val01) noexcept { engine.setUiExpressionOverride (val01); }

    void applyFactoryPreset (int index);
    void toggleABCompare();
    void copyActiveToInactiveAB();

    // ── V2 Stage 3: control plane, expression, MIDI Learn, undo, versioned presets (message thread) ──
    v2::ControlPlane& getControlPlane() noexcept { return controlPlane; }
    v2::MidiLearn& getMidiLearn() noexcept { return midiLearn; }
    const v2::MpeExpression& getExpression() const noexcept { return expression; }
    PresetLibrary& getPresetLibrary() noexcept { return presetLibrary; }
    v2::UndoHistory& getUndoHistory() noexcept { return undoHistory; }

    bool undo();
    bool redo();

    /** Metadata of the preset currently loaded (name, author, category, tags, description, favourite). */
    [[nodiscard]] const PresetRecord& getCurrentPresetMeta() const noexcept { return currentMeta; }

    /** Loads any V2-schema record (factory records route through the exact V1 factory path). */
    void loadPresetRecord (const PresetRecord& record);
    [[nodiscard]] PresetRecord captureCurrentPreset (const juce::String& name, const juce::String& author,
                                                     const juce::String& category, const juce::StringArray& tags,
                                                     const juce::String& description) const;
    juce::Result saveUserPreset (const PresetRecord& record);

    /** Message-thread housekeeping normally run by the 50 Hz timer (exposed for headless QA). */
    void runMessageThreadTasks (double nowMs);
    bool saveGlobalMidiMap() const;

    /** Transition time used for A/B swaps and undo/redo restores. */
    static constexpr float kStateTransitionMs = 60.0f;

private:
    /** Message-thread latency poll: the audio thread never posts messages or takes a lock for PDC updates. */
    void timerCallback() override;

    [[nodiscard]] v2::StateSnapshot captureSnapshot() const;
    void restoreSnapshot (const v2::StateSnapshot& s);
    [[nodiscard]] std::atomic<float>* resolveShadow (const char* id) noexcept { return controlPlane.resolve (id); }

    juce::AudioProcessorValueTreeState apvts;
    v2::ControlPlane controlPlane;  // V2: shadows feed the engine / texture layer (identity while idle)
    v2::MpeExpression expression;   // V2: per-note expression hook inside the engine's MIDI dispatch
    v2::MidiLearn midiLearn;
    dsp::SwarmEngine engine;
    v2::TextureLayer textureLayer; // V2 Layer A: additive, post-core, bypassed at 0 % mix
    TelemetrySnapshot telemetry;
    PresetManager presetManager;
    PresetLibrary presetLibrary { presetManager };
    PresetRecord currentMeta;
    juce::ValueTree controlSlotA, controlSlotB; // A/B routing snapshots (APVTS halves live in PresetManager)
    v2::UndoHistory undoHistory { [this] { return captureSnapshot(); },
                                  [this] (const v2::StateSnapshot& s) { restoreSnapshot (s); } };

    std::atomic<double> lastKnownBpm { 120.0 };
    static_assert (std::atomic<double>::is_always_lock_free, "host-tempo tap must be lock-free");

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MonolithAudioProcessor)
};

} // namespace monolith
