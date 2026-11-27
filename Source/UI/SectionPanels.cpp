#include "SectionPanels.h"
#include "UiText.h"
#include "../State/ParameterIDs.h"
#include "../V2/TextureAssetTable.h"

namespace monolith::ui
{

using Style = MonolithDial::Style;

namespace
{
    /** Every standard knob cell on the chassis is 80 px tall (8 px grid): 14 title + 46 dial + 14 value. */
    constexpr int kRowH = 80;

    /** Slide rests at its 0.5 centre; only show it once MPE / CC74 has actually moved it. */
    float slideForMeter (const v2::MpeExpression& ex) noexcept
    {
        const float s = ex.uiSlide.load (std::memory_order_relaxed);
        return (ex.uiMpeActive.load (std::memory_order_relaxed) || std::abs (s - 0.5f) > 0.001f) ? s : 0.0f;
    }

    void placeRow (juce::Rectangle<int> row, std::initializer_list<juce::Component*> items, int cellW)
    {
        const int total = cellW * static_cast<int> (items.size());
        int x = row.getX() + (row.getWidth() - total) / 2;
        for (auto* c : items)
        {
            c->setBounds (x, row.getY(), cellW, row.getHeight());
            x += cellW;
        }
    }
}

// ============================================================================
PanelBase::PanelBase (juce::String titleText, bool hasHeader) : title (std::move (titleText)), header (hasHeader)
{
    setOpaque (false);
}

void PanelBase::paint (juce::Graphics& g)
{
    chromeLayer.draw (g, getLocalBounds(), [this] (juce::Graphics& cg, juce::Rectangle<float> r) { renderChrome (cg, r); });
    paintLive (g);
}

void PanelBase::resized()
{
    chromeLayer.invalidate();
    if (tabbed)
    {
        tabs[0].setBounds (16, 4, 88, 28);
        tabs[1].setBounds (112, 4, 104, 28);
    }
    layoutChildren();
}

void PanelBase::renderChrome (juce::Graphics& g, juce::Rectangle<float> r)
{
    chrome::drawPlate (g, r, tabbed ? juce::String() : title, header);
    if (! header && title.isNotEmpty())
        chrome::drawCaption (g, { 16.0f, 4.0f, r.getWidth() - 32.0f, 28.0f }, title);
}

void PanelBase::initTabs (const juce::String& a, const juce::String& b)
{
    tabbed = true;
    for (int i = 0; i < 2; ++i)
    {
        auto& t = tabs[static_cast<size_t> (i)];
        t.setButtonText (i == 0 ? a : b);
        t.getProperties().set (MonolithLookAndFeel::segmentPropertyId, "tab");
        t.setRadioGroupId (0x7A8);
        t.setClickingTogglesState (true);
        t.onClick = [this, i] { setPage (i); };
        addAndMakeVisible (t);
    }
    tabs[0].setToggleState (true, juce::dontSendNotification);
}

void PanelBase::setPage (int page)
{
    activePage = std::clamp (page, 0, 1);
    tabs[static_cast<size_t> (activePage)].setToggleState (true, juce::dontSendNotification);
    for (int p = 0; p < 2; ++p)
        for (auto* c : pages[static_cast<size_t> (p)])
            c->setVisible (p == activePage);
    repaint();
}

// ============================================================================
EnsemblePanel::EnsemblePanel (MonolithAudioProcessor& p)
    : PanelBase ("ENSEMBLE & REGISTER"),
      sizeDial (p.getAPVTS(), params::ensembleSize, "SIZE", "Virtual ensemble density: 1-24 singers, budgeted across the chord.", "", 0),
      spreadDial (p.getAPVTS(), params::ensembleSpread, "SPREAD", "Stereo width, micro-detune dispersion, formant scatter and entrance stagger.", "", 0, true),
      vibratoDial (p.getAPVTS(), params::vibratoDepth, "VIBRATO", "Per-singer delayed natural vibrato depth (4.5-6.2 Hz).", "", 0, true),
      attackDial (p.getAPVTS(), params::attackTime, "ATTACK", "Lung swell attack time (5 ms - 6 s).", " s", 2),
      releaseDial (p.getAPVTS(), params::releaseTime, "RELEASE", "Note release tail (20 ms - 8 s).", " s", 2),
      onsetDial (p.getAPVTS(), params::onsetAmount, "ONSET", "Velocity-sensitive consonant articulation at note attack.", "", 0, true, Style::standard, "ivory")
{
    registerBox.addItemList ({ "Basso Profundo", "Tenor", "Alto", "Soprano", "SATB Choir" }, 1);
    registerBox.setTooltip ("Vocal register: physical tract scaling, or automatic SATB split by pitch.");
    registerBox.getProperties().set ("monolithParamId", params::registerMode);
    addAndMakeVisible (registerBox);
    registerAttach = std::make_unique<ComboAttach> (p.getAPVTS(), params::registerMode, registerBox);

    onsetBox.addItemList ({ "Soft Breath (H)", "Closed Hum (M/N)", "Ghost Sibilant (Sh)" }, 1);
    onsetBox.setTooltip ("Consonant articulator triggered on note onset.");
    onsetBox.getProperties().set ("monolithParamId", params::onsetMode);
    addAndMakeVisible (onsetBox);
    onsetAttach = std::make_unique<ComboAttach> (p.getAPVTS(), params::onsetMode, onsetBox);

    for (auto* d : { &sizeDial, &spreadDial, &vibratoDial, &attackDial, &releaseDial, &onsetDial })
        addAndMakeVisible (d);
}

void EnsemblePanel::layoutChildren()
{
    registerBox.setBounds (16, 40, 152, 24);
    onsetBox.setBounds (176, 40, 152, 24);
    placeRow ({ 16, 72, 312, kRowH }, { &sizeDial, &spreadDial, &vibratoDial }, 104);
    placeRow ({ 16, 160, 312, kRowH }, { &attackDial, &releaseDial, &onsetDial }, 104);
}

// ============================================================================
ToneExpressionPanel::ToneExpressionPanel (MonolithAudioProcessor& p)
    : PanelBase ("TONE & FORMANT"),
      tensionDial (p.getAPVTS(), params::glottalTension, "TENSION", "LF glottal effort: airy falsetto to dense fortissimo.", "", 0, true),
      breathDial (p.getAPVTS(), params::breathAmount, "BREATH", "Glottal-phase-modulated aspiration noise.", "", 0, true, Style::standard, "ivory"),
      throatDial (p.getAPVTS(), params::throatShift, "THROAT", "Formant tract length shift (-12 to +12 st), decoupled from pitch.", " st", 1),
      focusDial (p.getAPVTS(), params::formantFocus, "FOCUS", "Formant sharpness / Q: soft pad blend to razor choir.", "", 0, true),
      subAbyssDial (p.getAPVTS(), params::subAbyss, "SUB-ABYSS", "Phase-coherent sub-octave chest resonance.", "", 0, true),
      crownDial (p.getAPVTS(), params::spectralCrown, "CROWN", "Detuned airy upper-octave falsetto sheen.", "", 0, true, Style::standard, "ivory"),
      pbRangeDial (p.getAPVTS(), params::v2MpePbRange, "BEND RANGE", "MPE per-note pitch-bend range (0-48 st). Global setting, not stored in presets.", " st", 0),
      vibDial (p.getAPVTS(), params::v2ExprVibrato, "PRESS > VIB", "Pressure / aftertouch to per-voice vibrato depth.", "", 0, true),
      effortDial (p.getAPVTS(), params::v2ExprEffort, "PRESS > EFFORT", "Pressure / breath to vocal effort and spectral tilt.", "", 0, true),
      openDial (p.getAPVTS(), params::v2ExprOpenness, "SLIDE > OPEN", "MPE slide / CC74 to formant openness.", "", 0, true, Style::standard, "ivory"),
      swellDial (p.getAPVTS(), params::v2ExprSwell, "PRESS > SWELL", "Pressure to texture-layer swell (never un-bypasses the layer).", "", 0, true, Style::standard, "ivory")
{
    initTabs ("TONE", "EXPRESSION");
    for (auto* d : { &tensionDial, &breathDial, &throatDial, &focusDial, &subAbyssDial, &crownDial })
    {
        addAndMakeVisible (d);
        pages[0].push_back (d);
    }

    velCurveBox.addItemList ({ "Linear", "Soft", "Hard", "S-Curve", "Fixed 100" }, 1);
    velCurveBox.setTooltip ("Velocity response curve (global controller setting).");
    velCurveBox.getProperties().set ("monolithParamId", params::v2VelCurve);
    velCurveAttach = std::make_unique<ComboAttach> (p.getAPVTS(), params::v2VelCurve, velCurveBox);
    mpeBtn.setTooltip ("MPE lower zone (master ch 1, members 2-16): per-note bend, slide and pressure.");
    mpeBtn.getProperties().set ("monolithParamId", params::v2MpeEnable);
    mpeAttach = std::make_unique<ButtonAttach> (p.getAPVTS(), params::v2MpeEnable, mpeBtn);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &velCurveBox, &mpeBtn, &pbRangeDial, &vibDial, &effortDial, &openDial, &swellDial })
    {
        addChildComponent (c);
        pages[1].push_back (c);
    }
}

