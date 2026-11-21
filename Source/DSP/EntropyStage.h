#pragma once

#include "DspMath.h"
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <memory>
#include <vector>

namespace monolith::dsp
{

/**
    Per-voice pre-sum analog degradation & witch-house stage.
    Applies constant-latency 4x/8x polyphase IIR oversampled asymmetric tape/tube
    saturation with click-free mode crossfading, DC blocking, smoothed cubic-interpolated
    tape wow/flutter, and smoothed oxide high-frequency damping.
*/
class EntropyStage
{
public:
    void prepare (double sampleRate, int maxBlockSize, uint32_t voiceSeed);
    void reset() noexcept;

    /** Returns oversampling latency in samples for the selected mode. */
    [[nodiscard]] int getLatencySamples (bool highQuality) const noexcept;

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
                       bool highQuality) noexcept;

private:
    double   sr               = 48000.0;
    int      preparedMaxBlock = 512;
    uint32_t seed             = 1u;

    std::unique_ptr<juce::dsp::Oversampling<float>> osStd;
    std::unique_ptr<juce::dsp::Oversampling<float>> osHq;
    juce::AudioBuffer<float> scratchBuffer;
    juce::AudioBuffer<float> xfadeScratchBuffer;

    DCBlocker dcL, dcR;
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

    // First-order antiderivative anti-aliasing (ADAA) state for the waveshaper,
    // one set per oversampler (index 0 = 2x, 1 = 4x) and per channel.
    struct AdaaState
    {
        double prevU = 0.0;
        float  prevX = 0.0f;
        bool   primed = false;
    };
    std::array<std::array<AdaaState, 2>, 2> adaa {};

    // Click-free 4x <-> 8x oversampling switch crossfade
    bool  lastHq4x        = false;
    float osXfade         = 1.0f;
};

} // namespace monolith::dsp
