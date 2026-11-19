#include "HeaderBar.h"
#include "../State/ParameterIDs.h"

namespace monolith::ui
{

HeaderBar::HeaderBar (MonolithAudioProcessor& proc)
    : processor (proc),
      masterGainDial (proc.getAPVTS(),
                      params::masterGain,
                      "OUT",
                      "Master Output Trim (-18 to +6 dB) before transparent soft safety limiter.",
                      " dB",
                      1,
                      false,
                      false,
                      "amber")
{
    auto& pm = processor.getPresetManager();

    // Category filter ComboBox
    const auto categories = pm.getCategories();
    for (int i = 0; i < categories.size(); ++i)
        categoryBox.addItem (categories[i], i + 1);
    categoryBox.setSelectedId (1, juce::dontSendNotification);
    categoryBox.setTooltip ("Filter factory presets by genre / acoustic category.");
    categoryBox.onChange = [this] { repopulatePresetBox(); };
    addAndMakeVisible (categoryBox);

    presetBox.setTooltip ("Select one of the 30 calibrated factory presets.");
    presetBox.onChange = [this]
    {
        const int sel = presetBox.getSelectedItemIndex();
        if (sel >= 0 && sel < static_cast<int> (filteredIndices.size()))
        {
            processor.applyFactoryPreset (filteredIndices[static_cast<size_t> (sel)]);
        }
    };
    addAndMakeVisible (presetBox);

    prevBtn.setTooltip ("Previous preset in current category filter.");
    prevBtn.onClick = [this] { stepPreset (-1); };
    addAndMakeVisible (prevBtn);

    nextBtn.setTooltip ("Next preset in current category filter.");
    nextBtn.onClick = [this] { stepPreset (1); };
    addAndMakeVisible (nextBtn);

    abBtn.setTooltip ("Toggle between A and B comparison parameter snapshots.");
    abBtn.getProperties().set (MonolithLookAndFeel::accentPropertyId, "cyan");
    abBtn.onClick = [this]
    {
        processor.toggleABCompare();
        abBtn.setButtonText (processor.getPresetManager().isSlotBActive() ? "B" : "A");
    };
    addAndMakeVisible (abBtn);

    copyAbBtn.setTooltip ("Copy current active slot settings into the inactive A/B slot.");
    copyAbBtn.onClick = [this] { processor.copyActiveToInactiveAB(); };
    addAndMakeVisible (copyAbBtn);

    panicBtn.setTooltip ("Silence all active voices, clear sustain/freeze buffers, and reset reverb.");
    panicBtn.getProperties().set (MonolithLookAndFeel::accentPropertyId, "crimson");
    panicBtn.onClick = [this] { processor.triggerPanic(); };
    addAndMakeVisible (panicBtn);

    addAndMakeVisible (voiceBadge);
    addAndMakeVisible (outputMeter);
    addAndMakeVisible (masterGainDial);

    repopulatePresetBox();
}

void HeaderBar::repopulatePresetBox()
{
    auto& pm = processor.getPresetManager();
    const juce::String selectedCat = categoryBox.getText();
    filteredIndices = pm.getPresetIndicesForCategory (selectedCat.isEmpty() ? "All" : selectedCat);

    presetBox.clear (juce::dontSendNotification);
    const int currentGlobalIdx = pm.getCurrentPresetIndex();
    int itemToSelect = 1;

    for (size_t i = 0; i < filteredIndices.size(); ++i)
    {
        const int globalIdx = filteredIndices[i];
        presetBox.addItem (pm.getPreset (globalIdx).name, static_cast<int> (i) + 1);
        if (globalIdx == currentGlobalIdx)
            itemToSelect = static_cast<int> (i) + 1;
    }

    if (! filteredIndices.empty())
        presetBox.setSelectedId (itemToSelect, juce::dontSendNotification);
}

void HeaderBar::stepPreset (int delta)
{
    if (filteredIndices.empty())
        return;

    const int currentGlobal = processor.getPresetManager().getCurrentPresetIndex();
    int pos = 0;
    for (int i = 0; i < static_cast<int> (filteredIndices.size()); ++i)
    {
        if (filteredIndices[static_cast<size_t> (i)] == currentGlobal)
        {
            pos = i;
            break;
        }
    }

    const int count = static_cast<int> (filteredIndices.size());
    const int nextPos = (pos + delta + count) % count;
    processor.applyFactoryPreset (filteredIndices[static_cast<size_t> (nextPos)]);
    presetBox.setSelectedId (nextPos + 1, juce::dontSendNotification);
}

void HeaderBar::refreshPresetDisplay()
{
    const int currentGlobal = processor.getPresetManager().getCurrentPresetIndex();
    for (int i = 0; i < static_cast<int> (filteredIndices.size()); ++i)
    {
        if (filteredIndices[static_cast<size_t> (i)] == currentGlobal)
        {
            presetBox.setSelectedId (i + 1, juce::dontSendNotification);
            break;
        }
    }
    abBtn.setButtonText (processor.getPresetManager().isSlotBActive() ? "B" : "A");
}

void HeaderBar::updateFromTelemetry (const TelemetrySnapshot& telemetry) noexcept
{
    voiceBadge.update (telemetry.activeVoices.load (std::memory_order_relaxed),
                       telemetry.activeSingers.load (std::memory_order_relaxed));
    outputMeter.update (telemetry.rmsL.load (std::memory_order_relaxed),
                        telemetry.rmsR.load (std::memory_order_relaxed),
                        telemetry.peakL.load (std::memory_order_relaxed),
                        telemetry.peakR.load (std::memory_order_relaxed));
}

void HeaderBar::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (palette::panelSlate);
    g.fillRoundedRectangle (b, 3.0f);
    g.setColour (palette::gridHairline);
    g.drawRoundedRectangle (b, 3.0f, 1.0f);

