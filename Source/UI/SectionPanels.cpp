#include "SectionPanels.h"
#include "../State/ParameterIDs.h"

namespace monolith::ui
{

namespace
{
    void drawPanelChrome (juce::Graphics& g,
                          juce::Rectangle<int> bounds,
                          const juce::String& title,
                          juce::Colour accent = palette::moltenAmber)
    {
        const auto b = bounds.toFloat().reduced (0.5f);
        g.setColour (palette::panelSlate);
        g.fillRoundedRectangle (b, 3.0f);
        g.setColour (palette::gridHairline);
        g.drawRoundedRectangle (b, 3.0f, 1.0f);

        // Architectural header hairline + left accent notch
        g.setColour (accent.withAlpha (0.85f));
        g.fillRect (b.getX() + 10.0f, b.getY() + 9.0f, 3.0f, 11.0f);

        g.setColour (palette::boneWhite.withAlpha (0.86f));
        g.setFont (makeFont (10.5f, true, false));
        g.drawText (title,
                    b.reduced (18.0f, 7.0f).withHeight (15.0f),
                    juce::Justification::centredLeft,
                    false);

        g.setColour (palette::gridHairline);
        g.drawHorizontalLine (static_cast<int> (b.getY() + 26.0f), b.getX() + 10.0f, b.getRight() - 10.0f);
    }

    void layoutDialGrid3x2 (juce::Rectangle<int> area,
                            std::initializer_list<juce::Component*> row1,
                            std::initializer_list<juce::Component*> row2)
    {
        const int rowH = (area.getHeight() - 6) / 2;
        auto topRow = area.removeFromTop (rowH);
        area.removeFromTop (6);
        auto botRow = area;

        auto layoutRow = [] (juce::Rectangle<int> row, std::initializer_list<juce::Component*> items)
        {
            const int count = static_cast<int> (items.size());
            if (count <= 0) return;
            const int cellW = row.getWidth() / count;
            for (auto* c : items)
                c->setBounds (row.removeFromLeft (cellW).reduced (4, 1));
        };

        layoutRow (topRow, row1);
        layoutRow (botRow, row2);
    }
} // namespace

// ============================================================================
// 1. EnsemblePanel
// ============================================================================
EnsemblePanel::EnsemblePanel (juce::AudioProcessorValueTreeState& apvts)
    : sizeDial (apvts, params::ensembleSize, "SIZE",
                "Virtual ensemble density (1-24 singers dynamically budgeted across chords).",
                " v", 0, false, false, "amber"),
      spreadDial (apvts, params::ensembleSpread, "SPREAD",
                  "Stereo width, micro-detune dispersion, formant tract scatter, and entrance stagger.",
                  "", 0, true, false, "amber"),
      vibratoDial (apvts, params::vibratoDepth, "VIBRATO",
                   "Per-singer delayed natural vibrato depth (4.5-6.2 Hz).",
                   "", 0, true, false, "amber"),
      attackDial (apvts, params::attackTime, "ATTACK",
                  "Lung swell attack time (0.005 to 6.0 seconds).",
                  " s", 2, false, false, "amber"),
      releaseDial (apvts, params::releaseTime, "RELEASE",
                   "Note release tail time (0.02 to 8.0 seconds).",
                   " s", 2, false, false, "amber"),
      onsetDial (apvts, params::onsetAmount, "ONSET",
                 "Velocity-sensitive consonant articulation intensity at note attack.",
                 "", 0, true, false, "cyan")
{
    registerBox.addItemList ({ "Basso Profundo", "Tenor", "Alto", "Soprano", "SATB Choir" }, 1);
    registerBox.setTooltip ("Physical vocal register tract scaling or automatic SATB split by pitch.");
    addAndMakeVisible (registerBox);
    registerAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, params::registerMode, registerBox);

