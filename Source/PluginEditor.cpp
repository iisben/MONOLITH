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

    // Resizable from the bottom-right corner as before; the LookAndFeel draws no grip graphic.
    setResizable (true, true);
    if (auto* constrainer = getConstrainer())
    {
        constrainer->setFixedAspectRatio (static_cast<double> (kDesignWidth)
                                        / static_cast<double> (kDesignHeight));
        constrainer->setSizeLimits (885, 540, 2360, 1440);
    }

    setSize (kDesignWidth, kDesignHeight);
    startTimerHz (60);
}

MonolithAudioProcessorEditor::~MonolithAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MonolithAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Poured concrete ground between the slabs (shadow gaps read as joints)
    groundSurface.draw (g, getLocalBounds(), 3u, ui::palette::obsidianBg);
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

    const int currentIdx = audioProcessor.getPresetManager().getCurrentPresetIndex();
    if (currentIdx != lastSyncedPresetIdx)
    {
        lastSyncedPresetIdx = currentIdx;
        headerBar.refreshPresetDisplay();
    }
}

} // namespace monolith
