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

/** Pre-allocated power-of-2 64-bit ring buffer with Catmull-Rom cubic reads (denormal-flushing writes). */
class DelayLine64
{
public:
    void prepare (size_t minCapacitySamples)
    {
        size_t cap = 16;
        while (cap < minCapacitySamples + 8)
            cap <<= 1;
        buffer.assign (cap, 0.0);
        mask = cap - 1;
        writePos = 0;
    }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0);
        writePos = 0;
    }

    inline void push (double x) noexcept
    {
        // Finite-guard + explicit sub-1e-30 flush: denormal-free on every FPU regardless of FTZ/DAZ
        buffer[writePos] = (std::isfinite (x) && std::abs (x) > 1.0e-30) ? x : 0.0;
        writePos = (writePos + 1) & mask;
    }

    [[nodiscard]] inline double readCubic (double delaySamples) const noexcept
    {
        const double d = std::clamp (delaySamples, 1.0, static_cast<double> (mask - 4));
        const size_t id = static_cast<size_t> (d);
        const double t  = d - static_cast<double> (id);
        const double y0 = buffer[(writePos - id) & mask];
        const double y1 = buffer[(writePos - 1 - id) & mask];
        const double y2 = buffer[(writePos - 2 - id) & mask];
        const double y3 = buffer[(writePos - 3 - id) & mask];
        const double c1 = 0.5 * (y2 - y0);
        const double c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
        const double c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }

private:
    std::vector<double> buffer;
    size_t mask     = 0;
    size_t writePos = 0;
};

/**
    Custom choir-tuned Brutalist Architectural Reverb & Spectral Freeze engine.
    64-bit throughout: cubic-interpolated 4-stage allpass early diffuser, an 8x8 orthonormal
    (unitary) Hadamard FDN with rate-scaled mutually prime delays, gentle sinusoidal delay
    modulation, wall (2-pole) + delay-length-proportional air-absorption damping, 5 Hz DC
    blocking, a linear (saturation-free) feedback path guarded by a slow RMS safety gain,
    dual-grain dark shimmer, and click-free infinite Spectral Freeze with RMS stabilization.
*/
class BrutalistReverb
{
public:
    static constexpr int kNumFdnLines = 8;
    static constexpr int kNumAllpass  = 4;
    static constexpr int kNumSpaces   = 4;

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

    DelayLine64 preDelayL, preDelayR;

    struct AllpassStage
    {
        DelayLine64 line;

        void prepare (size_t maxLen) { line.prepare (maxLen + 8); }
        void clear() noexcept { line.reset(); }
        inline double process (double x, double delaySamples, double g) noexcept
        {
            const double delayed = line.readCubic (delaySamples);
            const double v = x + g * delayed;
            line.push (v);
            return delayed - g * v;
        }
    };
    std::array<AllpassStage, kNumAllpass> earlyApL, earlyApR;

    // 8x8 unitary FDN tank (64-bit)
    std::array<DelayLine64, kNumFdnLines> fdnLines;
    // Wall absorption: 64-bit TPT 2-pole low-pass, shared coefficients / per-line states
    double wallM1 = 1.0, wallM2 = 0.0, wallM3 = 0.0;
    std::array<double, kNumFdnLines>      wallIc1 {}, wallIc2 {};
    double lastWallCutHz = -1.0, lastAirFreeze = -1.0;
    std::array<double, kNumFdnLines>      lastAirDelay {};
    std::array<double, kNumFdnLines>      fdnAirZ {}; // air absorption one-pole state
    std::array<double, kNumFdnLines>      fdnAirP {}; // air absorption one-pole pole (delay-proportional)
    std::array<DCBlocker64, kNumFdnLines> fdnDc;
    std::array<double, kNumFdnLines>      lfoSin {}, lfoCos {}; // rotating-phasor LFOs (no per-sample sin)
    std::array<double, kNumFdnLines>      lfoRotS {}, lfoRotC {};
    std::array<double, kNumFdnLines>      lfoRates {};
    std::array<std::array<double, kNumFdnLines>, kNumSpaces> primeDelays {}; // rate-scaled mutually prime lengths

    // Dual-grain Hann-windowed pitch shimmer (stereo, 64-bit ring)
    struct GrainShifter
    {
        DelayLine64 ring;
        double grainSizeSamples = 3200.0;
        double phaseA           = 0.0;
        double phaseB           = 0.5;

        void prepare (double sampleRate)
        {
            ring.prepare (static_cast<size_t> (sampleRate * 0.25) + 64);
            grainSizeSamples = sampleRate * 0.075;
            phaseA = 0.0;
            phaseB = 0.5;
        }

        void reset() noexcept
        {
            ring.reset();
            phaseA = 0.0;
            phaseB = 0.5;
        }

        double delta = 0.0; // phase increment, refreshed at control rate by setPitch()

        void setPitch (double pitchSemitones) noexcept
        {
            delta = (1.0 - std::exp2 (pitchSemitones / 12.0)) / std::max (grainSizeSamples, 64.0);
        }

        /** Hann window 0.5(1 - cos 2 pi p) = cos^2(pi (p - 1/2)), even Taylor polynomial (|err| < 1e-5). */
        static inline double hann (double p) noexcept
        {
            const double x  = 3.14159265358979323846 * (p - 0.5);
            const double x2 = x * x;
            const double c  = 1.0 + x2 * (-0.5 + x2 * (1.0 / 24.0 + x2 * (-1.0 / 720.0 + x2 * (1.0 / 40320.0))));
            return c * c;
        }

        inline double process (double input) noexcept
        {
            ring.push (input);

            phaseA += delta;
            phaseB += delta;
            phaseA -= std::floor (phaseA);
            phaseB -= std::floor (phaseB);

            const double wA = hann (phaseA);
            const double wB = hann (phaseB);
            return ring.readCubic (12.0 + phaseA * grainSizeSamples) * wA
                 + ring.readCubic (12.0 + phaseB * grainSizeSamples) * wB;
        }
    };
    GrainShifter shimmerL, shimmerR;

    // Spectral Freeze state
    double freezeBlend          = 0.0;
    double frozenTargetRms      = 0.0;
    double currentTankRms       = 0.0;
    double safetyGain           = 1.0;
    float freezeEnergyIndicator = 0.0f;
    bool  wasFrozen             = false;

    // Smoothed controls & room geometry (eliminates clicks on Space switch & knob drag)
    bool   smoothersPrimed      = false;
    int    dampCtrlCounter      = 0;     // absolute control counter (block-size invariant)
    double smMix                = 0.46;
    double smShimmer            = 0.22;
    double smDampCutHz          = 6000.0;
    double smPreDelaySamples    = 1152.0;
    double smEarlyBalance       = 0.25;
    double smModDepthSamples    = 25.0;
    double smShimmerCap         = 0.28;
    double smShimmerStL         = 12.0;
    double smShimmerStR         = 12.0;
    std::array<double, kNumAllpass>  smApLenL {};
    std::array<double, kNumAllpass>  smApLenR {};
    std::array<double, kNumFdnLines> smFdnDelay {};
    std::array<double, kNumFdnLines> smFdnFeedback {};
};

} // namespace monolith::dsp
