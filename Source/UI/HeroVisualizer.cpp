#include "HeroVisualizer.h"
#include "../DSP/PhonemeField.h"
#include "../State/ParameterIDs.h"
#include <algorithm>
#include <cmath>

namespace monolith::ui
{

namespace
{
    constexpr float kSpecTop = 0.52f;     // spectrum band occupies the lower 48 % of the field
    constexpr float kSpecMinDb = -90.0f, kSpecMaxDb = -6.0f;

    struct AxisText { const char* left; const char* right; const char* top; const char* bottom; };
    constexpr std::array<AxisText, 3> kAxes {{
        { "BACK  \xc2\xb7  DARK", "FRONT  \xc2\xb7  BRIGHT", "OPEN", "CLOSED" },
        { "DEEP THROAT", "TIGHT FOCUS", "LIGHT", "SHADOW" },
        { "PRESET X  \xe2\x88\x92", "PRESET X  +", "PRESET Y  +", "PRESET Y  \xe2\x88\x92" },
    }};

    juce::RangedAudioParameter* ranged (MonolithAudioProcessor& p, const char* id)
    {
        return dynamic_cast<juce::RangedAudioParameter*> (p.getAPVTS().getParameter (id));
    }
}

// ============================================================================
SculptorField::SculptorField (MonolithAudioProcessor& p)
{
    setTooltip ("XY Sculptor: drag to sculpt. Vowel Field places the vowel anchor; Light / Shadow and Preset Axes drive the "
                "XY Sculptor (centre = neutral). Shift = fine, double-click = reset.");
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
    setOpaque (false);

    pPhX = ranged (p, params::phonemeX);
    pPhY = ranged (p, params::phonemeY);
    pXyX = ranged (p, params::v2XyX);
    pXyY = ranged (p, params::v2XyY);
    auto attach = [this] (juce::RangedAudioParameter* prm, float& dst) {
        auto a = std::make_unique<juce::ParameterAttachment> (*prm, [this, prm, &dst] (float v) { dst = prm->convertTo0to1 (v); repaint(); });
        a->sendInitialUpdate();
        return a;
    };
    aPhX = attach (pPhX, phX);
    aPhY = attach (pPhY, phY);
    aXyX = attach (pXyX, xyX);
    aXyY = attach (pXyY, xyY);
    if (auto* m = p.getAPVTS().getParameter (params::v2XyMode))
    {
        aMode = std::make_unique<juce::ParameterAttachment> (*m, [this] (float v) { mode = std::clamp (juce::roundToInt (v), 0, 2); repaint(); });
        aMode->sendInitialUpdate();
    }
    const auto h = handleNorm();
    handleTrail.fill (h);
    trailAge.fill (1.0f);
}

juce::Point<float> SculptorField::toScreen (float nx, float ny) const noexcept
{
    const auto r = field().reduced (16.0f, 14.0f);
    return { r.getX() + std::clamp (nx, 0.0f, 1.0f) * r.getWidth(), r.getBottom() - std::clamp (ny, 0.0f, 1.0f) * r.getHeight() };
}

juce::Point<float> SculptorField::handleNorm() const noexcept
{
    return mode == 0 ? juce::Point<float> { phX, phY } : juce::Point<float> { xyX, xyY };
}

juce::String SculptorField::getReadout() const
{
    const auto h = handleNorm();
    if (mode == 0)
        return "X " + juce::String (h.x, 2) + "   Y " + juce::String (h.y, 2);
    auto sgn = [] (float v) { const int pct = juce::roundToInt ((v - 0.5f) * 200.0f); return (pct > 0 ? "+" : "") + juce::String (pct); };
    return "X " + sgn (h.x) + "   Y " + sgn (h.y);
}

juce::String SculptorField::getNearestVowel() const
{
    const auto& anchors = dsp::PhonemeField::getAnchors();
    size_t best = 0;
    float bd = 1.0e9f;
    for (size_t k = 0; k < anchors.size(); ++k)
    {
        const float d = juce::square (effX - anchors[k].x) + juce::square (effY - anchors[k].y);
        if (d < bd) { bd = d; best = k; }
    }
    return juce::String (anchors[best].code) + juce::String::fromUTF8 ("  \xc2\xb7  ") + juce::String (anchors[best].name).toUpperCase();
}

void SculptorField::update (const TelemetrySnapshot& t, const OutputAnalyser& a, double dt)
{
    analyser = &a;
    const float k = static_cast<float> (1.0 - std::exp (-dt / 0.06));
    effX += (t.effectivePhonemeX.load (std::memory_order_relaxed) - effX) * std::min (1.0f, k * 2.0f);
    effY += (t.effectivePhonemeY.load (std::memory_order_relaxed) - effY) * std::min (1.0f, k * 2.0f);
    energy += (t.audioEnergy.load (std::memory_order_relaxed) - energy) * k;
    breath += (t.breathDensity.load (std::memory_order_relaxed) - breath) * k;
    spread = t.swarmSpreadLive.load (std::memory_order_relaxed);
    freeze += (t.freezeEnergy.load (std::memory_order_relaxed) - freeze) * k;
    singers = t.activeSingers.load (std::memory_order_relaxed);
    voices = t.activeVoices.load (std::memory_order_relaxed);
    for (size_t f = 0; f < formantEnergy.size(); ++f)
        formantEnergy[f] += (t.formantEnergy[f].load (std::memory_order_relaxed) - formantEnergy[f]) * k;

    phase = std::fmod (phase + static_cast<float> (dt) * 1.9f, juce::MathConstants<float>::twoPi * 64.0f);

    for (auto& age : trailAge)
        age = std::min (1.0f, age + static_cast<float> (dt) / 1.1f);
    if (dragging)
    {
        trailHead = (trailHead + 1) % kTrail;
        handleTrail[static_cast<size_t> (trailHead)] = handleNorm();
        trailAge[static_cast<size_t> (trailHead)] = 0.0f;
    }
    repaint();
}

// ── Static layer ─────────────────────────────────────────────────────────────
void SculptorField::renderStatic (juce::Graphics& g, juce::Rectangle<float> r) const
{
    chrome::drawWell (g, r, 4.0f);
    const auto inner = r.reduced (16.0f, 14.0f);

    // Fine architectural grid (eighths, quarters stronger)
    for (int i = 1; i < 8; ++i)
    {
        const float fx = inner.getX() + inner.getWidth() * static_cast<float> (i) / 8.0f;
        const float fy = inner.getY() + inner.getHeight() * static_cast<float> (i) / 8.0f;
        g.setColour (palette::hairline.withAlpha (i % 2 == 0 ? 0.55f : 0.28f));
        g.fillRect (fx, inner.getY(), 1.0f, inner.getHeight());
        g.fillRect (inner.getX(), fy, inner.getWidth(), 1.0f);
    }

    // Frequency scale for the contour (bottom band)
    const float specTop = inner.getY() + inner.getHeight() * kSpecTop;
    g.setFont (monoFont (8.5f));
    for (float hz : { 100.0f, 250.0f, 1000.0f, 2500.0f, 10000.0f })
    {
        const float x = inner.getX() + inner.getWidth() * OutputAnalyser::hzToNorm (hz);
        g.setColour (palette::hairlineHi.withAlpha (0.45f));
        for (float y = specTop; y < inner.getBottom(); y += 4.0f)
            g.fillRect (x, y, 1.0f, 2.0f);
        if (hz == 1000.0f)
            continue; // the centre slot carries the vertical axis caption
        g.setColour (palette::ash.withAlpha (0.85f));
        const auto label = hz >= 1000.0f ? juce::String (hz / 1000.0f, hz < 10000.0f && std::fmod (hz, 1000.0f) > 1.0f ? 1 : 0) + "k" : juce::String (juce::roundToInt (hz));
        g.drawText (label, juce::Rectangle<float> (x + 3.0f, inner.getBottom() - 11.0f, 36.0f, 10.0f), juce::Justification::centredLeft, false);
    }

    // Neutral cross (sculptor modes) or vowel map (vowel mode)
    const auto& anchors = dsp::PhonemeField::getAnchors();
    constexpr std::array<size_t, 7> perimeter { 1, 0, 3, 4, 6, 5, 2 };
    juce::Path poly;
    for (size_t i = 0; i < perimeter.size(); ++i)
    {
        const auto p = toScreen (anchors[perimeter[i]].x, anchors[perimeter[i]].y);
        if (i == 0) poly.startNewSubPath (p); else poly.lineTo (p);
    }
    poly.closeSubPath();
    g.setColour (palette::hairlineHi.withAlpha (bgMode == 0 ? 0.75f : 0.35f));
    const float dash[] { 2.0f, 3.0f };
    juce::Path dashed;
    juce::PathStrokeType (1.0f).createDashedStroke (dashed, poly, dash, 2);
    g.fillPath (dashed);

    if (bgMode != 0)
    {
        const auto c = toScreen (0.5f, 0.5f);
        g.setColour (palette::stone.withAlpha (0.45f));
        g.fillRect (c.x - 8.0f, c.y - 0.5f, 16.0f, 1.0f);
        g.fillRect (c.x - 0.5f, c.y - 8.0f, 1.0f, 16.0f);
    }
    for (const auto& a : anchors)
    {
        const auto p = toScreen (a.x, a.y);
        g.setColour (palette::well);
        g.fillEllipse (p.x - 3.5f, p.y - 3.5f, 7.0f, 7.0f);
        g.setColour (palette::ash.withAlpha (bgMode == 0 ? 1.0f : 0.5f));
        g.drawEllipse (p.x - 3.5f, p.y - 3.5f, 7.0f, 7.0f, 1.0f);
    }
}

// ── Live layers ──────────────────────────────────────────────────────────────
void SculptorField::paintSpectrum (juce::Graphics& g, juce::Rectangle<float> r) const
{
    if (analyser == nullptr)
        return;
    const auto inner = r.reduced (16.0f, 14.0f);
    const float top = inner.getY() + inner.getHeight() * kSpecTop;
    const float bottom = inner.getBottom() - 12.0f;
    auto yOf = [&] (float db) { return bottom - (bottom - top) * std::clamp ((db - kSpecMinDb) / (kSpecMaxDb - kSpecMinDb), 0.0f, 1.0f); };
    auto xOf = [&] (int band) { return inner.getX() + inner.getWidth() * static_cast<float> (band) / static_cast<float> (OutputAnalyser::kNumBands - 1); };

    const auto& spec = analyser->getSpectrum();
    const auto& env = analyser->getEnvelope();

    juce::Path fill, line, envLine;
    fill.startNewSubPath (xOf (0), bottom);
    for (int b = 0; b < OutputAnalyser::kNumBands; ++b)
    {
        const float x = xOf (b), y = yOf (spec[static_cast<size_t> (b)]);
        fill.lineTo (x, y);
        if (b == 0) line.startNewSubPath (x, y); else line.lineTo (x, y);
        const float ye = yOf (env[static_cast<size_t> (b)]);
        if (b == 0) envLine.startNewSubPath (x, ye); else envLine.lineTo (x, ye);
    }
    fill.lineTo (xOf (OutputAnalyser::kNumBands - 1), bottom);
    fill.closeSubPath();

    g.setColour (palette::ivory.withAlpha (0.045f));
    g.fillPath (fill);
    g.setColour (palette::ivory.withAlpha (0.22f));
    g.strokePath (line, juce::PathStrokeType (1.0f));
    g.setColour (palette::amber.withAlpha (0.20f));
    g.strokePath (envLine, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (palette::amber.withAlpha (0.92f));
    g.strokePath (envLine, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Formant contour peaks
    g.setFont (capsFont (8.5f));
    for (int i = 0; i < analyser->getNumFormants(); ++i)
    {
        const auto& f = analyser->getFormants()[static_cast<size_t> (i)];
        const float x = inner.getX() + inner.getWidth() * OutputAnalyser::hzToNorm (f.hz);
        const float y = yOf (f.db);
        g.setColour (palette::amber.withAlpha (0.30f));
        for (float yy = y + 6.0f; yy < bottom; yy += 4.0f)
            g.fillRect (x - 0.5f, yy, 1.0f, 2.0f);
        g.setColour (palette::amber);
        g.fillEllipse (x - 2.5f, y - 2.5f, 5.0f, 5.0f);
        const juce::String txt = "F" + juce::String (i + 1) + " " + (f.hz >= 1000.0f ? juce::String (f.hz / 1000.0f, 2) + "k" : juce::String (juce::roundToInt (f.hz)));
        const float tw = 52.0f;
        const float tx = std::clamp (x - tw * 0.5f, inner.getX(), inner.getRight() - tw);
        g.setColour (palette::ivory.withAlpha (0.85f));
        g.drawText (txt, juce::Rectangle<float> (tx, y - 16.0f, tw, 10.0f), juce::Justification::centred, false);
    }
}

void SculptorField::paintSwarm (juce::Graphics& g) const
{
    const auto node = toScreen (effX, effY);
    const bool frozen = freeze > 0.12f;
    const auto glowCol = frozen ? palette::ivory : palette::amber;

    // Presence haze (light, not decoration: radius and intensity follow output energy and breath density)
    const float haze = 46.0f + 70.0f * energy + 40.0f * breath;
    juce::ColourGradient aura (glowCol.withAlpha (0.05f + 0.16f * energy + (frozen ? 0.1f * freeze : 0.0f)), node.x, node.y,
                               glowCol.withAlpha (0.0f), node.x + haze, node.y, true);
    g.setGradientFill (aura);
    g.fillEllipse (node.x - haze, node.y - haze, haze * 2.0f, haze * 2.0f);

    // Formant rings F1..F5
    for (size_t f = 0; f < formantEnergy.size(); ++f)
    {
        const float e = formantEnergy[f];
        const float rr = 12.0f + static_cast<float> (f) * 11.0f + e * (8.0f + 3.0f * static_cast<float> (f))
                       + std::sin (phase * (1.0f + 0.27f * static_cast<float> (f))) * 0.8f;
        g.setColour ((f < 2 ? palette::amber : palette::ivory).withAlpha (std::clamp (0.06f + 0.45f * e, 0.0f, 0.6f)));
        g.drawEllipse (node.x - rr, node.y - rr, rr * 2.0f, rr * 2.0f, 1.0f);
    }

    // Singer motes (one per active virtual singer)
    const int n = std::clamp (singers, 0, 24);
    const float orbitR = 18.0f + 54.0f * spread + 12.0f * energy;
    for (int m = 0; m < n; ++m)
    {
        const float frac = static_cast<float> (m) / static_cast<float> (std::max (1, n));
        const float ang = frac * juce::MathConstants<float>::twoPi + phase * (0.30f + 0.07f * static_cast<float> (m % 4));
        const float rad = orbitR * (0.45f + 0.55f * (0.5f + 0.5f * std::sin (frac * 23.7f + phase * 0.35f)));
        const juce::Point<float> p { node.x + std::cos (ang) * rad, node.y + std::sin (ang) * rad * 0.82f };
        const float s = 2.0f + 1.4f * energy;
        g.setColour ((m % 3 == 0 ? palette::ivory : palette::amber).withAlpha (0.35f + 0.5f * energy));
        g.fillEllipse (p.x - s * 0.5f, p.y - s * 0.5f, s, s);
    }

    // Vowel labels (nearest highlighted)
    const auto& anchors = dsp::PhonemeField::getAnchors();
    g.setFont (capsFont (9.0f));
    for (const auto& a : anchors)
    {
        const auto p = toScreen (a.x, a.y);
        const float prox = std::clamp (1.0f - std::sqrt (juce::square (effX - a.x) + juce::square (effY - a.y)) * 2.6f, 0.0f, 1.0f);
        g.setColour ((prox > 0.45f ? palette::ivory : palette::stone).withAlpha (mode == 0 ? 0.6f + 0.4f * prox : 0.35f + 0.3f * prox));
        const float ly = a.y > 0.78f ? p.y - 18.0f : p.y + 7.0f;
        g.drawText (a.code, juce::Rectangle<float> (p.x - 30.0f, ly, 60.0f, 11.0f), juce::Justification::centred, false);
    }

    // Effective node
    g.setColour (glowCol.withAlpha (0.30f));
    g.fillEllipse (node.x - 9.0f, node.y - 9.0f, 18.0f, 18.0f);
    g.setColour (glowCol);
    g.fillEllipse (node.x - 4.5f, node.y - 4.5f, 9.0f, 9.0f);
}

void SculptorField::paintHandle (juce::Graphics& g) const
{
    const auto hn = handleNorm();
    const auto h = toScreen (hn.x, hn.y);
    const auto node = toScreen (effX, effY);

    // Tether to the effective vowel node
    if (mode == 0 && h.getDistanceFrom (node) > 4.0f)
    {
        juce::Path tether;
        tether.startNewSubPath (h);
        tether.lineTo (node);
        const float dash[] { 3.0f, 3.0f };
        juce::Path d;
        juce::PathStrokeType (1.0f).createDashedStroke (d, tether, dash, 2);
        g.setColour (palette::stone.withAlpha (0.55f));
        g.fillPath (d);
    }

    // Glowing trail (recent drag history, fades over ~1.1 s)
    for (int i = 1; i < kTrail; ++i)
    {
        const int a = (trailHead - i + 1 + kTrail) % kTrail;
        const int b = (trailHead - i + kTrail) % kTrail;
        const float age = std::max (trailAge[static_cast<size_t> (a)], trailAge[static_cast<size_t> (b)]);
        if (age >= 1.0f)
            continue;
        const auto pa = toScreen (handleTrail[static_cast<size_t> (a)].x, handleTrail[static_cast<size_t> (a)].y);
        const auto pb = toScreen (handleTrail[static_cast<size_t> (b)].x, handleTrail[static_cast<size_t> (b)].y);
        const float alpha = (1.0f - age) * (1.0f - age);
        g.setColour (palette::amber.withAlpha (0.18f * alpha));
        g.drawLine ({ pa, pb }, 6.0f);
        g.setColour (palette::ivory.withAlpha (0.65f * alpha));
        g.drawLine ({ pa, pb }, 1.4f);
    }

    if (dragging)
    {
        const auto f = field().reduced (16.0f, 14.0f);
        g.setColour (palette::ivory.withAlpha (0.10f));
        g.fillRect (f.getX(), h.y - 0.5f, f.getWidth(), 1.0f);
        g.fillRect (h.x - 0.5f, f.getY(), 1.0f, f.getHeight());
    }

    g.setColour (palette::amber.withAlpha (dragging ? 0.30f : 0.16f));
    g.fillEllipse (h.x - 14.0f, h.y - 14.0f, 28.0f, 28.0f);
    g.setColour (palette::ivory);
    g.drawEllipse (h.x - 8.0f, h.y - 8.0f, 16.0f, 16.0f, 1.8f);
    g.fillEllipse (h.x - 2.0f, h.y - 2.0f, 4.0f, 4.0f);

    if (dragging)
    {
        const auto txt = getReadout();
        const float w = 116.0f;
        auto box = juce::Rectangle<float> (h.x + 16.0f, h.y - 30.0f, w, 18.0f);
        if (box.getRight() > static_cast<float> (getWidth()) - 6.0f) box.setX (h.x - 16.0f - w);
        if (box.getY() < 6.0f) box.setY (h.y + 14.0f);
        g.setColour (palette::panelRaised.withAlpha (0.94f));
        g.fillRoundedRectangle (box, 3.0f);
        g.setColour (palette::hairlineHi);
        g.drawRoundedRectangle (box.reduced (0.5f), 3.0f, 1.0f);
        g.setColour (palette::ivory);
        g.setFont (monoFont (10.0f));
        g.drawText (txt, box, juce::Justification::centred, false);
    }
}

void SculptorField::paint (juce::Graphics& g)
{
    if (bgMode != mode)
    {
        bgMode = mode;
        bg.invalidate();
        labels.invalidate();
    }
    const auto r = field();
    bg.draw (g, getLocalBounds(), [this] (juce::Graphics& sg, juce::Rectangle<float> rr) { renderStatic (sg, rr); });

    g.saveState();
    g.reduceClipRegion (getLocalBounds().reduced (2));
    paintSpectrum (g, r);
    paintSwarm (g);
    g.restoreState();
    labels.draw (g, getLocalBounds(), [this] (juce::Graphics& lg, juce::Rectangle<float> rr) { renderLabels (lg, rr); });
    g.saveState();
    g.reduceClipRegion (getLocalBounds().reduced (2));
    paintHandle (g);
    g.restoreState();
}

// ── Axis caption chips (cached, drawn above the live swarm) ─────────────────
void SculptorField::renderLabels (juce::Graphics& g, juce::Rectangle<float> r) const
{
    const auto inner = r.reduced (16.0f, 14.0f);
    const auto& ax = kAxes[static_cast<size_t> (std::clamp (bgMode, 0, 2))];
    const auto font = capsFont (8.5f);
    g.setFont (font);

    // Each caption sits on a small recessed chip so motes, haze and the contour pass underneath it.
    auto chip = [&] (const char* utf8, float anchorX, float y, juce::Justification just)
    {
        const auto text = juce::String::fromUTF8 (utf8);
        const float w = juce::GlyphArrangement::getStringWidth (font, text) + 12.0f;
        const float x = just == juce::Justification::centredLeft    ? anchorX
                      : just == juce::Justification::centredRight   ? anchorX - w
                                                                    : anchorX - w * 0.5f;
        const juce::Rectangle<float> box (x, y, w, 14.0f);
        g.setColour (palette::well.withAlpha (0.88f));
        g.fillRoundedRectangle (box, 3.0f);
        g.setColour (palette::stone.withAlpha (0.8f));
        g.drawText (text, box, juce::Justification::centred, false);
    };
    chip (ax.top, r.getCentreX(), r.getY() + 2.0f, juce::Justification::centred);
    chip (ax.left, r.getX() + 4.0f, r.getCentreY() - 24.0f, juce::Justification::centredLeft);
    chip (ax.right, r.getRight() - 4.0f, r.getCentreY() - 24.0f, juce::Justification::centredRight);
    chip (ax.bottom, r.getCentreX(), inner.getBottom() - 13.0f, juce::Justification::centred);
}

// ── Interaction ──────────────────────────────────────────────────────────────
void SculptorField::beginGestures()
{
    if (mode == 0) { aPhX->beginGesture(); aPhY->beginGesture(); }
    else           { aXyX->beginGesture(); aXyY->beginGesture(); }
}

void SculptorField::endGestures()
{
    if (mode == 0) { aPhX->endGesture(); aPhY->endGesture(); }
    else           { aXyX->endGesture(); aXyY->endGesture(); }
}

void SculptorField::setHandle (float nx, float ny)
{
    nx = std::clamp (nx, 0.0f, 1.0f);
    ny = std::clamp (ny, 0.0f, 1.0f);
    if (mode == 0)
    {
        phX = nx; phY = ny;
        aPhX->setValueAsPartOfGesture (pPhX->convertFrom0to1 (nx));
        aPhY->setValueAsPartOfGesture (pPhY->convertFrom0to1 (ny));
    }
    else
    {
        xyX = nx; xyY = ny;
        aXyX->setValueAsPartOfGesture (pXyX->convertFrom0to1 (nx));
        aXyY->setValueAsPartOfGesture (pXyY->convertFrom0to1 (ny));
    }
    repaint();
}

void SculptorField::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;
    const auto r = field().reduced (16.0f, 14.0f);
    float nx = (e.position.x - r.getX()) / r.getWidth();
    float ny = (r.getBottom() - e.position.y) / r.getHeight();
    if (mode == 0)
    {
        for (const auto& a : dsp::PhonemeField::getAnchors())
            if (toScreen (a.x, a.y).getDistanceFrom (e.position) < 14.0f) { nx = a.x; ny = a.y; break; }
    }
    else if (toScreen (0.5f, 0.5f).getDistanceFrom (e.position) < 10.0f)
    {
        nx = 0.5f; ny = 0.5f; // snap to neutral
    }

    dragging = true;
    beginGestures();
    setHandle (nx, ny);
    dragStart = e.position;
    dragStartNorm = handleNorm();
}

void SculptorField::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging)
        return;
    const auto r = field().reduced (16.0f, 14.0f);
    if (e.mods.isShiftDown() || e.mods.isCommandDown())
    {
        setHandle (dragStartNorm.x + (e.position.x - dragStart.x) / r.getWidth() * 0.2f,
                   dragStartNorm.y - (e.position.y - dragStart.y) / r.getHeight() * 0.2f);
        return;
    }
    dragStart = e.position;
    dragStartNorm = { (e.position.x - r.getX()) / r.getWidth(), (r.getBottom() - e.position.y) / r.getHeight() };
    setHandle (dragStartNorm.x, dragStartNorm.y);
}

