#include "TopBar.h"
#include "OutputAnalyser.h"
#include "../State/ParameterIDs.h"

namespace monolith::ui
{

// ============================================================================
PresetDisplay::PresetDisplay()
{
    setTooltip ("Current preset. Click to open the Preset Browser (search, categories, favourites).");
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    star.setTooltip ("Favourite");
    star.getProperties().set (MonolithLookAndFeel::accentPropertyId, "amber");
    star.onClick = [this] { if (onToggleFavourite) onToggleFavourite(); };
    addAndMakeVisible (star);
}

void PresetDisplay::set (const juce::String& n, const juce::String& s, bool fav, bool canFav)
{
    star.setToggleState (fav, juce::dontSendNotification);
    star.setEnabled (canFav);
    if (n != name || s != sub)
    {
        name = n;
        sub = s;
        repaint();
    }
}

void PresetDisplay::resized()
{
    star.setBounds (4, (getHeight() - 24) / 2, 24, 24);
}

void PresetDisplay::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu() && e.mouseWasClicked() && onOpen)
        onOpen();
}

void PresetDisplay::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    chrome::drawWell (g, b, 4.0f);
    const bool hot = isMouseOver (true);
    if (hot)
    {
        g.setColour (palette::amber.withAlpha (0.35f));
        g.drawRoundedRectangle (b.reduced (0.5f), 4.0f, 1.0f);
    }
    auto text = b.withTrimmedLeft (32.0f).withTrimmedRight (32.0f);
    g.setColour (palette::ivory);
    g.setFont (uiFont (15.0f, Weight::demibold));
    g.drawText (name, text.withTrimmedBottom (b.getHeight() * 0.42f).withTrimmedTop (3.0f), juce::Justification::bottomLeft, true);
    g.setColour (palette::stone);
    g.setFont (capsFont (9.0f));
    g.drawText (sub, text.withTrimmedTop (b.getHeight() * 0.58f).withTrimmedBottom (4.0f), juce::Justification::topLeft, true);
    drawGlyph (g, GlyphButton::Glyph::browse, b.withTrimmedLeft (b.getWidth() - 32.0f), hot ? palette::amber : palette::stone);
}

// ============================================================================
MpeBadge::MpeBadge (MonolithAudioProcessor& p) : proc (p)
{
    setTooltip ("MPE: click to enable / disable (lower zone). Right-click for bend range. Glows while MPE data arrives.");
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    getProperties().set ("monolithParamId", params::v2MpeEnable);
    if (auto* prm = p.getAPVTS().getParameter (params::v2MpeEnable))
    {
        enableAttach = std::make_unique<juce::ParameterAttachment> (*prm, [this] (float v) { enabled = v > 0.5f; repaint(); });
        enableAttach->sendInitialUpdate();
    }
}

void MpeBadge::refresh()
{
    const bool a = proc.getExpression().uiMpeActive.load (std::memory_order_relaxed);
    const int br = juce::roundToInt (proc.getAPVTS().getRawParameterValue (params::v2MpePbRange)->load());
    if (a != active || br != bendRange)
    {
        active = a;
        bendRange = br;
        repaint();
    }
}

void MpeBadge::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mouseWasClicked())
        return;
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu m;
        m.addSectionHeader ("MPE bend range");
        for (int st : { 2, 12, 24, 48 })
            m.addItem (st, juce::String::fromUTF8 ("\xc2\xb1") + juce::String (st) + " semitones", true, bendRange == st);
        auto* prm = dynamic_cast<juce::RangedAudioParameter*> (proc.getAPVTS().getParameter (params::v2MpePbRange));
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [prm] (int r) {
            if (r > 0 && prm != nullptr)
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (prm->convertTo0to1 (static_cast<float> (r)));
                prm->endChangeGesture();
            }
        });
        return;
    }
    if (enableAttach)
        enableAttach->setValueAsCompleteGesture (enabled ? 0.0f : 1.0f);
}

void MpeBadge::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (active ? palette::amber.withAlpha (0.16f) : palette::panelRaised);
    g.fillRoundedRectangle (b, 3.0f);
    g.setColour (enabled ? palette::amber.withAlpha (active ? 0.95f : 0.55f) : palette::hairline);
    g.drawRoundedRectangle (b, 3.0f, 1.0f);
    const auto led = juce::Rectangle<float> (b.getX() + 8.0f, b.getCentreY() - 3.0f, 6.0f, 6.0f);
    if (active)
    {
        g.setColour (palette::amber.withAlpha (0.3f));
        g.fillEllipse (led.expanded (3.0f));
    }
    g.setColour (active ? palette::amber : (enabled ? palette::amberDeep : palette::graphite));
    g.fillEllipse (led);
    g.setColour (enabled ? palette::ivory : palette::stone);
    g.setFont (capsFont (10.5f));
    g.drawText ("MPE", b.withTrimmedLeft (18.0f), juce::Justification::centred, false);
}

