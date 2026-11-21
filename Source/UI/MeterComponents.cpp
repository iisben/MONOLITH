#include "MeterComponents.h"
#include <algorithm>
#include <cmath>

namespace monolith::ui
{

namespace
{
    [[nodiscard]] inline float ampToMeterNorm (float amp) noexcept
    {
        if (amp <= 1.0e-5f)
            return 0.0f;
        const float db = 20.0f * std::log10 (amp);
        return std::clamp ((db + 54.0f) / 54.0f, 0.0f, 1.0f);
    }
} // namespace

// ============================================================================
MonolithDial::MonolithDial (juce::AudioProcessorValueTreeState& apvts,
                            const juce::String& paramId,
                            const juce::String& titleText,
                            const juce::String& tooltipText,
                            const juce::String& unitSuffix,
                            int decimals,
                            bool isPercent,
                            bool isMacro,
                            const juce::String& accentName,
                            const juce::String& subCaptionText)
    : title (titleText),
      suffix (unitSuffix),
      subCaption (subCaptionText),
      numDecimals (decimals),
      percentMode (isPercent),
      macroMode (isMacro),
      accentColour (accentFromName (accentName))
{
    // Macros are standing stones (vertical, relative drag); everything else is a turned dial.
    slider.setSliderStyle (isMacro ? juce::Slider::LinearVertical : juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setSliderSnapsToMousePosition (false);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setMouseDragSensitivity (isMacro ? 190 : 165);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    slider.setTooltip (tooltipText);
    slider.getProperties().set (MonolithLookAndFeel::accentPropertyId, accentName);
    slider.getProperties().set (MonolithLookAndFeel::macroStylePropertyId, isMacro);

    if (auto* param = apvts.getParameter (paramId))
    {
        const float defDenorm = param->convertFrom0to1 (param->getDefaultValue());
        slider.setDoubleClickReturnValue (true, static_cast<double> (defDenorm));
    }

    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);
}

juce::String MonolithDial::formatCurrentValue() const
{
    const double val = slider.getValue();
    if (percentMode)
        return juce::String (static_cast<int> (std::lround (val * 100.0))) + "%";

    if (numDecimals == 0)
        return juce::String (static_cast<int> (std::lround (val))) + suffix;

    return juce::String (val, numDecimals) + suffix;
}

void MonolithDial::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const bool live = slider.isMouseOverOrDragging();

    if (macroMode)
    {
        // Stone on the left; name, reading and its three effects inscribed beside it
        auto text = b.withTrimmedLeft ((float) slider.getRight() + 6.0f).reduced (0.0f, 4.0f);
        g.setColour (live ? palette::boneWhite : palette::boneWhite.withAlpha (0.92f));
        g.setFont (makeRoleFont (FontRole::Inscription, 15.0f, true));
        drawTracked (g, title, text.removeFromTop (22.0f), juce::Justification::centredLeft, 0.16f);

        drawIncisedHorizontal (g, text.getY() + 3.0f, text.getX(), text.getRight() - 10.0f);
        text.removeFromTop (10.0f);

        g.setColour (live ? accentColour : palette::boneWhite);
        g.setFont (makeRoleFont (FontRole::Numeric, 22.0f, false));
        g.drawText (formatCurrentValue(), text.removeFromTop (28.0f), juce::Justification::centredLeft, false);

        if (subCaption.isNotEmpty())
        {
            g.setColour (palette::mutedLimestone);
            g.setFont (makeFont (8.0f, true, false));
            drawTracked (g, subCaption, text.removeFromTop (14.0f).withRight (b.getRight()), juce::Justification::centredLeft, 0.02f);
        }
        return;
    }

    if (isCompact())
    {
        auto text = b.withTrimmedLeft ((float) slider.getRight() + 4.0f);
        g.setColour (live ? palette::boneWhite : palette::mutedLimestone);
        g.setFont (makeFont (8.5f, true, false));
        drawTracked (g, title, text.removeFromTop (b.getHeight() * 0.5f), juce::Justification::bottomLeft, 0.16f);
        g.setColour (live ? accentColour : palette::boneWhite.withAlpha (0.85f));
        g.setFont (makeRoleFont (FontRole::Numeric, 10.0f, live));
        g.drawText (formatCurrentValue(), text, juce::Justification::topLeft, false);
        return;
    }

    g.setColour (live ? palette::boneWhite : palette::mutedLimestone);
    g.setFont (makeFont (9.0f, true, false));
    drawTracked (g, title, b.withHeight (13.0f), juce::Justification::centred, 0.16f);

