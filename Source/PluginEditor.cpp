#include "PluginEditor.h"
#include "V2/ModMatrix.h"

namespace monolith
{

namespace
{
    void collectDials (juce::Component& c, std::vector<ui::MonolithDial*>& out)
    {
        for (auto* child : c.getChildren())
        {
            if (auto* d = dynamic_cast<ui::MonolithDial*> (child))
                out.push_back (d);
            collectDials (*child, out);
        }
    }
}

MonolithAudioProcessorEditor::MonolithAudioProcessorEditor (MonolithAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      topBar (p),
      ensemblePanel (p),
      toneExpressionPanel (p),
      heroPanel (p),
      orbitStrip (p),
      entropyPanel (p),
      spaceTexturePanel (p),
      dock (p),
      browser (p),
      saveDialog (p),
      macroEditor (p)
{
    const float savedScale = std::clamp (p.getEditorScale(), 0.75f, 1.5f); // read before any resized() can store a new one
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);

    root.setBounds (0, 0, kDesignWidth, kDesignHeight);
    addAndMakeVisible (root);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &topBar, &ensemblePanel, &toneExpressionPanel, &heroPanel, &orbitStrip,
                                                                        &entropyPanel, &spaceTexturePanel, &dock })
        root.addAndMakeVisible (c);
    root.addChildComponent (browser);
    root.addChildComponent (saveDialog);
    root.addChildComponent (macroEditor);

    // 1200 x 800 design canvas on an 8 px grid
    topBar.setBounds              (0, 0, kDesignWidth, 64);
    ensemblePanel.setBounds       (16, 80, 344, 248);
    toneExpressionPanel.setBounds (16, 336, 344, 248);
    heroPanel.setBounds           (368, 80, 464, 400);
    orbitStrip.setBounds          (368, 488, 464, 96);
    entropyPanel.setBounds        (840, 80, 344, 248);
    spaceTexturePanel.setBounds   (840, 336, 344, 248);
    dock.setBounds                (16, 600, 1168, 184);
    browser.setBounds             (16, 80, 1168, 704);
    saveDialog.setBounds          (0, 0, kDesignWidth, kDesignHeight);
    macroEditor.setBounds         (0, 0, kDesignWidth, kDesignHeight);

    topBar.onBrowse = [this] { browser.isVisible() ? browser.close() : showPresetBrowser(); };
    topBar.onSave = [this] { showSaveDialog(); };
    topBar.onZoom = [this] (float s) { setZoom (s); };
    topBar.onClipReset = [this] { analyser.resetClip(); };
    dock.onEditMacro = [this] (int m) { showMacroEditor (m); };
    browser.onLoaded = [this] { refreshSlow(); };
    for (auto* o : std::initializer_list<std::function<void()>*> { &browser.onClosed, &saveDialog.onClosed, &macroEditor.onClosed })
        *o = [this] { refreshSlow(); };

    collectDials (root, dials);

    setResizable (true, true);
    if (auto* constrainer = getConstrainer())
    {
        constrainer->setFixedAspectRatio (static_cast<double> (kDesignWidth) / static_cast<double> (kDesignHeight));
        constrainer->setSizeLimits (kDesignWidth * 3 / 4, kDesignHeight * 3 / 4, kDesignWidth * 3 / 2, kDesignHeight * 3 / 2);
    }
    setSize (juce::roundToInt (kDesignWidth * savedScale), juce::roundToInt (kDesignHeight * savedScale));

    root.addMouseListener (&midiLearnMenu, true);

    auto& tap = p.getOutputTap();
    tap.discardPending();
    tap.setActive (true);
    lastFrameMs = juce::Time::getMillisecondCounterHiRes();
    refreshSlow();
    startTimerHz (60);
}

MonolithAudioProcessorEditor::~MonolithAudioProcessorEditor()
{
    stopTimer();
    audioProcessor.getOutputTap().setActive (false);
    root.removeMouseListener (&midiLearnMenu);
    setLookAndFeel (nullptr);
}

void MonolithAudioProcessorEditor::setZoom (float scale)
{
    scale = std::clamp (scale, 0.75f, 1.5f);
    setSize (juce::roundToInt (kDesignWidth * scale), juce::roundToInt (kDesignHeight * scale));
}

void MonolithAudioProcessorEditor::showPresetBrowser()
{
    saveDialog.setVisible (false);
    macroEditor.setVisible (false);
    browser.open();
}

void MonolithAudioProcessorEditor::showSaveDialog()
{
    browser.setVisible (false);
    macroEditor.setVisible (false);
    saveDialog.open();
}

void MonolithAudioProcessorEditor::showMacroEditor (int m)
{
    browser.setVisible (false);
    saveDialog.setVisible (false);
    macroEditor.open (m);
}

void MonolithAudioProcessorEditor::closeOverlays()
{
    for (juce::Component* c : std::initializer_list<juce::Component*> { &browser, &saveDialog, &macroEditor })
        c->setVisible (false);
    refreshSlow();
}