// ============================================================================
TopBar::TopBar (MonolithAudioProcessor& p) : proc (p), mpe (p)
{
    prevBtn.setTooltip ("Previous preset");
    nextBtn.setTooltip ("Next preset");
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (1); };
    display.onOpen = [this] { if (onBrowse) onBrowse(); };
    display.onToggleFavourite = [this] {
        auto& lib = proc.getPresetLibrary();
        const int idx = lib.indexOfKey (proc.getCurrentPresetMeta().key());
        if (idx >= 0)
            lib.setFavourite (idx, ! lib.get (idx).favorite);
        refresh();
    };

    saveBtn.setTooltip ("Save the current sound as a user preset (V2 schema: name, author, category, tags, description, macro labels).");
    saveBtn.onClick = [this] { if (onSave) onSave(); };
    initBtn.setTooltip ("Load the neutral INIT patch (all parameters at default).");
    initBtn.onClick = [this] { proc.loadInitPatch(); refresh(); };

    aBtn.getProperties().set (MonolithLookAndFeel::segmentPropertyId, "left");
    bBtn.getProperties().set (MonolithLookAndFeel::segmentPropertyId, "right");
    aBtn.setTooltip ("A / B compare: slot A (click-free 60 ms glide)");
    bBtn.setTooltip ("A / B compare: slot B (click-free 60 ms glide)");
    aBtn.onClick = [this] { if (proc.getPresetManager().isSlotBActive()) proc.toggleABCompare(); refresh(); };
    bBtn.onClick = [this] { if (! proc.getPresetManager().isSlotBActive()) proc.toggleABCompare(); refresh(); };
    copyBtn.setTooltip ("Copy the active slot into the other slot (parameters + macro routing)");
    copyBtn.onClick = [this] { proc.copyActiveToInactiveAB(); };

    undoBtn.getProperties().set (MonolithLookAndFeel::segmentPropertyId, "left");
    redoBtn.getProperties().set (MonolithLookAndFeel::segmentPropertyId, "right");
    undoBtn.setTooltip ("Undo (parameter edits, preset loads, A/B, routing)");
    redoBtn.setTooltip ("Redo");
    undoBtn.onClick = [this] { proc.undo(); refresh(); };
    redoBtn.onClick = [this] { proc.redo(); refresh(); };

    zoomBtn.setTooltip ("Interface size (drag the bottom-right corner for free sizing, aspect locked)");
    zoomBtn.onClick = [this] {
        juce::PopupMenu m;
        m.addSectionHeader ("Interface size");
        for (int pct : { 75, 100, 125, 150 })
            m.addItem (pct, juce::String (pct) + "%", true, zoomBtn.getButtonText() == juce::String (pct) + "%");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&zoomBtn), [this] (int r) {
            if (r > 0 && onZoom)
                onZoom (static_cast<float> (r) / 100.0f);
        });
    };

    meter.onClipReset = [this] { if (onClipReset) onClipReset(); };

    for (juce::Component* c : std::initializer_list<juce::Component*> { &prevBtn, &nextBtn, &display, &saveBtn, &initBtn, &aBtn, &bBtn, &copyBtn,
                                                                        &undoBtn, &redoBtn, &mpe, &zoomBtn, &meter })
        addAndMakeVisible (c);
    refresh();
}

void TopBar::setZoomPercent (int pct)
{
    zoomBtn.setButtonText (juce::String (pct) + "%");
}

void TopBar::stepPreset (int delta)
{
    auto& lib = proc.getPresetLibrary();
    if (lib.size() <= 0)
        return;
    const int cur = lib.indexOfKey (proc.getCurrentPresetMeta().key());
    const int n = lib.size();
    const int next = cur < 0 ? (delta > 0 ? 0 : n - 1) : (cur + delta + n) % n;
    proc.loadPresetRecord (lib.get (next));
    refresh();
}

