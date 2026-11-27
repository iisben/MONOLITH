/*
    MONOLITH II — Headless UI snapshot runner (Stage 4 visual self-inspection).

    Usage: MonolithUISnapshot [outputDir]   (default: <repo>/build/v2_qa/ui_snapshots)

    Builds the real processor + editor under ScopedJuceInitialiser_GUI, renders audio through the processor so
    the output tap, telemetry and control plane carry live data, drives editor frames manually (no message loop),
    and writes 1x and 2x (Retina) PNGs of:
      01_main_view        factory preset, sustained chord, live visualiser
      02_preset_browser   browser overlay open
      03_macro_mod_active per-preset macro routes, macros engaged, XY Light/Shadow, CC1 + pressure, mod rings
      04_macro_editor     macro routing editor overlay
      05_texture_expr     TEXTURE and EXPRESSION tabs, texture layer active
      06_save_dialog      save-preset dialog
    Also reports static-layer re-render counts (cache stability across live frames).
*/

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "State/ParameterIDs.h"
#include "V2/ModMatrix.h"

#include <iostream>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace
{
using namespace monolith;

struct Rig
{
    std::unique_ptr<MonolithAudioProcessor> proc;
    std::unique_ptr<juce::AudioProcessorEditor> editorHolder;
    MonolithAudioProcessorEditor* editor = nullptr;
    juce::AudioBuffer<float> buffer { 2, 400 };
    juce::MidiBuffer pending;

    void set (const char* id, float v)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (proc->getAPVTS().getParameter (id)))
            p->setValueNotifyingHost (p->convertTo0to1 (v));
    }

    /** Renders `frames` editor frames of audio (800 samples each at 48 kHz = 60 fps). */
    void play (int frames)
    {
        for (int f = 0; f < frames; ++f)
        {
            for (int b = 0; b < 2; ++b)
            {
                buffer.clear();
                juce::MidiBuffer midi;
                if (b == 0)
                {
                    midi.swapWith (pending);
                    pending.clear();
                }
                proc->processBlock (buffer, midi);
            }
            proc->runMessageThreadTasks (juce::Time::getMillisecondCounterHiRes());
            editor->runUiFrame (1.0 / 60.0);
        }
    }
};

bool writePng (const juce::Image& img, const juce::File& f)
{
    f.deleteFile();
    juce::FileOutputStream os (f);
    return os.openedOk() && juce::PNGImageFormat().writeImageToStream (img, os);
}