    onsetBox.addItemList ({ "Soft Breath (H)", "Closed Hum (M/N)", "Ghost Sibilant (Sh)" }, 1);
    onsetBox.setTooltip ("Consonant articulator mode triggered on note onset.");
    addAndMakeVisible (onsetBox);
    onsetAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, params::onsetMode, onsetBox);

    for (auto* d : { &sizeDial, &spreadDial, &vibratoDial, &attackDial, &releaseDial, &onsetDial })
        addAndMakeVisible (d);
}

void EnsemblePanel::paint (juce::Graphics& g)
{
    drawPanelChrome (g, getLocalBounds(), "ENSEMBLE & REGISTER", palette::moltenAmber);
}

void EnsemblePanel::resized()
{
    auto area = getLocalBounds().reduced (10, 8);
    area.removeFromTop (24);

    auto comboRow = area.removeFromTop (24);
    const int halfW = (comboRow.getWidth() - 6) / 2;
    registerBox.setBounds (comboRow.removeFromLeft (halfW));
    comboRow.removeFromLeft (6);
    onsetBox.setBounds (comboRow);

    area.removeFromTop (8);
    layoutDialGrid3x2 (area,
                       { &sizeDial, &spreadDial, &vibratoDial },
                       { &attackDial, &releaseDial, &onsetDial });
}

// ============================================================================
// 2. ToneLayersPanel
// ============================================================================
ToneLayersPanel::ToneLayersPanel (juce::AudioProcessorValueTreeState& apvts)
    : tensionDial (apvts, params::glottalTension, "TENSION",
                   "LF glottal effort morphing from airy falsetto to harmonically dense fortissimo.",
                   "", 0, true, false, "amber"),
      breathDial (apvts, params::breathAmount, "BREATH",
                  "Glottal-phase-modulated aspirated vocal breath noise.",
                  "", 0, true, false, "cyan"),
      throatDial (apvts, params::throatShift, "THROAT",
                  "Formant tract length shift (-12 to +12 semitones), decoupled from pitch.",
                  " st", 1, false, false, "amber"),
      focusDial (apvts, params::formantFocus, "FOCUS",
                 "Vocal-tract formant sharpness / Q selectivity (pad blend to razor choir).",
                 "", 0, true, false, "amber"),
      subAbyssDial (apvts, params::subAbyss, "SUB-ABYSS",
                    "Phase-coherent sub-octave chest resonance with soft saturation.",
                    "", 0, true, false, "amber"),
      crownDial (apvts, params::spectralCrown, "CROWN",
                 "Detuned airy upper-octave falsetto sheen opening on expressive swells.",
                 "", 0, true, false, "cyan")
{
    for (auto* d : { &tensionDial, &breathDial, &throatDial, &focusDial, &subAbyssDial, &crownDial })
        addAndMakeVisible (d);
}

void ToneLayersPanel::paint (juce::Graphics& g)
{
    drawPanelChrome (g, getLocalBounds(), "TONE, FORMANT & LAYERS", palette::moltenAmber);
}

void ToneLayersPanel::resized()
{
    auto area = getLocalBounds().reduced (10, 8);
    area.removeFromTop (24);
    layoutDialGrid3x2 (area,
                       { &tensionDial, &breathDial, &throatDial },
                       { &focusDial, &subAbyssDial, &crownDial });
}

// ============================================================================
// 3. OrbitPanel
// ============================================================================
OrbitPanel::OrbitPanel (juce::AudioProcessorValueTreeState& apvts)
    : rateDial (apvts, params::orbitRate, "RATE",
                "Organic vowel orbit speed (Hz or tempo-synced musical bars).",
                " Hz", 2, false, false, "cyan"),
      depthDial (apvts, params::orbitDepth, "DEPTH",
                 "Radial excursion depth across the 2D vowel field.",
                 "", 0, true, false, "cyan")
{
    orbitBtn.getProperties().set (MonolithLookAndFeel::accentPropertyId, "cyan");
    orbitBtn.setTooltip ("Enable continuous organic Lissajous drift across the 2D phoneme field.");
    addAndMakeVisible (orbitBtn);
    orbitAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, params::orbitEnable, orbitBtn);

    syncBtn.getProperties().set (MonolithLookAndFeel::accentPropertyId, "cyan");
    syncBtn.setTooltip ("Lock Organic Orbit rate to host tempo bar divisions.");
    addAndMakeVisible (syncBtn);
    syncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, params::orbitSync, syncBtn);

    addAndMakeVisible (rateDial);
    addAndMakeVisible (depthDial);
}

