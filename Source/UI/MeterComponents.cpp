#include "MeterComponents.h"
#include "OutputAnalyser.h"
#include "../V2/ModMatrix.h"
#include <algorithm>
#include <cmath>

namespace monolith::ui
{

// ============================================================================
// MonolithDial
// ============================================================================
MonolithDial::MonolithDial (juce::AudioProcessorValueTreeState& apvts,
                            const juce::String& paramId,
                            const juce::String& titleText,
                            const juce::String& tooltipText,
                            const juce::String& unitSuffix,
                            int decimals,
                            bool isPercent,
                            Style dialStyle,
                            const juce::String& accentName,
                            const juce::String& captionText)
    : paramIdent (paramId),
      title (titleText),
      suffix (unitSuffix),
      caption (captionText),
      numDecimals (decimals),
      percentMode (isPercent),
      style (dialStyle),
      accent (accentName == "ivory" ? palette::ivory : palette::amber),
      modDest (v2::findDestination (paramId))
{
    setTooltip (tooltipText);
    getProperties().set ("monolithParamId", paramId); // V2: right-click MIDI Learn target
    getProperties().set (MonolithLookAndFeel::accentPropertyId, accentName);
    setRepaintsOnMouseActivity (false);
    setWantsKeyboardFocus (false);
    setMouseCursor (juce::MouseCursor::UpDownLeftRightResizeCursor);

    param = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (paramId));
    jassert (param != nullptr);
    if (param != nullptr)
    {
        const auto& r = param->getNormalisableRange();
        // Ranges that contain zero (e.g. -12..0 st) draw their arc from the zero point.
        bipolarRange = r.start < 0.0f && r.end >= 0.0f;
        attachment = std::make_unique<juce::ParameterAttachment> (
            *param,
            [this] (float denorm)
            {
                const float n = param->convertTo0to1 (denorm);
                if (std::abs (n - norm) > 1.0e-6f)
                {
                    norm = n;
                    repaint();
                }
            });
        attachment->sendInitialUpdate();
    }
}

MonolithDial::~MonolithDial() = default;

void MonolithDial::setTitle (const juce::String& t)
{
    if (t != title)
    {
        title = t;
        repaint();
    }
}

void MonolithDial::setCaption (const juce::String& c)
{
    if (c != caption)
    {
        caption = c;
        repaint();
    }
}

void MonolithDial::setModulation (float m) noexcept
{
    const float v = m < 0.0f ? -1.0f : std::clamp (m, 0.0f, 1.0f);
    if (std::abs (v - modNorm) > 0.0025f || ((v < 0.0f) != (modNorm < 0.0f)))
    {
        modNorm = v;
        repaint();
    }
}

MonolithDial::Geometry MonolithDial::layout() const noexcept
{
    Geometry geo;
    auto b = getLocalBounds().toFloat();
    if (style == Style::inlineRow)
    {
        const float d = std::min (b.getHeight(), 48.0f);
        geo.dial = b.removeFromLeft (d).withSizeKeepingCentre (d, d);
        b.removeFromLeft (8.0f);
        const float midY = b.getCentreY();
        geo.title = { b.getX(), midY - 15.0f, b.getWidth(), 14.0f };
        geo.value = { b.getX(), midY + 1.0f, b.getWidth(), 14.0f };
    }
    else if (style == Style::hero)
    {
        geo.title   = b.removeFromTop (18.0f);
        geo.caption = b.removeFromBottom (14.0f);
        geo.value   = b.removeFromBottom (18.0f);
        b.removeFromTop (4.0f);
        const float d = std::min (b.getWidth() - 16.0f, b.getHeight());
        geo.dial = b.withSizeKeepingCentre (d, d);
    }
    else
    {
        geo.title = b.removeFromTop (14.0f);
        geo.value = b.removeFromBottom (14.0f);
        b.reduce (0.0f, 3.0f);
        const float d = std::min (b.getWidth() - 8.0f, b.getHeight());
        geo.dial = b.withSizeKeepingCentre (d, d);
    }

    geo.radius = geo.dial.getWidth() * 0.5f;
    const bool big = style == Style::hero;
    geo.stroke = big ? 4.0f : (style == Style::inlineRow ? 2.5f : 3.0f);
    geo.trackR = geo.radius - (big ? 9.0f : 6.0f);
    geo.capR   = geo.trackR - (big ? 8.0f : 5.0f);
    return geo;
}