    g.setColour (live ? accentColour : palette::boneWhite.withAlpha (0.85f));
    g.setFont (makeRoleFont (FontRole::Numeric, 9.5f, live));
    g.drawText (formatCurrentValue(), b.withTop (b.getBottom() - 13.0f), juce::Justification::centred, false);
}

void MonolithDial::resized()
{
    auto b = getLocalBounds();
    if (macroMode)
    {
        const int slabH = b.getHeight() - 8;
        slider.setBounds (b.getX(), b.getY() + 4, slabH * 4 / 9 + 26, slabH);
    }
    else if (isCompact())
        slider.setBounds (b.removeFromLeft (b.getHeight()));
    else
        slider.setBounds (b.withTrimmedTop (13).withTrimmedBottom (13));
}

// ============================================================================
void StereoLevelMeter::update (float rmsL, float rmsR, float peakL, float peakR) noexcept
{
    const float tL = ampToMeterNorm (rmsL * 1.4f);
    const float tR = ampToMeterNorm (rmsR * 1.4f);
    levelL = (tL > levelL) ? tL : (0.86f * levelL + 0.14f * tL);
    levelR = (tR > levelR) ? tR : (0.86f * levelR + 0.14f * tR);

    const float pL = ampToMeterNorm (peakL);
    const float pR = ampToMeterNorm (peakR);

    if (pL >= holdL)
    {
        holdL = pL;
        holdFramesL = 45;
    }
    else if (--holdFramesL <= 0)
    {
        holdL = std::max (0.0f, holdL - 0.015f);
    }

    if (pR >= holdR)
    {
        holdR = pR;
        holdFramesR = 45;
    }
    else if (--holdFramesR <= 0)
    {
        holdR = std::max (0.0f, holdR - 0.015f);
    }

    repaint();
}

void StereoLevelMeter::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    drawRecess (g, b);

    const auto inner = b.reduced (4.0f, 4.0f);
    const float barH = (inner.getHeight() - 3.0f) * 0.5f;
    constexpr int kSegments = 36;

    auto drawBar = [&g] (juce::Rectangle<float> rect, float val, float hold)
    {
        // Segmented like coursed stone: limestone, gilt over -9 dB, oxide in the last segment
        const float segW = rect.getWidth() / (float) kSegments;
        const int lit = (int) std::floor (std::clamp (val, 0.0f, 1.0f) * (float) kSegments + 0.0001f);
        for (int i = 0; i < kSegments; ++i)
        {
            const auto seg = juce::Rectangle<float> (rect.getX() + (float) i * segW, rect.getY(), segW - 1.0f, rect.getHeight());
            const float pos = (float) (i + 1) / (float) kSegments;
            juce::Colour on = pos > 0.97f ? palette::crimsonAlert : (pos > 0.83f ? palette::moltenAmber : palette::mutedLimestone);
            g.setColour (i < lit ? on : palette::panelElevated.withAlpha (0.65f));
            g.fillRect (seg);
        }
        if (hold > 0.01f)
        {
            const float hx = rect.getX() + rect.getWidth() * std::clamp (hold, 0.0f, 1.0f);
            g.setColour (hold > 0.97f ? palette::crimsonAlert : palette::boneWhite);
            g.fillRect (hx - 1.5f, rect.getY() - 1.0f, 1.5f, rect.getHeight() + 2.0f);
        }
    };

    drawBar (inner.withHeight (barH), levelL, holdL);
    drawBar (inner.withTop (inner.getY() + barH + 3.0f).withHeight (barH), levelR, holdR);
}

// ============================================================================
ExpressionBar::ExpressionBar (juce::AudioProcessorValueTreeState& apvts,
                                const juce::String& paramId,
                                std::function<void (float)> onUiDrag)
    : uiCallback (std::move (onUiDrag))
{
    setTooltip ("Expression / MIDI CC1 Swell: drives glottal tension, harmonic crown, and lung bloom.");
    if (auto* p = apvts.getParameter (paramId))
    {
        paramAttach = std::make_unique<juce::ParameterAttachment> (
            *p,
            [this] (float newVal)
            {
                paramVal = std::clamp (newVal, 0.0f, 1.0f);
                if (! isDragging)
                    repaint();
            });
        paramAttach->sendInitialUpdate();
    }
}

void ExpressionBar::updateLive (float liveExpr) noexcept
{
    const float clamped = std::clamp (liveExpr, 0.0f, 1.0f);
    if (std::abs (clamped - liveVal) > 0.004f)
    {
        liveVal = clamped;
        repaint();
    }
}

