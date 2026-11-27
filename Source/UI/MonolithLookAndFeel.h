#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace monolith::ui
{

/** MONOLITH II palette: obsidian / charcoal chassis, hairline bevels, molten amber + pale ivory light. */
namespace palette
{
    inline const juce::Colour chassis      { 0xFF0A0B0D }; // deepest body
    inline const juce::Colour chassisLift  { 0xFF0F1013 }; // top bar / dock strip
    inline const juce::Colour panel        { 0xFF131417 }; // section plates
    inline const juce::Colour panelRaised  { 0xFF1A1C20 }; // keycaps, raised controls
    inline const juce::Colour well         { 0xFF08090B }; // inset displays
    inline const juce::Colour hairline     { 0xFF24262B };
    inline const juce::Colour hairlineHi   { 0xFF34373E };
    inline const juce::Colour bevelLight   { 0x0FFFFFFF }; // top-edge catch light
    inline const juce::Colour bevelDark    { 0x8C000000 }; // bottom-edge shadow
    inline const juce::Colour ivory        { 0xFFEDE7DA }; // primary text / pointer / ivory arcs
    inline const juce::Colour stone        { 0xFF9C988E }; // secondary text
    inline const juce::Colour ash          { 0xFF62646B }; // tertiary text, inactive ticks
    inline const juce::Colour graphite     { 0xFF3A3C42 }; // tracks
    inline const juce::Colour amber        { 0xFFF0A14A }; // molten amber light
    inline const juce::Colour amberDeep    { 0xFFB8692A };
    inline const juce::Colour crimson      { 0xFFE2553F };

    // V1 names kept for any external reference
    inline const juce::Colour obsidianBg   = chassis;
    inline const juce::Colour boneWhite    = ivory;
} // namespace palette

/** 8 px layout grid. */
namespace grid
{
    inline constexpr int u = 8;
    [[nodiscard]] constexpr int g (int units) noexcept { return units * u; }
}

enum class Weight { regular = 0, medium, demibold, bold };

/** Geometric typography (Avenir Next; JUCE falls back to the platform sans where it is absent). */
[[nodiscard]] juce::Font uiFont (float heightPx, Weight w = Weight::medium, float tracking = 0.0f);
/** Fixed-width numerals for meters and live readouts. */
[[nodiscard]] juce::Font monoFont (float heightPx);
/** Spaced small caps for section titles and control captions. */
[[nodiscard]] inline juce::Font capsFont (float heightPx, Weight w = Weight::demibold) { return uiFont (heightPx, w, 0.09f); }

/** V1 helper signature kept for compatibility. */
[[nodiscard]] inline juce::Font makeFont (float heightPx, bool bold = false, bool mono = false)
{
    return mono ? monoFont (heightPx) : uiFont (heightPx, bold ? Weight::demibold : Weight::medium);
}

/** Static chrome painters (used inside CachedLayer renders). */
namespace chrome
{
    /** Brushed / matte chassis body with a deterministic fine grain. */
    void drawChassis (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour base, int seed);
    /** Raised section plate: hairline frame, top catch light, bottom shadow, optional title strip. */
    void drawPlate (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, bool headerStrip = true);
    /** Recessed display well (inner shadow on the top edge, catch light on the bottom edge). */
    void drawWell (juce::Graphics& g, juce::Rectangle<float> r, float radius = 3.0f);
    /** Hairline divider with a 1 px catch light beneath. */
    void drawDividerV (juce::Graphics& g, float x, float y0, float y1);
    void drawDividerH (juce::Graphics& g, float y, float x0, float x1);
    /** Section caption with a 2 px amber notch. */
    void drawCaption (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, bool notch = true);
}

/** MONOLITH II LookAndFeel: no default JUCE widget styling survives. */
class MonolithLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    MonolithLookAndFeel();

    static const juce::Identifier accentPropertyId;     // "amber" | "ivory" | "crimson"
    static const juce::Identifier macroStylePropertyId; // kept for V1 compatibility
    static const juce::Identifier segmentPropertyId;    // "left" | "mid" | "right" | "tab"

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;

    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                            bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText, const juce::Drawable* icon,
                            const juce::Colour* textColourToUse) override;
    void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>& area, const juce::String& sectionName) override;
    juce::Font getPopupMenuFont() override;
    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int standardMenuItemHeight,
                                    int& idealWidth, int& idealHeight) override;

    void fillTextEditorBackground (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                           float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;

    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height, bool isScrollbarVertical,
                        int thumbStartPosition, int thumbSize, bool isMouseOver, bool isMouseDown) override;
    int getDefaultScrollbarWidth() override { return 8; }

    void drawCornerResizer (juce::Graphics&, int w, int h, bool isMouseOver, bool isMouseDragging) override;

    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;

    juce::Font getLabelFont (juce::Label&) override;
};

[[nodiscard]] juce::Colour accentOf (const juce::Component& c, juce::Colour fallback = palette::amber);

} // namespace monolith::ui