void MonolithDial::resized()
{
    face.invalidate();
}

void MonolithDial::renderFace (juce::Graphics& g, const Geometry& geo) const
{
    const auto c = geo.dial.getCentre() - getLocalBounds().toFloat().getTopLeft();
    const bool big = style == Style::hero;

    // Tick ring
    const int ticks = big ? 21 : 11;
    for (int i = 0; i < ticks; ++i)
    {
        const float t = static_cast<float> (i) / static_cast<float> (ticks - 1);
        const float a = kStartAngle + t * (kEndAngle - kStartAngle);
        const bool major = big ? (i % 5 == 0) : (i == 0 || i == ticks - 1 || i == ticks / 2);
        const float r0 = geo.radius - (major ? 4.0f : 2.5f);
        const auto p1 = c.getPointOnCircumference (r0, a);
        const auto p2 = c.getPointOnCircumference (geo.radius - 0.5f, a);
        g.setColour (major ? palette::ash : palette::graphite);
        g.drawLine (p1.x, p1.y, p2.x, p2.y, major ? 1.2f : 1.0f);
    }

    // Track
    juce::Path track;
    track.addCentredArc (c.x, c.y, geo.trackR, geo.trackR, 0.0f, kStartAngle, kEndAngle, true);
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.strokePath (track, juce::PathStrokeType (geo.stroke + 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (palette::graphite.withAlpha (0.75f));
    g.strokePath (track, juce::PathStrokeType (geo.stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Machined cap: contact shadow, satin body, hairline rim, top catch light
    const auto cap = juce::Rectangle<float> (geo.capR * 2.0f, geo.capR * 2.0f).withCentre (c);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (cap.translated (0.0f, 1.5f).expanded (1.0f));
    juce::ColourGradient body (juce::Colour (0xFF24262B), c.x, cap.getY(), juce::Colour (0xFF121316), c.x, cap.getBottom(), false);
    g.setGradientFill (body);
    g.fillEllipse (cap);
    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.drawEllipse (cap, 1.0f);
    juce::Path catchLight;
    catchLight.addCentredArc (c.x, c.y, geo.capR - 1.0f, geo.capR - 1.0f, 0.0f, -1.05f, 1.05f, true);
    g.setColour (juce::Colours::white.withAlpha (0.09f));
    g.strokePath (catchLight, juce::PathStrokeType (1.0f));
    if (big)
    {
        // Concentric satin rings on hero caps
        for (float rr = geo.capR * 0.35f; rr < geo.capR - 3.0f; rr += 3.0f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.012f));
            g.drawEllipse (juce::Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre (c), 1.0f);
        }
    }
}

juce::String MonolithDial::formatValue (bool precise) const
{
    if (param == nullptr)
        return {};
    float v = param->convertFrom0to1 (norm);
    if (percentMode)
    {
        const float pct = v * 100.0f;
        return precise ? juce::String (pct, 1) + " %" : juce::String (juce::roundToInt (pct)) + "%";
    }
    const int dec = precise ? numDecimals + 1 : numDecimals;
    if (suffix == " Hz" && std::abs (v) >= 1000.0f)
        return juce::String (v / 1000.0f, precise ? 2 : 1) + " kHz";
    if (std::abs (v) < 0.5f * std::pow (10.0f, static_cast<float> (-dec)))
        v = 0.0f; // never print "-0.0"
    juce::String s = dec <= 0 ? juce::String (juce::roundToInt (v)) : juce::String (v, dec);
    if (bipolarRange && v > 0.0f)
        s = "+" + s;
    return s + suffix;
}

void MonolithDial::paint (juce::Graphics& g)
{
    const auto geo = layout();
    face.draw (g, getLocalBounds(), [this, &geo] (juce::Graphics& fg, juce::Rectangle<float>) { renderFace (fg, geo); });

    const auto c = geo.dial.getCentre();
    const bool live = dragging || isMouseOver();
    const float angle = kStartAngle + norm * (kEndAngle - kStartAngle);

    // Value arc (bipolar params grow from the centre)
    const float a0 = bipolarRange ? kStartAngle + param->convertTo0to1 (0.0f) * (kEndAngle - kStartAngle) : kStartAngle;
    if (std::abs (angle - a0) > 0.01f)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, geo.trackR, geo.trackR, 0.0f, std::min (a0, angle), std::max (a0, angle), true);
        g.setColour (accent.withAlpha (live ? 0.24f : 0.16f));
        g.strokePath (arc, juce::PathStrokeType (geo.stroke + 5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (accent);
        g.strokePath (arc, juce::PathStrokeType (geo.stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Modulation ring: live control-plane position outside the track
    if (modNorm >= 0.0f)
    {
        const float am = kStartAngle + modNorm * (kEndAngle - kStartAngle);
        const float rr = geo.radius - 1.5f;
        juce::Path ring;
        ring.addCentredArc (c.x, c.y, rr, rr, 0.0f, std::min (angle, am), std::max (angle, am), true);
        g.setColour (palette::ivory.withAlpha (0.85f));
        g.strokePath (ring, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const auto dot = c.getPointOnCircumference (rr, am);
        g.setColour (palette::ivory.withAlpha (0.25f));
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (dot));
        g.setColour (palette::ivory);
        g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (dot));
    }

    // Pointer
    const auto p0 = c.getPointOnCircumference (geo.capR * 0.30f, angle);
    const auto p1 = c.getPointOnCircumference (geo.capR * 0.88f, angle);
    g.setColour (palette::ivory);
    g.drawLine ({ p0, p1 }, style == Style::hero ? 2.4f : 1.8f);

    // Text
    if (style == Style::inlineRow)
    {
        g.setColour (live ? palette::ivory : palette::stone);
        g.setFont (capsFont (10.0f));
        g.drawText (title, geo.title, juce::Justification::centredLeft, false);
        g.setColour (live ? accent : palette::ivory.withAlpha (0.9f));
        g.setFont (uiFont (12.0f, Weight::demibold));
        g.drawText (formatValue (live), geo.value, juce::Justification::centredLeft, false);
        return;
    }

    const bool hero = style == Style::hero;
    g.setColour (live ? palette::ivory : (hero ? palette::ivory.withAlpha (0.92f) : palette::stone));
    g.setFont (capsFont (hero ? 12.5f : 10.0f, hero ? Weight::bold : Weight::demibold));
    g.drawText (title, geo.title, juce::Justification::centred, true);

    g.setColour (live ? accent : palette::ivory.withAlpha (hero ? 0.95f : 0.86f));
    g.setFont (uiFont (hero ? 14.0f : 11.5f, hero ? Weight::demibold : Weight::medium));
    g.drawText (formatValue (live), geo.value, juce::Justification::centred, false);

    if (hero && caption.isNotEmpty())
    {
        g.setColour (palette::ash.brighter (0.25f));
        g.setFont (capsFont (9.0f));
        g.drawText (caption, geo.caption, juce::Justification::centred, true);
    }
}

void MonolithDial::sendNorm (float n)
{
    n = std::clamp (n, 0.0f, 1.0f);
    if (param != nullptr && attachment != nullptr)
        attachment->setValueAsPartOfGesture (param->convertFrom0to1 (n));
    norm = param != nullptr ? param->convertTo0to1 (param->convertFrom0to1 (n)) : n;
    repaint();
}

void MonolithDial::resetToDefault()
{
    if (param == nullptr || attachment == nullptr)
        return;
    attachment->setValueAsCompleteGesture (param->convertFrom0to1 (param->getDefaultValue()));
    norm = param->getDefaultValue();
    repaint();
}

void MonolithDial::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() || attachment == nullptr)
        return; // MIDI Learn menu (editor-level listener)
    if (e.mods.isAltDown())
    {
        resetToDefault();
        return;
    }
    dragging = true;
    dragNorm = norm;
    lastDragPos = e.position;
    attachment->beginGesture();
    repaint();
}

void MonolithDial::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging)
        return;
    const float delta = (e.position.x - lastDragPos.x) - (e.position.y - lastDragPos.y);
    lastDragPos = e.position;
    const float pixelsForFull = style == Style::hero ? 260.0f : 200.0f;
    const bool fine = e.mods.isShiftDown() || e.mods.isCommandDown();
    dragNorm = std::clamp (dragNorm + delta / pixelsForFull * (fine ? 0.1f : 1.0f), 0.0f, 1.0f);
    sendNorm (dragNorm);
}