void ExpressionBar::setFromMouseX (float x)
{
    const float w = std::max (1.0f, static_cast<float> (getWidth()) - 8.0f);
    const float norm = std::clamp ((x - 4.0f) / w, 0.0f, 1.0f);
    paramVal = norm;
    liveVal  = norm;
    if (paramAttach)
        paramAttach->setValueAsPartOfGesture (norm);
    if (uiCallback)
        uiCallback (norm);
    repaint();
}

void ExpressionBar::mouseDown (const juce::MouseEvent& e)
{
    isDragging = true;
    if (paramAttach)
        paramAttach->beginGesture();
    setFromMouseX (e.position.x);
}

void ExpressionBar::mouseDrag (const juce::MouseEvent& e)
{
    setFromMouseX (e.position.x);
}

void ExpressionBar::mouseUp (const juce::MouseEvent& e)
{
    (void) e;
    if (isDragging)
    {
        isDragging = false;
        if (paramAttach)
            paramAttach->endGesture();
    }
}

void ExpressionBar::mouseDoubleClick (const juce::MouseEvent& e)
{
    (void) e;
    paramVal = 0.78f;
    liveVal  = 0.78f;
    if (paramAttach)
        paramAttach->setValueAsCompleteGesture (0.78f);
    if (uiCallback)
        uiCallback (0.78f);
    repaint();
}

void ExpressionBar::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const bool live = isMouseOverOrDragging();

    g.setColour (live ? palette::boneWhite : palette::mutedLimestone);
    g.setFont (makeFont (9.0f, true, false));
    drawTracked (g, juce::String::fromUTF8 ("EXPRESSION  \u00B7  CC1"), b.withHeight (13.0f), juce::Justification::centredLeft, 0.18f);

    const float val = liveVal;
    g.setColour (palette::boneWhite);
    g.setFont (makeRoleFont (FontRole::Numeric, 10.0f, true));
    g.drawText (juce::String (static_cast<int> (std::lround (val * 100.0f))) + "%",
                b.withHeight (13.0f), juce::Justification::centredRight, false);

    // A channel cut into the stone, filled with gilt to the current dynamic
    const auto track = juce::Rectangle<float> (b.getX(), b.getY() + 20.0f, b.getWidth(), 12.0f);
    drawRecess (g, track);
    const auto inner = track.reduced (3.0f, 3.0f);
    const float fillW = inner.getWidth() * std::clamp (val, 0.0f, 1.0f);
    if (fillW > 0.5f)
    {
        g.setColour (palette::moltenAmber.withAlpha (live ? 0.95f : 0.8f));
        g.fillRect (inner.withWidth (fillW));
    }

    // Quarter graduations cut below the channel
    g.setColour (palette::dimAsh);
    for (int i = 1; i < 4; ++i)
        g.fillRect (inner.getX() + inner.getWidth() * 0.25f * (float) i, track.getBottom() + 1.0f, 1.0f, 3.0f);

    // Fader edge
    const float tx = inner.getX() + fillW;
    g.setColour (palette::boneWhite);
    g.fillRect (tx - 1.0f, track.getY() - 2.0f, 2.0f, track.getHeight() + 4.0f);
}

// ============================================================================
void VoiceBadge::update (int voices, int singers) noexcept
{
    if (voices != voiceCount || singers != singerCount)
    {
        voiceCount  = voices;
        singerCount = singers;
        repaint();
    }
}

void VoiceBadge::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    drawRecess (g, b);

    const bool active = voiceCount > 0;
    g.setColour (active ? palette::moltenAmber : palette::dimAsh);
    g.fillRect (b.getX() + 8.0f, b.getCentreY() - 3.0f, 6.0f, 6.0f);

    auto area = b.withTrimmedLeft (22.0f).withTrimmedRight (8.0f);
    auto half = area.removeFromLeft (area.getWidth() * 0.5f);

    auto drawStat = [&g, active] (juce::Rectangle<float> r, const char* label, int value)
    {
        g.setColour (palette::mutedLimestone);
        g.setFont (makeFont (8.5f, true, false));
        drawTracked (g, label, r, juce::Justification::centredLeft, 0.18f);
        g.setColour (active ? palette::boneWhite : palette::mutedLimestone);
        g.setFont (makeRoleFont (FontRole::Numeric, 11.0f, true));
        g.drawText (juce::String (value).paddedLeft ('0', 2), r, juce::Justification::centredRight, false);
    };
    drawStat (half.withTrimmedRight (8.0f), "VOICES", voiceCount);
    drawStat (area.withTrimmedLeft (4.0f), "SINGERS", singerCount);
    drawIncisedVertical (g, half.getRight() - 1.0f, b.getY() + 6.0f, b.getBottom() - 6.0f);
}

} // namespace monolith::ui
