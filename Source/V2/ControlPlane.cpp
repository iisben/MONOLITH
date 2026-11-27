#include "ControlPlane.h"
#include <algorithm>
#include <cmath>

namespace monolith::v2
{

namespace
{
    struct BuiltinRoute { int source; const char* dest; float depth; };

    // XY Sculptor mode 0: Vowel Field — X = front/back vowel trajectory, Y = open/closed (both bipolar)
    constexpr std::array<BuiltinRoute, 2> kXyVowel {{
        { static_cast<int> (ModSource::xyX), params::phonemeX, 0.50f },
        { static_cast<int> (ModSource::xyY), params::phonemeY, 0.50f },
    }};

    // XY Sculptor mode 1: Light / Shadow — X = throat size & focus, Y = brightness vs darkness
    constexpr std::array<BuiltinRoute, 6> kXyLight {{
        { static_cast<int> (ModSource::xyX), params::throatShift,    0.25f },
        { static_cast<int> (ModSource::xyX), params::formantFocus,   0.25f },
        { static_cast<int> (ModSource::xyY), params::spectralCrown,  0.35f },
        { static_cast<int> (ModSource::xyY), params::glottalTension, 0.20f },
        { static_cast<int> (ModSource::xyY), params::subAbyss,      -0.25f },
        { static_cast<int> (ModSource::xyY), params::v2TexLowPass,   0.30f },
    }};

    [[nodiscard]] float onePoleAlpha (double dtSec, double tauSec) noexcept
    {
        return static_cast<float> (1.0 - std::exp (-dtSec / tauSec));
    }

