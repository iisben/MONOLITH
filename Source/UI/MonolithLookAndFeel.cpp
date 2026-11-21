#include "MonolithLookAndFeel.h"
#include <algorithm>
#include <cmath>

namespace monolith::ui
{

const juce::Identifier MonolithLookAndFeel::accentPropertyId     ("monolith_accent");
const juce::Identifier MonolithLookAndFeel::macroStylePropertyId ("monolith_macro");

// ============================================================================
// Type
// ============================================================================
namespace
{
    struct Families
    {
        juce::String label, inscription, numeric;
    };

    const Families& families()
    {
        static const Families f = []
        {
            Families r;
           #if JUCE_MAC
            r.label       = "Avenir Next";
            r.inscription = "Optima";       // drawn from carved Roman inscriptions
            r.numeric     = "Menlo";
           #elif JUCE_WINDOWS
            r.label       = "Segoe UI";
            r.inscription = "Palatino Linotype";
            r.numeric     = "Consolas";
           #else
            r.label       = juce::Font::getDefaultSansSerifFontName();
            r.inscription = juce::Font::getDefaultSerifFontName();
            r.numeric     = juce::Font::getDefaultMonospacedFontName();
           #endif
            // Fall back gracefully if a family is missing on this machine
            const auto available = juce::Font::findAllTypefaceNames();
            if (! available.contains (r.label))       r.label       = juce::Font::getDefaultSansSerifFontName();
            if (! available.contains (r.inscription)) r.inscription = juce::Font::getDefaultSerifFontName();
            if (! available.contains (r.numeric))     r.numeric     = juce::Font::getDefaultMonospacedFontName();
            return r;
        }();
        return f;
    }
} // namespace

juce::Font makeRoleFont (FontRole role, float heightPx, bool bold)
{
    const auto& f = families();
    const auto& family = role == FontRole::Inscription ? f.inscription
                       : role == FontRole::Numeric     ? f.numeric
                                                       : f.label;
    return juce::Font (juce::FontOptions (family, heightPx, bold ? juce::Font::bold : juce::Font::plain));
}

juce::Font makeFont (float heightPx, bool bold, bool mono)
{
    return makeRoleFont (mono ? FontRole::Numeric : FontRole::Label, heightPx, bold);
}

void drawTracked (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                  juce::Justification just, float trackingEm)
{
    g.setFont (g.getCurrentFont().withExtraKerningFactor (trackingEm));
    g.drawText (text, area, just, false);
}

juce::Colour accentFromName (const juce::String& name) noexcept
{
    if (name == "cyan" || name == "verdigris") return palette::paleCyan;
    if (name == "crimson" || name == "oxide")  return palette::crimsonAlert;
    return palette::moltenAmber;
}

// ============================================================================
// Materials
// ============================================================================
void ConcreteSurface::draw (juce::Graphics& g, juce::Rectangle<int> area, uint32_t seed, juce::Colour base)
{
    const int w = area.getWidth(), h = area.getHeight();
    if (w <= 0 || h <= 0)
        return;

    if (cache.isNull() || cache.getWidth() != w || cache.getHeight() != h || cachedSeed != seed || cachedBase != base)
    {
        cache = juce::Image (juce::Image::ARGB, w, h, true);
        cachedSeed = seed;
        cachedBase = base;

        juce::Graphics ig (cache);
        juce::Random rng (static_cast<juce::int64> (seed) * 7919 + 17);

        // Board-formed planks: each pour against a slightly different board
        int y = 0;
        while (y < h)
        {
            const int bh = 10 + rng.nextInt (7);
            const float tone = (rng.nextFloat() - 0.5f) * 0.07f;
            ig.setColour (base.withMultipliedBrightness (1.0f + tone));
            ig.fillRect (0, y, w, bh);

            // Wood-grain imprint: long faint horizontal streaks
            const int streaks = std::max (1, (w * bh) / 150);
            for (int s = 0; s < streaks; ++s)
            {
                const bool light = rng.nextBool();
                ig.setColour ((light ? juce::Colours::white : juce::Colours::black)
                                  .withAlpha (light ? 0.018f + 0.02f * rng.nextFloat() : 0.05f + 0.05f * rng.nextFloat()));
                ig.fillRect (rng.nextInt (w), y + rng.nextInt (std::max (1, bh)), 8 + rng.nextInt (60), 1);
            }

            // Plank seam: shadowed joint with a lit lip below
            if (y > 0)
            {
                ig.setColour (juce::Colours::black.withAlpha (0.30f));
                ig.fillRect (0, y, w, 1);
                ig.setColour (juce::Colours::white.withAlpha (0.035f));
                ig.fillRect (0, y + 1, w, 1);
            }
            y += bh;
        }

        // Aggregate pores and the occasional bug hole
        const int pores = (w * h) / 260;
        for (int p = 0; p < pores; ++p)
        {
            ig.setColour (juce::Colours::black.withAlpha (0.16f + 0.30f * rng.nextFloat()));
            ig.fillRect (rng.nextInt (w), rng.nextInt (h), 1 + (rng.nextInt (5) == 0 ? 1 : 0), 1);
        }
        const int holes = (w * h) / 9000;
        for (int p = 0; p < holes; ++p)
        {
            const float px = rng.nextFloat() * (float) w, py = rng.nextFloat() * (float) h;
            const float r = 0.9f + 1.1f * rng.nextFloat();
            ig.setColour (juce::Colours::black.withAlpha (0.45f));
            ig.fillEllipse (px - r, py - r, r * 2.0f, r * 2.0f);
            ig.setColour (juce::Colours::white.withAlpha (0.05f));
            ig.drawEllipse (px - r, py - r + 0.6f, r * 2.0f, r * 2.0f, 0.5f);
        }
    }

    g.drawImageAt (cache, area.getX(), area.getY());
}

void drawFormTie (juce::Graphics& g, juce::Point<float> c, float r)
{
    // Conical void: dark core, lit lower rim, faint shadow ring above
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (c.x - r * 0.55f, c.y - r * 0.55f, r * 1.1f, r * 1.1f);
    juce::Path lip;
    lip.addCentredArc (c.x, c.y, r, r, 0.0f, juce::MathConstants<float>::pi * 0.6f, juce::MathConstants<float>::pi * 1.4f, true);
    g.setColour (palette::edgeLight.withAlpha (0.9f));
    g.strokePath (lip, juce::PathStrokeType (0.8f));
}

void drawIncisedHorizontal (juce::Graphics& g, float y, float x1, float x2)
{
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRect (x1, y, x2 - x1, 1.0f);
    g.setColour (palette::edgeLight.withAlpha (0.55f));
    g.fillRect (x1, y + 1.0f, x2 - x1, 1.0f);
}

void drawIncisedVertical (juce::Graphics& g, float x, float y1, float y2)
{
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRect (x, y1, 1.0f, y2 - y1);
    g.setColour (palette::edgeLight.withAlpha (0.45f));
    g.fillRect (x + 1.0f, y1, 1.0f, y2 - y1);
}

void drawRecess (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fill)
{
    g.setColour (fill);
    g.fillRect (r);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRect (r.getX(), r.getY(), r.getWidth(), 1.0f);
    g.fillRect (r.getX(), r.getY(), 1.0f, r.getHeight());
    g.setColour (palette::edgeLight.withAlpha (0.65f));
    g.fillRect (r.getX(), r.getBottom() - 1.0f, r.getWidth(), 1.0f);
    g.fillRect (r.getRight() - 1.0f, r.getY(), 1.0f, r.getHeight());
}

/**
    A cast slab: board-formed concrete face, a shadow line where it meets the floor,
    a form tie in each corner and its title cut into the top like an inscription.
*/
void drawSlabPanel (juce::Graphics& g,
                      ConcreteSurface& surface,
                      juce::Rectangle<int> bounds,
                      uint32_t seed,
                      const juce::String& numeral,
                      const juce::String& title,
                      juce::Colour accent)
{
    surface.draw (g, bounds, seed, palette::panelSlate);

    const auto b = bounds.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.75f));
    g.drawRect (b, 1.0f);
    g.setColour (palette::edgeLight.withAlpha (0.8f));
    g.fillRect (b.getX() + 1.0f, b.getY() + 1.0f, b.getWidth() - 2.0f, 1.0f);

    const float inset = 8.5f;
    for (auto pt : { juce::Point<float> (b.getX() + inset, b.getY() + inset),
                     juce::Point<float> (b.getRight() - inset, b.getY() + inset),
                     juce::Point<float> (b.getX() + inset, b.getBottom() - inset),
                     juce::Point<float> (b.getRight() - inset, b.getBottom() - inset) })
        drawFormTie (g, pt, 2.6f);

    auto header = juce::Rectangle<float> (b.getX() + 20.0f, b.getY() + 6.0f, b.getWidth() - 40.0f, 18.0f);
    g.setColour (accent);
    g.setFont (makeRoleFont (FontRole::Inscription, 14.0f, true));
    g.drawText (numeral, header.removeFromLeft (30.0f), juce::Justification::centredLeft, false);

    g.setColour (palette::boneWhite.withAlpha (0.92f));
    g.setFont (makeRoleFont (FontRole::Inscription, 11.5f, true));
    drawTracked (g, title, header, juce::Justification::centredLeft, 0.20f);

    drawIncisedHorizontal (g, b.getY() + 27.0f, b.getX() + 14.0f, b.getRight() - 14.0f);
}