bool snapshot (Rig& rig, const juce::File& dir, const juce::String& name)
{
    bool ok = true;
    for (float scale : { 1.0f, 2.0f })
    {
        const auto img = rig.editor->createComponentSnapshot (rig.editor->getLocalBounds(), true, scale);
        const auto f = dir.getChildFile (name + (scale > 1.5f ? "_2x.png" : "_1x.png"));
        const bool w = writePng (img, f);
        ok = ok && w;
        std::cout << (w ? "  wrote " : "  FAILED ") << f.getFileName() << "  " << img.getWidth() << "x" << img.getHeight() << "\n";
    }
    return ok;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    using namespace monolith::params;

    const juce::File outDir = argc > 1 ? juce::File (juce::String (argv[1]))
                                       : juce::File (MONOLITH_SOURCE_DIR).getChildFile ("build/v2_qa/ui_snapshots");
    outDir.createDirectory();
    const auto presetDir = outDir.getSiblingFile ("ui_snapshot_presets");
    presetDir.deleteRecursively();
    presetDir.createDirectory();

    Rig rig;
    rig.proc = std::make_unique<monolith::MonolithAudioProcessor>();
    rig.proc->getPresetLibrary().setUserDirectory (presetDir); // deterministic: factory presets only
    rig.proc->getMidiLearn().clearAll();
    rig.proc->setRateAndBufferSizeDetails (48000.0, 512);
    rig.proc->prepareToPlay (48000.0, 512);
    rig.proc->applyFactoryPreset (0);
    rig.proc->getPresetLibrary().setFavourite (rig.proc->getPresetLibrary().indexOfKey ("factory:" + rig.proc->getProgramName (0)), true);
    rig.proc->getPresetLibrary().setFavourite (rig.proc->getPresetLibrary().indexOfKey ("factory:" + rig.proc->getProgramName (9)), true);

    rig.editorHolder.reset (rig.proc->createEditor());
    rig.editor = dynamic_cast<monolith::MonolithAudioProcessorEditor*> (rig.editorHolder.get());
    if (rig.editor == nullptr)
    {
        std::cerr << "editor creation failed\n";
        return 1;
    }
    rig.editor->setZoom (1.0f);

    bool ok = true;
    std::cout << "MONOLITH II UI snapshots -> " << outDir.getFullPathName() << "\n";

    // (a) Main view: sustained C3 / C4 chord through the factory preset
    for (int n : { 48, 55, 60, 64 })
        rig.pending.addEvent (juce::MidiMessage::noteOn (1, n, static_cast<juce::uint8> (104)), 0);
    rig.play (90);
    ok = snapshot (rig, outDir, "01_main_view") && ok;

    // Cache stability: 120 live frames must not re-render any static layer
    {
        juce::Image probe (juce::Image::ARGB, rig.editor->getWidth(), rig.editor->getHeight(), true);
        {
            juce::Graphics g (probe);
            rig.editor->paintEntireComponent (g, true);
        }
        auto& root = *rig.editor->getChildComponent (0);
        const int chassisBefore = dynamic_cast<monolith::ui::ChassisBackground&> (root).getStaticRenderCount();
        for (int i = 0; i < 120; ++i)
        {
            rig.play (1);
            juce::Graphics g (probe);
            rig.editor->paintEntireComponent (g, true);
        }
        const int chassisAfter = dynamic_cast<monolith::ui::ChassisBackground&> (root).getStaticRenderCount();
        std::cout << "  static chassis re-renders over 120 live frames: " << (chassisAfter - chassisBefore) << "\n";
        ok = ok && chassisAfter == chassisBefore;
    }

    // (b) Preset browser
    rig.editor->showPresetBrowser();
    rig.play (2);
    ok = snapshot (rig, outDir, "02_preset_browser") && ok;
    rig.editor->closeOverlays();

    // (c) Macro / modulation active
    {
        auto& cp = rig.proc->getControlPlane();
        cp.setMacroLabel (0, "SIZE");
        cp.setMacroLabel (1, "AIR");
        cp.setMacroLabel (2, "GRIT");
        cp.setMacroLabel (3, "VOID");
        cp.setLegacyMacroCurve (0, false);
        cp.setRoute (0, 0, ensembleSize, 0.30f, false);
        cp.setRoute (0, 1, subAbyss, 0.35f, false);
        cp.setRoute (0, 2, ensembleSpread, 0.25f, false);
        cp.setRoute (1, 0, spectralCrown, 0.45f, false);
        cp.setRoute (1, 1, breathAmount, 0.30f, false);
        cp.setRoute (2, 0, driveAmount, 0.40f, false);
        cp.setRoute (2, 1, tapeInstability, 0.35f, false);
        cp.setRoute (3, 0, spaceDecay, 0.30f, false);
        cp.setRoute (3, 1, spaceDamp, 0.25f, false);
        cp.setRoute (3, 2, spaceShimmer, 0.40f, false);
        cp.setRoute (3, 3, spaceMix, 0.20f, false);
        cp.setRoute (static_cast<int> (monolith::v2::ModSource::modWheel), 0, vibratoDepth, 0.35f, false);
        cp.setRoute (static_cast<int> (monolith::v2::ModSource::pressure), 0, v2TexMix, 0.25f, false);
        rig.set (macroColossus, 0.72f);
        rig.set (macroSpecter, 0.48f);
        rig.set (macroRot, 0.30f);
        rig.set (macroAbyss, 0.84f);
        rig.set (v2XyMode, 1.0f);
        rig.set (v2XyX, 0.70f);
        rig.set (v2XyY, 0.71f);
        rig.set (v2TexMix, 0.22f);
        rig.set (orbitEnable, 1.0f);
        rig.pending.addEvent (juce::MidiMessage::controllerEvent (1, 1, 92), 0);
        rig.pending.addEvent (juce::MidiMessage::controllerEvent (1, 2, 64), 0);
        rig.pending.addEvent (juce::MidiMessage::channelPressureChange (1, 98), 0);
        for (int n : { 50, 57, 62, 65 })
            rig.pending.addEvent (juce::MidiMessage::noteOn (1, n, static_cast<juce::uint8> (110)), 1);
        rig.play (75);
        ok = snapshot (rig, outDir, "03_macro_mod_active") && ok;
    }

    // (d) Macro routing editor
    rig.editor->showMacroEditor (3);
    rig.play (1);
    ok = snapshot (rig, outDir, "04_macro_editor") && ok;
    rig.editor->closeOverlays();

    // (e) TEXTURE + EXPRESSION tabs
    rig.editor->getSpaceTexturePanel().showTexturePage();
    rig.editor->getToneExpressionPanel().showExpressionPage();
    rig.set (v2MpeEnable, 1.0f);
    rig.play (20);
    ok = snapshot (rig, outDir, "05_texture_expression") && ok;

    // (f) Save dialog
    rig.editor->showSaveDialog();
    rig.play (1);
    ok = snapshot (rig, outDir, "06_save_dialog") && ok;
    rig.editor->closeOverlays();

    // Zoom check: 75 % and 150 % layouts keep the aspect ratio
    for (float z : { 0.75f, 1.5f })
    {
        rig.editor->setZoom (z);
        rig.play (1);
        const auto img = rig.editor->createComponentSnapshot (rig.editor->getLocalBounds(), true, 1.0f);
        const auto f = outDir.getChildFile ("07_zoom_" + juce::String (juce::roundToInt (z * 100.0f)) + ".png");
        ok = writePng (img, f) && ok;
        std::cout << "  wrote " << f.getFileName() << "  " << img.getWidth() << "x" << img.getHeight() << "\n";
    }
    rig.editor->setZoom (1.0f);

    // (V2 Stage 5) Flagship presets: main view with preset macro names / routes, and the browser on the longest
    // category names ("Dark Cinematic & Witch House Rituals", "Ethereal Hybrids & Evolving Textures").
    {
        const int v1 = rig.proc->getPresetManager().getNumPresets();
        rig.pending.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        rig.proc->setCurrentProgram (v1 + 16); // 17 Prism Bloom: texture layer, XY preset axes, custom macros
        rig.editor->closeOverlays();
        for (int n : { 48, 55, 60, 62 })
            rig.pending.addEvent (juce::MidiMessage::noteOn (1, n, static_cast<juce::uint8> (104)), 0);
        rig.play (90);
        ok = snapshot (rig, outDir, "08_flagship_main") && ok;

        rig.proc->setCurrentProgram (v1 + 13); // 14 Haunted Mixtape
        rig.editor->showPresetBrowser();
        rig.play (2);
        ok = snapshot (rig, outDir, "09_flagship_browser") && ok;
        rig.editor->closeOverlays();
    }

    rig.editorHolder.reset();
    rig.proc->releaseResources();
    presetDir.deleteRecursively();
    std::cout << (ok ? "SNAPSHOTS OK\n" : "SNAPSHOTS FAILED\n");
    return ok ? 0 : 1;
}
