#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace monolith::ui
{

/**
    Material palette: cast concrete, carved limestone inscriptions, gilt and verdigris.
    No glows, no gradients-as-decoration: surfaces are matte, light comes from above,
    and the only saturated colours are the two metals and oxide red for danger.
*/
namespace palette
{
    // Concrete
    inline const juce::Colour obsidianBg     { 0xFF151513 }; // poured floor / editor ground
    inline const juce::Colour panelSlate     { 0xFF1E1D1B }; // cast slab face
    inline const juce::Colour panelElevated  { 0xFF262522 }; // raised slab / control face
    inline const juce::Colour recess         { 0xFF0F0F0E }; // cut into the slab (fields, meters)
    inline const juce::Colour gridHairline   { 0xFF34322E }; // incised line (dark part)
    inline const juce::Colour edgeLight      { 0xFF3B3934 }; // sunlit top edge of an incision
    inline const juce::Colour borderBright   { 0xFF4A4741 };

    // Inscription
    inline const juce::Colour boneWhite      { 0xFFD8D1C2 }; // limestone
    inline const juce::Colour mutedLimestone { 0xFF928C7F };
    inline const juce::Colour dimAsh         { 0xFF5D5950 };

    // Metals
    inline const juce::Colour moltenAmber    { 0xFFC9A44E }; // gilt (energy, tone, voice)
    inline const juce::Colour amberGlow      { 0x33C9A44E };
    inline const juce::Colour paleCyan       { 0xFF86A596 }; // verdigris (space, time, motion)
    inline const juce::Colour cyanGlow       { 0x3386A596 };
    inline const juce::Colour crimsonAlert   { 0xFFB4482F }; // oxide red (clip, panic)
} // namespace palette

enum class FontRole
{
    Label,        // tracked uppercase control labels
    Inscription,  // carved titles / wordmark
    Numeric       // tabular readouts
};

[[nodiscard]] juce::Font makeFont (float heightPx, bool bold = false, bool mono = false);
[[nodiscard]] juce::Font makeRoleFont (FontRole role, float heightPx, bool bold = false);

/** Draws text with explicit letter-spacing (inscriptions are tracked wide). */
void drawTracked (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                  juce::Justification just, float trackingEm);

/** Accent colour by property name ("amber"/"gilt", "cyan"/"verdigris", "crimson"/"oxide"). */
[[nodiscard]] juce::Colour accentFromName (const juce::String& name) noexcept;

/**
    Board-formed concrete (beton brut): horizontal plank seams, grain streaks and
    aggregate pores, generated deterministically from a seed and cached per size.
*/
class ConcreteSurface
{
public:
    void draw (juce::Graphics& g, juce::Rectangle<int> area, uint32_t seed, juce::Colour base);

private:
    juce::Image cache;
    uint32_t cachedSeed = 0;
    juce::Colour cachedBase;
};

/** The small round voids left by form ties in cast concrete. */
void drawFormTie (juce::Graphics& g, juce::Point<float> centre, float radius = 3.2f);

/** An incised line: dark cut with a sunlit lower lip. */
void drawIncisedHorizontal (juce::Graphics& g, float y, float x1, float x2);
void drawIncisedVertical (juce::Graphics& g, float x, float y1, float y2);

/** A rectangle cut into the slab (shadow on top/left, light on bottom/right). */
void drawRecess (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fill = palette::recess);

/** A cast slab panel: concrete face, corner form ties, Roman numeral + inscribed title. */
void drawSlabPanel (juce::Graphics& g, ConcreteSurface& surface, juce::Rectangle<int> bounds, uint32_t seed,
                    const juce::String& numeral, const juce::String& title,
                    juce::Colour accent = palette::moltenAmber);

class MonolithLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    MonolithLookAndFeel();

    static const juce::Identifier accentPropertyId;
    static const juce::Identifier macroStylePropertyId;

    void drawRotarySlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPosProportional,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider& slider) override;

    void drawLinearSlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle style,
                           juce::Slider& slider) override;

    void drawToggleButton (juce::Graphics& g,
                           juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics& g,
                         juce::TextButton& button,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;

    void drawComboBox (juce::Graphics& g,
                       int width, int height,
                       bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox& box) override;

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override;

    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override;

    void drawPopupMenuItem (juce::Graphics& g,
                            const juce::Rectangle<int>& area,
                            bool isSeparator,
                            bool isActive,
                            bool isHighlighted,
                            bool isTicked,
                            bool hasSubMenu,
                            const juce::String& text,
                            const juce::String& shortcutKeyText,
                            const juce::Drawable* icon,
                            const juce::Colour* textColourToUse) override;

    void drawPopupMenuSectionHeader (juce::Graphics& g,
                                     const juce::Rectangle<int>& area,
                                     const juce::String& sectionName) override;

    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;

    void drawTooltip (juce::Graphics& g,
                      const juce::String& text,
                      int width,
                      int height) override;

    /** The resize corner keeps working, but draws nothing: no grip graphic. */
    void drawCornerResizer (juce::Graphics&, int, int, bool, bool) override {}
};

} // namespace monolith::ui
