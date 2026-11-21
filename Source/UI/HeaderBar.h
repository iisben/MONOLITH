#pragma once

#include "../PluginProcessor.h"
#include "MeterComponents.h"
#include "MonolithLookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

namespace monolith::ui
{

class HeaderBar final : public juce::Component
{
public:
    explicit HeaderBar (MonolithAudioProcessor& proc);

    void updateFromTelemetry (const TelemetrySnapshot& telemetry) noexcept;
    void refreshPresetDisplay();

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void repopulatePresetBox();
    void stepPreset (int delta);

    MonolithAudioProcessor& processor;
    ConcreteSurface surface;

    juce::ComboBox categoryBox;
    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" };
    juce::TextButton nextBtn { ">" };
    juce::TextButton abBtn { "A" };
    juce::TextButton copyAbBtn { "COPY" };
    juce::TextButton panicBtn { "PANIC" };

    VoiceBadge voiceBadge;
    StereoLevelMeter outputMeter;
    MonolithDial masterGainDial;

    std::vector<int> filteredIndices;
};

} // namespace monolith::ui