    [[nodiscard]] bool isXySource (int s) noexcept
    {
        return s == static_cast<int> (ModSource::xyX) || s == static_cast<int> (ModSource::xyY);
    }
} // namespace

ControlPlane::ControlPlane()
{
    for (auto& s : shadow) s.store (0.0f, std::memory_order_relaxed);
    for (auto& s : uiSource) s.store (0.0f, std::memory_order_relaxed);
    for (int i = 0; i < kNumMacros; ++i)
        labels[static_cast<size_t> (i)] = kDefaultMacroLabels[static_cast<size_t> (i)];
    publish();
}

void ControlPlane::bind (juce::AudioProcessorValueTreeState& apvts)
{
    auto bindSlot = [&] (int k, const char* id) {
        basePtr[static_cast<size_t> (k)] = apvts.getRawParameterValue (id);
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
            range[static_cast<size_t> (k)] = rp->getNormalisableRange();
        jassert (basePtr[static_cast<size_t> (k)] != nullptr);
        shadow[static_cast<size_t> (k)].store (basePtr[static_cast<size_t> (k)]->load(), std::memory_order_relaxed);
    };
    for (int d = 0; d < kNumModDestinations; ++d)
        bindSlot (d, kModDestinations[static_cast<size_t> (d)]);
    for (int m = 0; m < kNumMacros; ++m)
    {
        bindSlot (kNumModDestinations + m, kMacroParamIds[static_cast<size_t> (m)]);
        macroRaw[static_cast<size_t> (m)] = apvts.getRawParameterValue (kMacroParamIds[static_cast<size_t> (m)]);
    }
    texMixShadow = findDestination (params::v2TexMix);
    for (size_t i = 0; i < kXyVowel.size(); ++i) xyVowelDest[i] = findDestination (kXyVowel[i].dest);
    for (size_t i = 0; i < kXyLight.size(); ++i) xyLightDest[i] = findDestination (kXyLight[i].dest);
    pXyX      = apvts.getRawParameterValue (params::v2XyX);
    pXyY      = apvts.getRawParameterValue (params::v2XyY);
    pXyMode   = apvts.getRawParameterValue (params::v2XyMode);
    pVelCurve = apvts.getRawParameterValue (params::v2VelCurve);
    pSwell    = apvts.getRawParameterValue (params::v2ExprSwell);
}

void ControlPlane::prepare (double sampleRate) noexcept
{
    sr = std::max (8000.0, sampleRate);
    primed = false;
    glidePos = glideLen = 0;
    computeShadows (0, true); // shadows valid before the first block (prepareToPlay reads PDC through them)
}

std::atomic<float>* ControlPlane::resolve (const char* paramId) noexcept
{
    const juce::String id (paramId);
    if (const int d = findDestination (id); d >= 0)
        return &shadow[static_cast<size_t> (d)];
    for (int m = 0; m < kNumMacros; ++m)
        if (id == kMacroParamIds[static_cast<size_t> (m)])
            return &shadow[static_cast<size_t> (kNumModDestinations + m)];
    return nullptr;
}

// ── message thread ──────────────────────────────────────────────────────────

void ControlPlane::publish()
{
    configBuffer.back() = master;
    configBuffer.publish();
    ++revision;
}

void ControlPlane::resetToFactory (const std::array<juce::String, kNumMacros>& newLabels)
{
    master = ModConfig {};
    labels = newLabels;
    publish();
}

bool ControlPlane::setRoute (int source, int slot, const juce::String& destId, float depth, bool bipolar)
{
    if (source < 0 || source >= kNumModSources || slot < 0 || slot >= kRoutesPerSource)
        return false;
    const int d = destId.isEmpty() ? -1 : findDestination (destId);
    if (destId.isNotEmpty() && d < 0)
        return false;
    master.routes[static_cast<size_t> (source)][static_cast<size_t> (slot)] =
        { static_cast<int16_t> (d), std::clamp (depth, -1.0f, 1.0f), bipolar };
    publish();
    return true;
}

void ControlPlane::clearRoutes (int source)
{
    if (source < 0 || source >= kNumModSources)
        return;
    master.routes[static_cast<size_t> (source)].fill (ModRoute {});
    publish();
}

void ControlPlane::setLegacyMacroCurve (int macro, bool enabled)
{
    if (macro < 0 || macro >= kNumMacros)
        return;
    master.legacyMacroCurve[static_cast<size_t> (macro)] = enabled;
    publish();
}

void ControlPlane::setMacroLabel (int macro, const juce::String& label)
{
    if (macro < 0 || macro >= kNumMacros)
        return;
    labels[static_cast<size_t> (macro)] = label.substring (0, 24);
    ++revision;
}

juce::String ControlPlane::getMacroLabel (int macro) const
{
    return (macro >= 0 && macro < kNumMacros) ? labels[static_cast<size_t> (macro)] : juce::String();
}

juce::ValueTree ControlPlane::toValueTree() const
{
    juce::ValueTree t ("V2Control");
    for (int s = 0; s < kNumModSources; ++s)
    {
        juce::ValueTree src ("Source");
        src.setProperty ("id", kModSourceIds[static_cast<size_t> (s)], nullptr);
        if (s < kNumMacros)
        {
            src.setProperty ("label", labels[static_cast<size_t> (s)], nullptr);
            src.setProperty ("legacy", master.legacyMacroCurve[static_cast<size_t> (s)], nullptr);
        }
        for (const auto& r : master.routes[static_cast<size_t> (s)])
        {
            if (r.dest < 0)
                continue;
            juce::ValueTree rt ("Route");
            rt.setProperty ("dest", kModDestinations[static_cast<size_t> (r.dest)], nullptr);
            rt.setProperty ("depth", static_cast<double> (r.depth), nullptr);
            rt.setProperty ("bipolar", r.bipolar, nullptr);
            src.appendChild (rt, nullptr);
        }
        t.appendChild (src, nullptr);
    }
    return t;
}

void ControlPlane::fromValueTree (const juce::ValueTree& tree)
{
    master = ModConfig {};
    for (int i = 0; i < kNumMacros; ++i)
        labels[static_cast<size_t> (i)] = kDefaultMacroLabels[static_cast<size_t> (i)];

    if (tree.isValid() && tree.hasType ("V2Control"))
    {
        for (const auto& src : tree)
        {
            const int s = findSource (src.getProperty ("id").toString());
            if (s < 0)
                continue;
            if (s < kNumMacros)
            {
                if (src.hasProperty ("label"))
                    labels[static_cast<size_t> (s)] = src.getProperty ("label").toString().substring (0, 24);
                master.legacyMacroCurve[static_cast<size_t> (s)] = static_cast<bool> (src.getProperty ("legacy", true));
            }
            int slot = 0;
            for (const auto& rt : src)
            {
                if (slot >= kRoutesPerSource || ! rt.hasType ("Route"))
                    continue;
                const int d = findDestination (rt.getProperty ("dest").toString());
                if (d < 0)
                    continue;
                master.routes[static_cast<size_t> (s)][static_cast<size_t> (slot++)] = {
                    static_cast<int16_t> (d),
                    std::clamp (static_cast<float> (static_cast<double> (rt.getProperty ("depth", 0.0))), -1.0f, 1.0f),
                    static_cast<bool> (rt.getProperty ("bipolar", false)) };
            }
        }
    }
    publish();
}

void ControlPlane::requestTransition (float ms) noexcept
{
    transitionRequestMs.store (std::max (1.0f, ms), std::memory_order_release);
}

int ControlPlane::applyVelocityCurve (int velocity, int curve) noexcept
{
    if (velocity <= 0 || curve <= 0)
        return velocity; // note-off-as-velocity-0 and Linear stay untouched
    const float v = static_cast<float> (std::min (velocity, 127)) / 127.0f;
    float o = v;
    switch (curve)
    {
        case 1:  o = std::pow (v, 0.6f); break;            // Soft: light touch speaks louder
        case 2:  o = std::pow (v, 1.8f); break;            // Hard: needs a firm hand
        case 3:  o = v * v * (3.0f - 2.0f * v); break;     // S-Curve: compressed extremes, expressive middle
        default: o = 100.0f / 127.0f; break;               // Fixed: every note at velocity 100
    }
    return std::clamp (static_cast<int> (std::lround (o * 127.0f)), 1, 127);
}

// ── audio thread ────────────────────────────────────────────────────────────

void ControlPlane::process (juce::MidiBuffer& midi, int numSamples, MidiLearn* learn) noexcept
{
    const int curve = pVelCurve != nullptr ? static_cast<int> (std::lround (pVelCurve->load (std::memory_order_relaxed))) : 0;

    for (const auto meta : midi)
    {
        const uint8_t* d = meta.data;
        if (meta.numBytes < 2)
            continue;
        const int status = d[0] & 0xF0;
        const int ch = d[0] & 0x0F;

        if (status == 0x90 && meta.numBytes >= 3 && d[2] > 0)
        {
            if (curve > 0)
                const_cast<uint8_t*> (d)[2] = static_cast<uint8_t> (applyVelocityCurve (d[2], curve)); // in place: no allocation
        }
        else if ((status == 0x80 || (status == 0x90 && meta.numBytes >= 3 && d[2] == 0)))
        {
            polyPressure[static_cast<size_t> (d[1] & 0x7F)] = 0.0f;
        }
        else if (status == 0xA0 && meta.numBytes >= 3)
        {
            polyPressure[static_cast<size_t> (d[1] & 0x7F)] = static_cast<float> (d[2]) / 127.0f;
        }
        else if (status == 0xD0)
        {
            chPressure[static_cast<size_t> (ch)] = static_cast<float> (d[1] & 0x7F) / 127.0f;
        }
        else if (status == 0xB0 && meta.numBytes >= 3)
        {
            const int cc = d[1], val = d[2];
            if (cc == 1)      modWheel = static_cast<float> (val) / 127.0f;
            else if (cc == 2) breath   = static_cast<float> (val) / 127.0f;
            if (learn != nullptr)
                learn->handleCc (cc, val);
        }
    }

    computeShadows (numSamples, false);
}

void ControlPlane::computeShadows (int numSamples, bool primeAll) noexcept
{
    const ModConfig& cfg = configBuffer.read();

    // ── sources (sample-rate-invariant one-pole smoothing on the real block duration) ──
    float pressure = 0.0f;
    for (const float p : chPressure)   pressure = std::max (pressure, p);
    for (const float p : polyPressure) pressure = std::max (pressure, p);

    std::array<float, kNumModSources> target {};
    for (int m = 0; m < kNumMacros; ++m)
        target[static_cast<size_t> (m)] = macroRaw[static_cast<size_t> (m)] != nullptr ? macroRaw[static_cast<size_t> (m)]->load (std::memory_order_relaxed) : 0.0f;
    target[static_cast<size_t> (ModSource::xyX)]      = pXyX != nullptr ? pXyX->load (std::memory_order_relaxed) : 0.5f;
    target[static_cast<size_t> (ModSource::xyY)]      = pXyY != nullptr ? pXyY->load (std::memory_order_relaxed) : 0.5f;
    target[static_cast<size_t> (ModSource::modWheel)] = modWheel;
    target[static_cast<size_t> (ModSource::pressure)] = pressure;
    target[static_cast<size_t> (ModSource::breath)]   = breath;

    const double dt = static_cast<double> (std::max (0, numSamples)) / sr;
    const bool prime = primeAll || ! primed;
    for (int s = 0; s < kNumModSources; ++s)
    {
        float& v = srcSmoothed[static_cast<size_t> (s)];
        const float t = target[static_cast<size_t> (s)];
        if (prime)
            v = t;
        else
        {
            const double tau = (s < kNumMacros) ? 0.015 : (isXySource (s) ? 0.060 : 0.020);
            v += onePoleAlpha (dt, tau) * (t - v);
            if (std::abs (t - v) < 1.0e-6f)
                v = t; // settle exactly so a source returned to neutral yields an exact-zero offset
        }
        uiSource[static_cast<size_t> (s)].store (v, std::memory_order_relaxed);
    }
    primed = true;

    // ── route offsets (normalised domain) ──
    offsets.fill (0.0f);
    const int xyMode = pXyMode != nullptr ? static_cast<int> (std::lround (pXyMode->load (std::memory_order_relaxed))) : 0;
    auto addOffset = [this] (int dest, float amount) noexcept {
        if (dest >= 0 && dest < kNumModDestinations)
            offsets[static_cast<size_t> (dest)] += amount;
    };

    for (int s = 0; s < kNumModSources; ++s)
    {
        if (isXySource (s) && xyMode != 2)
            continue; // built-in XY axes below
        const float v = srcSmoothed[static_cast<size_t> (s)];
        for (const auto& r : cfg.routes[static_cast<size_t> (s)])
        {
            if (r.dest < 0 || r.depth == 0.0f)
                continue;
            const float f = (r.bipolar || isXySource (s)) ? (2.0f * v - 1.0f) : v;
            if (f != 0.0f)
                addOffset (r.dest, r.depth * f);
        }
    }

    if (xyMode == 0 || xyMode == 1)
    {
        auto applyBuiltin = [&] (const auto& table, const auto& destIdx) noexcept {
            for (size_t i = 0; i < table.size(); ++i)
            {
                const float f = 2.0f * srcSmoothed[static_cast<size_t> (table[i].source)] - 1.0f;
                if (f != 0.0f)
                    addOffset (destIdx[i], table[i].depth * f);
            }
        };
        if (xyMode == 0) applyBuiltin (kXyVowel, xyVowelDest);
        else             applyBuiltin (kXyLight, xyLightDest);
    }

    // ── click-free transition glide (A/B, undo/redo) ──
    const float req = transitionRequestMs.exchange (0.0f, std::memory_order_acq_rel);
    if (req > 0.0f && ! prime)
    {
        glideFrom = lastBase;
        glidePos = 0;
        glideLen = std::max<int64_t> (1, static_cast<int64_t> (std::llround (static_cast<double> (req) * 0.001 * sr)));
    }
    float glideT = 1.0f;
    if (glidePos < glideLen)
    {
        glidePos += numSamples;
        glideT = std::clamp (static_cast<float> (static_cast<double> (glidePos) / static_cast<double> (glideLen)), 0.0f, 1.0f);
    }

    // Texture swell: pressure / breath scales an already-audible texture mix (never un-bypasses a 0 % layer)
    const float swell = std::max (srcSmoothed[static_cast<size_t> (ModSource::pressure)], srcSmoothed[static_cast<size_t> (ModSource::breath)])
                      * (pSwell != nullptr ? pSwell->load (std::memory_order_relaxed) : 0.0f);

    // ── shadows ──
    for (int k = 0; k < kNumShadows; ++k)
    {
        auto* bp = basePtr[static_cast<size_t> (k)];
        if (bp == nullptr)
            continue;
        float base = bp->load (std::memory_order_relaxed);
        if (glideT < 1.0f)
            base = glideFrom[static_cast<size_t> (k)] + (base - glideFrom[static_cast<size_t> (k)]) * glideT;
        lastBase[static_cast<size_t> (k)] = base;

        float out = base;
        if (k < kNumModDestinations)
        {
            const float off = offsets[static_cast<size_t> (k)];
            if (off != 0.0f)
            {
                const auto& rg = range[static_cast<size_t> (k)];
                out = rg.convertFrom0to1 (std::clamp (rg.convertTo0to1 (base) + off, 0.0f, 1.0f));
            }
            if (k == texMixShadow && out > 0.0f && swell > 0.0f)
                out = std::min (1.0f, out * (1.0f + swell));
        }
        else if (! cfg.legacyMacroCurve[static_cast<size_t> (k - kNumModDestinations)])
        {
            out = 0.0f; // V1 hard-wired macro curve disabled: the macro drives only its routes
        }
        shadow[static_cast<size_t> (k)].store (out, std::memory_order_relaxed);
    }
}

} // namespace monolith::v2
