#pragma once

#include "../V2/ModMatrix.h"
#include <juce_core/juce_core.h>

namespace monolith::ui
{

/** Short engraving names for modulation destinations (index-aligned with v2::kModDestinations). */
inline constexpr std::array<const char*, v2::kNumModDestinations> kDestShortNames {
    "VOWEL X", "VOWEL Y", "ORBIT RATE", "ORBIT DEPTH",
    "SIZE", "SPREAD", "VIBRATO", "ONSET",
    "TENSION", "BREATH", "THROAT", "FOCUS",
    "SUB", "CROWN", "OUTPUT",
    "DRIVE", "TAPE", "OXIDE", "SCREW PITCH", "SCREW",
    "SPACE MIX", "DECAY", "DAMP", "SHIMMER",
    "TEXTURE", "TEX TUNE", "TEX HP", "TEX LP", "TEX WIDTH",
    "TEX ATTACK", "TEX RELEASE"
};

/** V1 hard-wired macro curve summaries (legacy routing). */
inline constexpr std::array<const char*, v2::kNumMacros> kLegacyMacroCaptions {
    "SCALE \xc2\xb7 SUB \xc2\xb7 WIDTH", "BREATH \xc2\xb7 HOLLOW \xc2\xb7 GHOST",
    "TAPE \xc2\xb7 GRIT \xc2\xb7 DECAY", "SPACE \xc2\xb7 DARK \xc2\xb7 VOID"
};

[[nodiscard]] inline juce::String destShortName (int dest)
{
    return (dest >= 0 && dest < v2::kNumModDestinations) ? juce::String (kDestShortNames[static_cast<size_t> (dest)]) : juce::String();
}

/** Panel group a destination lives in (index-aligned with v2::kModDestinations), for the routing menus. */
[[nodiscard]] inline juce::String destGroupName (int dest)
{
    if (dest < 0 || dest >= v2::kNumModDestinations) return {};
    if (dest < 4)   return "FIELD";
    if (dest < 8)   return "ENSEMBLE";
    if (dest < 14)  return "TONE";
    if (dest == 14) return "MASTER";
    if (dest < 20)  return "ENTROPY";
    if (dest < 24)  return "SPACE";
    return "TEXTURE";
}

/** One-line routing summary for a macro (shown under the hero knob). */
[[nodiscard]] inline juce::String macroCaption (const v2::ModConfig& cfg, int macro)
{
    juce::StringArray names;
    int count = 0;
    for (const auto& r : cfg.routes[static_cast<size_t> (macro)])
        if (r.dest >= 0)
        {
            ++count;
            if (names.size() < 3 && ! names.contains (destShortName (r.dest)))
                names.add (destShortName (r.dest));
        }
    if (count > 0)
        return names.joinIntoString (juce::String::fromUTF8 (" \xc2\xb7 ")) + (count > names.size() ? " +" + juce::String (count - names.size()) : juce::String());
    if (cfg.legacyMacroCurve[static_cast<size_t> (macro)])
        return juce::String::fromUTF8 (kLegacyMacroCaptions[static_cast<size_t> (macro)]);
    return "UNASSIGNED";
}

} // namespace monolith::ui