    // Brutalist Monolith emblem (3 staggered vertical stone slabs)
    const float sx = b.getX() + 14.0f;
    const float sy = b.getY() + 12.0f;
    g.setColour (palette::moltenAmber);
    g.fillRect (sx, sy + 6.0f, 4.0f, 22.0f);
    g.setColour (palette::boneWhite);
    g.fillRect (sx + 6.0f, sy, 6.0f, 28.0f);
    g.setColour (palette::paleCyan.withAlpha (0.85f));
    g.fillRect (sx + 14.0f, sy + 9.0f, 4.0f, 19.0f);

    // MONOLITH Wordmark
    g.setColour (palette::boneWhite);
    g.setFont (makeFont (17.5f, true, false));
    g.drawText ("MONOLITH", static_cast<int> (sx + 26.0f), 9, 138, 22, juce::Justification::centredLeft, false);

    g.setColour (palette::mutedLimestone);
    g.setFont (makeFont (8.8f, true, false));
    g.drawText ("PHYSICAL CHORAL SWARM", static_cast<int> (sx + 27.0f), 30, 146, 13, juce::Justification::centredLeft, false);
}

void HeaderBar::resized()
{
    auto area = getLocalBounds().reduced (10, 8);
    area.removeFromLeft (188); // Wordmark & emblem

    const int gap = 5;
    categoryBox.setBounds (area.removeFromLeft (142).reduced (0, 5));
    area.removeFromLeft (gap);

    prevBtn.setBounds (area.removeFromLeft (24).reduced (0, 5));
    area.removeFromLeft (3);
    presetBox.setBounds (area.removeFromLeft (212).reduced (0, 5));
    area.removeFromLeft (3);
    nextBtn.setBounds (area.removeFromLeft (24).reduced (0, 5));
    area.removeFromLeft (gap + 3);

    abBtn.setBounds (area.removeFromLeft (36).reduced (0, 5));
    area.removeFromLeft (3);
    copyAbBtn.setBounds (area.removeFromLeft (46).reduced (0, 5));

    // Right-aligned status & panic cluster
    panicBtn.setBounds (area.removeFromRight (58).reduced (0, 5));
    area.removeFromRight (gap);
    masterGainDial.setBounds (area.removeFromRight (54).expanded (0, 6));
    area.removeFromRight (gap);
    outputMeter.setBounds (area.removeFromRight (104).reduced (0, 5));
    area.removeFromRight (gap);
    voiceBadge.setBounds (area.removeFromRight (156).reduced (0, 6));
}

} // namespace monolith::ui
