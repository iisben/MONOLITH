#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

namespace monolith::params
{

/** Drop-in replacement for juce::AudioParameterBool that only ever stores a snapped 0 / 1 normalised value.

    JUCE 8.0.15's AudioParameterBool::setValue keeps whatever raw float a host writes (e.g. 0.43). When state is
    restored, the APVTS skips any parameter whose (snapped) stored value already matches the current one, so a raw
    0.43 survives a restore of "off" and hosts / pluginval read back a normalised value the state never held.

    Snapping at the same >= 0.5 threshold AudioParameterBool::get() uses keeps the effective on / off state, the APVTS
    raw value the engine reads, the IDs, names, defaults, step count and On / Off text exactly as before.
*/
class SnappedBoolParameter final : public juce::RangedAudioParameter
{
public:
    SnappedBoolParameter (const juce::ParameterID& parameterID,
                          const juce::String& parameterName,
                          bool defaultValue,
                          const juce::AudioParameterBoolAttributes& attributes = {})
        : RangedAudioParameter (parameterID, parameterName, attributes.getAudioProcessorParameterWithIDAttributes()),
          value (defaultValue ? 1.0f : 0.0f),
          valueDefault (defaultValue ? 1.0f : 0.0f),
          stringFromBool (attributes.getStringFromValueFunction() != nullptr
                              ? attributes.getStringFromValueFunction()
                              : [] (bool v, int) { return v ? TRANS ("On") : TRANS ("Off"); }),
          boolFromString (attributes.getValueFromStringFunction() != nullptr
                              ? attributes.getValueFromStringFunction()
                              : [] (const juce::String& text) { return parseBool (text); })
    {
       #if __cpp_lib_atomic_is_always_lock_free
        static_assert (std::atomic<float>::is_always_lock_free, "SnappedBoolParameter requires a lock-free std::atomic<float>");
       #endif
    }

    /** Returns the parameter's current boolean value. */
    bool get() const noexcept          { return value.load (std::memory_order_relaxed) >= 0.5f; }
    operator bool() const noexcept     { return get(); }

    const juce::NormalisableRange<float>& getNormalisableRange() const override { return range; }

    float getValue() const override                { return value.load (std::memory_order_relaxed); }
    void setValue (float newValue) override        { value.store (snap (newValue), std::memory_order_relaxed); }
    float getDefaultValue() const override         { return valueDefault; }
    int getNumSteps() const override               { return 2; }
    bool isDiscrete() const override               { return true; }
    bool isBoolean() const override                { return true; }

    juce::String getText (float v, int maximumLength) const override { return stringFromBool (v >= 0.5f, maximumLength); }
    float getValueForText (const juce::String& text) const override  { return boolFromString (text) ? 1.0f : 0.0f; }

private:
    static float snap (float v) noexcept { return v >= 0.5f ? 1.0f : 0.0f; } // NaN -> off

    // Same parser as juce::AudioParameterBool's default.
    static bool parseBool (const juce::String& text)
    {
        static const juce::StringArray onStrings  { TRANS ("on"),  TRANS ("yes"), TRANS ("true") };
        static const juce::StringArray offStrings { TRANS ("off"), TRANS ("no"),  TRANS ("false") };

        const auto lowercaseText = text.toLowerCase();

        for (const auto& s : onStrings)
            if (lowercaseText == s)
                return true;

        for (const auto& s : offStrings)
            if (lowercaseText == s)
                return false;

        return text.getIntValue() != 0;
    }

    const juce::NormalisableRange<float> range { 0.0f, 1.0f, 1.0f };
    std::atomic<float> value;
    const float valueDefault;
    std::function<juce::String (bool, int)> stringFromBool;
    std::function<bool (const juce::String&)> boolFromString;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SnappedBoolParameter)
};

} // namespace monolith::params