void MonolithDial::mouseUp (const juce::MouseEvent&)
{
    if (dragging)
    {
        dragging = false;
        if (attachment != nullptr)
            attachment->endGesture();
        repaint();
    }
}

void MonolithDial::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
        resetToDefault();
}

void MonolithDial::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (param == nullptr || attachment == nullptr)
        return;
    const float dy = (std::abs (w.deltaY) > std::abs (w.deltaX) ? w.deltaY : -w.deltaX) * (w.isReversed ? -1.0f : 1.0f);
    const bool fine = e.mods.isShiftDown() || e.mods.isCommandDown();
    const float n = std::clamp (norm + dy * (fine ? 0.02f : 0.12f), 0.0f, 1.0f);
    attachment->setValueAsCompleteGesture (param->convertFrom0to1 (n));
    norm = param->convertTo0to1 (param->convertFrom0to1 (n));
    repaint();
}

std::unique_ptr<juce::AccessibilityHandler> MonolithDial::createAccessibilityHandler()
{
    struct DialValue final : public juce::AccessibilityRangedNumericValueInterface
    {
        explicit DialValue (MonolithDial& d) : dial (d) {}
        bool isReadOnly() const override { return false; }
        double getCurrentValue() const override { return dial.norm; }
        void setValue (double v) override { dial.attachment->setValueAsCompleteGesture (dial.param->convertFrom0to1 (static_cast<float> (v))); }
        AccessibleValueRange getRange() const override { return { { 0.0, 1.0 }, 0.01 }; }
        MonolithDial& dial;
    };
    if (param == nullptr || attachment == nullptr)
        return nullptr;
    return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::slider,
                                                         juce::AccessibilityActions(),
                                                         juce::AccessibilityHandler::Interfaces { std::make_unique<DialValue> (*this) });
}