void ToneExpressionPanel::layoutChildren()
{
    placeRow ({ 16, 56, 312, kRowH }, { &tensionDial, &breathDial, &throatDial }, 104);
    placeRow ({ 16, 152, 312, kRowH }, { &focusDial, &subAbyssDial, &crownDial }, 104);

    velCurveBox.setBounds (104, 40, 128, 24);
    mpeBtn.setBounds (240, 40, 88, 24);
    placeRow ({ 16, 72, 312, kRowH }, { &pbRangeDial, &vibDial, &effortDial }, 104);
    placeRow ({ 16, 160, 312, kRowH }, { &openDial, &swellDial }, 104);
}

void ToneExpressionPanel::paintLive (juce::Graphics& g)
{
    if (activePage == 1)
    {
        g.setColour (palette::stone);
        g.setFont (capsFont (10.0f));
        g.drawText ("VELOCITY", juce::Rectangle<float> (16.0f, 40.0f, 84.0f, 24.0f), juce::Justification::centredLeft, false);
    }
}

// ============================================================================
EntropyPanel::EntropyPanel (MonolithAudioProcessor& p)
    : PanelBase ("ENTROPY & WITCH-HOUSE"),
      driveDial (p.getAPVTS(), params::driveAmount, "DRIVE", "Per-voice oversampled asymmetric tape & tube saturation.", "", 0, true),
      tapeDial (p.getAPVTS(), params::tapeInstability, "TAPE DRIFT", "Cassette wow, flutter and micro-pitch instability.", "", 0, true),
      oxideDial (p.getAPVTS(), params::oxideDamping, "OXIDE", "Tape head bump and magnetic oxide HF roll-off.", "", 0, true),
      screwPitchDial (p.getAPVTS(), params::screwPitch, "SCREW PITCH", "Formant-detached pitch drop (-12 to 0 st) for chopped & screwed shadows.", " st", 1, false, Style::standard, "ivory"),
      screwDepthDial (p.getAPVTS(), params::screwDepth, "SCREW SHADOW", "Witch-house tract darkening and slow tape-sag droop.", "", 0, true, Style::standard, "ivory")
{
    os4xBtn.setTooltip ("Drive oversampling: 2x (low CPU) or 4x (HQ).");
    os4xBtn.getProperties().set ("monolithParamId", params::oversampleHq);
    addAndMakeVisible (os4xBtn);
    os4xAttach = std::make_unique<ButtonAttach> (p.getAPVTS(), params::oversampleHq, os4xBtn);
    for (auto* d : { &driveDial, &tapeDial, &oxideDial, &screwPitchDial, &screwDepthDial })
        addAndMakeVisible (d);
}