void MonolithAudioProcessorEditor::MidiLearnMenu::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
        return;

    juce::String paramId;
    for (auto* c = e.eventComponent; c != nullptr && paramId.isEmpty(); c = c->getParentComponent())
        paramId = c->getProperties().getWithDefault ("monolithParamId", "").toString();
    if (paramId.isEmpty())
        return;

    const auto& params = proc.getParameters();
    int paramIndex = -1;
    for (int i = 0; i < params.size(); ++i)
        if (auto* wp = dynamic_cast<juce::AudioProcessorParameterWithID*> (params[i]); wp != nullptr && wp->paramID == paramId)
            paramIndex = i;
    if (paramIndex < 0)
        return;

    auto& learn = proc.getMidiLearn();
    const int cc = learn.getCcForParam (paramIndex);
    const bool armedHere = learn.getArmedParam() == paramIndex;
    const bool global = learn.getScope() == v2::MidiLearn::Scope::global;

    juce::PopupMenu menu;
    menu.addSectionHeader (params[paramIndex]->getName (40) + (cc >= 0 ? "  (CC " + juce::String (cc) + ")" : juce::String()));
    menu.addItem (1, armedHere ? "Waiting for CC... (click to cancel)" : "MIDI Learn");
    menu.addItem (2, "Clear CC" + (cc >= 0 ? " " + juce::String (cc) : juce::String()), cc >= 0);
    menu.addSeparator();
    menu.addItem (3, "Map stored per preset", true, ! global);
    menu.addItem (4, "Map stored globally", true, global);
    menu.addItem (5, "Clear all MIDI mappings", learn.getNumBindings() > 0);

    juce::Component::SafePointer<juce::Component> target (e.eventComponent);
    auto* procPtr = &proc;
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (e.eventComponent),
        [procPtr, paramIndex, armedHere, target] (int result) {
            if (target == nullptr) return;
            auto& ml = procPtr->getMidiLearn();
            switch (result)
            {
                case 1: if (armedHere) ml.cancelLearn(); else ml.armLearn (paramIndex); break;
                case 2: ml.clearParam (paramIndex); break;
                case 3: ml.setScope (v2::MidiLearn::Scope::preset); break;
                case 4: ml.setScope (v2::MidiLearn::Scope::global); break;
                case 5: ml.clearAll(); break;
                default: return;
            }
            if (result != 1 && ml.getScope() == v2::MidiLearn::Scope::global)
                (void) procPtr->saveGlobalMidiMap();
        });
}

void MonolithAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::palette::chassis);
}

void MonolithAudioProcessorEditor::resized()
{
    if (getWidth() <= 0 || getHeight() <= 0)
        return;
    const float sx = static_cast<float> (getWidth()) / static_cast<float> (kDesignWidth);
    const float sy = static_cast<float> (getHeight()) / static_cast<float> (kDesignHeight);
    root.setBounds (0, 0, kDesignWidth, kDesignHeight);
    root.setTransform (juce::AffineTransform::scale (sx, sy));
    audioProcessor.setEditorScale (sx);
    topBar.setZoomPercent (juce::roundToInt (sx * 100.0f));
}

namespace
{
    /** Visible through every parent up to the editor (unlike isShowing(), independent of a native peer). */
    bool isVisibleWithin (const juce::Component* c, const juce::Component* root) noexcept
    {
        for (; c != nullptr && c != root; c = c->getParentComponent())
            if (! c->isVisible())
                return false;
        return c == root;
    }
}

void MonolithAudioProcessorEditor::refreshSlow()
{
    topBar.refresh();
    dock.refresh (audioProcessor.getTelemetry());
    spaceTexturePanel.updateFreezeGlow (audioProcessor.getTelemetry().freezeEnergy.load (std::memory_order_relaxed));

    // Modulation rings on every routed destination (live control-plane shadow vs base value)
    const auto& cp = audioProcessor.getControlPlane();
    for (auto* d : dials)
    {
        const int dest = d->getModDestination();
        auto* prm = d->getParameter();
        if (dest < 0 || prm == nullptr || ! isVisibleWithin (d, this))
            continue;
        const float modNorm = prm->convertTo0to1 (cp.getShadowValue (dest));
        d->setModulation (std::abs (modNorm - prm->getValue()) > 0.003f ? modNorm : -1.0f);
    }
}

void MonolithAudioProcessorEditor::runUiFrame (double dt)
{
    const double rate = audioProcessor.getSampleRate();
    if (rate > 0.0 && std::abs (rate - analyserRate) > 0.5)
    {
        analyserRate = rate;
        analyser.prepare (rate);
    }
    analyser.process (audioProcessor.getOutputTap(), dt);

    const auto& telemetry = audioProcessor.getTelemetry();
    if (! modalOpen())
        topBar.updateMeter (analyser);
    if (! browser.isVisible() && ! modalOpen())
        heroPanel.update (telemetry, analyser, dt);

    if ((++frameCounter & 1) == 0 && ! modalOpen())
        refreshSlow();

    // V2: persist a freshly learned CC to the global map (learn completes on the audio thread)
    if (audioProcessor.getMidiLearn().consumeLearnCompleted()
        && audioProcessor.getMidiLearn().getScope() == v2::MidiLearn::Scope::global)
        (void) audioProcessor.saveGlobalMidiMap();
}

void MonolithAudioProcessorEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double dt = (now - lastFrameMs) * 0.001;
    lastFrameMs = now;
    runUiFrame (dt);
}

} // namespace monolith
