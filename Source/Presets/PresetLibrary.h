#pragma once

#include "PresetManager.h"
#include <array>
#include <optional>
#include <set>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>

namespace monolith
{

/** One preset in the V2 schema (factory or user). Parameter values are real (denormalised) units. */
struct PresetRecord
{
    juce::String name, author, category, description;
    juce::StringArray tags;
    std::array<juce::String, 4> macroLabels { "COLOSSUS", "SPECTER", "ROT", "ABYSS" };
    bool favorite = false;
    int factoryIndex = -1;   // >= 0: built-in V1 factory preset
    int flagshipIndex = -1;  // >= 0: built-in MONOLITH II flagship preset (embedded V2 XML, assets/v2_presets)
    int sourceVersion = 2;   // schema version the record was read from (1 = legacy APVTS state XML)
    juce::File file;         // user presets only
    std::vector<std::pair<juce::String, float>> params;
    juce::ValueTree control; // V2Control routing tree (invalid = factory V1 routing)
    juce::ValueTree midiMap; // optional per-preset MIDI-Learn map

    [[nodiscard]] bool isFactory() const noexcept { return factoryIndex >= 0; }
    [[nodiscard]] bool isFlagship() const noexcept { return flagshipIndex >= 0; }
    [[nodiscard]] bool isBuiltIn() const noexcept { return isFactory() || isFlagship(); }
    [[nodiscard]] juce::String key() const
    {
        return isFactory() ? "factory:" + name : isFlagship() ? "flagship:" + name : "user:" + file.getFileName();
    }
};

namespace preset_schema
{
    inline constexpr int kVersion = 2;
    inline constexpr const char* kRootTag = "MonolithPreset";
    inline constexpr const char* kFileExtension = ".mnlpreset";

    /** <MonolithPreset version="2" name author category tags description favorite>
            <MacroLabels m1.. m4/> <Params><Param id value/>..</Params> <V2Control/> <MidiMap/> </MonolithPreset> */
    [[nodiscard]] std::unique_ptr<juce::XmlElement> toXml (const PresetRecord& r);

    /** Accepts version-2 presets and legacy V1 data (an APVTS "MONOLITH_STATE" XML with PARAM children). */
    [[nodiscard]] std::optional<PresetRecord> fromXml (const juce::XmlElement& xml);

    /** Lossless conversion of a built-in V1 factory preset (resolved defaults + overrides) to the V2 schema. */
    [[nodiscard]] PresetRecord fromFactory (const PresetManager& pm, int index);

    /** The 20 MONOLITH II flagship presets embedded from assets/v2_presets (sorted by file name, flagshipIndex set). */
    [[nodiscard]] std::vector<PresetRecord> loadFlagshipPresets();

    /** Same schema read from *.mnlpreset files in a directory (QA tuning loop; sorted by file name). */
    [[nodiscard]] std::vector<PresetRecord> loadFlagshipPresets (const juce::File& directory);
}

/**
    Versioned Preset Manager (message thread only): the 20 MONOLITH II flagship presets, the 30 legacy factory
    presets and user presets on disk, with categories, tags, free-text search and favourites (persisted in
    favourites.xml beside the presets).
*/
class PresetLibrary
{
public:
    explicit PresetLibrary (const PresetManager& factory);

    [[nodiscard]] static juce::File defaultUserDirectory();
    void setUserDirectory (const juce::File& dir); // also rescans
    [[nodiscard]] juce::File getUserDirectory() const { return userDir; }

    void rescan();
    [[nodiscard]] int size() const noexcept { return static_cast<int> (records.size()); }

    /** Built-in flagship presets (parsed once per library; independent of rescans and the user directory). */
    [[nodiscard]] int getNumFlagship() const noexcept { return static_cast<int> (flagship.size()); }
    [[nodiscard]] const PresetRecord* getFlagship (int index) const noexcept
    {
        return (index >= 0 && index < getNumFlagship()) ? &flagship[static_cast<size_t> (index)] : nullptr;
    }
    [[nodiscard]] const PresetRecord& get (int index) const;
    [[nodiscard]] int indexOfKey (const juce::String& key) const;

    [[nodiscard]] juce::StringArray getCategories() const;
    /** Case-insensitive match on name, category, tags, author and description; empty text matches all. */
    [[nodiscard]] std::vector<int> search (const juce::String& text, const juce::String& category = {}, bool favouritesOnly = false) const;

    bool setFavourite (int index, bool fav);

    /** Writes <userDir>/<category>/<name>.mnlpreset and rescans. */
    juce::Result saveUserPreset (const PresetRecord& record);
    bool deleteUserPreset (int index);

private:
    void loadFavourites();
    void saveFavourites() const;

    const PresetManager& factoryPresets;
    std::vector<PresetRecord> flagship;
    juce::File userDir;
    std::vector<PresetRecord> records;
    std::set<juce::String> favourites;
};

} // namespace monolith