void EntropyPanel::renderChrome (juce::Graphics& g, juce::Rectangle<float> r)
{
    PanelBase::renderChrome (g, r);
    chrome::drawDividerH (g, 144.0f, 16.0f, r.getRight() - 16.0f);
}

void EntropyPanel::layoutChildren()
{
    os4xBtn.setBounds (getWidth() - 16 - 80, 4, 80, 24);
    placeRow ({ 16, 48, 312, kRowH }, { &driveDial, &tapeDial, &oxideDial }, 104);
    placeRow ({ 16, 160, 312, kRowH }, { &screwPitchDial, &screwDepthDial }, 104);
}

// ============================================================================
SpaceTexturePanel::SpaceTexturePanel (MonolithAudioProcessor& p)
    : PanelBase ("ARCHITECTURAL SPACE"),
      mixDial (p.getAPVTS(), params::spaceMix, "MIX", "Dry / wet into the architectural FDN reverb.", "", 0, true),
      decayDial (p.getAPVTS(), params::spaceDecay, "DECAY", "FDN decay time.", "", 0, true),
      dampDial (p.getAPVTS(), params::spaceDamp, "DAMP", "Stone / concrete HF absorption in the tank.", "", 0, true),
      shimmerDial (p.getAPVTS(), params::spaceShimmer, "SHIMMER", "Granular pitch-shifted harmonic feedback cloud.", "", 0, true, Style::standard, "ivory"),
      tuneDial (p.getAPVTS(), params::v2TexTune, "TUNE", "Texture transpose (-24 to +24 st).", " st", 1),
      keytrackDial (p.getAPVTS(), params::v2TexKeytrack, "KEYTRACK", "How far the texture follows the keyboard.", "", 0, true),
      texAttackDial (p.getAPVTS(), params::v2TexAttack, "ATTACK", "Texture envelope attack.", " s", 2),
      texReleaseDial (p.getAPVTS(), params::v2TexRelease, "RELEASE", "Texture envelope release.", " s", 2),
      velDial (p.getAPVTS(), params::v2TexVelSens, "VELOCITY", "Texture velocity sensitivity.", "", 0, true),
      hpDial (p.getAPVTS(), params::v2TexHighPass, "HIGH-PASS", "Texture high-pass cutoff.", " Hz", 0, false, Style::standard, "ivory"),
      lpDial (p.getAPVTS(), params::v2TexLowPass, "LOW-PASS", "Texture low-pass cutoff.", " Hz", 0, false, Style::standard, "ivory"),
      widthDial (p.getAPVTS(), params::v2TexWidth, "WIDTH", "Texture stereo width (M/S, 0-200 %).", "", 0, true, Style::standard, "ivory")
{
    initTabs ("SPACE", "TEXTURE");

    spaceBox.addItemList ({ "Cathedral", "Concrete Silo", "Obsidian Abyss", "Ghost Void" }, 1);
    spaceBox.setTooltip ("Architectural chamber and shimmer tuning.");
    spaceBox.getProperties().set ("monolithParamId", params::spaceType);
    spaceAttach = std::make_unique<ComboAttach> (p.getAPVTS(), params::spaceType, spaceBox);
    cc64Btn.setTooltip ("Sustain pedal (CC64) toggles Spectral Freeze instead of sustain.");
    cc64Btn.getProperties().set ("monolithParamId", params::cc64Freeze);
    cc64Attach = std::make_unique<ButtonAttach> (p.getAPVTS(), params::cc64Freeze, cc64Btn);
    freezeBtn.setTooltip ("Click-free Spectral Freeze: holds the current chordal cloud indefinitely.");
    freezeBtn.getProperties().set (MonolithLookAndFeel::accentPropertyId, "ivory");
    freezeBtn.getProperties().set ("monolithParamId", params::spaceFreeze);
    freezeAttach = std::make_unique<ButtonAttach> (p.getAPVTS(), params::spaceFreeze, freezeBtn);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &spaceBox, &cc64Btn, &mixDial, &decayDial, &dampDial, &shimmerDial, &freezeBtn })
    {
        addAndMakeVisible (c);
        pages[0].push_back (c);
    }

    for (const auto& a : v2::kTextureAssets)
        assetBox.addItem (a.displayName, assetBox.getNumItems() + 1);
    assetBox.setTooltip ("Texture source (curated breath / air / choir assets, pre-decoded).");
    assetBox.getProperties().set ("monolithParamId", params::v2TexAsset);
    assetAttach = std::make_unique<ComboAttach> (p.getAPVTS(), params::v2TexAsset, assetBox);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &assetBox, &tuneDial, &keytrackDial, &texAttackDial, &texReleaseDial, &velDial, &hpDial, &lpDial, &widthDial })
    {
        addChildComponent (c);
        pages[1].push_back (c);
    }
}

