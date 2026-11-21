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
    auto r = getLocalBounds().toFloat();
    return r.withTrimmedLeft (36.0f).withTrimmedTop (40.0f).withTrimmedRight (62.0f).withTrimmedBottom (40.0f);
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
    drawSlabPanel (g, surface, getLocalBounds(), 29u, "III", "VOWEL PLAN", palette::moltenAmber);

    const auto field = getFieldRect();
    const auto& anchors = dsp::PhonemeField::getAnchors();
    const bool frozen = freezeGlow > 0.12f;
    const auto datumCol = frozen ? palette::paleCyan : palette::moltenAmber;

    // The drawing is cut into the slab
    drawRecess (g, field.expanded (6.0f), palette::recess);

    // Rulers (left + bottom), drafting style
    g.setFont (makeRoleFont (FontRole::Numeric, 8.0f, false));
    for (int i = 0; i <= 20; ++i)
    {
        const float t = (float) i / 20.0f;
        const float len = (i % 10 == 0) ? 7.0f : ((i % 2 == 0) ? 4.5f : 2.5f);
        const float rx = field.getX() + t * field.getWidth();
        const float ry = field.getBottom() - t * field.getHeight();
        g.setColour (palette::dimAsh.brighter (0.2f));
        g.fillRect (rx, field.getBottom() + 7.0f, 1.0f, len);
        g.fillRect (field.getX() - 7.0f - len, ry, len, 1.0f);
        if (i % 10 == 0)
        {
            g.setColour (palette::mutedLimestone);
            const auto lab = juce::String (t, 1); // left ruler only: the bottom edge carries the legend
            g.drawText (lab, juce::Rectangle<float> (field.getX() - 36.0f, ry - 5.0f, 18.0f, 10.0f), juce::Justification::centredRight, false);
        }
    }

    g.saveState();
    g.reduceClipRegion (field.expanded (5.0f).toNearestInt());

    // Survey dot grid; every fourth intersection marked with a cross
    const float step = field.getWidth() / 24.0f;
    for (int ix = 0; ix <= 24; ++ix)
        for (int iy = 0; (float) iy * step <= field.getHeight() + 0.5f; ++iy)
        {
            const float px = field.getX() + (float) ix * step;
            const float py = field.getBottom() - (float) iy * step;
            if (ix % 4 == 0 && iy % 4 == 0)
            {
                g.setColour (palette::dimAsh.withAlpha (0.8f));
                g.fillRect (px - 2.5f, py, 6.0f, 1.0f);
                g.fillRect (px, py - 2.5f, 1.0f, 6.0f);
            }
            else
            {
                g.setColour (palette::dimAsh.withAlpha (0.45f));
                g.fillRect (px, py, 1.0f, 1.0f);
            }
        }

    // The vowel perimeter drawn as a wall: double line with a hollow core
    constexpr std::array<size_t, 7> perimeterOrder { 1, 0, 3, 4, 6, 5, 2 };
    juce::Path wall;
    for (size_t idx = 0; idx < perimeterOrder.size(); ++idx)
    {
        const auto p = normToScreen (anchors[perimeterOrder[idx]].x, anchors[perimeterOrder[idx]].y);
        if (idx == 0) wall.startNewSubPath (p);
        else          wall.lineTo (p);
    }
    wall.closeSubPath();
    g.setColour (palette::borderBright.withAlpha (0.75f));
    g.strokePath (wall, juce::PathStrokeType (4.0f, juce::PathStrokeType::mitered));
    g.setColour (palette::recess);
    g.strokePath (wall, juce::PathStrokeType (2.0f, juce::PathStrokeType::mitered));

    const auto livePt   = normToScreen (liveEffX, liveEffY);
    const auto handlePt = normToScreen (anchorX, anchorY);

    // Construction lines locating the sounding point on both rulers
    {
        const float dashes[2] { 2.0f, 4.0f };
        juce::Path h, v;
        h.startNewSubPath (field.getX() - 6.0f, livePt.y);  h.lineTo (field.getRight() + 6.0f, livePt.y);
        v.startNewSubPath (livePt.x, field.getY() - 6.0f);  v.lineTo (livePt.x, field.getBottom() + 6.0f);
        juce::PathStrokeType st (1.0f);
        st.createDashedStroke (h, h, dashes, 2);
        st.createDashedStroke (v, v, dashes, 2);
        g.setColour (datumCol.withAlpha (0.38f));
        g.fillPath (h);
        g.fillPath (v);
    }

    // Compass arcs: radius follows the sound (energy) and the breath
    {
        const float dashes[2] { 5.0f, 4.0f };
        for (int k = 0; k < 2; ++k)
        {
            const float r = (k == 0) ? 22.0f + 70.0f * audioEnergy : 30.0f + 60.0f * breathDensity + 20.0f * audioEnergy;
            juce::Path circ;
            circ.addEllipse (livePt.x - r, livePt.y - r, r * 2.0f, r * 2.0f);
            juce::PathStrokeType (0.9f).createDashedStroke (circ, circ, dashes, 2);
            g.setColour ((k == 0 ? palette::mutedLimestone : palette::paleCyan).withAlpha (0.20f + 0.35f * (k == 0 ? audioEnergy : breathDensity)));
            g.fillPath (circ);
        }
    }

    // Orbit path (verdigris) and tether to the hand-placed point
    if (livePt.getDistanceFrom (handlePt) > 2.5f)
    {
        juce::Path trail;
        bool started = false;
        for (int i = 0; i < kTrailLen; ++i)
        {
            const int idx = (trailHead + 1 + i) % kTrailLen;
            const auto ptS = normToScreen (orbitTrail[static_cast<size_t> (idx)].x, orbitTrail[static_cast<size_t> (idx)].y);
            if (! started) { trail.startNewSubPath (ptS); started = true; }
            else           { trail.lineTo (ptS); }
        }
        g.setColour (palette::paleCyan.withAlpha (0.65f));
        g.strokePath (trail, juce::PathStrokeType (1.2f));

        const float dashes[2] { 3.0f, 3.0f };
        juce::Path tether;
        tether.startNewSubPath (handlePt);
        tether.lineTo (livePt);
        juce::PathStrokeType (1.0f).createDashedStroke (tether, tether, dashes, 2);
        g.setColour (palette::mutedLimestone.withAlpha (0.6f));
        g.fillPath (tether);
    }

    // The choir in its stalls: singers seated on arcs of an apse around the sounding point
    {
        const int singers = std::clamp (activeSingers > 0 ? activeSingers : 12, 1, 24);
        const float baseR = 16.0f + 26.0f * swarmSpread;
        int placed = 0;
        for (int row = 0; row < 2 && placed < singers; ++row)
        {
            const int inRow = (row == 0) ? std::min (singers - placed, std::max (1, singers / 2)) : singers - placed;
            const float r = baseR + (float) row * 9.0f + std::sin (animPhase * 0.7f + (float) row) * 0.8f * audioEnergy;
            for (int k = 0; k < inRow; ++k, ++placed)
            {
                // a 200-degree arc opening upward, like stalls facing the altar
                const float t = (inRow == 1) ? 0.5f : (float) k / (float) (inRow - 1);
                const float a = juce::MathConstants<float>::pi * (0.5f + 1.1f * (t - 0.5f));
                const float sx = livePt.x + std::cos (a) * r;
                const float sy = livePt.y + std::sin (a) * r * 0.82f;
                const float sz = 2.2f;
                g.setColour (palette::boneWhite.withAlpha (activeSingers > 0 ? 0.35f + 0.35f * audioEnergy : 0.18f));
                g.fillRect (sx - sz * 0.5f, sy - sz * 0.5f, sz, sz);
            }
        }
    }

    // Column bases (the seven vowels)
    int nearestIdx = 0;
    float bestDist = 1.0e9f;
    for (size_t k = 0; k < anchors.size(); ++k)
    {
        const float dx = liveEffX - anchors[k].x, dy = liveEffY - anchors[k].y;
        const float d2 = dx * dx + dy * dy;
        if (d2 < bestDist) { bestDist = d2; nearestIdx = (int) k; }
    }
    for (size_t k = 0; k < anchors.size(); ++k)
    {
        const auto p = normToScreen (anchors[k].x, anchors[k].y);
        const bool nearest = (int) k == nearestIdx;
        const auto base = juce::Rectangle<float> (9.0f, 9.0f).withCentre (p);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRect (base.translated (1.0f, 1.0f));
        g.setColour (nearest ? palette::moltenAmber : palette::panelElevated.brighter (0.2f));
        g.fillRect (base);
        g.setColour (palette::borderBright);
        g.drawRect (base, 1.0f);

        g.setColour (nearest ? palette::boneWhite : palette::mutedLimestone);
        g.setFont (makeRoleFont (FontRole::Inscription, 10.5f, true));
        const float labelY = (anchors[k].y > 0.78f) ? (p.y - 21.0f) : (p.y + 8.0f);
        drawTracked (g, anchors[k].code, juce::Rectangle<float> (p.x - 34.0f, labelY, 68.0f, 13.0f),
                     juce::Justification::centred, 0.18f);
    }

    // Datum: the sounding point (diamond) and the hand-placed point (open square)
    {
        juce::Path diamond;
        diamond.addRectangle (-4.5f, -4.5f, 9.0f, 9.0f);
        diamond.applyTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::pi * 0.25f).translated (livePt));
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillPath (diamond, juce::AffineTransform::translation (1.0f, 1.0f));
        g.setColour (datumCol);
        g.fillPath (diamond);

        const auto hs = juce::Rectangle<float> (15.0f, 15.0f).withCentre (handlePt);
        g.setColour (dragging ? juce::Colours::white : palette::boneWhite);
        g.drawRect (hs, 1.5f);
        g.fillRect (juce::Rectangle<float> (3.0f, 3.0f).withCentre (handlePt));
    }

    g.restoreState();

    // Ruler markers where the construction lines land
    {
        juce::Path tri;
        tri.addTriangle (livePt.x - 4.0f, field.getBottom() + 18.0f + 6.0f, livePt.x + 4.0f, field.getBottom() + 18.0f + 6.0f, livePt.x, field.getBottom() + 12.0f);
        tri.addTriangle (field.getX() - 22.0f, livePt.y - 4.0f, field.getX() - 22.0f, livePt.y + 4.0f, field.getX() - 16.0f, livePt.y);
        g.setColour (datumCol);
        g.fillPath (tri);
    }

    // Organ pipes: F1-F5 resonance energy
    {
        const float pipeArea = 46.0f;
        const auto pipes = juce::Rectangle<float> (field.getRight() + 14.0f, field.getY() + 6.0f, pipeArea, field.getHeight() - 6.0f);
        const float pw = 6.0f, gap = (pipes.getWidth() - 5.0f * pw) / 4.0f;
        g.setFont (makeRoleFont (FontRole::Numeric, 8.0f, false));
        g.setColour (palette::mutedLimestone);
        g.drawText ("F", juce::Rectangle<float> (pipes.getX(), pipes.getBottom() + 13.0f, pipes.getWidth(), 9.0f),
                    juce::Justification::centred, false);
        for (int f = 0; f < 5; ++f)
        {
            const float x = pipes.getX() + (float) f * (pw + gap);
            // pipes of a rank get shorter toward the treble
            const float maxH = pipes.getHeight() * (1.0f - 0.11f * (float) f);
            const auto bore = juce::Rectangle<float> (x, pipes.getBottom() - maxH, pw, maxH);
            drawRecess (g, bore);
            const float e = std::clamp (formantEnergy[(size_t) f], 0.0f, 1.0f);
            const float hh = (maxH - 2.0f) * e;
            if (hh > 0.5f)
            {
                g.setColour (palette::boneWhite.withAlpha (0.55f));
                g.fillRect (bore.getX() + 1.0f, bore.getBottom() - 1.0f - hh, pw - 2.0f, hh);
                g.setColour (palette::moltenAmber);
                g.fillRect (bore.getX() + 1.0f, bore.getBottom() - 1.0f - hh, pw - 2.0f, 1.5f);
            }
            // mouth of the pipe
            g.setColour (palette::recess);
            g.fillRect (bore.getX(), bore.getBottom() - maxH * 0.18f, pw, 2.0f);
            g.setColour (palette::mutedLimestone);
            g.drawText (juce::String (f + 1), juce::Rectangle<float> (x - 3.0f, pipes.getBottom() + 3.0f, pw + 6.0f, 9.0f),
                        juce::Justification::centred, false);
        }
    }

    // Legend: coordinates left, nearest vowel right
    const auto footer = juce::Rectangle<float> (field.getX(), (float) getHeight() - 18.0f, field.getWidth(), 12.0f);
    g.setColour (palette::mutedLimestone);
    g.setFont (makeRoleFont (FontRole::Numeric, 9.5f, false));
    g.drawText ("X " + juce::String (anchorX, 2) + "   Y " + juce::String (anchorY, 2),
                footer.translated (0.0f, -1.0f), juce::Justification::centredLeft, false);

    const auto& nearest = anchors[static_cast<size_t> (nearestIdx)];
    g.setColour (datumCol);
    g.setFont (makeRoleFont (FontRole::Inscription, 10.5f, true));
    drawTracked (g, juce::String (nearest.code) + juce::String::fromUTF8 ("  \u00B7  ")
                     + juce::String (nearest.name).toUpperCase() + (frozen ? juce::String::fromUTF8 ("  \u00B7  FROZEN") : juce::String()),
                 footer.translated (0.0f, -1.0f), juce::Justification::centredRight, 0.14f);
}

} // namespace monolith::ui
