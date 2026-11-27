#pragma once

#include "DspMath.h"
#include <juce_dsp/juce_dsp.h>
#include <memory>
#include <vector>

namespace monolith::dsp
{

/**
    Per-voice pre-sum analog degradation & witch-house stage.
    Applies constant-latency 2x/4x polyphase IIR oversampled asymmetric tape/tube
    saturation with click-free mode crossfading, DC blocking, smoothed cubic-interpolated
    tape wow/flutter, and smoothed oxide high-frequency damping.
*/
class EntropyStage
{
public:
    void prepare (double sampleRate, int maxBlockSize, uint32_t voiceSeed);
    void reset() noexcept;

    /**
        Exact integer latency (measured impulse peak at prepare) for the selected mode.
        Returns 0 when the oversampled drive stage is bypassed (drive disengaged).
    */
    [[nodiscard]] int getLatencySamples (bool highQuality4x, bool driveEngaged = true) const noexcept;

    /**
        Processes stereo audio in-place inside pre-allocated buffers.
        Guaranteed zero allocations for numSamples <= preparedMaxBlockSize.
    */
    void processBlock (float* left,
                       float* right,
                       int numSamples,
                       float drive01,
                       float tapeInstability01,
                       float oxideDamping01,
                       bool highQuality4x,
                       bool allowModeSwitch = true) noexcept;

private:
    double   sr               = 48000.0;
    int      preparedMaxBlock = 512;
    uint32_t seed             = 1u;

    std::unique_ptr<juce::dsp::Oversampling<float>> os2x;
    std::unique_ptr<juce::dsp::Oversampling<float>> os4x;
    juce::AudioBuffer<float> scratchBuffer;
    juce::AudioBuffer<float> xfadeScratchBuffer;

    DCBlocker64 dcL, dcR;   // end-of-chain 64-bit DC blocker (post drive, tape & oxide)
    DCBlocker64 dc2L, dc2R; // 2nd (5 Hz) stage: 2nd-order rejection of sub-Hz drive-DC drift
    CubicDelayLine delayL, delayR;
    PinkNoiseGen flutterNoise;

    ZdfSvf oxideLpL, oxideLpR;
    ZdfSvf headBumpBpL, headBumpBpR;

    float wowPhase        = 0.0f;
    float flutterPhase    = 0.0f;
    float wowRateHz       = 0.55f;
    float flutterRateHz   = 7.9f;
    float smoothedFlutter = 0.0f;

    // Per-sample parameter smoothers (prevent zipper/delay-jump clicks)
    float smDrive         = 0.0f;
    float smTape          = 0.0f;
    float smOxide         = 0.0f;
    bool  smootherPrimed  = false;

    // Routing: 0 = bypass (no OS, zero latency), 1 = 2x, 2 = 4x. Switched only on absolute
    // control-cell boundaries with a click-free 15 ms crossfade.
    int   routeMode       = 1;
    int   prevRouteMode   = 1;
    float osXfade         = 1.0f;
    bool  tapeActive      = false;
    bool  oxideActive     = false;
    int   oxideCtrlCounter = 0;
    int   latency2x       = 0;
    int   latency4x       = 0;
};

} // namespace monolith::dsp
