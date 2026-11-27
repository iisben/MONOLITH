#pragma once

#include "../PluginProcessor.h"
#include "CachedLayer.h"
#include "MonolithLookAndFeel.h"
#include "OutputAnalyser.h"
#include <array>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace monolith::ui
{

/**
    Hero centrepiece: real-time spectral / formant contour (from the output tap analyser) beneath the swarm
    presence field (live vowel node, formant rings, singer motes from lock-free telemetry), with the draggable
    XY Vowel / Atmosphere Sculptor on top.
      Vowel Field mode    : the handle places the vowel anchor (phoneme_x / phoneme_y); the glowing node is the
                            effective vowel after orbit, sculptor offsets and macro routes.
      Light / Shadow, Axes: the handle drives the XY Sculptor (v2_xy_x / v2_xy_y), neutral at the centre cross.
    Static grid, axes, vowel map and frequency scale are a cached layer; paint() draws only live content.
*/
class SculptorField final : public juce::Component,
                            public juce::SettableTooltipClient
{
public:
    explicit SculptorField (MonolithAudioProcessor& p);

    void update (const TelemetrySnapshot& t, const OutputAnalyser& a, double dtSeconds);
    [[nodiscard]] int getMode() const noexcept { return mode; }
    [[nodiscard]] juce::String getReadout() const;
    [[nodiscard]] juce::String getNearestVowel() const;

    void paint (juce::Graphics& g) override;
    void resized() override { bg.invalidate(); labels.invalidate(); }
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

private:
    [[nodiscard]] juce::Rectangle<float> field() const noexcept { return getLocalBounds().toFloat(); }
    [[nodiscard]] juce::Point<float> toScreen (float nx, float ny) const noexcept;
    [[nodiscard]] juce::Point<float> handleNorm() const noexcept;
    void setHandle (float nx, float ny);
    void beginGestures();
    void endGestures();
    void renderStatic (juce::Graphics& g, juce::Rectangle<float> r) const;
    void renderLabels (juce::Graphics& g, juce::Rectangle<float> r) const;
    void paintSpectrum (juce::Graphics& g, juce::Rectangle<float> r) const;
    void paintSwarm (juce::Graphics& g) const;
    void paintHandle (juce::Graphics& g) const;

    juce::RangedAudioParameter* pPhX = nullptr;
    juce::RangedAudioParameter* pPhY = nullptr;
    juce::RangedAudioParameter* pXyX = nullptr;
    juce::RangedAudioParameter* pXyY = nullptr;
    std::unique_ptr<juce::ParameterAttachment> aPhX, aPhY, aXyX, aXyY, aMode;

    float phX = 0.5f, phY = 0.86f, xyX = 0.5f, xyY = 0.5f;
    int mode = 0;

    // Live telemetry (smoothed on the message thread)
    float effX = 0.5f, effY = 0.86f, energy = 0.0f, breath = 0.0f, spread = 0.68f, freeze = 0.0f;
    int singers = 0, voices = 0;
    std::array<float, 5> formantEnergy {};
    const OutputAnalyser* analyser = nullptr;
    float phase = 0.0f;

    static constexpr int kTrail = 48;
    std::array<juce::Point<float>, kTrail> handleTrail {};
    std::array<float, kTrail> trailAge {};
    int trailHead = 0;
    bool dragging = false;
    juce::Point<float> dragStart;
    juce::Point<float> dragStartNorm;

    CachedLayer bg;     // well, grid, frequency scale, vowel map
    CachedLayer labels; // axis caption chips, composited above the live swarm so they never get buried
    int bgMode = -1;
};

/** Hero panel: plate + mode selector + SculptorField + live status footer. */
class HeroPanel final : public juce::Component
{
public:
    explicit HeroPanel (MonolithAudioProcessor& p);
    void update (const TelemetrySnapshot& t, const OutputAnalyser& a, double dtSeconds);
    void paint (juce::Graphics& g) override;
    void resized() override;
    [[nodiscard]] SculptorField& getField() noexcept { return fieldComp; }

private:
    MonolithAudioProcessor& proc;
    SculptorField fieldComp;
    std::array<juce::TextButton, 3> modeButtons;
    std::unique_ptr<juce::ParameterAttachment> modeAttach;
    juce::String footerLeft, footerMid, footerRight;
    CachedLayer chromeLayer;
};

} // namespace monolith::ui
