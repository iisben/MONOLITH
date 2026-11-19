#pragma once

#include "DSP/SwarmEngine.h"
#include "Presets/PresetManager.h"
#include "State/ParameterIDs.h"
#include "State/TelemetrySnapshot.h"
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>

namespace monolith
{

class MonolithAudioProcessor final : public juce::AudioProcessor,
                                     private juce::AudioProcessorValueTreeState::Listener,
                                     private juce::AsyncUpdater
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

    void triggerPanic() noexcept { engine.requestPanic(); }
    void setUiExpression (float val01) noexcept { engine.setUiExpressionOverride (val01); }

    void applyFactoryPreset (int index);
    void toggleABCompare();
    void copyActiveToInactiveAB();

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;

    juce::AudioProcessorValueTreeState apvts;
    dsp::SwarmEngine engine;
    TelemetrySnapshot telemetry;
    PresetManager presetManager;

    std::atomic<double> lastKnownBpm { 120.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MonolithAudioProcessor)
};

} // namespace monolith
