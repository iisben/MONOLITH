#pragma once

#include "../PluginProcessor.h"
#include "MeterComponents.h"
#include "MonolithLookAndFeel.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>

namespace monolith::ui
{

// ── 1. Ensemble & Register ────────────────────────────────────────────────────
class EnsemblePanel final : public juce::Component
{
public:
    explicit EnsemblePanel (juce::AudioProcessorValueTreeState& apvts);
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    ConcreteSurface surface;
    juce::ComboBox registerBox;
    juce::ComboBox onsetBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> registerAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> onsetAttach;

    MonolithDial sizeDial, spreadDial, vibratoDial;
    MonolithDial attackDial, releaseDial, onsetDial;
};

// ── 2. Tone, Formant & Layers ─────────────────────────────────────────────────
class ToneLayersPanel final : public juce::Component
{
public:
    explicit ToneLayersPanel (juce::AudioProcessorValueTreeState& apvts);
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    ConcreteSurface surface;
    MonolithDial tensionDial, breathDial, throatDial;
    MonolithDial focusDial, subAbyssDial, crownDial;
};

// ── 3. Organic Orbit Bar ──────────────────────────────────────────────────────
class OrbitPanel final : public juce::Component
{
public:
    explicit OrbitPanel (juce::AudioProcessorValueTreeState& apvts);
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    ConcreteSurface surface;
    juce::ToggleButton orbitBtn { "ORBIT" };
    juce::ToggleButton syncBtn  { "BPM SYNC" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> orbitAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttach;

    MonolithDial rateDial, depthDial;
};

// ── 4. Entropy & Degradation ──────────────────────────────────────────────────
class EntropyPanel final : public juce::Component
{
public:
    explicit EntropyPanel (juce::AudioProcessorValueTreeState& apvts);
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    ConcreteSurface surface;
    juce::ToggleButton os4xBtn { "8X OS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> os4xAttach;

    MonolithDial driveDial, tapeDial, oxideDial;
    MonolithDial screwPitchDial, screwDepthDial;
};

// ── 5. Architectural Space + Freeze ───────────────────────────────────────────
class SpacePanel final : public juce::Component
{
public:
    explicit SpacePanel (juce::AudioProcessorValueTreeState& apvts);
    void updateFreezeGlow (float freezeEnergy) noexcept;
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    ConcreteSurface surface;
    juce::ComboBox spaceBox;
    juce::ToggleButton cc64FreezeBtn { "CC64 -> FRZ" };
    juce::ToggleButton freezeBtn { "SPECTRAL FREEZE" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> spaceAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> cc64Attach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttach;

    MonolithDial mixDial, decayDial, dampDial, shimmerDial;
    float freezeEnergyGlow = 0.0f;
};

// ── 6. Bottom Performance Dock (4 Master Macros + Expression & Legato) ─────────
class PerformanceDock final : public juce::Component
{
public:
    explicit PerformanceDock (MonolithAudioProcessor& proc);
    void updateFromTelemetry (const TelemetrySnapshot& telemetry) noexcept;
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    ConcreteSurface surface;
    MonolithDial colossusDial, specterDial, rotDial, abyssDial;
    ExpressionBar exprBar;
    juce::ToggleButton legatoBtn { "LEGATO" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> legatoAttach;
    MonolithDial glideDial;
};

} // namespace monolith::ui
