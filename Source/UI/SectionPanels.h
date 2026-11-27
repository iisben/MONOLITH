#pragma once

#include "../PluginProcessor.h"
#include "CachedLayer.h"
#include "MeterComponents.h"
#include "MonolithLookAndFeel.h"
#include <array>
#include <functional>
#include <memory>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace monolith::ui
{

using ComboAttach  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;

/** Section plate whose static chrome (plate, bevels, title, dividers) is one cached image. */
class PanelBase : public juce::Component
{
public:
    PanelBase (juce::String titleText, bool hasHeader = true);
    void paint (juce::Graphics& g) override;
    void resized() override;

protected:
    virtual void renderChrome (juce::Graphics& g, juce::Rectangle<float> r);
    virtual void layoutChildren() = 0;
    virtual void paintLive (juce::Graphics&) {}

    /** Two-page header tabs (replace the title). */
    void initTabs (const juce::String& a, const juce::String& b);
    void setPage (int page);
    std::array<std::vector<juce::Component*>, 2> pages;
    std::array<juce::TextButton, 2> tabs;
    int activePage = 0;
    bool tabbed = false;

    juce::String title;
    bool header = true;
    CachedLayer chromeLayer;
};

// ── Ensemble & Register ──────────────────────────────────────────────────────
class EnsemblePanel final : public PanelBase
{
public:
    explicit EnsemblePanel (MonolithAudioProcessor& p);

private:
    void layoutChildren() override;
    juce::ComboBox registerBox, onsetBox;
    std::unique_ptr<ComboAttach> registerAttach, onsetAttach;
    MonolithDial sizeDial, spreadDial, vibratoDial, attackDial, releaseDial, onsetDial;
};

// ── Tone & Formant | Expression (MPE / expressive MIDI) ──────────────────────
class ToneExpressionPanel final : public PanelBase
{
public:
    explicit ToneExpressionPanel (MonolithAudioProcessor& p);
    void showExpressionPage() { setPage (1); }

private:
    void layoutChildren() override;
    void paintLive (juce::Graphics& g) override;
    MonolithDial tensionDial, breathDial, throatDial, focusDial, subAbyssDial, crownDial;
    juce::ComboBox velCurveBox;
    std::unique_ptr<ComboAttach> velCurveAttach;
    juce::ToggleButton mpeBtn { "MPE" };
    std::unique_ptr<ButtonAttach> mpeAttach;
    MonolithDial pbRangeDial, vibDial, effortDial, openDial, swellDial;
};

// ── Entropy & Witch-House ────────────────────────────────────────────────────
class EntropyPanel final : public PanelBase
{
public:
    explicit EntropyPanel (MonolithAudioProcessor& p);

private:
    void layoutChildren() override;
    void renderChrome (juce::Graphics& g, juce::Rectangle<float> r) override;
    juce::ToggleButton os4xBtn { "4X OS" };
    std::unique_ptr<ButtonAttach> os4xAttach;
    MonolithDial driveDial, tapeDial, oxideDial, screwPitchDial, screwDepthDial;
};

// ── Architectural Space | Texture Layer ──────────────────────────────────────
class SpaceTexturePanel final : public PanelBase
{
public:
    explicit SpaceTexturePanel (MonolithAudioProcessor& p);
    void updateFreezeGlow (float freezeEnergy) noexcept;
    void showTexturePage() { setPage (1); }

private:
    void layoutChildren() override;
    void paintLive (juce::Graphics& g) override;
    juce::ComboBox spaceBox;
    std::unique_ptr<ComboAttach> spaceAttach;
    juce::ToggleButton cc64Btn { "CC64 FRZ" }, freezeBtn { "SPECTRAL FREEZE" };
    std::unique_ptr<ButtonAttach> cc64Attach, freezeAttach;
    MonolithDial mixDial, decayDial, dampDial, shimmerDial;
    float freezeGlow = 0.0f;

    juce::ComboBox assetBox;
    std::unique_ptr<ComboAttach> assetAttach;
    MonolithDial tuneDial, keytrackDial, texAttackDial, texReleaseDial, velDial, hpDial, lpDial, widthDial;
};

// ── Organic Orbit strip ──────────────────────────────────────────────────────
class OrbitStrip final : public PanelBase
{
public:
    explicit OrbitStrip (MonolithAudioProcessor& p);

private:
    void layoutChildren() override;
    juce::ToggleButton orbitBtn { "ORBIT" }, syncBtn { "BPM SYNC" };
    std::unique_ptr<ButtonAttach> orbitAttach, syncAttach;
    MonolithDial rateDial, depthDial;
};

// ── Bottom performance dock ──────────────────────────────────────────────────
class PerformanceDock final : public PanelBase
{
public:
    explicit PerformanceDock (MonolithAudioProcessor& p);

    /** 30 Hz: macro labels / captions, live macro rings, expression monitors. */
    void refresh (const TelemetrySnapshot& t);
    std::function<void (int)> onEditMacro;
    [[nodiscard]] MonolithDial& getMacroDial (int m) noexcept { return *macroDials[static_cast<size_t> (m)]; }

private:
    void layoutChildren() override;
    void renderChrome (juce::Graphics& g, juce::Rectangle<float> r) override;

    MonolithAudioProcessor& proc;
    std::array<std::unique_ptr<MonolithDial>, 4> macroDials;
    std::array<std::unique_ptr<GlyphButton>, 4> routeButtons;
    uint32_t lastRevision = 0xFFFFFFFFu;

    MonolithDial texMixDial, spaceMixDial;
    juce::ComboBox assetBox;
    std::unique_ptr<ComboAttach> assetAttach;

    juce::TextButton panicBtn { "PANIC" };
    ExpressionBar exprBar;
    ExpressionMeters exprMeters;
    juce::ToggleButton legatoBtn { "LEGATO" };
    std::unique_ptr<ButtonAttach> legatoAttach;
    MonolithDial glideDial, outDial;
};

} // namespace monolith::ui
