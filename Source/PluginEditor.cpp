#include "PluginEditor.h"

namespace monolith
{

MonolithAudioProcessorEditor::MonolithAudioProcessorEditor (MonolithAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      headerBar (p),
      ensemblePanel (p.getAPVTS()),
      toneLayersPanel (p.getAPVTS()),
      phonemeField (p.getAPVTS()),
      orbitPanel (p.getAPVTS()),
      entropyPanel (p.getAPVTS()),
      spacePanel (p.getAPVTS()),
      performanceDock (p)
{
    setLookAndFeel (&lookAndFeel);

    rootContainer.setBounds (0, 0, kDesignWidth, kDesignHeight);
    addAndMakeVisible (rootContainer);

    rootContainer.addAndMakeVisible (headerBar);
    rootContainer.addAndMakeVisible (ensemblePanel);
    rootContainer.addAndMakeVisible (toneLayersPanel);
    rootContainer.addAndMakeVisible (phonemeField);
    rootContainer.addAndMakeVisible (orbitPanel);
    rootContainer.addAndMakeVisible (entropyPanel);
    rootContainer.addAndMakeVisible (spacePanel);
    rootContainer.addAndMakeVisible (performanceDock);

    // Layout children cleanly on the 1180x720 reference canvas
    headerBar.setBounds       (10, 10, 1160, 54);
    ensemblePanel.setBounds   (10, 72, 340, 248);
    toneLayersPanel.setBounds (10, 328, 340, 226);
    phonemeField.setBounds    (358, 72, 464, 390);
    orbitPanel.setBounds      (358, 470, 464, 84);
    entropyPanel.setBounds    (830, 72, 340, 236);
    spacePanel.setBounds      (830, 316, 340, 238);
    performanceDock.setBounds (10, 562, 1160, 148);

    setResizable (true, true);
    if (auto* constrainer = getConstrainer())
    {
        constrainer->setFixedAspectRatio (static_cast<double> (kDesignWidth)
                                        / static_cast<double> (kDesignHeight));
        constrainer->setSizeLimits (885, 540, 2360, 1440);
    }

    setSize (kDesignWidth, kDesignHeight);
    rootContainer.addMouseListener (&midiLearnMenu, true);
    startTimerHz (60);
}

MonolithAudioProcessorEditor::~MonolithAudioProcessorEditor()
{
    rootContainer.removeMouseListener (&midiLearnMenu);
    stopTimer();
    setLookAndFeel (nullptr);
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
    g.fillAll (ui::palette::obsidianBg);
}

void MonolithAudioProcessorEditor::resized()
{
    const float sx = static_cast<float> (getWidth()) / static_cast<float> (kDesignWidth);
    const float sy = static_cast<float> (getHeight()) / static_cast<float> (kDesignHeight);
    rootContainer.setBounds (0, 0, kDesignWidth, kDesignHeight);
    rootContainer.setTransform (juce::AffineTransform::scale (sx, sy));
}

void MonolithAudioProcessorEditor::timerCallback()
{
    const auto& telemetry = audioProcessor.getTelemetry();
    headerBar.updateFromTelemetry (telemetry);
    phonemeField.updateFromTelemetry (telemetry);
    spacePanel.updateFreezeGlow (telemetry.freezeEnergy.load (std::memory_order_relaxed));
    performanceDock.updateFromTelemetry (telemetry);

    // V2: persist a freshly learned CC to the global map (learn completes on the audio thread)
    if (audioProcessor.getMidiLearn().consumeLearnCompleted()
        && audioProcessor.getMidiLearn().getScope() == v2::MidiLearn::Scope::global)
        (void) audioProcessor.saveGlobalMidiMap();

    const int currentIdx = audioProcessor.getPresetManager().getCurrentPresetIndex();
    if (currentIdx != lastSyncedPresetIdx)
    {
        lastSyncedPresetIdx = currentIdx;
        headerBar.refreshPresetDisplay();
    }
}

} // namespace monolith
