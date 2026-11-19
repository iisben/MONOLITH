#pragma once

#include "DspMath.h"
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
    Combines a 4-stage allpass early reflection diffuser, an 8x8 normalized
    Hadamard modulated FDN, dual-grain Hann-windowed pitch-shifted dark shimmer,
    and click-free infinite Spectral Freeze with RMS cloud energy stabilization.
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

    // Pre-delay & Allpass early diffuser
    CubicDelayLine preDelayL, preDelayR;
    struct AllpassStage
    {
        std::vector<float> buf;
        size_t idx  = 0;
        size_t len  = 1;
        float  gain = 0.62f;

        void allocate (size_t maxLen)
        {
            buf.assign (maxLen + 4, 0.0f);
            idx = 0;
        }
        void clear() noexcept
        {
            std::fill (buf.begin(), buf.end(), 0.0f);
            idx = 0;
        }
        inline float process (float x, size_t delayLen, float g) noexcept
        {
            const size_t d = std::min (delayLen, buf.size() - 1);
            const size_t readIdx = (idx + buf.size() - d) % buf.size();
            const float delayed = buf[readIdx];
            const float v = sanitize (x + g * delayed);
            buf[idx] = v;
            idx = (idx + 1) % buf.size();
            return delayed - g * v;
        }
    };
    std::array<AllpassStage, kNumAllpass> earlyApL, earlyApR;

    // 8x8 Modulated FDN tank
    std::array<CubicDelayLine, kNumFdnLines> fdnLines;
    std::array<ZdfSvf, kNumFdnLines> fdnDampLp;
    std::array<DCBlocker, kNumFdnLines> fdnDc;
    std::array<float, kNumFdnLines> lfoPhases {};
    std::array<float, kNumFdnLines> lfoRates {};

    // Dual-grain Hann-windowed pitch shimmer (stereo)
    struct GrainShifter
    {
        CubicDelayLine ring;
        float grainSizeSamples = 3200.0f;
        float phaseA           = 0.0f;
        float phaseB           = 0.5f;

        void prepare (double sampleRate)
        {
            const size_t cap = static_cast<size_t> (sampleRate * 0.25) + 64;
            ring.prepare (cap);
            grainSizeSamples = static_cast<float> (sampleRate * 0.075);
            phaseA = 0.0f;
            phaseB = 0.5f;
        }

        void reset() noexcept
        {
            ring.reset();
            phaseA = 0.0f;
            phaseB = 0.5f;
        }

        inline float process (float input, float pitchSemitones) noexcept
        {
            ring.push (input);
            const float ratio = semitonesToRatio (pitchSemitones);
            // Delay rate = (1 - ratio) / grainSizeSamples
            const float delta = (1.0f - ratio) / std::max (grainSizeSamples, 64.0f);

            phaseA += delta;
            phaseB += delta;
            if (phaseA >= 1.0f) phaseA -= 1.0f;
            if (phaseA <  0.0f) phaseA += 1.0f;
            if (phaseB >= 1.0f) phaseB -= 1.0f;
            if (phaseB <  0.0f) phaseB += 1.0f;

            const float dA = 12.0f + phaseA * grainSizeSamples;
            const float dB = 12.0f + phaseB * grainSizeSamples;

            const float wA = 0.5f * (1.0f - std::cos (kTwoPi * phaseA));
            const float wB = 0.5f * (1.0f - std::cos (kTwoPi * phaseB));

            const float sA = ring.readLinear (dA) * wA;
            const float sB = ring.readLinear (dB) * wB;
            return sA + sB;
        }
    };
    GrainShifter shimmerL, shimmerR;

    // Spectral Freeze state
    float freezeBlend           = 0.0f;
    float frozenTargetRms       = 0.0f;
    float currentTankRms        = 0.0f;
    float freezeEnergyIndicator = 0.0f;
    bool  wasFrozen             = false;
};

} // namespace monolith::dsp