void SpaceTexturePanel::layoutChildren()
{
    spaceBox.setBounds (16, 40, 200, 24);
    cc64Btn.setBounds (224, 40, 104, 24);
    placeRow ({ 16, 80, 312, kRowH }, { &mixDial, &decayDial, &dampDial, &shimmerDial }, 78);
    freezeBtn.setBounds (16, 184, 312, 40);

    assetBox.setBounds (88, 40, 240, 24);
    placeRow ({ 16, 72, 312, kRowH }, { &tuneDial, &keytrackDial, &texAttackDial, &texReleaseDial }, 78);
    placeRow ({ 16, 160, 312, kRowH }, { &velDial, &hpDial, &lpDial, &widthDial }, 78);
}

void SpaceTexturePanel::updateFreezeGlow (float e) noexcept
{
    if (std::abs (e - freezeGlow) > 0.01f)
    {
        freezeGlow = e;
        repaint (freezeBtn.getBounds().expanded (6));
    }
}

void SpaceTexturePanel::paintLive (juce::Graphics& g)
{
    if (activePage == 0 && freezeGlow > 0.02f)
    {
        g.setColour (palette::ivory.withAlpha (0.14f * freezeGlow));
        g.fillRoundedRectangle (freezeBtn.getBounds().toFloat().expanded (3.0f), 5.0f);
    }
    if (activePage == 1)
    {
        g.setColour (palette::stone);
        g.setFont (capsFont (10.0f));
        g.drawText ("SOURCE", juce::Rectangle<float> (16.0f, 40.0f, 68.0f, 24.0f), juce::Justification::centredLeft, false);
    }
}

