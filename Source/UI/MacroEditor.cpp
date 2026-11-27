#include "MacroEditor.h"
#include "UiText.h"
#include "../V2/ModMatrix.h"

namespace monolith::ui
{

namespace
{
    constexpr int kRowY = 112, kRowH = 40;
}

MacroEditorOverlay::MacroEditorOverlay (MonolithAudioProcessor& p) : proc (p)
{
    setWantsKeyboardFocus (true);

    labelEd.setFont (uiFont (15.0f, Weight::demibold));
    labelEd.setIndents (10, 7);
    labelEd.setInputRestrictions (16);
    labelEd.setTextToShowWhenEmpty ("MACRO NAME", palette::ash);
    labelEd.onTextChange = [this] {
        if (! loading)
            proc.getControlPlane().setMacroLabel (macro, labelEd.getText().toUpperCase().trim());
    };
    labelEd.onReturnKey = [this] { close(); };
    labelEd.onEscapeKey = [this] { close(); };
    addAndMakeVisible (labelEd);

    legacyBtn.setTooltip ("Keep the original MONOLITH V1 hard-wired curve for this macro (factory presets use it).");
    legacyBtn.onClick = [this] {
        if (! loading)
            proc.getControlPlane().setLegacyMacroCurve (macro, legacyBtn.getToggleState());
        repaint();
    };
    addAndMakeVisible (legacyBtn);

    const auto& apvts = proc.getAPVTS();
    for (int s = 0; s < static_cast<int> (rows.size()); ++s)
    {
        auto row = std::make_unique<Row>();
        row->dest.addItem (juce::String::fromUTF8 ("\xe2\x80\x94  unassigned"), 1);
        for (int d = 0; d < v2::kNumModDestinations; ++d)
        {
            juce::String full;
            if (auto* prm = apvts.getParameter (v2::kModDestinations[static_cast<size_t> (d)]))
                full = prm->getName (32);
            row->dest.addItem (destGroupName (d) + juce::String::fromUTF8 ("   \xc2\xb7   ") + full, d + 2);
        }
        row->dest.onChange = [this, s] { writeRow (s); };
        row->depth.setSliderStyle (juce::Slider::LinearHorizontal);
        row->depth.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        row->depth.setRange (-100.0, 100.0, 1.0);
        row->depth.setDoubleClickReturnValue (true, 0.0);
        row->depth.setTooltip ("Route depth (% of the destination range). Double-click = 0.");
        row->depth.getProperties().set (MonolithLookAndFeel::accentPropertyId, "amber");
        row->depth.onValueChange = [this, s] { writeRow (s); repaint(); };
        row->bipolar.setTooltip ("Bipolar: neutral at macro 50 %. Unipolar: neutral at 0 %.");
        row->bipolar.getProperties().set (MonolithLookAndFeel::accentPropertyId, "ivory");
        row->bipolar.onClick = [this, s] { writeRow (s); };
        row->clear.setTooltip ("Clear this route");
        row->clear.onClick = [this, s] {
            auto& r = *rows[static_cast<size_t> (s)];
            loading = true;
            r.dest.setSelectedId (1, juce::dontSendNotification);
            r.depth.setValue (0.0, juce::dontSendNotification);
            r.bipolar.setToggleState (false, juce::dontSendNotification);
            loading = false;
            writeRow (s);
            repaint();
        };
        for (juce::Component* c : std::initializer_list<juce::Component*> { &row->dest, &row->depth, &row->bipolar, &row->clear })
            addAndMakeVisible (c);
        rows[static_cast<size_t> (s)] = std::move (row);
    }

    clearAllBtn.getProperties().set (MonolithLookAndFeel::accentPropertyId, "crimson");
    clearAllBtn.onClick = [this] {
        proc.getControlPlane().clearRoutes (macro);
        loadFromControlPlane();
    };
    doneBtn.getProperties().set (MonolithLookAndFeel::accentPropertyId, "amber");
    doneBtn.setToggleState (true, juce::dontSendNotification);
    doneBtn.onClick = [this] { close(); };
    addAndMakeVisible (clearAllBtn);
    addAndMakeVisible (doneBtn);
    setVisible (false);
}

juce::Rectangle<int> MacroEditorOverlay::panelBounds() const
{
    return juce::Rectangle<int> (656, 528).withCentre (getLocalBounds().getCentre());
}

void MacroEditorOverlay::open (int m)
{
    macro = std::clamp (m, 0, v2::kNumMacros - 1);
    loadFromControlPlane();
    chromeLayer.invalidate();
    setVisible (true);
    toFront (true);
    labelEd.grabKeyboardFocus();
}

void MacroEditorOverlay::close()
{
    setVisible (false);
    proc.getUndoHistory().commit();
    if (onClosed)
        onClosed();
}

void MacroEditorOverlay::loadFromControlPlane()
{
    loading = true;
    auto& cp = proc.getControlPlane();
    labelEd.setText (cp.getMacroLabel (macro), false);
    legacyBtn.setToggleState (cp.getConfig().legacyMacroCurve[static_cast<size_t> (macro)], juce::dontSendNotification);
    const auto& routes = cp.getConfig().routes[static_cast<size_t> (macro)];
    for (size_t s = 0; s < rows.size(); ++s)
    {
        auto& r = *rows[s];
        const auto& route = routes[s];
        r.dest.setSelectedId (route.dest >= 0 ? route.dest + 2 : 1, juce::dontSendNotification);
        r.depth.setValue (static_cast<double> (route.depth) * 100.0, juce::dontSendNotification);
        r.bipolar.setToggleState (route.bipolar, juce::dontSendNotification);
    }
    loading = false;
    repaint();
}

void MacroEditorOverlay::writeRow (int slot)
{
    if (loading)
        return;
    auto& r = *rows[static_cast<size_t> (slot)];
    const int id = r.dest.getSelectedId();
    const juce::String destId = id >= 2 ? juce::String (v2::kModDestinations[static_cast<size_t> (id - 2)]) : juce::String();
    proc.getControlPlane().setRoute (macro, slot, destId, static_cast<float> (r.depth.getValue() / 100.0), r.bipolar.getToggleState());
}

bool MacroEditorOverlay::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey)
    {
        close();
        return true;
    }
    return false;
}

