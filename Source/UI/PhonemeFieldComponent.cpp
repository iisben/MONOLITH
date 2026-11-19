#include "PhonemeFieldComponent.h"
#include "../State/ParameterIDs.h"
#include <algorithm>
#include <cmath>

namespace monolith::ui
{

PhonemeFieldComponent::PhonemeFieldComponent (juce::AudioProcessorValueTreeState& apvts)
{
    setTooltip ("2D Phoneme & Acoustic Field: Drag to morph across 7 physical vowel tracts. Double-click to reset.");

    if (auto* px = apvts.getParameter (params::phonemeX))
    {
        xAttach = std::make_unique<juce::ParameterAttachment> (
            *px,
            [this] (float v)
            {
                anchorX = std::clamp (v, 0.0f, 1.0f);
                repaint();
            });
        xAttach->sendInitialUpdate();
    }

    if (auto* py = apvts.getParameter (params::phonemeY))
    {
        yAttach = std::make_unique<juce::ParameterAttachment> (
            *py,
            [this] (float v)
            {
                anchorY = std::clamp (v, 0.0f, 1.0f);
                repaint();
            });
        yAttach->sendInitialUpdate();
    }

    for (auto& p : orbitTrail)
        p = { anchorX, anchorY };
}

juce::Rectangle<float> PhonemeFieldComponent::getFieldRect() const noexcept
{
    return getLocalBounds().toFloat().reduced (18.0f, 26.0f);
}

juce::Point<float> PhonemeFieldComponent::normToScreen (float nx, float ny) const noexcept
{
    const auto r = getFieldRect();
    return {
        r.getX() + std::clamp (nx, 0.0f, 1.0f) * r.getWidth(),
        r.getBottom() - std::clamp (ny, 0.0f, 1.0f) * r.getHeight()
    };
}

void PhonemeFieldComponent::updateFromTelemetry (const TelemetrySnapshot& telemetry) noexcept
{
    liveEffX      = telemetry.effectivePhonemeX.load (std::memory_order_relaxed);
    liveEffY      = telemetry.effectivePhonemeY.load (std::memory_order_relaxed);
    const float rawEnergy = telemetry.audioEnergy.load (std::memory_order_relaxed);
    audioEnergy   = 0.72f * audioEnergy + 0.28f * rawEnergy;
    breathDensity = 0.76f * breathDensity + 0.24f * telemetry.breathDensity.load (std::memory_order_relaxed);
    swarmSpread   = telemetry.swarmSpreadLive.load (std::memory_order_relaxed);
    freezeGlow    = 0.80f * freezeGlow + 0.20f * telemetry.freezeEnergy.load (std::memory_order_relaxed);
    activeSingers = telemetry.activeSingers.load (std::memory_order_relaxed);

    for (size_t i = 0; i < 5; ++i)
    {
        const float fVal = telemetry.formantEnergy[i].load (std::memory_order_relaxed);
        formantEnergy[i] = 0.70f * formantEnergy[i] + 0.30f * fVal;
    }

    trailHead = (trailHead + 1) % kTrailLen;
    orbitTrail[static_cast<size_t> (trailHead)] = { liveEffX, liveEffY };

    animPhase += 0.032f;
    if (animPhase > 6283.18f)
        animPhase = 0.0f;

    repaint();
}

void PhonemeFieldComponent::setFromPoint (juce::Point<float> pt, bool isInitialClick)
{
    const auto r = getFieldRect();
    if (r.getWidth() <= 1.0f || r.getHeight() <= 1.0f)
        return;

    float nx = std::clamp ((pt.x - r.getX()) / r.getWidth(), 0.0f, 1.0f);
    float ny = std::clamp ((r.getBottom() - pt.y) / r.getHeight(), 0.0f, 1.0f);

    if (isInitialClick)
    {
        // Click directly on an anchor badge snaps to that curated phoneme
        for (const auto& anchor : dsp::PhonemeField::getAnchors())
        {
            const auto scr = normToScreen (anchor.x, anchor.y);
            if (scr.getDistanceFrom (pt) < 15.0f)
            {
                nx = anchor.x;
                ny = anchor.y;
                break;
            }
        }
    }

    anchorX = nx;
    anchorY = ny;
    if (xAttach) xAttach->setValueAsPartOfGesture (nx);
    if (yAttach) yAttach->setValueAsPartOfGesture (ny);
    repaint();
}

void PhonemeFieldComponent::mouseDown (const juce::MouseEvent& e)
{
    dragging = true;
    if (xAttach) xAttach->beginGesture();
    if (yAttach) yAttach->beginGesture();
    setFromPoint (e.position, true);
    dragStartPos = e.position;
    dragStartX   = anchorX;
    dragStartY   = anchorY;
}

void PhonemeFieldComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isShiftDown())
    {
        const auto r = getFieldRect();
        const float dx = (e.position.x - dragStartPos.x) / std::max (r.getWidth(), 1.0f) * 0.20f;
        const float dy = -(e.position.y - dragStartPos.y) / std::max (r.getHeight(), 1.0f) * 0.20f;
        const float nx = std::clamp (dragStartX + dx, 0.0f, 1.0f);
        const float ny = std::clamp (dragStartY + dy, 0.0f, 1.0f);
        anchorX = nx;
        anchorY = ny;
        if (xAttach) xAttach->setValueAsPartOfGesture (nx);
        if (yAttach) yAttach->setValueAsPartOfGesture (ny);
        repaint();
        return;
    }
    setFromPoint (e.position, false);
}