// ============================================================================
OrbitStrip::OrbitStrip (MonolithAudioProcessor& p)
    : PanelBase ("ORGANIC ORBIT", false),
      rateDial (p.getAPVTS(), params::orbitRate, "RATE", "Organic vowel orbit speed (Hz, or bars when synced).", " Hz", 2, false, Style::inlineRow, "ivory"),
      depthDial (p.getAPVTS(), params::orbitDepth, "DEPTH", "Radial excursion across the vowel field.", "", 0, true, Style::inlineRow, "ivory")
{
    orbitBtn.setTooltip ("Continuous organic Lissajous drift across the vowel field.");
    orbitBtn.getProperties().set (MonolithLookAndFeel::accentPropertyId, "ivory");
    orbitBtn.getProperties().set ("monolithParamId", params::orbitEnable);
    orbitAttach = std::make_unique<ButtonAttach> (p.getAPVTS(), params::orbitEnable, orbitBtn);
    syncBtn.setTooltip ("Lock the orbit rate to host tempo bar divisions.");
    syncBtn.getProperties().set (MonolithLookAndFeel::accentPropertyId, "ivory");
    syncBtn.getProperties().set ("monolithParamId", params::orbitSync);
    syncAttach = std::make_unique<ButtonAttach> (p.getAPVTS(), params::orbitSync, syncBtn);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &orbitBtn, &syncBtn, &rateDial, &depthDial })
        addAndMakeVisible (c);
}

