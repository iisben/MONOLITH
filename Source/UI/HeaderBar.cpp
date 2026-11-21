#include "HeaderBar.h"
#include "../State/ParameterIDs.h"

namespace monolith::ui
{

HeaderBar::HeaderBar (MonolithAudioProcessor& proc)
    : processor (proc),
      masterGainDial (proc.getAPVTS(),
                      params::masterGain,
                      "OUT",
                      "Master Output Trim (-18 to +6 dB) into the glue compressor and -1 dBFS lookahead brickwall limiter.",
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
    categoryBox.setTooltip ("Filter factory presets by purpose: epic, sacred, solo, low, pads, tension, lo-fi, darkwave.");
    categoryBox.onChange = [this] { repopulatePresetBox(); repaint(); };
    addAndMakeVisible (categoryBox);

    presetBox.setTooltip ("Select one of the 38 calibrated factory presets.");
    presetBox.onChange = [this]
    {
        const int sel = presetBox.getSelectedId() - 1;
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

    const bool showHeadings = selectedCat.isEmpty() || selectedCat == "All";
    juce::String lastCategory;
    for (size_t i = 0; i < filteredIndices.size(); ++i)
    {
        const int globalIdx = filteredIndices[i];
        const auto& preset = pm.getPreset (globalIdx);
        if (showHeadings && preset.category != lastCategory)
        {
            presetBox.addSectionHeading (preset.category.toUpperCase());
            lastCategory = preset.category;
        }
        presetBox.addItem (preset.name, static_cast<int> (i) + 1);
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
    surface.draw (g, getLocalBounds(), 5u, palette::panelSlate);
    const auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.75f));
    g.drawRect (b, 1.0f);
    g.setColour (palette::edgeLight.withAlpha (0.8f));
    g.fillRect (b.getX() + 1.0f, b.getY() + 1.0f, b.getWidth() - 2.0f, 1.0f);

    // Emblem: one black slab in the proportions 1 : 4 : 9, its top edge gilded
    const auto slab = juce::Rectangle<float> (b.getX() + 16.0f, b.getY() + 9.0f, 16.0f, 36.0f);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRect (slab.translated (2.0f, 2.0f));
    g.setColour (juce::Colour (0xFF050505));
    g.fillRect (slab);
    g.setColour (palette::moltenAmber);
    g.fillRect (slab.withHeight (1.5f));
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.fillRect (slab.withWidth (1.0f).translated (1.0f, 0.0f));

    // Wordmark, cut like an inscription
    g.setColour (palette::boneWhite);
    g.setFont (makeRoleFont (FontRole::Inscription, 21.0f, true));
    drawTracked (g, "MONOLITH", juce::Rectangle<float> (b.getX() + 42.0f, b.getY() + 7.0f, 160.0f, 24.0f),
                 juce::Justification::centredLeft, 0.30f);

    g.setColour (palette::mutedLimestone);
    g.setFont (makeFont (8.0f, true, false));
    drawTracked (g, "PHYSICAL CHORAL SWARM", juce::Rectangle<float> (b.getX() + 43.0f, b.getY() + 31.0f, 160.0f, 12.0f),
                 juce::Justification::centredLeft, 0.26f);

    // Category blurb under the browser
    g.setColour (palette::dimAsh.brighter (0.25f));
    g.setFont (makeFont (8.5f, false, false));
    g.drawText (processor.getPresetManager().getCategoryBlurb (categoryBox.getText().isEmpty() ? "All" : categoryBox.getText()),
                categoryBox.getBounds().toFloat().translated (0.0f, 22.0f).withHeight (11.0f).withWidth (360.0f),
                juce::Justification::centredLeft, false);
}

void HeaderBar::resized()
{
    auto area = getLocalBounds().reduced (10, 8).withTrimmedBottom (12).translated (0, -2);
    area.removeFromLeft (200); // Wordmark & emblem

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
    masterGainDial.setBounds (area.removeFromRight (86).withSizeKeepingCentre (86, 34));
    area.removeFromRight (gap);
    outputMeter.setBounds (area.removeFromRight (100).reduced (0, 3));
    area.removeFromRight (gap);
    voiceBadge.setBounds (area.removeFromRight (156).reduced (0, 6));
}

} // namespace monolith::ui