void OrbitPanel::paint (juce::Graphics& g)
{
    drawPanelChrome (g, getLocalBounds(), "ORGANIC ORBIT ENGINE", palette::paleCyan);
}

void OrbitPanel::resized()
{
    auto area = getLocalBounds().reduced (10, 6);
    area.removeFromTop (22);

    auto toggleCol = area.removeFromLeft (118);
    orbitBtn.setBounds (toggleCol.removeFromTop (23));
    toggleCol.removeFromTop (4);
    syncBtn.setBounds (toggleCol.removeFromTop (23));

    area.removeFromLeft (12);
    const int dialW = std::min (105, area.getWidth() / 2);
    rateDial.setBounds (area.removeFromLeft (dialW).expanded (0, 6));
    area.removeFromLeft (10);
    depthDial.setBounds (area.removeFromLeft (dialW).expanded (0, 6));
}

// ============================================================================
// 4. EntropyPanel
// ============================================================================
EntropyPanel::EntropyPanel (juce::AudioProcessorValueTreeState& apvts)
    : driveDial (apvts, params::driveAmount, "DRIVE",
                 "Per-voice pre-sum oversampled asymmetric tape & tube saturation.",
                 "", 0, true, false, "amber"),
      tapeDial (apvts, params::tapeInstability, "TAPE DRIFT",
                "Musical cassette wow, flutter, and micro-pitch instability.",
                "", 0, true, false, "amber"),
      oxideDial (apvts, params::oxideDamping, "OXIDE",
                 "Warm tape head bump + high-frequency magnetic oxide rolloff.",
                 "", 0, true, false, "amber"),
      screwPitchDial (apvts, params::screwPitch, "SCREW PITCH",
                      "Formant-detached pitch drop (-12 to 0 st) for chopped & screwed shadows.",
                      " st", 1, false, false, "cyan"),
      screwDepthDial (apvts, params::screwDepth, "SCREW SHADOW",
                      "Witch-house vocal tract darkening and slow tape-sag droop.",
                      "", 0, true, false, "cyan")
{
    os4xBtn.getProperties().set (MonolithLookAndFeel::accentPropertyId, "amber");
    os4xBtn.setTooltip ("Switch per-voice pre-sum saturation oversampling between 2x (low CPU) and 4x (HQ).");
    addAndMakeVisible (os4xBtn);
    os4xAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, params::oversampleHq, os4xBtn);

    for (auto* d : { &driveDial, &tapeDial, &oxideDial, &screwPitchDial, &screwDepthDial })
        addAndMakeVisible (d);
}

void EntropyPanel::paint (juce::Graphics& g)
{
    drawPanelChrome (g, getLocalBounds(), "ENTROPY & WITCH-HOUSE", palette::moltenAmber);

    // Subtle divider framing the Witch-House Screw duo
    const auto b = getLocalBounds().toFloat();
    g.setColour (palette::gridHairline.withAlpha (0.7f));
    g.drawHorizontalLine (static_cast<int> (b.getY() + 126.0f), b.getX() + 14.0f, b.getRight() - 14.0f);
}