// ============================================================================
// OutputMeter
// ============================================================================
namespace
{
    constexpr float kMeterMinDb = -48.0f;
    [[nodiscard]] float meterNorm (float db) noexcept { return std::clamp ((db - kMeterMinDb) / -kMeterMinDb, 0.0f, 1.0f); }
}

OutputMeter::OutputMeter()
{
    setTooltip ("Output: sample peak L/R with hold, short-term loudness (BS.1770, 3 s) and clip latch. Click to reset the clip LED.");
}

void OutputMeter::update (const OutputAnalyser& a)
{
    const float nl = a.getBarDb (0), nr = a.getBarDb (1), hl = a.getHoldDb (0), hr = a.getHoldDb (1);
    const float st = a.getShortTermLufs();
    const bool c = a.isClipped();
    if (std::abs (nl - barL) > 0.2f || std::abs (nr - barR) > 0.2f || std::abs (hl - holdL) > 0.2f || std::abs (hr - holdR) > 0.2f
        || std::abs (st - lufs) > 0.05f || c != clip)
    {
        barL = nl; barR = nr; holdL = hl; holdR = hr; lufs = st; clip = c;
        peakMax = std::max (hl, hr);
        repaint();
    }
}

void OutputMeter::mouseDown (const juce::MouseEvent&)
{
    if (onClipReset)
        onClipReset();
}