// ============================================================================
MonolithLookAndFeel::MonolithLookAndFeel()
{
    setColour (juce::TooltipWindow::backgroundColourId, palette::recess);
    setColour (juce::TooltipWindow::textColourId, palette::boneWhite);
    setColour (juce::TooltipWindow::outlineColourId, palette::borderBright);
    setColour (juce::ComboBox::backgroundColourId, palette::panelElevated);
    setColour (juce::ComboBox::textColourId, palette::boneWhite);
    setColour (juce::ComboBox::outlineColourId, palette::gridHairline);
    setColour (juce::PopupMenu::backgroundColourId, palette::panelSlate);
    setColour (juce::PopupMenu::textColourId, palette::boneWhite);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, palette::panelElevated);
    setColour (juce::PopupMenu::highlightedTextColourId, palette::moltenAmber);
    setColour (juce::PopupMenu::headerTextColourId, palette::moltenAmber);
}

// ── Rotary: a turned stone dial with an engraved scale and a cut notch ───────
void MonolithLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                            int x, int y, int width, int height,
                                            float pos,
                                            float startAngle,
                                            float endAngle,
                                            juce::Slider& slider)
{
    const juce::Colour accent = accentFromName (slider.getProperties().getWithDefault (accentPropertyId, "amber").toString());
    const bool live = slider.isMouseOverOrDragging();

    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (2.0f);
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    if (radius <= 6.0f)
        return;

    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    if (radius < 17.0f)
    {
        // Compact dial (header trim etc.): inlay + body + notch, no engraved scale
        const float bandR = radius - 1.5f;
        juce::Path track, val;
        track.addCentredArc (c.x, c.y, bandR, bandR, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.strokePath (track, juce::PathStrokeType (2.2f));
        val.addCentredArc (c.x, c.y, bandR, bandR, 0.0f, startAngle, angle, true);
        g.setColour (accent);
        g.strokePath (val, juce::PathStrokeType (1.8f));
        const float bodyR = bandR - 3.5f;
        g.setColour (live ? palette::panelElevated.brighter (0.1f) : palette::panelElevated);
        g.fillEllipse (juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (c));
        const auto n1 = c.getPointOnCircumference (bodyR * 0.2f, angle);
        const auto n2 = c.getPointOnCircumference (bodyR * 0.95f, angle);
        g.setColour (palette::boneWhite);
        g.drawLine (n1.x, n1.y, n2.x, n2.y, 1.6f);
        return;
    }

    // Engraved scale: 21 marks, every fifth one longer
    for (int i = 0; i <= 20; ++i)
    {
        const float t  = (float) i / 20.0f;
        const float a  = startAngle + t * (endAngle - startAngle);
        const bool major = (i % 5) == 0;
        const auto p1 = c.getPointOnCircumference (radius - (major ? 4.0f : 2.2f), a);
        const auto p2 = c.getPointOnCircumference (radius, a);
        g.setColour (major ? palette::mutedLimestone.withAlpha (0.75f) : palette::dimAsh.withAlpha (0.8f));
        g.drawLine (p1.x, p1.y, p2.x, p2.y, major ? 1.1f : 0.8f);
    }

    // Value band: a thin gilt/verdigris inlay running inside the scale
    const float bandR = radius - 6.5f;
    juce::Path track;
    track.addCentredArc (c.x, c.y, bandR, bandR, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.strokePath (track, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    if (pos > 0.001f)
    {
        juce::Path val;
        val.addCentredArc (c.x, c.y, bandR, bandR, 0.0f, startAngle, angle, true);
        g.setColour (live ? accent.brighter (0.15f) : accent);
        g.strokePath (val, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    }

    // The dial body: flat turned stone, lit from above
    const float bodyR = bandR - 4.5f;
    const auto body = juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (c);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (body.translated (0.0f, 1.2f));
    g.setColour (live ? palette::panelElevated.brighter (0.08f) : palette::panelElevated);
    g.fillEllipse (body);
    juce::Path topLip;
    topLip.addCentredArc (c.x, c.y, bodyR - 0.5f, bodyR - 0.5f, 0.0f,
                          -juce::MathConstants<float>::pi * 0.42f, juce::MathConstants<float>::pi * 0.42f, true);
    g.setColour (palette::edgeLight.brighter (0.25f));
    g.strokePath (topLip, juce::PathStrokeType (0.9f));

    // Cut notch
    const auto n1 = c.getPointOnCircumference (bodyR * 0.30f, angle);
    const auto n2 = c.getPointOnCircumference (bodyR * 0.92f, angle);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawLine (n1.x + 0.6f, n1.y + 0.8f, n2.x + 0.6f, n2.y + 0.8f, 2.4f);
    g.setColour (live ? juce::Colours::white : palette::boneWhite);
    g.drawLine (n1.x, n1.y, n2.x, n2.y, 1.8f);
}

// ── Linear (macro slabs): a standing stone that rises from its plinth ───────
void MonolithLookAndFeel::drawLinearSlider (juce::Graphics& g,
                                            int x, int y, int width, int height,
                                            float sliderPos, float minSliderPos, float maxSliderPos,
                                            juce::Slider::SliderStyle style,
                                            juce::Slider& slider)
{
    const bool isMacro = static_cast<bool> (slider.getProperties().getWithDefault (macroStylePropertyId, false));
    if (! isMacro || ! slider.isVertical())
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        return;
    }

    const juce::Colour accent = accentFromName (slider.getProperties().getWithDefault (accentPropertyId, "amber").toString());
    const bool live = slider.isMouseOverOrDragging();

    // 1 : 4 : 9 - the proportions of the monolith
    const float h = (float) height;
    const float w = std::min ((float) width, h * 4.0f / 9.0f);
    const auto slab = juce::Rectangle<float> ((float) x + 12.0f, (float) y, w, h);

    // The cavity the stone rises from
    drawRecess (g, slab, palette::recess);

    // Scale: incised ticks every 10 % on both flanks
    for (int i = 1; i < 10; ++i)
    {
        const float ty = slab.getBottom() - slab.getHeight() * (float) i / 10.0f;
        const float len = (i == 5) ? 7.0f : 4.0f;
        g.setColour (palette::dimAsh);
        g.fillRect (slab.getX() - len - 3.0f, ty, len, 1.0f);
        g.fillRect (slab.getRight() + 3.0f, ty, len, 1.0f);
    }

    // The risen stone
    (void) sliderPos;
    const float prop = (float) slider.valueToProportionOfLength (slider.getValue());
    // At rest the stone still shows a low course, so the slot never reads as an empty hole
    const float topY = std::min (slab.getBottom() - 7.0f,
                                 slab.getBottom() - std::clamp (prop, 0.0f, 1.0f) * slab.getHeight());
    {
        const auto stone = juce::Rectangle<float> (slab.getX() + 2.0f, topY, slab.getWidth() - 4.0f, slab.getBottom() - topY - 2.0f);
        g.setColour (live ? palette::panelElevated.brighter (0.22f) : palette::panelElevated.brighter (0.12f));
        g.fillRect (stone);
        // strata
        g.setColour (juce::Colours::black.withAlpha (0.22f));
        for (float sy = stone.getBottom() - 6.0f; sy > stone.getY() + 3.0f; sy -= 6.0f)
            g.fillRect (stone.getX(), sy, stone.getWidth(), 1.0f);
        // gilded capstone
        g.setColour (prop > 0.005f ? accent : accent.withAlpha (0.45f));
        g.fillRect (stone.getX(), stone.getY(), stone.getWidth(), 2.0f);
        g.setColour (accent.withAlpha (0.18f));
        g.fillRect (stone.getX(), stone.getY() + 2.0f, stone.getWidth(), 3.0f);
    }
}

// ── Toggles: an inset square that is gilded when engaged ─────────────────────
void MonolithLookAndFeel::drawToggleButton (juce::Graphics& g,
                                            juce::ToggleButton& button,
                                            bool highlighted,
                                            bool down)
{
    (void) down;
    const auto b = button.getLocalBounds().toFloat();
    const bool on = button.getToggleState();
    const juce::Colour accent = accentFromName (button.getProperties().getWithDefault (accentPropertyId, "amber").toString());

    g.setColour (highlighted ? palette::panelElevated.brighter (0.06f) : palette::panelElevated);
    g.fillRect (b);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawRect (b, 1.0f);
    g.setColour (palette::edgeLight);
    g.fillRect (b.getX() + 1.0f, b.getY() + 1.0f, b.getWidth() - 2.0f, 1.0f);

    const float s = std::min (8.0f, b.getHeight() - 8.0f);
    const auto box = juce::Rectangle<float> (b.getX() + 7.0f, b.getCentreY() - s * 0.5f, s, s);
    drawRecess (g, box, palette::recess);
    if (on)
    {
        g.setColour (accent);
        g.fillRect (box.reduced (1.5f));
    }

    g.setColour (on ? palette::boneWhite : (highlighted ? palette::boneWhite.withAlpha (0.85f) : palette::mutedLimestone));
    g.setFont (makeFont (std::clamp (b.getHeight() * 0.42f, 9.0f, 12.0f), true, false));
    drawTracked (g, button.getButtonText().toUpperCase(), b.withTrimmedLeft (s + 13.0f).withTrimmedRight (4.0f),
                 juce::Justification::centredLeft, 0.08f);
}

void MonolithLookAndFeel::drawButtonBackground (juce::Graphics& g,
                                                juce::Button& button,
                                                const juce::Colour&,
                                                bool highlighted,
                                                bool down)
{
    const auto b = button.getLocalBounds().toFloat();
    const bool active = button.getToggleState();
    const juce::Colour accent = accentFromName (button.getProperties().getWithDefault (accentPropertyId, "amber").toString());

    if (down)
        drawRecess (g, b, palette::recess.brighter (0.15f));
    else
    {
        g.setColour (highlighted ? palette::panelElevated.brighter (0.07f) : palette::panelElevated);
        g.fillRect (b);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawRect (b, 1.0f);
        g.setColour (palette::edgeLight);
        g.fillRect (b.getX() + 1.0f, b.getY() + 1.0f, b.getWidth() - 2.0f, 1.0f);
    }
    if (active)
    {
        g.setColour (accent);
        g.fillRect (b.getX() + 1.0f, b.getBottom() - 3.0f, b.getWidth() - 2.0f, 2.0f);
    }
}

void MonolithLookAndFeel::drawButtonText (juce::Graphics& g,
                                          juce::TextButton& button,
                                          bool highlighted,
                                          bool down)
{
    (void) down;
    const auto accentName = button.getProperties().getWithDefault (accentPropertyId, "amber").toString();
    juce::Colour col = (highlighted || button.getToggleState()) ? palette::boneWhite : palette::mutedLimestone;
    if ((accentName == "crimson" || accentName == "oxide") && highlighted)
        col = palette::crimsonAlert.brighter (0.3f);

    g.setColour (col);
    g.setFont (makeFont (std::clamp ((float) button.getHeight() * 0.40f, 9.0f, 12.0f), true, false));
    drawTracked (g, button.getButtonText(), button.getLocalBounds().toFloat().reduced (3.0f, 1.0f),
                 juce::Justification::centred, 0.10f);
}

// ── Combo: a flat raised block with a cut chevron ───────────────────────────
void MonolithLookAndFeel::drawComboBox (juce::Graphics& g,
                                        int width, int height,
                                        bool down,
                                        int, int, int, int,
                                        juce::ComboBox& box)
{
    const auto b = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    if (down)
        drawRecess (g, b, palette::recess.brighter (0.12f));
    else
    {
        g.setColour (box.isMouseOver (true) ? palette::panelElevated.brighter (0.07f) : palette::panelElevated);
        g.fillRect (b);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawRect (b, 1.0f);
        g.setColour (palette::edgeLight);
        g.fillRect (1.0f, 1.0f, b.getWidth() - 2.0f, 1.0f);
    }

    const float cx = (float) width - 13.0f, cy = (float) height * 0.5f;
    drawIncisedVertical (g, (float) width - 25.0f, 5.0f, (float) height - 5.0f);
    juce::Path chevron;
    chevron.addTriangle (cx - 3.5f, cy - 1.5f, cx + 3.5f, cy - 1.5f, cx, cy + 2.5f);
    g.setColour (box.isMouseOver (true) ? palette::moltenAmber : palette::mutedLimestone);
    g.fillPath (chevron);
}

void MonolithLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 32, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void MonolithLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (palette::panelSlate);
    g.setColour (juce::Colours::black);
    g.drawRect (0, 0, width, height, 1);
    g.setColour (palette::edgeLight);
    g.fillRect (1, 1, width - 2, 1);
}

void MonolithLookAndFeel::drawPopupMenuItem (juce::Graphics& g,
                                             const juce::Rectangle<int>& area,
                                             bool isSeparator,
                                             bool isActive,
                                             bool isHighlighted,
                                             bool isTicked,
                                             bool, const juce::String& text, const juce::String&,
                                             const juce::Drawable*, const juce::Colour*)
{
    if (isSeparator)
    {
        drawIncisedHorizontal (g, (float) area.getCentreY(), (float) area.getX() + 8.0f, (float) area.getRight() - 8.0f);
        return;
    }

    if (isHighlighted && isActive)
    {
        g.setColour (palette::panelElevated.brighter (0.08f));
        g.fillRect (area.reduced (2, 0));
    }
    if (isTicked)
    {
        g.setColour (palette::moltenAmber);
        g.fillRect (area.getX() + 6, area.getCentreY() - 3, 6, 6);
    }

    g.setColour (! isActive ? palette::dimAsh : (isHighlighted || isTicked ? palette::boneWhite : palette::boneWhite.withAlpha (0.82f)));
    g.setFont (makeFont (12.5f, isTicked, false));
    g.drawFittedText (text, area.reduced (20, 0), juce::Justification::centredLeft, 1);
}

void MonolithLookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g,
                                                      const juce::Rectangle<int>& area,
                                                      const juce::String& sectionName)
{
    const auto r = area.toFloat().reduced (10.0f, 0.0f);
    g.setColour (palette::moltenAmber);
    g.setFont (makeFont (9.5f, true, false));
    drawTracked (g, sectionName, r.withTrimmedTop (6.0f), juce::Justification::centredLeft, 0.22f);
    drawIncisedHorizontal (g, r.getBottom() - 2.0f, r.getX(), r.getRight());
}

juce::Font MonolithLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return makeFont (std::clamp ((float) box.getHeight() * 0.48f, 10.5f, 13.0f), false, false);
}

juce::Font MonolithLookAndFeel::getPopupMenuFont()
{
    return makeFont (12.5f, false, false);
}

void MonolithLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto b = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    g.setColour (palette::recess);
    g.fillRect (b);
    g.setColour (palette::borderBright);
    g.drawRect (b, 1.0f);
    g.setColour (palette::moltenAmber);
    g.fillRect (0.0f, 0.0f, 2.0f, b.getHeight());

    g.setColour (palette::boneWhite);
    g.setFont (makeFont (11.5f, false, false));
    g.drawFittedText (text, juce::Rectangle<int> (width, height).reduced (9, 4), juce::Justification::centredLeft, 3);
}

} // namespace monolith::ui