void SculptorField::mouseUp (const juce::MouseEvent&)
{
    if (dragging)
    {
        dragging = false;
        endGestures();
        repaint();
    }
}

void SculptorField::mouseDoubleClick (const juce::MouseEvent&)
{
    if (mode == 0)
    {
        aPhX->setValueAsCompleteGesture (params::defaults::phonemeX);
        aPhY->setValueAsCompleteGesture (params::defaults::phonemeY);
    }
    else
    {
        aXyX->setValueAsCompleteGesture (params::defaults::v2XyX);
        aXyY->setValueAsCompleteGesture (params::defaults::v2XyY);
    }
}

// ============================================================================
HeroPanel::HeroPanel (MonolithAudioProcessor& p) : proc (p), fieldComp (p)
{
    static const char* const names[] { "VOWEL", "LIGHT", "AXES" };
    static const char* const tips[] {
        "Vowel Field: drag the vowel anchor across the 7 physical vowel tracts.",
        "Light / Shadow: X = throat size & formant focus, Y = brightness (crown, tension, texture air) vs shadow (sub).",
        "Preset Axes: X / Y drive the routes stored with the preset (macro routing editor, XY sources)." };
    static const char* const segs[] { "left", "mid", "right" };
    for (int i = 0; i < 3; ++i)
    {
        auto& b = modeButtons[static_cast<size_t> (i)];
        b.setButtonText (names[i]);
        b.setTooltip (tips[i]);
        b.setClickingTogglesState (false);
        b.getProperties().set (MonolithLookAndFeel::segmentPropertyId, segs[i]);
        b.onClick = [this, i] {
            if (auto* m = proc.getAPVTS().getParameter (params::v2XyMode); m != nullptr && modeAttach != nullptr)
                modeAttach->setValueAsCompleteGesture (static_cast<float> (i));
        };
        b.getProperties().set ("monolithParamId", params::v2XyMode);
        addAndMakeVisible (b);
    }
    if (auto* m = p.getAPVTS().getParameter (params::v2XyMode))
    {
        modeAttach = std::make_unique<juce::ParameterAttachment> (*m, [this] (float v) {
            const int idx = std::clamp (juce::roundToInt (v), 0, 2);
            for (int i = 0; i < 3; ++i)
                modeButtons[static_cast<size_t> (i)].setToggleState (i == idx, juce::dontSendNotification);
        });
        modeAttach->sendInitialUpdate();
    }
    addAndMakeVisible (fieldComp);
}