void OrbitStrip::layoutChildren()
{
    orbitBtn.setBounds (16, 44, 88, 28);
    syncBtn.setBounds (112, 44, 104, 28);
    rateDial.setBounds (232, 34, 104, 48);
    depthDial.setBounds (344, 34, 104, 48);
}

// ============================================================================
PerformanceDock::PerformanceDock (MonolithAudioProcessor& p)
    : PanelBase ("PERFORMANCE", false),
      proc (p),
      texMixDial (p.getAPVTS(), params::v2TexMix, "TEXTURE", "Texture layer mix (0 % = bypassed, zero latency).", "", 0, true, Style::standard, "ivory"),
      spaceMixDial (p.getAPVTS(), params::spaceMix, "SPACE", "Architectural reverb mix.", "", 0, true, Style::standard, "ivory"),
      exprBar (p.getAPVTS(), params::expressionLevel, [&p] (float v) { p.setUiExpression (v); }),
      glideDial (p.getAPVTS(), params::glideTime, "GLIDE", "Legato portamento time with vocal pitch-scoop.", " s", 2, false, Style::inlineRow),
      outDial (p.getAPVTS(), params::masterGain, "OUTPUT", "Master output trim (-18 to +6 dB) before the soft safety limiter.", " dB", 1, false, Style::inlineRow)
{
    static const char* const tips[] {
        "MACRO 1: per-preset multi-destination macro. Click the routing icon to edit names and routes.",
        "MACRO 2: per-preset multi-destination macro. Click the routing icon to edit names and routes.",
        "MACRO 3: per-preset multi-destination macro. Click the routing icon to edit names and routes.",
        "MACRO 4: per-preset multi-destination macro. Click the routing icon to edit names and routes." };
    for (int m = 0; m < 4; ++m)
    {
        macroDials[static_cast<size_t> (m)] = std::make_unique<MonolithDial> (
            p.getAPVTS(), v2::kMacroParamIds[static_cast<size_t> (m)], p.getControlPlane().getMacroLabel (m), tips[m], "", 0, true,
            Style::hero, m % 2 == 0 ? "amber" : "ivory", macroCaption (p.getControlPlane().getConfig(), m));
        addAndMakeVisible (*macroDials[static_cast<size_t> (m)]);
        routeButtons[static_cast<size_t> (m)] = std::make_unique<GlyphButton> ("route" + juce::String (m + 1), GlyphButton::Glyph::route, false);
        routeButtons[static_cast<size_t> (m)]->setTooltip ("Edit MACRO " + juce::String (m + 1) + " name and routing");
        routeButtons[static_cast<size_t> (m)]->onClick = [this, m] { if (onEditMacro) onEditMacro (m); };
        addAndMakeVisible (*routeButtons[static_cast<size_t> (m)]);
    }

    for (const auto& a : v2::kTextureAssets)
        assetBox.addItem (a.displayName, assetBox.getNumItems() + 1);
    assetBox.setTooltip ("Texture source");
    assetBox.getProperties().set ("monolithParamId", params::v2TexAsset);
    assetAttach = std::make_unique<ComboAttach> (p.getAPVTS(), params::v2TexAsset, assetBox);

    panicBtn.setTooltip ("Silence all voices, clear sustain / freeze and reset the reverb.");
    panicBtn.getProperties().set (MonolithLookAndFeel::accentPropertyId, "crimson");
    panicBtn.onClick = [&p] { p.triggerPanic(); };

    legatoBtn.setTooltip ("Monophonic vocal legato with portamento and pitch-scoop.");
    legatoBtn.getProperties().set ("monolithParamId", params::legatoEnable);
    legatoAttach = std::make_unique<ButtonAttach> (p.getAPVTS(), params::legatoEnable, legatoBtn);

    for (juce::Component* c : std::initializer_list<juce::Component*> { &texMixDial, &spaceMixDial, &assetBox, &panicBtn, &exprBar, &exprMeters, &legatoBtn, &glideDial, &outDial })
        addAndMakeVisible (c);
}

