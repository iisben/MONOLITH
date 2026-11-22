#pragma once

#include "DspMath.h"
#include <array>
#include <vector>

namespace monolith::dsp
{

enum class SpaceType : int
{
    Cathedral     = 0,
    ConcreteSilo  = 1,
    ObsidianAbyss = 2,
    GhostVoid     = 3
};

/**
    Custom choir-tuned Brutalist Architectural Reverb & Spectral Freeze engine.
    Combines a fractional-interpolated 4-stage allpass early reflection diffuser,
    an 8x8 normalized Hadamard modulated FDN, dual-grain Hann-windowed pitch-shifted
    dark shimmer, and click-free infinite Spectral Freeze with RMS stabilization.
    All continuous parameters and Space Model delay geometries morph smoothly per sample.
*/
class BrutalistReverb
{
public:
    static constexpr int kNumFdnLines = 8;
    static constexpr int kNumAllpass  = 4;

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    void processBlock (float* left,
                       float* right,
                       int numSamples,
                       SpaceType space,
                       float mix01,
                       float decay01,
                       float damp01,
                       float shimmer01,
                       bool freezeEnabled) noexcept;

    [[nodiscard]] float getFreezeEnergy() const noexcept { return freezeEnergyIndicator; }

private:
    double sr = 48000.0;

    CubicDelayLine preDelayL, preDelayR;

    struct AllpassStage
    {
        CubicDelayLine line;

        void prepare (size_t maxLen)
        {
            line.prepare (maxLen + 8);
        }
        void clear() noexcept
        {
            line.reset();
        }
        inline float process (float x, float delaySamples, float g) noexcept
        {
            const float delayed = line.readCubic (delaySamples);
            const float v = sanitize (x + g * delayed);
            line.push (v);
            return delayed - g * v;
        }
    };
    std::array<AllpassStage, kNumAllpass> earlyApL, earlyApR;

    // 8x8 Modulated FDN tank
    std::array<CubicDelayLine, kNumFdnLines> fdnLines;
    std::array<ZdfSvf, kNumFdnLines> fdnDampLp;
    std::array<DCBlocker, kNumFdnLines> fdnDc;
    std::array<QuadratureLfo, kNumFdnLines> fdnLfo {};

    // Dual-grain Hann-windowed pitch shimmer (stereo)
    struct GrainShifter
    {
        CubicDelayLine ring;
        float grainSizeSamples = 3200.0f;
        double phaseA          = 0.0;
        double phaseB          = 0.5;
        float cachedSt         = 1.0e9f;
        float cachedDelta      = 0.0f;

        void prepare (double sampleRate)
        {
            const size_t cap = static_cast<size_t> (sampleRate * 0.25) + 64;
            ring.prepare (cap);
            grainSizeSamples = static_cast<float> (sampleRate * 0.075);
            phaseA = 0.0;
            phaseB = 0.5;
            cachedSt = 1.0e9f;
        }

        void reset() noexcept
        {
            ring.reset();
            phaseA = 0.0;
            phaseB = 0.5;
            cachedSt = 1.0e9f;
        }

        inline float process (float input, float pitchSemitones) noexcept
        {
            ring.push (input);
            if (std::abs (pitchSemitones - cachedSt) > 1.0e-4f)
            {
                cachedSt = pitchSemitones;
                const float ratio = semitonesToRatio (pitchSemitones);
                cachedDelta = (1.0f - ratio) / std::max (grainSizeSamples, 64.0f);
            }

            phaseA += static_cast<double> (cachedDelta);
            phaseB += static_cast<double> (cachedDelta);
            phaseA -= std::floor (phaseA);
            phaseB -= std::floor (phaseB);

            const float dA = 12.0f + static_cast<float> (phaseA) * grainSizeSamples;
            const float dB = 12.0f + static_cast<float> (phaseB) * grainSizeSamples;

            const auto& trig = FastTrig::instance();
            // Hann window. Dual grains a half-period apart sum to 1 (COLA).
            const float wA = 0.5f * (1.0f - trig.cosTurns (static_cast<float> (phaseA)));
            const float wB = 0.5f * (1.0f - trig.cosTurns (static_cast<float> (phaseB)));

            return ring.readCubic (dA) * wA + ring.readCubic (dB) * wB;
        }
    };
    GrainShifter shimmerL, shimmerR;

    // Spectral Freeze state
    float freezeBlend           = 0.0f;
    float frozenTargetRms       = 0.0f;
    float currentTankRms        = 0.0f;
    float freezeEnergyIndicator = 0.0f;
    bool  wasFrozen             = false;

    // Smoothed controls & room geometry (eliminates clicks on Space switch & knob drag)
    bool  smoothersPrimed       = false;
    float smMix                 = 0.46f;
    float smShimmer             = 0.22f;
    float smDampCutHz           = 6000.0f;
    float smPreDelaySamples     = 1152.0f;
    float smEarlyBalance        = 0.25f;
    float smModDepthSamples     = 25.0f;
    float smShimmerCap          = 0.28f;
    float smShimmerStL          = 12.0f;
    float smShimmerStR          = 12.0f;
    std::array<float, kNumAllpass>  smApLenL {};
    std::array<float, kNumAllpass>  smApLenR {};
    std::array<float, kNumFdnLines> smFdnDelay {};
    std::array<float, kNumFdnLines> smFdnFeedback {};
};

} // namespace monolith::dsp
