#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

namespace monolith
{

struct PresetParamValue
{
    const char* id;
    float value;
};

struct FactoryPreset
{
    juce::String name;
    juce::String category;
    juce::String description;
    std::vector<PresetParamValue> params;
};

/**
    Manages the 30 calibrated factory presets, category filtering,
    and A/B compare snapshots on the message thread.
*/
class PresetManager
{
public:
    PresetManager();

    [[nodiscard]] const std::vector<FactoryPreset>& getAllPresets() const noexcept { return presets; }
    [[nodiscard]] int getNumPresets() const noexcept { return static_cast<int> (presets.size()); }
    [[nodiscard]] const FactoryPreset& getPreset (int index) const;

    [[nodiscard]] juce::StringArray getCategories() const;
    [[nodiscard]] std::vector<int> getPresetIndicesForCategory (const juce::String& category) const;

    [[nodiscard]] int getCurrentPresetIndex() const noexcept { return currentIndex; }
    void setCurrentPresetIndex (int idx) noexcept;

    /** Full resolved parameter list of a factory preset (baseline defaults, then overrides; one entry per ID). */
    [[nodiscard]] std::vector<PresetParamValue> getResolvedParams (int index) const;

    /** Resets all APVTS parameters to baseline defaults, then applies preset overrides. */
    void applyPreset (int index, juce::AudioProcessorValueTreeState& apvts);

    // A/B Compare support
    void initializeAB (juce::AudioProcessorValueTreeState& apvts);
    void toggleAB (juce::AudioProcessorValueTreeState& apvts);
    void copyActiveToInactive (juce::AudioProcessorValueTreeState& apvts);
    [[nodiscard]] bool isSlotBActive() const noexcept { return activeIsB; }

    void saveStateToXml (juce::XmlElement& parent) const;
    void loadStateFromXml (const juce::XmlElement& parent);

private:
    std::vector<FactoryPreset> presets;
    int  currentIndex = 0;
    bool activeIsB    = false;
    juce::ValueTree slotA;
    juce::ValueTree slotB;
};

} // namespace monolith
