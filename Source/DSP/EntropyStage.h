#pragma once

#include "DspMath.h"
#include <juce_dsp/juce_dsp.h>
#include <memory>
#include <vector>

namespace monolith::dsp
{

/**
    Per-voice pre-sum analog degradation & witch-house stage.
    Constant-latency 2x/4x polyphase IIR oversampled asymmetric tape/tube saturation
    (HF pre/de-emphasis + envelope-tracked bias), click-free mode crossfading with the
    shorter oversampler time-aligned to the longer one, DC blocking, cubic wow/flutter,
    and smoothed oxide damping.
*/
class EntropyStage
{
public:
    void prepare (double sampleRate, int maxBlockSize, uint32_t voiceSeed);
    void reset() noexcept;

    /**
        Latency is constant across 2x and 4x: the shorter polyphase group delay is
        padded with an integer delay so a live oversampling switch stays time-aligned.
    */
    [[nodiscard]] int getLatencySamples (bool highQuality4x) const noexcept;

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
                       bool highQuality4x) noexcept;

private:
    double   sr               = 48000.0;
    int      preparedMaxBlock = 512;
    uint32_t seed             = 1u;

    std::unique_ptr<juce::dsp::Oversampling<float>> os2x;
    std::unique_ptr<juce::dsp::Oversampling<float>> os4x;
    juce::AudioBuffer<float> scratchBuffer;
    juce::AudioBuffer<float> xfadeScratchBuffer;

    DCBlocker dcL, dcR;
    CubicDelayLine delayL, delayR;
    PinkNoiseGen flutterNoise;

    ZdfSvf oxideLpL, oxideLpR;
    ZdfSvf headBumpBpL, headBumpBpR;

    // HF shelf around the waveshaper. Pre and post poles are matched so a linear
    // saturator cancels exactly (DC gain 1); the clipper only sees the HF lift.
    struct Emphasis
    {
        double preL = 0.0, preR = 0.0, postL = 0.0, postR = 0.0;
        double envL = 0.0, envR = 0.0;
        void reset() noexcept
        {
            preL = preR = postL = postR = envL = envR = 0.0;
        }
    };

    // Integer delay that pads the lower-latency oversampler up to the longer one.
    class AlignDelay
    {
    public:
        void assign (int delaySamples)
        {
            delay = std::max (0, delaySamples);
            bufL.assign (static_cast<size_t> (std::max (delay, 1)), 0.0f);
            bufR.assign (static_cast<size_t> (std::max (delay, 1)), 0.0f);
            write = 0;
        }

        void reset() noexcept
        {
            std::fill (bufL.begin(), bufL.end(), 0.0f);
            std::fill (bufR.begin(), bufR.end(), 0.0f);
            write = 0;
        }

        void process (float* left, float* right, int n) noexcept
        {
            if (delay <= 0 || left == nullptr || right == nullptr)
                return;

            const int cap = static_cast<int> (bufL.size());
            for (int i = 0; i < n; ++i)
            {
                const float outL = bufL[static_cast<size_t> (write)];
                const float outR = bufR[static_cast<size_t> (write)];
                bufL[static_cast<size_t> (write)] = left[i];
                bufR[static_cast<size_t> (write)] = right[i];
                if (++write >= cap)
                    write = 0;
                left[i] = outL;
                right[i] = outR;
            }
        }

    private:
        int delay = 0;
        int write = 0;
        std::vector<float> bufL, bufR;
    };

    Emphasis emph2, emph4;
    AlignDelay align2, align4;
    int reportedLatency = 0;

    QuadratureLfo wowLfo;
    QuadratureLfo flutterLfo;
    QuadratureLfo flutterSideLfo;
    float wowRateHz       = 0.55f;
    float flutterRateHz   = 7.9f;
    float flutterSmooth   = 0.015f;
    float smoothedFlutter = 0.0f;

    // Per-sample parameter smoothers (prevent zipper/delay-jump clicks)
    float smDrive         = 0.0f;
    float smTape          = 0.0f;
    float smOxide         = 0.0f;
    bool  smootherPrimed  = false;

    // Click-free 2x <-> 4x oversampling switch crossfade
    bool  lastHq4x        = false;
    float osXfade         = 1.0f;
};

} // namespace monolith::dsp