void OutputMeter::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto bars = juce::Rectangle<float> (b.getX(), b.getY(), b.getWidth() - 56.0f, b.getHeight());
    const float barW = bars.getWidth() - 22.0f;
    const float x0 = bars.getX() + 10.0f;
    const float yL = b.getCentreY() - 7.0f, yR = b.getCentreY() + 2.0f;

    scale.draw (g, getLocalBounds(), [&] (juce::Graphics& sg, juce::Rectangle<float>) {
        chrome::drawWell (sg, b, 3.0f);
        sg.setFont (capsFont (8.5f));
        sg.setColour (palette::ash);
        sg.drawText ("L", juce::Rectangle<float> (bars.getX() + 2.0f, yL - 2.0f, 8.0f, 9.0f), juce::Justification::centred);
        sg.drawText ("R", juce::Rectangle<float> (bars.getX() + 2.0f, yR - 2.0f, 8.0f, 9.0f), juce::Justification::centred);
        for (float db : { -36.0f, -24.0f, -12.0f, -6.0f, 0.0f })
        {
            const float x = x0 + barW * meterNorm (db);
            sg.setColour (db >= -0.1f ? palette::crimson.withAlpha (0.5f) : palette::graphite);
            sg.fillRect (x - 0.5f, yL - 3.0f, 1.0f, 2.0f);
            sg.fillRect (x - 0.5f, yR + 6.0f, 1.0f, 2.0f);
        }
        for (float y : { yL, yR })
        {
            sg.setColour (palette::chassis);
            sg.fillRect (x0, y, barW, 5.0f);
        }
        chrome::drawDividerV (sg, b.getRight() - 56.0f, b.getY() + 6.0f, b.getBottom() - 6.0f);
        sg.setColour (palette::ash);
        sg.setFont (capsFont (8.0f));
        sg.drawText ("LUFS S", juce::Rectangle<float> (b.getRight() - 54.0f, b.getBottom() - 15.0f, 52.0f, 10.0f), juce::Justification::centred);
    });

    auto drawBar = [&] (float y, float db, float hold) {
        const float w = barW * meterNorm (db);
        if (w > 0.5f)
        {
            const float warm = meterNorm (-12.0f) * barW;
            g.setColour (palette::ivory.withAlpha (0.78f));
            g.fillRect (x0, y, std::min (w, warm), 5.0f);
            if (w > warm)
            {
                g.setColour (palette::amber);
                g.fillRect (x0 + warm, y, w - warm, 5.0f);
            }
        }
        if (hold > kMeterMinDb)
        {
            g.setColour (hold > -0.5f ? palette::crimson : palette::ivory);
            g.fillRect (x0 + barW * meterNorm (hold) - 1.0f, y, 2.0f, 5.0f);
        }
    };
    drawBar (yL, barL, holdL);
    drawBar (yR, barR, holdR);

    // Clip LED
    const auto led = juce::Rectangle<float> (bars.getRight() - 9.0f, b.getCentreY() - 4.0f, 6.0f, 8.0f);
    if (clip)
    {
        g.setColour (palette::crimson.withAlpha (0.35f));
        g.fillRoundedRectangle (led.expanded (2.0f), 2.0f);
    }
    g.setColour (clip ? palette::crimson : palette::graphite);
    g.fillRoundedRectangle (led, 1.5f);

    // Loudness readout
    g.setColour (lufs > -70.0f ? palette::ivory : palette::ash);
    g.setFont (monoFont (12.0f));
    const juce::String txt = lufs > -70.0f ? juce::String (lufs, 1) : juce::String::fromUTF8 ("\xe2\x80\x94");
    g.drawText (txt, juce::Rectangle<float> (b.getRight() - 54.0f, b.getY() + 5.0f, 52.0f, 16.0f), juce::Justification::centred, false);
}

// ============================================================================
// ExpressionBar
// ============================================================================
ExpressionBar::ExpressionBar (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId,
                              std::function<void (float)> onUiDrag)
    : uiCallback (std::move (onUiDrag))
{
    setTooltip ("Expression swell: glottal tension, harmonic crown and lung bloom. Follows CC11 / CC1; drag to perform, double-click to reset.");
    getProperties().set ("monolithParamId", paramId);
    if (auto* p = apvts.getParameter (paramId))
    {
        defaultVal = p->getDefaultValue();
        paramAttach = std::make_unique<juce::ParameterAttachment> (
            *p,
            [this] (float v) {
                paramVal = std::clamp (v, 0.0f, 1.0f);
                if (! isDragging)
                    repaint();
            });
        paramAttach->sendInitialUpdate();
        liveVal = paramVal;
    }
}

juce::Rectangle<float> ExpressionBar::trackRect() const noexcept
{
    return getLocalBounds().toFloat().withTrimmedTop (18.0f).withHeight (10.0f);
}

void ExpressionBar::updateLive (float liveExpr) noexcept
{
    const float c = std::clamp (liveExpr, 0.0f, 1.0f);
    if (std::abs (c - liveVal) > 0.003f)
    {
        liveVal = c;
        repaint();
    }
}

void ExpressionBar::setFromMouseX (float x)
{
    const auto t = trackRect();
    const float n = std::clamp ((x - t.getX()) / std::max (1.0f, t.getWidth()), 0.0f, 1.0f);
    paramVal = liveVal = n;
    if (paramAttach) paramAttach->setValueAsPartOfGesture (n);
    if (uiCallback) uiCallback (n);
    repaint();
}

