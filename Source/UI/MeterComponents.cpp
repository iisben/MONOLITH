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
      accentColour (accentName == "cyan" ? palette::paleCyan : palette::moltenAmber)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setMouseDragSensitivity (isMacro ? 190 : 165);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    slider.setTooltip (tooltipText);
    slider.getProperties().set (MonolithLookAndFeel::accentPropertyId, accentName);
    slider.getProperties().set (MonolithLookAndFeel::macroStylePropertyId, isMacro);
    slider.getProperties().set ("monolithParamId", paramId); // V2: right-click MIDI Learn target
    getProperties().set ("monolithParamId", paramId);

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
    const bool hoverOrDrag = slider.isMouseOverOrDragging();

    if (macroMode)
    {
        // Macro caption above + value & micro-subtitle below
        g.setColour (hoverOrDrag ? palette::boneWhite : palette::boneWhite.withAlpha (0.90f));
        g.setFont (makeFont (11.5f, true, false));
        g.drawText (title, b.withHeight (16.0f), juce::Justification::centred, false);

        const float bottomY = b.getBottom() - 26.0f;
        g.setColour (hoverOrDrag ? accentColour : palette::boneWhite);
        g.setFont (makeFont (11.5f, true, true));
        g.drawText (formatCurrentValue(), juce::Rectangle<float> (b.getX(), bottomY, b.getWidth(), 13.0f),
                    juce::Justification::centred, false);

        if (subCaption.isNotEmpty())
        {
            g.setColour (palette::dimAsh);
            g.setFont (makeFont (8.5f, true, false));
            g.drawText (subCaption, juce::Rectangle<float> (b.getX(), bottomY + 13.0f, b.getWidth(), 11.0f),
                        juce::Justification::centred, false);
        }
        return;
    }

    // Standard panel dial: label top, live readout bottom
    g.setColour (hoverOrDrag ? palette::boneWhite : palette::mutedLimestone);
    g.setFont (makeFont (9.5f, true, false));
    g.drawText (title, b.withHeight (13.0f), juce::Justification::centred, false);

    g.setColour (hoverOrDrag ? accentColour : palette::boneWhite.withAlpha (0.88f));
    g.setFont (makeFont (9.5f, hoverOrDrag, true));
    g.drawText (formatCurrentValue(),
                b.withTop (b.getBottom() - 13.0f),
                juce::Justification::centred,
                false);
}

void MonolithDial::resized()
{
    auto b = getLocalBounds();
    if (macroMode)
        slider.setBounds (b.withTrimmedTop (15).withTrimmedBottom (26));
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
    const auto b = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (palette::obsidianBg);
    g.fillRoundedRectangle (b, 2.0f);
    g.setColour (palette::gridHairline);
    g.drawRoundedRectangle (b, 2.0f, 1.0f);

    const auto inner = b.reduced (4.0f, 5.0f);
    const float barH = (inner.getHeight() - 3.0f) * 0.5f;

    auto drawBar = [&g] (juce::Rectangle<float> rect, float val, float hold)
    {
        g.setColour (palette::panelElevated);
        g.fillRect (rect);

        const float fillW = rect.getWidth() * std::clamp (val, 0.0f, 1.0f);
        if (fillW > 0.5f)
        {
            juce::ColourGradient grad (palette::paleCyan, rect.getX(), rect.getY(),
                                       palette::moltenAmber, rect.getRight(), rect.getY(), false);
            g.setGradientFill (grad);
            g.fillRect (rect.withWidth (fillW));
        }

        if (hold > 0.01f)
        {
            const float hx = rect.getX() + rect.getWidth() * std::clamp (hold, 0.0f, 1.0f);
            g.setColour (hold > 0.94f ? palette::crimsonAlert : palette::boneWhite);
            g.fillRect (hx - 1.0f, rect.getY(), 2.0f, rect.getHeight());
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
    const auto b = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (palette::obsidianBg);
    g.fillRoundedRectangle (b, 2.5f);
    g.setColour (isMouseOverOrDragging() ? palette::borderBright : palette::gridHairline);
    g.drawRoundedRectangle (b, 2.5f, 1.0f);

    const auto track = b.reduced (6.0f, 18.0f).translated (0.0f, 4.0f);
    g.setColour (palette::panelElevated);
    g.fillRect (track);

    const float val = liveVal;
    const float fillW = track.getWidth() * std::clamp (val, 0.0f, 1.0f);
    if (fillW > 0.5f)
    {
        juce::ColourGradient grad (palette::paleCyan.withAlpha (0.75f), track.getX(), track.getY(),
                                   palette::moltenAmber, track.getRight(), track.getY(), false);
        g.setGradientFill (grad);
        g.fillRect (track.withWidth (fillW));
    }

    // Precision fader thumb line
    const float tx = track.getX() + fillW;
    g.setColour (palette::boneWhite);
    g.fillRect (tx - 1.0f, track.getY() - 3.0f, 2.0f, track.getHeight() + 6.0f);

    g.setColour (palette::mutedLimestone);
    g.setFont (makeFont (9.5f, true, false));
    g.drawText ("EXPRESSION  /  CC1", b.reduced (7.0f, 4.0f).withHeight (13.0f), juce::Justification::centredLeft, false);

    g.setColour (palette::boneWhite);
    g.setFont (makeFont (10.0f, true, true));
    g.drawText (juce::String (static_cast<int> (std::lround (val * 100.0f))) + "%",
                b.reduced (7.0f, 4.0f).withHeight (13.0f),
                juce::Justification::centredRight,
                false);
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
    const auto b = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (palette::obsidianBg);
    g.fillRoundedRectangle (b, 2.5f);
    g.setColour (palette::gridHairline);
    g.drawRoundedRectangle (b, 2.5f, 1.0f);

    const bool active = voiceCount > 0;
    g.setColour (active ? palette::paleCyan : palette::dimAsh);
    g.fillEllipse (b.getX() + 7.0f, b.getCentreY() - 2.5f, 5.0f, 5.0f);

    g.setColour (active ? palette::boneWhite : palette::mutedLimestone);
    g.setFont (makeFont (9.8f, true, true));
    const juce::String txt = "VOICES " + juce::String (voiceCount).paddedLeft ('0', 2)
                           + "  |  SINGERS " + juce::String (singerCount).paddedLeft ('0', 2);
    g.drawText (txt, b.withTrimmedLeft (15.0f).withTrimmedRight (4.0f), juce::Justification::centred, false);
}

} // namespace monolith::ui
