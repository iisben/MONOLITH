#pragma once

#include "MonolithLookAndFeel.h"
#include "../State/TelemetrySnapshot.h"
#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace monolith::ui
{

/**
    Architectural rotary dial with label, formatted live readout, Shift-drag
    fine control, double-click default reset, and optional macro styling.
*/
class MonolithDial final : public juce::Component
{
public:
    MonolithDial (juce::AudioProcessorValueTreeState& apvts,
                  const juce::String& paramId,
                  const juce::String& titleText,
                  const juce::String& tooltipText,
                  const juce::String& unitSuffix = "",
                  int decimals = 2,
                  bool isPercent = false,
                  bool isMacro = false,
                  const juce::String& accentName = "amber",
                  const juce::String& subCaptionText = "");

    void paint (juce::Graphics& g) override;
    void resized() override;

    juce::Slider& getSlider() noexcept { return slider; }

private:
    juce::String formatCurrentValue() const;

    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    juce::String title;
    juce::String suffix;
    juce::String subCaption;
    int numDecimals   = 2;
    bool percentMode  = false;
    bool macroMode    = false;
    juce::Colour accentColour;
};

/** 60 FPS Stereo RMS & Peak-Hold Output Meter. */
class StereoLevelMeter final : public juce::Component
{
public:
    void update (float rmsL, float rmsR, float peakL, float peakR) noexcept;
    void paint (juce::Graphics& g) override;

private:
    float levelL = 0.0f;
    float levelR = 0.0f;
    float holdL  = 0.0f;
    float holdR  = 0.0f;
    int   holdFramesL = 0;
    int   holdFramesR = 0;
};

/** Interactive Expression / CC1 bar reading live telemetry and writing APVTS/MIDI expression. */
class ExpressionBar final : public juce::Component,
                            public juce::SettableTooltipClient
{
public:
    explicit ExpressionBar (juce::AudioProcessorValueTreeState& apvts,
                            const juce::String& paramId,
                            std::function<void (float)> onUiDrag = {});

    void updateLive (float liveExpr) noexcept;
    void paint (juce::Graphics& g) override;

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

private:
    void setFromMouseX (float x);

    std::unique_ptr<juce::ParameterAttachment> paramAttach;
    std::function<void (float)> uiCallback;
    float paramVal   = 0.78f;
    float liveVal    = 0.78f;
    bool  isDragging = false;
};

/** Compact voice & active virtual-singer status badge. */
class VoiceBadge final : public juce::Component
{
public:
    void update (int voices, int singers) noexcept;
    void paint (juce::Graphics& g) override;

private:
    int voiceCount  = 0;
    int singerCount = 0;
};

} // namespace monolith::ui
