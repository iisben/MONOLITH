#pragma once

#include "PluginProcessor.h"
#include "UI/HeaderBar.h"
#include "UI/MonolithLookAndFeel.h"
#include "UI/PhonemeFieldComponent.h"
#include "UI/SectionPanels.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace monolith
{

class MonolithAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    static constexpr int kDesignWidth  = 1180;
    static constexpr int kDesignHeight = 720;

    explicit MonolithAudioProcessorEditor (MonolithAudioProcessor&);
    ~MonolithAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    MonolithAudioProcessor& audioProcessor;
    ui::MonolithLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow { this, 500 };

    juce::Component rootContainer;
    ui::HeaderBar headerBar;
    ui::EnsemblePanel ensemblePanel;
    ui::ToneLayersPanel toneLayersPanel;
    ui::PhonemeFieldComponent phonemeField;
    ui::OrbitPanel orbitPanel;
    ui::EntropyPanel entropyPanel;
    ui::SpacePanel spacePanel;
    ui::PerformanceDock performanceDock;

    int lastSyncedPresetIdx = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MonolithAudioProcessorEditor)
};

} // namespace monolith