void ExpressionBar::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;
    isDragging = true;
    if (paramAttach) paramAttach->beginGesture();
    setFromMouseX (e.position.x);
}

void ExpressionBar::mouseDrag (const juce::MouseEvent& e)
{
    if (isDragging)
        setFromMouseX (e.position.x);
}

void ExpressionBar::mouseUp (const juce::MouseEvent&)
{
    if (isDragging)
    {
        isDragging = false;
        if (paramAttach) paramAttach->endGesture();
    }
}

void ExpressionBar::mouseDoubleClick (const juce::MouseEvent&)
{
    paramVal = liveVal = defaultVal;
    if (paramAttach) paramAttach->setValueAsCompleteGesture (defaultVal);
    if (uiCallback) uiCallback (defaultVal);
    repaint();
}

void ExpressionBar::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const bool hot = isMouseOverOrDragging();
    g.setColour (hot ? palette::ivory : palette::stone);
    g.setFont (capsFont (10.0f));
    g.drawText ("EXPRESSION", b.withHeight (14.0f), juce::Justification::centredLeft, false);
    g.setColour (hot ? palette::amber : palette::ivory.withAlpha (0.9f));
    g.setFont (uiFont (11.5f, Weight::demibold));
    g.drawText (juce::String (juce::roundToInt (liveVal * 100.0f)) + "%", b.withHeight (14.0f), juce::Justification::centredRight, false);

    const auto t = trackRect();
    chrome::drawWell (g, t, 3.0f);
    const float w = (t.getWidth() - 2.0f) * liveVal;
    if (w > 0.5f)
    {
        g.setColour (palette::amber.withAlpha (0.18f));
        g.fillRoundedRectangle (t.reduced (1.0f).withWidth (w).expanded (0.0f, 2.0f), 2.0f);
        g.setColour (palette::amber);
        g.fillRoundedRectangle (t.reduced (1.0f, 3.0f).withWidth (w), 1.5f);
    }
    const float px = t.getX() + 1.0f + (t.getWidth() - 2.0f) * paramVal;
    g.setColour (palette::ivory);
    g.fillRoundedRectangle (px - 1.5f, t.getY() - 3.0f, 3.0f, t.getHeight() + 6.0f, 1.0f);
}

// ============================================================================
// ExpressionMeters
// ============================================================================
ExpressionMeters::ExpressionMeters()
{
    setTooltip ("Live expressive input: Pressure (MPE / poly / channel aftertouch), Slide (CC74), Mod Wheel (CC1), Breath (CC2).");
    setInterceptsMouseClicks (true, false);
}

void ExpressionMeters::update (float pressure, float slide, float modWheel, float breath) noexcept
{
    const std::array<float, 4> v { pressure, slide, modWheel, breath };
    bool changed = false;
    for (size_t i = 0; i < 4; ++i)
        if (std::abs (v[i] - values[i]) > 0.004f)
        {
            values[i] = std::clamp (v[i], 0.0f, 1.0f);
            changed = true;
        }
    if (changed)
        repaint();
}

void ExpressionMeters::paint (juce::Graphics& g)
{
    static const char* const names[] { "PRESS", "SLIDE", "MOD", "BRTH" };
    const auto b = getLocalBounds().toFloat();
    const float cellW = (b.getWidth() - 3.0f * 8.0f) / 4.0f;
    for (int i = 0; i < 4; ++i)
    {
        const auto cell = juce::Rectangle<float> (b.getX() + static_cast<float> (i) * (cellW + 8.0f), b.getY(), cellW, b.getHeight());
        const float v = values[static_cast<size_t> (i)];
        g.setColour (v > 0.01f ? palette::ivory.withAlpha (0.85f) : palette::ash);
        g.setFont (capsFont (8.5f));
        g.drawText (names[i], cell.withHeight (12.0f), juce::Justification::centredLeft, false);
        const auto t = cell.withTrimmedTop (16.0f).withHeight (6.0f);
        chrome::drawWell (g, t, 2.0f);
        if (v > 0.005f)
        {
            g.setColour (palette::amber);
            g.fillRoundedRectangle (t.reduced (1.0f).withWidth ((t.getWidth() - 2.0f) * v), 1.5f);
        }
    }
}