void EntropyPanel::resized()
{
    auto b = getLocalBounds();
    os4xBtn.setBounds (b.getRight() - 78, 5, 68, 18);

    auto area = b.reduced (10, 8);
    area.removeFromTop (24);

    const int rowH = (area.getHeight() - 8) / 2;
    auto topRow = area.removeFromTop (rowH);
    const int w3 = topRow.getWidth() / 3;
    driveDial.setBounds (topRow.removeFromLeft (w3).reduced (3, 1));
    tapeDial.setBounds (topRow.removeFromLeft (w3).reduced (3, 1));
    oxideDial.setBounds (topRow.reduced (3, 1));

    area.removeFromTop (8);
    auto botRow = area.reduced (24, 0);
    const int w2 = botRow.getWidth() / 2;
    screwPitchDial.setBounds (botRow.removeFromLeft (w2).reduced (8, 1));
    screwDepthDial.setBounds (botRow.reduced (8, 1));
}

// ============================================================================
// 5. SpacePanel
// ============================================================================
SpacePanel::SpacePanel (juce::AudioProcessorValueTreeState& apvts)
    : mixDial (apvts, params::spaceMix, "MIX",
               "Dry/Wet balance into the brutalist architectural reverb.",
               "", 0, true, false, "cyan"),
      decayDial (apvts, params::spaceDecay, "DECAY",
                 "Architectural FDN reverberation decay time.",
                 "", 0, true, false, "cyan"),
      dampDial (apvts, params::spaceDamp, "DAMP",
                "High-frequency stone/concrete absorption inside the reverb tank.",
                "", 0, true, false, "cyan"),
      shimmerDial (apvts, params::spaceShimmer, "SHIMMER",
                   "Granular pitch-shifted harmonic/sub-harmonic feedback cloud.",
                   "", 0, true, false, "cyan")
{
    spaceBox.addItemList ({ "Cathedral", "Concrete Silo", "Obsidian Abyss", "Ghost Void" }, 1);
    spaceBox.setTooltip ("Select architectural reverb chamber & shimmer tuning.");
    addAndMakeVisible (spaceBox);
    spaceAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, params::spaceType, spaceBox);

    cc64FreezeBtn.getProperties().set (MonolithLookAndFeel::accentPropertyId, "cyan");
    cc64FreezeBtn.setTooltip ("Assign Sustain Pedal (CC64) to toggle Spectral Freeze instead of note sustain.");
    addAndMakeVisible (cc64FreezeBtn);
    cc64Attach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, params::cc64Freeze, cc64FreezeBtn);

    freezeBtn.getProperties().set (MonolithLookAndFeel::accentPropertyId, "cyan");
    freezeBtn.setTooltip ("Click-free Spectral Freeze: locks the current chordal cloud for infinite sustain.");
    addAndMakeVisible (freezeBtn);
    freezeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, params::spaceFreeze, freezeBtn);

    for (auto* d : { &mixDial, &decayDial, &dampDial, &shimmerDial })
        addAndMakeVisible (d);
}

void SpacePanel::updateFreezeGlow (float freezeEnergy) noexcept
{
    if (std::abs (freezeEnergy - freezeEnergyGlow) > 0.01f)
    {
        freezeEnergyGlow = freezeEnergy;
        repaint();
    }
}

void SpacePanel::paint (juce::Graphics& g)
{
    drawPanelChrome (g, getLocalBounds(), "ARCHITECTURAL SPACE", palette::paleCyan);
    if (freezeEnergyGlow > 0.02f)
    {
        const auto fb = freezeBtn.getBounds().toFloat().expanded (2.0f);
        g.setColour (palette::paleCyan.withAlpha (0.20f * freezeEnergyGlow));
        g.fillRoundedRectangle (fb, 4.0f);
    }
}

void SpacePanel::resized()
{
    auto area = getLocalBounds().reduced (10, 8);
    area.removeFromTop (24);

    auto topRow = area.removeFromTop (24);
    cc64FreezeBtn.setBounds (topRow.removeFromRight (106));
    topRow.removeFromRight (6);
    spaceBox.setBounds (topRow);

    area.removeFromTop (10);
    freezeBtn.setBounds (area.removeFromBottom (30));
    area.removeFromBottom (8);

    const int w4 = area.getWidth() / 4;
    mixDial.setBounds (area.removeFromLeft (w4).reduced (1, 0));
    decayDial.setBounds (area.removeFromLeft (w4).reduced (1, 0));
    dampDial.setBounds (area.removeFromLeft (w4).reduced (1, 0));
    shimmerDial.setBounds (area.reduced (1, 0));
}