void HeroPanel::resized()
{
    chromeLayer.invalidate();
    const int w = getWidth();
    for (int i = 0; i < 3; ++i)
        modeButtons[static_cast<size_t> (i)].setBounds (w - 16 - 3 * 64 + i * 64, 4, 64, 24);
    fieldComp.setBounds (8, 40, w - 16, getHeight() - 40 - 32);
}

void HeroPanel::update (const TelemetrySnapshot& t, const OutputAnalyser& a, double dt)
{
    fieldComp.update (t, a, dt);
    const int v = t.activeVoices.load (std::memory_order_relaxed);
    const int s = t.activeSingers.load (std::memory_order_relaxed);
    auto l = "VOICES " + juce::String (v).paddedLeft ('0', 2) + juce::String::fromUTF8 ("   \xc2\xb7   SINGERS ") + juce::String (s).paddedLeft ('0', 2);
    auto m = fieldComp.getReadout();
    auto r = fieldComp.getNearestVowel();
    if (l != footerLeft || m != footerMid || r != footerRight)
    {
        footerLeft = std::move (l);
        footerMid = std::move (m);
        footerRight = std::move (r);
        repaint (getLocalBounds().removeFromBottom (32));
    }
}

void HeroPanel::paint (juce::Graphics& g)
{
    chromeLayer.draw (g, getLocalBounds(), [] (juce::Graphics& cg, juce::Rectangle<float> r) {
        chrome::drawPlate (cg, r, "SCULPTOR", true);
        cg.setColour (palette::ash);
        cg.setFont (capsFont (9.0f, Weight::medium));
        cg.drawText ("SPECTRAL FORMANT FIELD", juce::Rectangle<float> (r.getX() + 112.0f, r.getY(), 180.0f, 32.0f), juce::Justification::centredLeft, false);
    });

    const auto footer = getLocalBounds().removeFromBottom (32).reduced (16, 0).toFloat();
    g.setFont (capsFont (9.5f));
    g.setColour (palette::stone);
    g.drawText (footerLeft, footer, juce::Justification::centredLeft, false);
    g.setFont (monoFont (10.0f));
    g.setColour (palette::ivory.withAlpha (0.85f));
    g.drawText (footerMid, footer, juce::Justification::centred, false);
    g.setFont (capsFont (9.5f));
    g.setColour (palette::amber);
    g.drawText (footerRight, footer, juce::Justification::centredRight, false);
}

} // namespace monolith::ui
