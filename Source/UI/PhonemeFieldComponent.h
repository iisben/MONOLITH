#pragma once

#include "../DSP/PhonemeField.h"
#include "../State/TelemetrySnapshot.h"
#include "MonolithLookAndFeel.h"
#include <array>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace monolith::ui
{

/**
    60 FPS lock-free Interactive 2D Phoneme & Acoustic Field hero display.
    Allows dragging/clicking across the 7 curated vocal anchors while rendering
    5 concentric vocal-tract formant rings, breath mist, and orbiting swarm motes.
*/
class PhonemeFieldComponent final : public juce::Component,
                                    public juce::SettableTooltipClient
{
public:
    explicit PhonemeFieldComponent (juce::AudioProcessorValueTreeState& apvts);

    void updateFromTelemetry (const TelemetrySnapshot& telemetry) noexcept;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

private:
    juce::Rectangle<float> getFieldRect() const noexcept;
    juce::Point<float> normToScreen (float nx, float ny) const noexcept;
    void setFromPoint (juce::Point<float> pt, bool isInitialClick);

    std::unique_ptr<juce::ParameterAttachment> xAttach;
    std::unique_ptr<juce::ParameterAttachment> yAttach;

    float anchorX   = 0.50f;
    float anchorY   = 0.86f;
    float liveEffX  = 0.50f;
    float liveEffY  = 0.86f;

    float audioEnergy   = 0.0f;
    float breathDensity = 0.0f;
    float swarmSpread   = 0.68f;
    float freezeGlow    = 0.0f;
    int   activeSingers = 0;
    std::array<float, 5> formantEnergy {};

    float animPhase = 0.0f;
    bool  dragging  = false;
    juce::Point<float> dragStartPos {};
    float dragStartX = 0.50f;
    float dragStartY = 0.86f;

    static constexpr int kTrailLen = 28;
    std::array<juce::Point<float>, kTrailLen> orbitTrail {};
    int trailHead = 0;
};

} // namespace monolith::ui
