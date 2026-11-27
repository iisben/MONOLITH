#pragma once

#include "CachedLayer.h"
#include "MonolithLookAndFeel.h"
#include "../State/TelemetrySnapshot.h"
#include <functional>
#include <memory>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace monolith::ui
{

class OutputAnalyser;

/**
    MONOLITH II rotary control bound to one APVTS parameter.
    Static face (ticks, track, machined cap) is cached per physical scale; paint() only draws the value arc,
    pointer, modulation ring and live text. Interaction: relative drag (Shift / Cmd = 10x fine), mouse wheel,
    double-click or Alt-click resets to default, right-click is left to the editor's MIDI Learn menu.
    Hover / drag shows the exact value at one extra decimal.
*/
class MonolithDial final : public juce::Component,
                           public juce::SettableTooltipClient
{
public:
    enum class Style { standard, hero, inlineRow };

    MonolithDial (juce::AudioProcessorValueTreeState& apvts,
                  const juce::String& paramId,
                  const juce::String& titleText,
                  const juce::String& tooltipText,
                  const juce::String& unitSuffix = "",
                  int decimals = 2,
                  bool isPercent = false,
                  Style style = Style::standard,
                  const juce::String& accentName = "amber",
                  const juce::String& captionText = "");
    ~MonolithDial() override;

    void setTitle (const juce::String& t);
    void setCaption (const juce::String& c);
    /** Live modulated position (normalised); < 0 hides the ring. Repaints only on visible change. */
    void setModulation (float modNorm) noexcept;
    /** Destination index into v2::kModDestinations (-1 = not modulatable). */
    [[nodiscard]] int getModDestination() const noexcept { return modDest; }
    [[nodiscard]] juce::RangedAudioParameter* getParameter() const noexcept { return param; }
    [[nodiscard]] float getNormalisedValue() const noexcept { return norm; }
    [[nodiscard]] const juce::String& getParamId() const noexcept { return paramIdent; }
    [[nodiscard]] Style getStyle() const noexcept { return style; }

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

private:
    struct Geometry
    {
        juce::Rectangle<float> dial, title, value, caption;
        float radius = 0, trackR = 0, capR = 0, stroke = 3;
    };
    [[nodiscard]] Geometry layout() const noexcept;
    [[nodiscard]] juce::String formatValue (bool precise) const;
    void renderFace (juce::Graphics& g, const Geometry& geo) const;
    void sendNorm (float n);
    void resetToDefault();

    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    juce::String paramIdent, title, suffix, caption;
    int numDecimals = 2;
    bool percentMode = false;
    bool bipolarRange = false;
    Style style = Style::standard;
    juce::Colour accent;
    int modDest = -1;

    float norm = 0.0f, dragNorm = 0.0f, modNorm = -1.0f;
    bool dragging = false;
    juce::Point<float> lastDragPos;
    CachedLayer face;

    static constexpr float kStartAngle = juce::MathConstants<float>::pi * 1.22f;
    static constexpr float kEndAngle   = juce::MathConstants<float>::pi * 2.78f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MonolithDial)
};

/** Stereo sample-peak bars (with hold), BS.1770 short-term loudness readout and latched clip LED. */
class OutputMeter final : public juce::Component,
                          public juce::SettableTooltipClient
{
public:
    OutputMeter();
    void update (const OutputAnalyser& a);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent&) override;
    std::function<void()> onClipReset;

private:
    float barL = -120.0f, barR = -120.0f, holdL = -120.0f, holdR = -120.0f, lufs = -120.0f, peakMax = -120.0f;
    bool clip = false;
    CachedLayer scale;
};

/** Interactive expression fader (CC11-style UI override) reading live telemetry. */
class ExpressionBar final : public juce::Component,
                            public juce::SettableTooltipClient
{
public:
    ExpressionBar (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId,
                   std::function<void (float)> onUiDrag = {});

    void updateLive (float liveExpr) noexcept;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

private:
    [[nodiscard]] juce::Rectangle<float> trackRect() const noexcept;
    void setFromMouseX (float x);

    std::unique_ptr<juce::ParameterAttachment> paramAttach;
    std::function<void (float)> uiCallback;
    float defaultVal = 0.78f;
    float paramVal = 0.78f;
    float liveVal  = 0.78f;
    bool  isDragging = false;
};

/** Live expressive-input monitor: pressure, slide (CC74), mod wheel (CC1), breath (CC2). */
class ExpressionMeters final : public juce::Component,
                               public juce::SettableTooltipClient
{
public:
    ExpressionMeters();
    void update (float pressure, float slide, float modWheel, float breath) noexcept;
    void paint (juce::Graphics& g) override;

private:
    std::array<float, 4> values {};
};

/** Keycap button with a vector glyph (no bitmap assets). */
class GlyphButton final : public juce::Button
{
public:
    enum class Glyph { prev, next, undo, redo, star, browse, route, close, search };
    GlyphButton (const juce::String& name, Glyph glyph, bool keycap = true);
    void setGlyph (Glyph g) { glyph = g; repaint(); }
    void paintButton (juce::Graphics& g, bool highlighted, bool down) override;

private:
    Glyph glyph;
    bool drawKeycap;
};

/** Draws a glyph centred in r (shared by GlyphButton and painted chrome). */
void drawGlyph (juce::Graphics& g, GlyphButton::Glyph glyph, juce::Rectangle<float> r, juce::Colour c, bool filled = false);

} // namespace monolith::ui
