#include "MonolithLookAndFeel.h"
#include <algorithm>
#include <cmath>

namespace monolith::ui
{

const juce::Identifier MonolithLookAndFeel::accentPropertyId     ("monolith_accent");
const juce::Identifier MonolithLookAndFeel::macroStylePropertyId ("monolith_macro");
const juce::Identifier MonolithLookAndFeel::segmentPropertyId    ("monolith_segment");

// Platform typography: Avenir Next / Menlo on macOS, Segoe UI / Consolas on Windows, and the fontconfig default
// sans / mono on Linux. Weights map to the nearest style each family actually ships.
juce::Font uiFont (float heightPx, Weight w, float tracking)
{
   #if JUCE_MAC || JUCE_IOS
    static const juce::String family ("Avenir Next");
    static const char* const styles[] { "Regular", "Medium", "Demi Bold", "Bold" };
   #elif JUCE_WINDOWS
    static const juce::String family ("Segoe UI");
    static const char* const styles[] { "Regular", "Semibold", "Semibold", "Bold" };
   #else
    static const juce::String family (juce::Font::getDefaultSansSerifFontName());
    static const char* const styles[] { "Regular", "Regular", "Bold", "Bold" };
   #endif
    auto opts = juce::FontOptions (family, styles[static_cast<int> (w)], heightPx);
   #if ! (JUCE_MAC || JUCE_IOS || JUCE_WINDOWS)
    opts = opts.withHorizontalScale (0.86f); // DejaVu / Noto Sans run ~10-15 % wider than Avenir Next: keep labels unclipped
   #endif
    if (tracking != 0.0f)
        opts = opts.withKerningFactor (tracking);
    return juce::Font (opts);
}

juce::Font monoFont (float heightPx)
{
   #if JUCE_MAC || JUCE_IOS
    return juce::Font (juce::FontOptions ("Menlo", "Regular", heightPx));
   #elif JUCE_WINDOWS
    return juce::Font (juce::FontOptions ("Consolas", "Regular", heightPx));
   #else
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), "Regular", heightPx));
   #endif
}

juce::Colour accentOf (const juce::Component& c, juce::Colour fallback)
{
    const auto name = c.getProperties().getWithDefault (MonolithLookAndFeel::accentPropertyId, "").toString();
    if (name == "ivory")   return palette::ivory;
    if (name == "crimson") return palette::crimson;
    if (name == "amber")   return palette::amber;
    return fallback;
}

// ============================================================================
namespace chrome
{
void drawChassis (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour base, int seed)
{
    g.setColour (base);
    g.fillRect (area);

    // Brushed grain: 1 px horizontal strokes of slowly varying luminance (deterministic, sub-3 % contrast)
    juce::Random rng (seed);
    for (float y = area.getY(); y < area.getBottom(); y += 1.0f)
    {
        const float a = 0.006f + 0.016f * rng.nextFloat();
        g.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (a));
        const float x0 = area.getX() + rng.nextFloat() * area.getWidth() * 0.35f;
        const float len = area.getWidth() * (0.45f + 0.55f * rng.nextFloat());
        g.fillRect (x0, y, std::min (len, area.getRight() - x0), 1.0f);
    }

    // Matte light falloff from the top edge (one low-contrast wash, no banding)
    juce::ColourGradient wash (juce::Colours::white.withAlpha (0.018f), area.getCentreX(), area.getY(),
                               juce::Colours::transparentWhite, area.getCentreX(), area.getY() + area.getHeight() * 0.55f, false);
    g.setGradientFill (wash);
    g.fillRect (area);
}