void TopBar::refresh()
{
    const auto& meta = proc.getCurrentPresetMeta();
    auto& lib = proc.getPresetLibrary();
    const int idx = lib.indexOfKey (meta.key());
    juce::String sub = meta.category.toUpperCase();
    if (meta.author.isNotEmpty())
        sub << juce::String::fromUTF8 ("  \xc2\xb7  ") << meta.author.toUpperCase();
    display.set (meta.name.isNotEmpty() ? meta.name : juce::String ("Untitled"), sub, idx >= 0 && lib.get (idx).favorite, idx >= 0);

    const bool b = proc.getPresetManager().isSlotBActive();
    aBtn.setToggleState (! b, juce::dontSendNotification);
    bBtn.setToggleState (b, juce::dontSendNotification);
    copyBtn.setButtonText (juce::String::fromUTF8 (b ? "B \xe2\x86\x92 A" : "A \xe2\x86\x92 B"));

    undoBtn.setEnabled (proc.getUndoHistory().canUndo());
    redoBtn.setEnabled (proc.getUndoHistory().canRedo());
    mpe.refresh();
}

void TopBar::resized()
{
    prevBtn.setBounds (216, 16, 28, 32);
    display.setBounds (248, 12, 272, 40);
    nextBtn.setBounds (524, 16, 28, 32);
    saveBtn.setBounds (560, 16, 48, 32);
    initBtn.setBounds (616, 16, 48, 32);

    aBtn.setBounds (696, 16, 32, 32);
    bBtn.setBounds (728, 16, 32, 32);
    copyBtn.setBounds (768, 16, 56, 32);

    undoBtn.setBounds (840, 16, 32, 32);
    redoBtn.setBounds (872, 16, 32, 32);

    mpe.setBounds (936, 16, 56, 32);
    zoomBtn.setBounds (1000, 16, 48, 32);
    meter.setBounds (1056, 12, 128, 40);
}

// ============================================================================
void ChassisBackground::paint (juce::Graphics& g)
{
    layer.draw (g, getLocalBounds(), [] (juce::Graphics& cg, juce::Rectangle<float> r) {
        chrome::drawChassis (cg, r, palette::chassis, 0x4D4E4C);

        // Top bar strip
        const auto bar = r.withHeight (64.0f);
        chrome::drawChassis (cg, bar, palette::chassisLift, 0x2B1);
        cg.setColour (juce::Colours::black.withAlpha (0.8f));
        cg.fillRect (bar.getX(), bar.getBottom() - 1.0f, bar.getWidth(), 1.0f);
        cg.setColour (palette::bevelLight);
        cg.fillRect (bar.getX(), bar.getBottom(), bar.getWidth(), 1.0f);

        // Emblem: three staggered obsidian slabs, centre slab lit
        const float ex = 16.0f, ey = 16.0f;
        cg.setColour (palette::hairlineHi);
        cg.fillRect (ex, ey + 8.0f, 5.0f, 24.0f);
        cg.setColour (palette::ivory);
        cg.fillRect (ex + 8.0f, ey, 7.0f, 32.0f);
        cg.setColour (palette::amber);
        cg.fillRect (ex + 18.0f, ey + 12.0f, 5.0f, 20.0f);

        // Wordmark
        cg.setColour (palette::ivory);
        cg.setFont (uiFont (20.0f, Weight::demibold, 0.16f));
        cg.drawText ("MONOLITH", juce::Rectangle<float> (ex + 34.0f, 13.0f, 140.0f, 24.0f), juce::Justification::centredLeft, false);
        const float wordW = juce::GlyphArrangement::getStringWidth (uiFont (20.0f, Weight::demibold, 0.16f), "MONOLITH");
        cg.setColour (palette::amber);
        cg.setFont (uiFont (20.0f, Weight::medium, 0.10f));
        cg.drawText ("II", juce::Rectangle<float> (ex + 34.0f + wordW + 6.0f, 13.0f, 30.0f, 24.0f), juce::Justification::centredLeft, false);
        cg.setColour (palette::stone);
        cg.setFont (capsFont (8.5f, Weight::medium));
        cg.drawText ("PHYSICAL CHORAL SWARM", juce::Rectangle<float> (ex + 35.0f, 36.0f, 150.0f, 12.0f), juce::Justification::centredLeft, false);

        chrome::drawDividerV (cg, 680.0f, 14.0f, 50.0f);
        chrome::drawDividerV (cg, 920.0f, 14.0f, 50.0f);
    });
}

} // namespace monolith::ui