// ============================================================================
// GlyphButton
// ============================================================================
GlyphButton::GlyphButton (const juce::String& name, Glyph g, bool keycap)
    : juce::Button (name), glyph (g), drawKeycap (keycap)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void drawGlyph (juce::Graphics& g, GlyphButton::Glyph glyph, juce::Rectangle<float> r, juce::Colour c, bool filled)
{
    using G = GlyphButton::Glyph;
    const auto cx = r.getCentreX(), cy = r.getCentreY();
    const float s = std::min (r.getWidth(), r.getHeight());
    juce::Path p;
    const juce::PathStrokeType stroke (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour (c);
    switch (glyph)
    {
        case G::prev:
        case G::next:
        {
            const float d = glyph == G::prev ? -1.0f : 1.0f;
            p.startNewSubPath (cx - 2.0f * d, cy - 4.5f);
            p.lineTo (cx + 2.5f * d, cy);
            p.lineTo (cx - 2.0f * d, cy + 4.5f);
            g.strokePath (p, stroke);
            break;
        }
        case G::undo:
        case G::redo:
        {
            const float d = glyph == G::undo ? 1.0f : -1.0f;
            p.addCentredArc (cx, cy + 1.0f, 5.0f, 5.0f, 0.0f, d * -2.2f, d * 1.25f, true);
            g.strokePath (p, stroke);
            const auto tip = juce::Point<float> (cx, cy + 1.0f).getPointOnCircumference (5.0f, d * -2.2f);
            juce::Path head;
            head.addTriangle (tip.x - 3.2f * d, tip.y - 3.0f, tip.x + 2.8f * d, tip.y - 0.6f, tip.x - 0.4f * d, tip.y + 3.4f);
            g.fillPath (head);
            break;
        }
        case G::star:
        {
            p.addStar ({ cx, cy }, 5, s * 0.17f, s * 0.38f, 0.0f);
            if (filled) g.fillPath (p);
            else        g.strokePath (p, juce::PathStrokeType (1.2f, juce::PathStrokeType::mitered));
            break;
        }
        case G::browse:
        {
            for (int i = -1; i <= 1; ++i)
                g.fillRect (cx - 5.5f, cy + static_cast<float> (i) * 4.0f - 0.75f, 11.0f, 1.5f);
            break;
        }
        case G::route:
        {
            g.fillEllipse (cx - 6.0f, cy - 1.5f, 3.0f, 3.0f);
            p.startNewSubPath (cx - 3.0f, cy);
            p.cubicTo (cx, cy, cx, cy - 4.0f, cx + 4.5f, cy - 4.0f);
            p.startNewSubPath (cx - 3.0f, cy);
            p.cubicTo (cx, cy, cx, cy + 4.0f, cx + 4.5f, cy + 4.0f);
            g.strokePath (p, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.fillEllipse (cx + 3.5f, cy - 5.25f, 2.5f, 2.5f);
            g.fillEllipse (cx + 3.5f, cy + 2.75f, 2.5f, 2.5f);
            break;
        }
        case G::close:
        {
            p.startNewSubPath (cx - 4.0f, cy - 4.0f); p.lineTo (cx + 4.0f, cy + 4.0f);
            p.startNewSubPath (cx + 4.0f, cy - 4.0f); p.lineTo (cx - 4.0f, cy + 4.0f);
            g.strokePath (p, stroke);
            break;
        }
        case G::search:
        {
            g.drawEllipse (cx - 5.0f, cy - 5.0f, 8.0f, 8.0f, 1.4f);
            g.drawLine (cx + 2.0f, cy + 2.0f, cx + 5.0f, cy + 5.0f, 1.6f);
            break;
        }
    }
}

void GlyphButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    if (drawKeycap)
        getLookAndFeel().drawButtonBackground (g, *this, palette::panelRaised, highlighted, down);
    juce::Colour c = getToggleState() ? accentOf (*this) : (highlighted ? palette::ivory : palette::stone);
    if (! isEnabled())
        c = palette::ash.withAlpha (0.55f);
    drawGlyph (g, glyph, getLocalBounds().toFloat(), c, getToggleState());
}

} // namespace monolith::ui