void MacroEditorOverlay::mouseDown (const juce::MouseEvent& e)
{
    if (! panelBounds().contains (e.getPosition()))
        close();
}

void MacroEditorOverlay::resized()
{
    chromeLayer.invalidate();
    const auto p = panelBounds();
    labelEd.setBounds (p.getX() + 128, p.getY() + 12, 264, 36);
    legacyBtn.setBounds (p.getRight() - 24 - 112, p.getY() + 16, 112, 28);
    for (size_t s = 0; s < rows.size(); ++s)
    {
        auto& r = *rows[s];
        const int y = p.getY() + kRowY + static_cast<int> (s) * kRowH;
        r.dest.setBounds (p.getX() + 56, y + 6, 256, 28);
        r.depth.setBounds (p.getX() + 328, y + 6, 136, 28);
        r.bipolar.setBounds (p.getX() + 520, y + 6, 80, 28);
        r.clear.setBounds (p.getX() + 608, y + 8, 24, 24);
    }
    clearAllBtn.setBounds (p.getX() + 24, p.getBottom() - 56, 136, 36);
    doneBtn.setBounds (p.getRight() - 24 - 120, p.getBottom() - 56, 120, 36);
}

void MacroEditorOverlay::paint (juce::Graphics& g)
{
    const auto p = panelBounds().toFloat();
    const int m = macro;
    chromeLayer.draw (g, getLocalBounds(), [p, m] (juce::Graphics& cg, juce::Rectangle<float> r) {
        cg.setColour (juce::Colours::black.withAlpha (0.62f));
        cg.fillRect (r);
        cg.setColour (juce::Colours::black.withAlpha (0.5f));
        cg.fillRoundedRectangle (p.translated (0.0f, 4.0f).expanded (2.0f), 6.0f);
        chrome::drawPlate (cg, p, {}, false);
        chrome::drawCaption (cg, { p.getX() + 24.0f, p.getY() + 10.0f, 100.0f, 40.0f }, "MACRO " + juce::String (m + 1));
        chrome::drawDividerH (cg, p.getY() + 60.0f, p.getX() + 1.0f, p.getRight() - 1.0f);
        cg.setColour (palette::ash);
        cg.setFont (capsFont (9.0f));
        cg.drawText ("#", juce::Rectangle<float> (p.getX() + 24.0f, p.getY() + 76.0f, 24.0f, 24.0f), juce::Justification::centredLeft, false);
        cg.drawText ("DESTINATION", juce::Rectangle<float> (p.getX() + 56.0f, p.getY() + 76.0f, 200.0f, 24.0f), juce::Justification::centredLeft, false);
        cg.drawText ("DEPTH", juce::Rectangle<float> (p.getX() + 328.0f, p.getY() + 76.0f, 100.0f, 24.0f), juce::Justification::centredLeft, false);
        cg.drawText ("MODE", juce::Rectangle<float> (p.getX() + 520.0f, p.getY() + 76.0f, 80.0f, 24.0f), juce::Justification::centredLeft, false);
        chrome::drawDividerH (cg, p.getY() + 104.0f, p.getX() + 24.0f, p.getRight() - 24.0f);
        cg.setFont (monoFont (10.0f));
        for (int s = 0; s < 8; ++s)
        {
            const float y = p.getY() + static_cast<float> (kRowY + s * kRowH);
            cg.setColour (palette::ash);
            cg.drawText (juce::String (s + 1), juce::Rectangle<float> (p.getX() + 24.0f, y + 6.0f, 24.0f, 28.0f), juce::Justification::centredLeft, false);
        }
        chrome::drawDividerH (cg, p.getBottom() - 72.0f, p.getX() + 1.0f, p.getRight() - 1.0f);
    });

    // Live depth readouts
    g.setFont (monoFont (11.0f));
    for (size_t s = 0; s < rows.size(); ++s)
    {
        const auto& r = *rows[s];
        const bool assigned = r.dest.getSelectedId() >= 2;
        const int v = juce::roundToInt (r.depth.getValue());
        g.setColour (assigned ? (v != 0 ? palette::amber : palette::ivory) : palette::ash);
        g.drawText ((v > 0 ? "+" : "") + juce::String (v) + "%",
                    juce::Rectangle<float> (p.getX() + 464.0f, p.getY() + static_cast<float> (kRowY) + static_cast<float> (s) * kRowH + 6.0f, 48.0f, 28.0f),
                    juce::Justification::centredRight, false);
    }
    g.setColour (palette::stone);
    g.setFont (uiFont (12.0f, Weight::medium));
    g.drawText (legacyBtn.getToggleState() ? "V1 curve active: routes add on top of the original macro behaviour."
                                           : "Routes only: the macro drives exactly the destinations listed.",
                juce::Rectangle<float> (p.getX() + 176.0f, p.getBottom() - 56.0f, p.getWidth() - 176.0f - 160.0f, 36.0f),
                juce::Justification::centredLeft, true);
}

} // namespace monolith::ui