void drawPlate (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, bool headerStrip)
{
    constexpr float radius = 4.0f;
    // Drop shadow under the plate
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.5f), radius);

    g.setColour (palette::panel);
    g.fillRoundedRectangle (r, radius);

    // Subtle matte depth: 2 % top lift
    juce::ColourGradient lift (juce::Colours::white.withAlpha (0.022f), r.getX(), r.getY(),
                               juce::Colours::transparentWhite, r.getX(), r.getY() + 48.0f, false);
    g.setGradientFill (lift);
    g.fillRoundedRectangle (r, radius);

    g.setColour (palette::hairline);
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    g.setColour (palette::bevelLight);
    g.drawHorizontalLine (juce::roundToInt (r.getY() + 1.0f), r.getX() + radius, r.getRight() - radius);

    if (headerStrip)
    {
        const float hy = r.getY() + 32.0f;
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRect (r.getX() + 1.0f, hy, r.getWidth() - 2.0f, 1.0f);
        g.setColour (palette::bevelLight);
        g.fillRect (r.getX() + 1.0f, hy + 1.0f, r.getWidth() - 2.0f, 1.0f);
        if (title.isNotEmpty())
            drawCaption (g, { r.getX() + 16.0f, r.getY(), r.getWidth() - 32.0f, 32.0f }, title);
    }
}

void drawWell (juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    g.setColour (palette::well);
    g.fillRoundedRectangle (r, radius);
    // Inner top shadow
    juce::ColourGradient shade (juce::Colours::black.withAlpha (0.55f), r.getX(), r.getY(),
                                juce::Colours::transparentBlack, r.getX(), r.getY() + 6.0f, false);
    g.setGradientFill (shade);
    g.fillRoundedRectangle (r, radius);
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    g.setColour (palette::bevelLight);
    g.drawHorizontalLine (juce::roundToInt (r.getBottom()), r.getX() + radius, r.getRight() - radius);
}

void drawDividerV (juce::Graphics& g, float x, float y0, float y1)
{
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRect (x, y0, 1.0f, y1 - y0);
    g.setColour (palette::bevelLight);
    g.fillRect (x + 1.0f, y0, 1.0f, y1 - y0);
}

void drawDividerH (juce::Graphics& g, float y, float x0, float x1)
{
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRect (x0, y, x1 - x0, 1.0f);
    g.setColour (palette::bevelLight);
    g.fillRect (x0, y + 1.0f, x1 - x0, 1.0f);
}

void drawCaption (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, bool notch)
{
    float x = r.getX();
    if (notch)
    {
        g.setColour (palette::amber);
        g.fillRect (x, r.getCentreY() - 4.0f, 2.0f, 8.0f);
        x += 10.0f;
    }
    g.setColour (palette::ivory.withAlpha (0.82f));
    g.setFont (capsFont (11.0f));
    g.drawText (text, juce::Rectangle<float> (x, r.getY(), r.getRight() - x, r.getHeight()), juce::Justification::centredLeft, false);
}
} // namespace chrome

// ============================================================================
MonolithLookAndFeel::MonolithLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, palette::chassis);
    setColour (juce::TooltipWindow::backgroundColourId, palette::panelRaised);
    setColour (juce::TooltipWindow::textColourId, palette::ivory);
    setColour (juce::TooltipWindow::outlineColourId, palette::hairlineHi);
    setColour (juce::ComboBox::backgroundColourId, palette::well);
    setColour (juce::ComboBox::textColourId, palette::ivory);
    setColour (juce::ComboBox::outlineColourId, palette::hairline);
    setColour (juce::ComboBox::arrowColourId, palette::stone);
    setColour (juce::PopupMenu::backgroundColourId, palette::panelRaised);
    setColour (juce::PopupMenu::textColourId, palette::ivory);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, palette::panel);
    setColour (juce::PopupMenu::highlightedTextColourId, palette::amber);
    setColour (juce::TextEditor::backgroundColourId, palette::well);
    setColour (juce::TextEditor::textColourId, palette::ivory);
    setColour (juce::TextEditor::highlightColourId, palette::amber.withAlpha (0.30f));
    setColour (juce::TextEditor::highlightedTextColourId, palette::ivory);
    setColour (juce::TextEditor::outlineColourId, palette::hairline);
    setColour (juce::TextEditor::focusedOutlineColourId, palette::amber.withAlpha (0.70f));
    setColour (juce::CaretComponent::caretColourId, palette::amber);
    setColour (juce::Label::textColourId, palette::ivory);
    setColour (juce::Label::textWhenEditingColourId, palette::ivory);
    setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ScrollBar::thumbColourId, palette::hairlineHi);
    setColour (juce::Slider::thumbColourId, palette::ivory);
    setColour (juce::Slider::trackColourId, palette::amber);
    setColour (juce::Slider::backgroundColourId, palette::well);
    setColour (juce::TextButton::buttonColourId, palette::panelRaised);
    setColour (juce::TextButton::textColourOffId, palette::stone);
    setColour (juce::TextButton::textColourOnId, palette::ivory);
}

void MonolithLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                                bool highlighted, bool down)
{
    const auto seg = button.getProperties().getWithDefault (segmentPropertyId, "").toString();
    const auto accent = accentOf (button);
    const bool on = button.getToggleState();
    auto r = button.getLocalBounds().toFloat();

    if (seg == "tab")
    {
        // Panel header tab: text only, amber underline when selected
        if (on)
        {
            g.setColour (accent);
            g.fillRect (r.getX() + 2.0f, r.getBottom() - 2.0f, r.getWidth() - 4.0f, 2.0f);
        }
        else if (highlighted)
        {
            g.setColour (palette::hairlineHi);
            g.fillRect (r.getX() + 2.0f, r.getBottom() - 1.0f, r.getWidth() - 4.0f, 1.0f);
        }
        return;
    }

    r = r.reduced (0.5f);
    const float radius = 3.0f;
    juce::Path shape;
    const bool roundL = seg.isEmpty() || seg == "left";
    const bool roundR = seg.isEmpty() || seg == "right";
    shape.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), radius, radius,
                               roundL, roundR, roundL, roundR);

    juce::Colour fill = palette::panelRaised;
    if (on)               fill = palette::panelRaised.interpolatedWith (accent, 0.10f);
    else if (down)        fill = palette::panelRaised.brighter (0.18f);
    else if (highlighted) fill = palette::panelRaised.brighter (0.08f);
    g.setColour (fill);
    g.fillPath (shape);

    // Keycap catch light + base shadow
    if (! down)
    {
        g.setColour (palette::bevelLight.withMultipliedAlpha (1.4f));
        g.drawHorizontalLine (juce::roundToInt (r.getY() + 1.0f), r.getX() + (roundL ? radius : 0.0f), r.getRight() - (roundR ? radius : 0.0f));
    }
    g.setColour (on ? accent.withAlpha (0.55f) : (highlighted ? palette::hairlineHi : palette::hairline));
    g.strokePath (shape, juce::PathStrokeType (1.0f));
}

void MonolithLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool highlighted, bool)
{
    const bool on = button.getToggleState();
    const auto accent = accentOf (button);
    const bool isTab = button.getProperties().getWithDefault (segmentPropertyId, "").toString() == "tab";
    juce::Colour c = on ? (isTab ? palette::ivory : accent) : (highlighted ? palette::ivory : palette::stone);
    if (! button.isEnabled())
        c = palette::ash.withAlpha (0.6f);
    g.setColour (c);
    g.setFont (capsFont (isTab ? 11.0f : 10.5f));
    g.drawText (button.getButtonText(), button.getLocalBounds().reduced (4, 0), juce::Justification::centred, false);
}

void MonolithLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool)
{
    const bool on = button.getToggleState();
    const auto accent = accentOf (button);
    const auto r = button.getLocalBounds().toFloat().reduced (0.5f);

    g.setColour (on ? palette::panelRaised.interpolatedWith (accent, 0.08f)
                    : (highlighted ? palette::panelRaised.brighter (0.06f) : palette::panelRaised));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (palette::bevelLight.withMultipliedAlpha (1.4f));
    g.drawHorizontalLine (juce::roundToInt (r.getY() + 1.0f), r.getX() + 3.0f, r.getRight() - 3.0f);
    g.setColour (on ? accent.withAlpha (0.50f) : (highlighted ? palette::hairlineHi : palette::hairline));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);

    // LED
    const auto led = juce::Rectangle<float> (r.getX() + 8.0f, r.getCentreY() - 3.0f, 6.0f, 6.0f);
    if (on)
    {
        g.setColour (accent.withAlpha (0.28f));
        g.fillEllipse (led.expanded (3.0f));
    }
    g.setColour (on ? accent : palette::graphite);
    g.fillEllipse (led);

    g.setColour (on ? palette::ivory : (highlighted ? palette::ivory.withAlpha (0.85f) : palette::stone));
    g.setFont (capsFont (10.5f));
    g.drawText (button.getButtonText(), r.withTrimmedLeft (22.0f).withTrimmedRight (6.0f), juce::Justification::centredLeft, false);
}

void MonolithLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height)).reduced (0.5f);
    g.setColour (palette::well);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (box.isMouseOver (true) ? palette::hairlineHi : palette::hairline);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    g.setColour (palette::bevelLight);
    g.drawHorizontalLine (juce::roundToInt (r.getBottom()), r.getX() + 3.0f, r.getRight() - 3.0f);

    const float cx = static_cast<float> (width) - 12.0f;
    const float cy = static_cast<float> (height) * 0.5f;
    juce::Path chevron;
    chevron.startNewSubPath (cx - 3.5f, cy - 1.5f);
    chevron.lineTo (cx, cy + 2.0f);
    chevron.lineTo (cx + 3.5f, cy - 1.5f);
    g.setColour (box.isMouseOver (true) ? palette::amber : palette::stone);
    g.strokePath (chevron, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void MonolithLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (4, 0, box.getWidth() - 26, box.getHeight());
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

juce::Font MonolithLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return uiFont (std::clamp (static_cast<float> (box.getHeight()) * 0.50f, 11.0f, 13.0f), Weight::medium);
}

void MonolithLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (palette::panelRaised);
    g.setColour (palette::hairlineHi);
    g.drawRect (0, 0, width, height, 1);
}

void MonolithLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                                             bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                                             const juce::String&, const juce::Drawable*, const juce::Colour*)
{
    if (isSeparator)
    {
        g.setColour (palette::hairline);
        g.fillRect (area.reduced (8, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }
    if (isHighlighted && isActive)
    {
        g.setColour (palette::panel);
        g.fillRect (area.reduced (2, 1));
    }
    if (isTicked)
    {
        g.setColour (palette::amber);
        g.fillEllipse (static_cast<float> (area.getX() + 9), static_cast<float> (area.getCentreY() - 3), 6.0f, 6.0f);
    }
    g.setColour (! isActive ? palette::ash : (isHighlighted ? palette::amber : palette::ivory));
    g.setFont (getPopupMenuFont());
    g.drawFittedText (text, area.withTrimmedLeft (24).withTrimmedRight (12), juce::Justification::centredLeft, 1);
    if (hasSubMenu)
    {
        const float cx = static_cast<float> (area.getRight() - 12), cy = static_cast<float> (area.getCentreY());
        juce::Path p;
        p.startNewSubPath (cx - 2.0f, cy - 3.5f);
        p.lineTo (cx + 1.5f, cy);
        p.lineTo (cx - 2.0f, cy + 3.5f);
        g.strokePath (p, juce::PathStrokeType (1.2f));
    }
}

void MonolithLookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& sectionName)
{
    g.setColour (palette::stone);
    g.setFont (capsFont (10.0f));
    g.drawFittedText (sectionName.toUpperCase(), area.withTrimmedLeft (12).withTrimmedRight (8), juce::Justification::bottomLeft, 1);
}

juce::Font MonolithLookAndFeel::getPopupMenuFont()
{
    return uiFont (13.0f, Weight::medium);
}

void MonolithLookAndFeel::getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int, int& idealWidth, int& idealHeight)
{
    if (isSeparator)
    {
        idealWidth = 60;
        idealHeight = 9;
        return;
    }
    idealHeight = 26;
    idealWidth = juce::GlyphArrangement::getStringWidthInt (getPopupMenuFont(), text) + 48;
}

void MonolithLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor&)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height));
    g.setColour (palette::well);
    g.fillRoundedRectangle (r, 3.0f);
}

void MonolithLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& ed)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height)).reduced (0.5f);
    g.setColour (ed.hasKeyboardFocus (true) ? palette::amber.withAlpha (0.65f) : palette::hairline);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
}

void MonolithLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float, float,
                                            juce::Slider::SliderStyle, juce::Slider& slider)
{
    // Bipolar depth bar (centre = 0) used by the macro routing editor
    const auto r = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y) + static_cast<float> (height) * 0.5f - 3.0f,
                                           static_cast<float> (width), 6.0f);
    chrome::drawWell (g, r, 3.0f);
    const float range = static_cast<float> (slider.getMaximum() - slider.getMinimum());
    const float zeroPos = static_cast<float> (x) + static_cast<float> (width) * static_cast<float> ((0.0 - slider.getMinimum()) / std::max (1.0e-6f, range));
    const float a = std::min (zeroPos, sliderPos), b = std::max (zeroPos, sliderPos);
    if (b - a > 0.5f)
    {
        g.setColour (accentOf (slider).withAlpha (0.9f));
        g.fillRect (juce::Rectangle<float> (a, r.getY() + 1.0f, b - a, r.getHeight() - 2.0f));
    }
    g.setColour (palette::hairlineHi);
    g.fillRect (zeroPos - 0.5f, r.getY() - 3.0f, 1.0f, r.getHeight() + 6.0f);
    g.setColour (slider.isMouseOverOrDragging() ? palette::amber : palette::ivory);
    g.fillRoundedRectangle (juce::Rectangle<float> (sliderPos - 2.0f, r.getY() - 4.0f, 4.0f, r.getHeight() + 8.0f), 1.5f);
}

void MonolithLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int width, int height, bool vertical,
                                         int thumbStart, int thumbSize, bool isMouseOver, bool isMouseDown)
{
    juce::Rectangle<float> thumb;
    if (vertical) thumb = { static_cast<float> (x) + 2.0f, static_cast<float> (thumbStart), static_cast<float> (width) - 4.0f, static_cast<float> (thumbSize) };
    else          thumb = { static_cast<float> (thumbStart), static_cast<float> (y) + 2.0f, static_cast<float> (thumbSize), static_cast<float> (height) - 4.0f };
    g.setColour ((isMouseOver || isMouseDown) ? palette::stone.withAlpha (0.6f) : palette::hairlineHi);
    g.fillRoundedRectangle (thumb, 2.0f);
}

void MonolithLookAndFeel::drawCornerResizer (juce::Graphics& g, int w, int h, bool isMouseOver, bool isMouseDragging)
{
    g.setColour ((isMouseOver || isMouseDragging) ? palette::amber : palette::ash);
    for (int i = 0; i < 3; ++i)
    {
        const float o = 4.0f + static_cast<float> (i) * 4.0f;
        g.drawLine (static_cast<float> (w) - o, static_cast<float> (h) - 2.0f, static_cast<float> (w) - 2.0f, static_cast<float> (h) - o, 1.0f);
    }
}

juce::Rectangle<int> MonolithLookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea)
{
    const auto font = uiFont (12.0f, Weight::medium);
    const int w = std::min (320, juce::GlyphArrangement::getStringWidthInt (font, tipText) + 24);
    const int lines = std::max (1, (juce::GlyphArrangement::getStringWidthInt (font, tipText) + 24) / 320 + 1);
    const int h = 10 + lines * 16;
    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
        .constrainedWithin (parentArea);
}

void MonolithLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height));
    g.setColour (palette::panelRaised);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (palette::hairlineHi);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    g.setColour (palette::ivory.withAlpha (0.92f));
    g.setFont (uiFont (12.0f, Weight::medium));
    g.drawFittedText (text, juce::Rectangle<int> (width, height).reduced (12, 5), juce::Justification::centredLeft, 4);
}

juce::Font MonolithLookAndFeel::getLabelFont (juce::Label& label)
{
    return uiFont (std::clamp (static_cast<float> (label.getHeight()) * 0.55f, 11.0f, 14.0f), Weight::medium);
}

} // namespace monolith::ui
