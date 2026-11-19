#include "MonolithLookAndFeel.h"
#include <algorithm>
#include <cmath>

namespace monolith::ui
{

const juce::Identifier MonolithLookAndFeel::accentPropertyId     ("monolith_accent");
const juce::Identifier MonolithLookAndFeel::macroStylePropertyId ("monolith_macro");

MonolithLookAndFeel::MonolithLookAndFeel()
{
    setColour (juce::TooltipWindow::backgroundColourId, palette::panelElevated);
    setColour (juce::TooltipWindow::textColourId, palette::boneWhite);
    setColour (juce::TooltipWindow::outlineColourId, palette::borderBright);
    setColour (juce::ComboBox::backgroundColourId, palette::panelElevated);
    setColour (juce::ComboBox::textColourId, palette::boneWhite);
    setColour (juce::ComboBox::outlineColourId, palette::gridHairline);
    setColour (juce::PopupMenu::backgroundColourId, palette::panelSlate);
    setColour (juce::PopupMenu::textColourId, palette::boneWhite);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, palette::panelElevated);
    setColour (juce::PopupMenu::highlightedTextColourId, palette::moltenAmber);
}

void MonolithLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                            int x, int y, int width, int height,
                                            float sliderPosProportional,
                                            float rotaryStartAngle,
                                            float rotaryEndAngle,
                                            juce::Slider& slider)
{
    const bool isMacro = static_cast<bool> (slider.getProperties().getWithDefault (macroStylePropertyId, false));
    const juce::String accentName = slider.getProperties().getWithDefault (accentPropertyId, "amber").toString();
    const juce::Colour accent = (accentName == "cyan") ? palette::paleCyan : palette::moltenAmber;

    const auto bounds = juce::Rectangle<float> (static_cast<float> (x),
                                                static_cast<float> (y),
                                                static_cast<float> (width),
                                                static_cast<float> (height)).reduced (isMacro ? 4.0f : 3.0f);

    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    if (radius <= 2.0f)
        return;

    const auto centre = bounds.getCentre();
    const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // Subtle outer tick marks
    const int numTicks = isMacro ? 15 : 11;
    for (int i = 0; i < numTicks; ++i)
    {
        const float t = static_cast<float> (i) / static_cast<float> (numTicks - 1);
        const float tickAngle = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
        const float rOuter = radius;
        const float rInner = radius - (isMacro ? 4.5f : 3.0f);
        const auto p1 = centre.getPointOnCircumference (rInner, tickAngle);
        const auto p2 = centre.getPointOnCircumference (rOuter, tickAngle);
        g.setColour ((t <= sliderPosProportional + 0.01f) ? accent.withAlpha (0.75f) : palette::dimAsh.withAlpha (0.55f));
        g.drawLine (p1.x, p1.y, p2.x, p2.y, isMacro ? 1.4f : 1.0f);
    }

    const float arcRadius = radius - (isMacro ? 7.5f : 5.5f);
    const float strokeW   = isMacro ? 3.2f : 2.3f;

    // Background track arc
    juce::Path bgArc;
    bgArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (palette::gridHairline.brighter (0.15f));
    g.strokePath (bgArc, juce::PathStrokeType (strokeW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Active value arc + soft glow
    if (sliderPosProportional > 0.002f)
    {
        juce::Path valArc;
        valArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                              rotaryStartAngle, angle, true);
        g.setColour (accent.withAlpha (0.22f));
        g.strokePath (valArc, juce::PathStrokeType (strokeW + 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (accent);
        g.strokePath (valArc, juce::PathStrokeType (strokeW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Brutalist machined dial cap
    const float capRadius = arcRadius - (isMacro ? 6.0f : 4.5f);
    const auto capBounds = juce::Rectangle<float> (capRadius * 2.0f, capRadius * 2.0f).withCentre (centre);
    juce::ColourGradient capGrad (palette::panelElevated.brighter (0.12f), centre.x, capBounds.getY(),
                                  palette::obsidianBg, centre.x, capBounds.getBottom(), false);
    g.setGradientFill (capGrad);
    g.fillEllipse (capBounds);

    g.setColour (slider.isMouseOverOrDragging() ? palette::borderBright.brighter (0.35f) : palette::gridHairline);
    g.drawEllipse (capBounds, 1.1f);

    // Center indicator pointer line
    const auto ptrStart = centre.getPointOnCircumference (capRadius * 0.25f, angle);
    const auto ptrEnd   = centre.getPointOnCircumference (capRadius * 0.86f, angle);
    g.setColour (palette::boneWhite);
    g.drawLine (ptrStart.x, ptrStart.y, ptrEnd.x, ptrEnd.y, isMacro ? 2.2f : 1.7f);
}

void MonolithLookAndFeel::drawToggleButton (juce::Graphics& g,
                                            juce::ToggleButton& button,
                                            bool shouldDrawButtonAsHighlighted,
                                            bool shouldDrawButtonAsDown)
{
    (void) shouldDrawButtonAsDown;
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = button.getToggleState();

    const juce::String accentName = button.getProperties().getWithDefault (accentPropertyId, "amber").toString();
    const juce::Colour accent = (accentName == "crimson")
        ? palette::crimsonAlert
        : ((accentName == "cyan") ? palette::paleCyan : palette::moltenAmber);

    const auto bgCol = on ? accent.withAlpha (0.16f)
                          : (shouldDrawButtonAsHighlighted ? palette::panelElevated.brighter (0.12f) : palette::panelElevated);
    const auto borderCol = on ? accent.withAlpha (0.85f)
                              : (shouldDrawButtonAsHighlighted ? palette::borderBright : palette::gridHairline);

    g.setColour (bgCol);
    g.fillRoundedRectangle (bounds, 2.5f);
    g.setColour (borderCol);
    g.drawRoundedRectangle (bounds, 2.5f, 1.0f);

    // Small status LED bar on left
    const auto ledBounds = juce::Rectangle<float> (bounds.getX() + 6.0f, bounds.getCentreY() - 3.0f, 3.0f, 6.0f);
    g.setColour (on ? accent : palette::dimAsh);
    g.fillRect (ledBounds);

    g.setColour (on ? palette::boneWhite : palette::mutedLimestone);
    g.setFont (makeFont (std::clamp (bounds.getHeight() * 0.44f, 9.5f, 12.5f), true, false));
    g.drawText (button.getButtonText().toUpperCase(),
                bounds.withTrimmedLeft (13.0f).withTrimmedRight (4.0f),
                juce::Justification::centredLeft,
                true);
}

void MonolithLookAndFeel::drawButtonBackground (juce::Graphics& g,
                                                juce::Button& button,
                                                const juce::Colour& backgroundColour,
                                                bool shouldDrawButtonAsHighlighted,
                                                bool shouldDrawButtonAsDown)
{
    (void) backgroundColour;
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool isActive = button.getToggleState();
    const juce::String accentName = button.getProperties().getWithDefault (accentPropertyId, "amber").toString();
    const juce::Colour accent = (accentName == "crimson")
        ? palette::crimsonAlert
        : ((accentName == "cyan") ? palette::paleCyan : palette::moltenAmber);

    juce::Colour bg = palette::panelElevated;
    if (isActive)
        bg = accent.withAlpha (0.20f);
    else if (shouldDrawButtonAsDown)
        bg = palette::panelElevated.brighter (0.22f);
    else if (shouldDrawButtonAsHighlighted)
        bg = palette::panelElevated.brighter (0.10f);

    g.setColour (bg);
    g.fillRoundedRectangle (bounds, 2.5f);

    g.setColour (isActive ? accent : (shouldDrawButtonAsHighlighted ? palette::borderBright : palette::gridHairline));
    g.drawRoundedRectangle (bounds, 2.5f, 1.0f);
}

void MonolithLookAndFeel::drawButtonText (juce::Graphics& g,
                                          juce::TextButton& button,
                                          bool shouldDrawButtonAsHighlighted,
                                          bool shouldDrawButtonAsDown)
{
    (void) shouldDrawButtonAsDown;
    const auto bounds = button.getLocalBounds().toFloat();
    const bool isActive = button.getToggleState();
    const juce::String accentName = button.getProperties().getWithDefault (accentPropertyId, "amber").toString();

    juce::Colour textCol = isActive ? palette::boneWhite
                                    : (shouldDrawButtonAsHighlighted ? palette::boneWhite : palette::mutedLimestone);
    if (accentName == "crimson" && shouldDrawButtonAsHighlighted)
        textCol = palette::crimsonAlert.brighter (0.25f);

    g.setColour (textCol);
    g.setFont (makeFont (std::clamp (bounds.getHeight() * 0.44f, 9.5f, 12.5f), true, false));
    g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (3, 1), juce::Justification::centred, 1);
}

void MonolithLookAndFeel::drawComboBox (juce::Graphics& g,
                                        int width, int height,
                                        bool isButtonDown,
                                        int buttonX, int buttonY, int buttonW, int buttonH,
                                        juce::ComboBox& box)
{
    (void) isButtonDown;
    (void) buttonX;
    (void) buttonY;
    (void) buttonW;
    (void) buttonH;

    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height)).reduced (0.5f);
    g.setColour (palette::panelElevated);
    g.fillRoundedRectangle (bounds, 2.5f);

    g.setColour (box.isMouseOver() ? palette::borderBright : palette::gridHairline);
    g.drawRoundedRectangle (bounds, 2.5f, 1.0f);

    // Architectural chevron arrow
    const float cx = static_cast<float> (width) - 12.0f;
    const float cy = static_cast<float> (height) * 0.5f;
    juce::Path arrow;
    arrow.startNewSubPath (cx - 3.5f, cy - 1.8f);
    arrow.lineTo (cx, cy + 2.2f);
    arrow.lineTo (cx + 3.5f, cy - 1.8f);
    g.setColour (palette::mutedLimestone);
    g.strokePath (arrow, juce::PathStrokeType (1.3f));
}

void MonolithLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (palette::panelSlate);
    g.setColour (palette::borderBright);
    g.drawRect (0, 0, width, height, 1);
}

void MonolithLookAndFeel::drawPopupMenuItem (juce::Graphics& g,
                                             const juce::Rectangle<int>& area,
                                             bool isSeparator,
                                             bool isActive,
                                             bool isHighlighted,
                                             bool isTicked,
                                             bool hasSubMenu,
                                             const juce::String& text,
                                             const juce::String& shortcutKeyText,
                                             const juce::Drawable* icon,
                                             const juce::Colour* textColourToUse)
{
    (void) hasSubMenu;
    (void) shortcutKeyText;
    (void) icon;
    (void) textColourToUse;

    if (isSeparator)
    {
        g.setColour (palette::gridHairline);
        g.fillRect (area.reduced (6, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }

    if (isHighlighted && isActive)
    {
        g.setColour (palette::panelElevated.brighter (0.15f));
        g.fillRect (area.reduced (2, 1));
    }

    if (isTicked)
    {
        g.setColour (palette::moltenAmber);
        g.fillRect (area.getX() + 4, area.getY() + 4, 2, area.getHeight() - 8);
    }

    g.setColour (! isActive ? palette::dimAsh
                           : (isHighlighted ? palette::moltenAmber : palette::boneWhite));
    g.setFont (makeFont (12.0f, isTicked, false));
    g.drawFittedText (text, area.reduced (12, 2), juce::Justification::centredLeft, 1);
}

juce::Font MonolithLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return makeFont (std::clamp (static_cast<float> (box.getHeight()) * 0.44f, 10.5f, 12.5f), false, false);
}

juce::Font MonolithLookAndFeel::getPopupMenuFont()
{
    return makeFont (12.0f, false, false);
}

void MonolithLookAndFeel::drawTooltip (juce::Graphics& g,
                                       const juce::String& text,
                                       int width,
                                       int height)
{
    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height));
    g.setColour (palette::panelElevated);
    g.fillRect (bounds);
    g.setColour (palette::borderBright);
    g.drawRect (bounds, 1.0f);

    g.setColour (palette::boneWhite);
    g.setFont (makeFont (11.0f, false, false));
    g.drawFittedText (text, juce::Rectangle<int> (width, height).reduced (6, 4), juce::Justification::centredLeft, 3);
}

} // namespace monolith::ui