// ============================================================================
// 6. PerformanceDock
// ============================================================================
PerformanceDock::PerformanceDock (MonolithAudioProcessor& proc)
    : colossusDial (proc.getAPVTS(), params::macroColossus, "COLOSSUS",
                    "Scales ensemble size, sub-abyss, stereo spread, and cathedral grandeur.",
                    "", 0, true, true, "amber", "SCALE · SUB · WIDTH"),
      specterDial (proc.getAPVTS(), params::macroSpecter, "SPECTER",
                   "Morphs toward whispered breath, hollow formants, and ghost shimmer.",
                   "", 0, true, true, "cyan", "BREATH · HOLLOW · GHOST"),
      rotDial (proc.getAPVTS(), params::macroRot, "ROT",
               "Engages cassette wow/flutter, pre-sum drive, and oxide decay.",
               "", 0, true, true, "amber", "TAPE · GRIT · DECAY"),
      abyssDial (proc.getAPVTS(), params::macroAbyss, "ABYSS",
                 "Expands architectural decay, darkens damping, and swells the void.",
                 "", 0, true, true, "cyan", "SPACE · DARK · VOID"),
      exprBar (proc.getAPVTS(), params::expressionLevel,
               [&proc] (float v) { proc.setUiExpression (v); }),
      glideDial (proc.getAPVTS(), params::glideTime, "GLIDE",
                 "Legato portamento glide time with subtle upward vocal pitch-scoop.",
                 " s", 2, false, false, "amber")
{
    for (auto* m : { &colossusDial, &specterDial, &rotDial, &abyssDial })
        addAndMakeVisible (m);

    addAndMakeVisible (exprBar);

    legatoBtn.getProperties().set (MonolithLookAndFeel::accentPropertyId, "amber");
    legatoBtn.setTooltip ("Monophonic Vocal Legato mode with smooth portamento and pitch-scoop.");
    addAndMakeVisible (legatoBtn);
    legatoAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.getAPVTS(), params::legatoEnable, legatoBtn);

    addAndMakeVisible (glideDial);
}

void PerformanceDock::updateFromTelemetry (const TelemetrySnapshot& telemetry) noexcept
{
    exprBar.updateLive (telemetry.liveExpression.load (std::memory_order_relaxed));
}

void PerformanceDock::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (palette::panelSlate);
    g.fillRoundedRectangle (b, 3.0f);
    g.setColour (palette::gridHairline);
    g.drawRoundedRectangle (b, 3.0f, 1.0f);

    // Subtle vertical divider separating the 4 Master Macros from Expression & Legato
    const int divX = getWidth() - 378;
    g.drawVerticalLine (divX, 10.0f, static_cast<float> (getHeight() - 10));
}

void PerformanceDock::resized()
{
    auto area = getLocalBounds().reduced (10, 6);

    auto rightPanel = area.removeFromRight (364);
    area.removeFromRight (16);

    // 4 large Master Macros evenly distributed
    const int macroW = area.getWidth() / 4;
    colossusDial.setBounds (area.removeFromLeft (macroW).reduced (6, 0));
    specterDial.setBounds (area.removeFromLeft (macroW).reduced (6, 0));
    rotDial.setBounds (area.removeFromLeft (macroW).reduced (6, 0));
    abyssDial.setBounds (area.reduced (6, 0));

    // Right sub-panel: Expression bar + Legato toggle + Glide dial
    glideDial.setBounds (rightPanel.removeFromRight (92).reduced (2, 8));
    rightPanel.removeFromRight (10);

    exprBar.setBounds (rightPanel.removeFromTop (56).reduced (0, 6));
    rightPanel.removeFromTop (10);
    legatoBtn.setBounds (rightPanel.removeFromTop (32));
}

} // namespace monolith::ui