void PhonemeFieldComponent::mouseUp (const juce::MouseEvent& e)
{
    (void) e;
    if (dragging)
    {
        dragging = false;
        if (xAttach) xAttach->endGesture();
        if (yAttach) yAttach->endGesture();
    }
}

void PhonemeFieldComponent::mouseDoubleClick (const juce::MouseEvent& e)
{
    (void) e;
    anchorX = params::defaults::phonemeX;
    anchorY = params::defaults::phonemeY;
    if (xAttach) xAttach->setValueAsCompleteGesture (anchorX);
    if (yAttach) yAttach->setValueAsCompleteGesture (anchorY);
    repaint();
}

void PhonemeFieldComponent::paint (juce::Graphics& g)
{
    const auto outer = getLocalBounds().toFloat().reduced (0.5f);

    // Panel backdrop
    g.setColour (palette::panelSlate);
    g.fillRoundedRectangle (outer, 3.0f);
    g.setColour (palette::gridHairline);
    g.drawRoundedRectangle (outer, 3.0f, 1.0f);

    // Header label strip
    g.setColour (palette::mutedLimestone);
    g.setFont (makeFont (10.0f, true, false));
    g.drawText ("2D PHONEME & VOCAL TRACT OBSERVATORY",
                outer.reduced (12.0f, 6.0f).withHeight (14.0f),
                juce::Justification::centredLeft,
                false);

    const auto field = getFieldRect();

    // Inner obsidian acoustic chamber
    g.setColour (palette::obsidianBg);
    g.fillRoundedRectangle (field, 2.5f);

    // Architectural grid hairlines & concentric acoustic rings
    g.saveState();
    g.reduceClipRegion (field.toNearestInt());

    const auto fieldCentre = field.getCentre();
    g.setColour (palette::gridHairline.withAlpha (0.45f));
    for (int i = 1; i < 4; ++i)
    {
        const float fx = field.getX() + field.getWidth() * (static_cast<float> (i) * 0.25f);
        const float fy = field.getY() + field.getHeight() * (static_cast<float> (i) * 0.25f);
        g.drawVerticalLine (static_cast<int> (fx), field.getY(), field.getBottom());
        g.drawHorizontalLine (static_cast<int> (fy), field.getX(), field.getRight());
    }

    for (float rad : { 56.0f, 112.0f, 168.0f })
        g.drawEllipse (fieldCentre.x - rad, fieldCentre.y - rad, rad * 2.0f, rad * 2.0f, 0.8f);

    // Vowel polygon connecting outer perimeter states (OH -> AH -> EH -> EE -> WHISPER -> HUM -> OO -> OH)
    const auto& anchors = dsp::PhonemeField::getAnchors();
    constexpr std::array<size_t, 7> perimeterOrder { 1, 0, 3, 4, 6, 5, 2 };
    juce::Path vowelPoly;
    for (size_t idx = 0; idx < perimeterOrder.size(); ++idx)
    {
        const auto p = normToScreen (anchors[perimeterOrder[idx]].x, anchors[perimeterOrder[idx]].y);
        if (idx == 0) vowelPoly.startNewSubPath (p);
        else          vowelPoly.lineTo (p);
    }
    vowelPoly.closeSubPath();
    g.setColour (palette::borderBright.withAlpha (0.45f));
    g.strokePath (vowelPoly, juce::PathStrokeType (1.0f));

    // Reactive breath mist + frozen cloud aura around live effective phoneme node
    const auto livePt = normToScreen (liveEffX, liveEffY);
    const auto handlePt = normToScreen (anchorX, anchorY);

    const float mistRadius = 70.0f + 85.0f * breathDensity + 65.0f * audioEnergy;
    const juce::Colour mistCol = (freezeGlow > 0.12f)
        ? palette::paleCyan.withAlpha (0.14f + 0.16f * freezeGlow)
        : palette::moltenAmber.withAlpha (0.07f + 0.14f * audioEnergy + 0.08f * breathDensity);

    juce::ColourGradient auraGrad (mistCol, livePt.x, livePt.y,
                                   juce::Colours::transparentBlack, livePt.x + mistRadius, livePt.y, true);
    g.setGradientFill (auraGrad);
    g.fillEllipse (livePt.x - mistRadius, livePt.y - mistRadius, mistRadius * 2.0f, mistRadius * 2.0f);

    // 5 concentric reactive vocal-tract formant rings (F1..F5)
    for (size_t f = 0; f < 5; ++f)
    {
        const float energy = formantEnergy[f];
        const float baseR  = 14.0f + static_cast<float> (f) * 14.0f;
        const float pulseR = baseR + energy * (12.0f + 4.0f * static_cast<float> (f))
                           + std::sin (animPhase * (1.2f + 0.3f * static_cast<float> (f))) * 1.5f;
        const float alpha  = std::clamp (0.12f + 0.55f * energy + 0.18f * audioEnergy, 0.08f, 0.78f);
        const auto ringCol = (f < 2) ? palette::moltenAmber : palette::paleCyan;
        g.setColour (ringCol.withAlpha (alpha));
        g.drawEllipse (livePt.x - pulseR, livePt.y - pulseR, pulseR * 2.0f, pulseR * 2.0f, 1.2f + energy * 0.9f);
    }

    // Orbit trail when effective position diverges from anchor
    if (livePt.getDistanceFrom (handlePt) > 2.5f)
    {
        juce::Path trail;
        bool started = false;
        for (int i = 0; i < kTrailLen; ++i)
        {
            const int idx = (trailHead + 1 + i) % kTrailLen;
            const auto& ptN = orbitTrail[static_cast<size_t> (idx)];
            const auto ptS = normToScreen (ptN.x, ptN.y);
            if (! started) { trail.startNewSubPath (ptS); started = true; }
            else           { trail.lineTo (ptS); }
        }
        g.setColour (palette::paleCyan.withAlpha (0.40f));
        g.strokePath (trail, juce::PathStrokeType (1.4f));

        // Tether line between anchor handle and orbit-displaced node
        const float dashes[2] { 3.0f, 3.0f };
        juce::Path tether;
        tether.startNewSubPath (handlePt);
        tether.lineTo (livePt);
        g.setColour (palette::mutedLimestone.withAlpha (0.50f));
        juce::PathStrokeType (1.0f).createDashedStroke (tether, tether, dashes, 2);
        g.fillPath (tether);
    }

    // Orbiting swarm motes representing virtual singers
    const int numMotes = std::clamp (activeSingers > 0 ? activeSingers : 12, 1, 24);
    const float orbitR = 14.0f + 48.0f * swarmSpread + 14.0f * audioEnergy;
    for (int m = 0; m < numMotes; ++m)
    {
        const float frac  = static_cast<float> (m) / static_cast<float> (numMotes);
        const float ang   = frac * 6.2831853f + animPhase * (0.45f + 0.15f * (m % 3));
        const float rad   = orbitR * (0.55f + 0.45f * std::sin (frac * 17.3f + animPhase * 0.4f));
        const float mx    = livePt.x + std::cos (ang) * rad;
        const float my    = livePt.y + std::sin (ang) * rad;
        const float mSize = (activeSingers > 0) ? (2.2f + 1.6f * audioEnergy) : 1.7f;
        const auto mCol   = (m % 3 == 0) ? palette::paleCyan : palette::moltenAmber;
        g.setColour (mCol.withAlpha (activeSingers > 0 ? (0.45f + 0.45f * audioEnergy) : 0.25f));
        g.fillEllipse (mx - mSize * 0.5f, my - mSize * 0.5f, mSize, mSize);
    }

    // 7 Vowel Anchor Nodes
    int nearestIdx = 0;
    float bestDist = 1.0e9f;
    for (size_t k = 0; k < anchors.size(); ++k)
    {
        const float dx = liveEffX - anchors[k].x;
        const float dy = liveEffY - anchors[k].y;
        const float d2 = dx * dx + dy * dy;
        if (d2 < bestDist)
        {
            bestDist   = d2;
            nearestIdx = static_cast<int> (k);
        }

        const auto p = normToScreen (anchors[k].x, anchors[k].y);
        const float proximity = std::clamp (1.0f - std::sqrt (d2) * 2.4f, 0.0f, 1.0f);

        g.setColour (palette::panelElevated.withAlpha (0.90f));
        g.fillEllipse (p.x - 4.0f, p.y - 4.0f, 8.0f, 8.0f);
        g.setColour (proximity > 0.35f ? palette::moltenAmber : palette::borderBright);
        g.drawEllipse (p.x - 4.0f, p.y - 4.0f, 8.0f, 8.0f, 1.2f);

        g.setColour (proximity > 0.4f ? palette::boneWhite : palette::mutedLimestone);
        g.setFont (makeFont (9.8f, proximity > 0.4f, false));
        const float labelY = (anchors[k].y > 0.78f) ? (p.y - 18.0f) : (p.y + 7.0f);
        g.drawText (anchors[k].code,
                    juce::Rectangle<float> (p.x - 28.0f, labelY, 56.0f, 12.0f),
                    juce::Justification::centred,
                    false);
    }

    // Glowing effective acoustic node
    g.setColour (palette::moltenAmber.withAlpha (0.35f));
    g.fillEllipse (livePt.x - 10.0f, livePt.y - 10.0f, 20.0f, 20.0f);
    g.setColour (palette::moltenAmber);
    g.fillEllipse (livePt.x - 5.0f, livePt.y - 5.0f, 10.0f, 10.0f);

    // Interactive anchor handle
    g.setColour (palette::boneWhite);
    g.drawEllipse (handlePt.x - 7.5f, handlePt.y - 7.5f, 15.0f, 15.0f, 1.7f);
    g.fillEllipse (handlePt.x - 2.2f, handlePt.y - 2.2f, 4.4f, 4.4f);

    g.restoreState();

    // Inner border
    g.setColour (palette::gridHairline);
    g.drawRoundedRectangle (field, 2.5f, 1.0f);

    // Bottom status bar inside panel: Coordinates + Dominant Vowel
    const auto footer = outer.reduced (18.0f, 6.0f).withTop (outer.getBottom() - 20.0f);
    g.setColour (palette::mutedLimestone);
    g.setFont (makeFont (10.0f, false, true));
    g.drawText ("X " + juce::String (anchorX, 2) + "   Y " + juce::String (anchorY, 2),
                footer, juce::Justification::centredLeft, false);

    const auto& nearest = anchors[static_cast<size_t> (nearestIdx)];
    g.setColour (palette::moltenAmber);
    g.setFont (makeFont (10.0f, true, false));
    g.drawText (juce::String (nearest.code) + "  ·  " + juce::String (nearest.name).toUpperCase(),
                footer, juce::Justification::centredRight, false);
}

} // namespace monolith::ui
