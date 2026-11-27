#pragma once

#include "PluginProcessor.h"
#include "UI/HeroVisualizer.h"
#include "UI/MacroEditor.h"
#include "UI/MonolithLookAndFeel.h"
#include "UI/OutputAnalyser.h"
#include "UI/PresetBrowser.h"
#include "UI/SectionPanels.h"
#include "UI/TopBar.h"
#include <vector>
#include <juce_gui_basics/juce_gui_basics.h>

namespace monolith
{

/**
    MONOLITH II editor. Everything is laid out once on a 1200 x 800 design canvas (8 px grid) inside a root
    component that is scaled by an affine transform (zoom 75-150 %, corner drag with locked aspect ratio,
    persisted in the session). Static chrome lives in cached Retina layers; the 60 Hz frame only repaints the
    hero field, the meter and controls whose value actually moved.
*/
class MonolithAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    static constexpr int kDesignWidth  = 1200;
    static constexpr int kDesignHeight = 800;

    explicit MonolithAudioProcessorEditor (MonolithAudioProcessor&);
    ~MonolithAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void setZoom (float scale);
    void showPresetBrowser();
    void showSaveDialog();
    void showMacroEditor (int macroIndex);
    void closeOverlays();

    /** One UI frame (the 60 Hz timer body); public so the headless snapshot runner can drive frames. */
    void runUiFrame (double dtSeconds);

    [[nodiscard]] ui::PresetBrowserOverlay& getPresetBrowser() noexcept { return browser; }
    [[nodiscard]] ui::SpaceTexturePanel& getSpaceTexturePanel() noexcept { return spaceTexturePanel; }
    [[nodiscard]] ui::ToneExpressionPanel& getToneExpressionPanel() noexcept { return toneExpressionPanel; }

private:
    void timerCallback() override;
    void refreshSlow();
    [[nodiscard]] bool modalOpen() const noexcept { return saveDialog.isVisible() || macroEditor.isVisible(); }

    /** V2: right-click any parameter control -> MIDI Learn / clear CC / map scope (message thread). */
    struct MidiLearnMenu final : public juce::MouseListener
    {
        explicit MidiLearnMenu (MonolithAudioProcessor& p) : proc (p) {}
        void mouseDown (const juce::MouseEvent& e) override;
        MonolithAudioProcessor& proc;
    };

    MonolithAudioProcessor& audioProcessor;
    ui::MonolithLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow { this, 650 };
    ui::OutputAnalyser analyser;

    ui::ChassisBackground root;
    ui::TopBar topBar;
    ui::EnsemblePanel ensemblePanel;
    ui::ToneExpressionPanel toneExpressionPanel;
    ui::HeroPanel heroPanel;
    ui::OrbitStrip orbitStrip;
    ui::EntropyPanel entropyPanel;
    ui::SpaceTexturePanel spaceTexturePanel;
    ui::PerformanceDock dock;
    ui::PresetBrowserOverlay browser;
    ui::SavePresetOverlay saveDialog;
    ui::MacroEditorOverlay macroEditor;

    std::vector<ui::MonolithDial*> dials;
    double analyserRate = 0.0;
    double lastFrameMs = 0.0;
    int frameCounter = 0;
    MidiLearnMenu midiLearnMenu { audioProcessor }; // declared after audioProcessor (initialisation order)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MonolithAudioProcessorEditor)
};

} // namespace monolith