void PerformanceDock::renderChrome (juce::Graphics& g, juce::Rectangle<float> r)
{
    chrome::drawPlate (g, r, {}, false);
    chrome::drawCaption (g, { 16.0f, 4.0f, 300.0f, 28.0f }, "PERFORMANCE MACROS");
    chrome::drawCaption (g, { 624.0f, 4.0f, 200.0f, 28.0f }, "LAYERS");
    chrome::drawCaption (g, { 864.0f, 4.0f, 200.0f, 28.0f }, "PERFORM");
    chrome::drawDividerV (g, 608.0f, 16.0f, r.getBottom() - 16.0f);
    chrome::drawDividerV (g, 848.0f, 16.0f, r.getBottom() - 16.0f);
    // Engraved macro slot indices
    g.setFont (monoFont (9.0f));
    g.setColour (palette::ash);
    for (int m = 0; m < 4; ++m)
        g.drawText ("M" + juce::String (m + 1), juce::Rectangle<float> (16.0f + static_cast<float> (m) * 148.0f, 36.0f, 24.0f, 18.0f),
                    juce::Justification::centredLeft, false);
}

void PerformanceDock::layoutChildren()
{
    for (int m = 0; m < 4; ++m)
    {
        const int x = 16 + m * 148;
        macroDials[static_cast<size_t> (m)]->setBounds (x, 36, 140, 140);
        // Routing glyph sits on the value line, beside the destination caption it edits
        routeButtons[static_cast<size_t> (m)]->setBounds (x + 112, 145, 24, 18);
    }
    texMixDial.setBounds (624, 36, 104, 104);
    spaceMixDial.setBounds (736, 36, 104, 104);
    assetBox.setBounds (624, 148, 216, 24);

    panicBtn.setBounds (1088, 8, 64, 24);
    exprBar.setBounds (864, 40, 288, 32);
    exprMeters.setBounds (864, 84, 288, 24);
    legatoBtn.setBounds (864, 128, 80, 28);
    glideDial.setBounds (952, 118, 96, 48);
    outDial.setBounds (1056, 118, 96, 48);
}

void PerformanceDock::refresh (const TelemetrySnapshot& t)
{
    auto& cp = proc.getControlPlane();
    if (cp.getRevision() != lastRevision)
    {
        lastRevision = cp.getRevision();
        for (int m = 0; m < 4; ++m)
        {
            macroDials[static_cast<size_t> (m)]->setTitle (cp.getMacroLabel (m));
            macroDials[static_cast<size_t> (m)]->setCaption (macroCaption (cp.getConfig(), m));
        }
    }
    for (int m = 0; m < 4; ++m)
    {
        auto& d = *macroDials[static_cast<size_t> (m)];
        const float live = cp.getSourceValue (m);
        d.setModulation (std::abs (live - d.getNormalisedValue()) > 0.004f || live > 0.004f ? live : -1.0f);
    }
    exprBar.updateLive (t.liveExpression.load (std::memory_order_relaxed));
    const auto& ex = proc.getExpression();
    exprMeters.update (std::max (ex.uiPressure.load (std::memory_order_relaxed), cp.getSourceValue (static_cast<int> (v2::ModSource::pressure))),
                       slideForMeter (ex),
                       cp.getSourceValue (static_cast<int> (v2::ModSource::modWheel)),
                       cp.getSourceValue (static_cast<int> (v2::ModSource::breath)));
}

} // namespace monolith::ui
