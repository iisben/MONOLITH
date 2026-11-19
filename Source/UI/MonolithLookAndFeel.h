#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace monolith::ui
{

namespace palette
{
    inline const juce::Colour obsidianBg     { 0xFF0A0B0E };
    inline const juce::Colour panelSlate     { 0xFF111318 };
    inline const juce::Colour panelElevated  { 0xFF171A22 };
    inline const juce::Colour gridHairline   { 0xFF252933 };
    inline const juce::Colour borderBright   { 0xFF353B49 };
    inline const juce::Colour boneWhite      { 0xFFECEAE4 };
    inline const juce::Colour mutedLimestone { 0xFF8C919E };
    inline const juce::Colour dimAsh         { 0xFF565B67 };
    inline const juce::Colour moltenAmber    { 0xFFE8923A };
    inline const juce::Colour amberGlow      { 0x55E8923A };
    inline const juce::Colour paleCyan       { 0xFF7AD7E8 };
    inline const juce::Colour cyanGlow       { 0x557AD7E8 };
    inline const juce::Colour crimsonAlert   { 0xFFD9534F };
} // namespace palette

[[nodiscard]] inline juce::Font makeFont (float heightPx, bool bold = false, bool mono = false)
{
    const auto family = mono
        ? juce::Font::getDefaultMonospacedFontName()
        : juce::Font::getDefaultSansSerifFontName();
    return juce::Font (juce::FontOptions (family,
                                          heightPx,
                                          bold ? juce::Font::bold : juce::Font::plain));
}

/** Custom Brutalist Cinematic Luxury LookAndFeel (zero default JUCE widget styling). */
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

    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;

    void drawTooltip (juce::Graphics& g,
                      const juce::String& text,
                      int width,
                      int height) override;
};

} // namespace monolith::ui
