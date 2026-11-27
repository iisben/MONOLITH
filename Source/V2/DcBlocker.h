#pragma once

#include <array>
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>

namespace monolith::v2
{

/**
    Output DC / infrasonic blocker: 2nd-order Butterworth high-pass at 5 Hz, double precision, per channel.

    The frozen engine can drift below a few Hz when the vowel orbit sweeps closed vowels (Stage 5 finding,
    up to about -76 dBFS of DC on user patches). The processor runs this on the engine output, before the
    additive texture layer and outside the frozen core: -0.003 dB at 30 Hz, -44 dB at 0.4 Hz, no latency.
    Real-time safe (no allocation, no locks).
*/
class DcBlocker
{
public:
    static constexpr double cutoffHz = 5.0;

    void prepare (double sampleRate) noexcept
    {
        const double w0 = 2.0 * 3.14159265358979323846 * cutoffHz / sampleRate;
        const double cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * 0.70710678118654752440);
        const double a0 = 1.0 + alpha;
        b0 = (1.0 + cw) * 0.5 / a0;
        b1 = -(1.0 + cw) / a0;
        b2 = b0;
        a1 = -2.0 * cw / a0;
        a2 = (1.0 - alpha) / a0;
        reset();
    }

    void reset() noexcept { state = {}; }

    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        const int nch = std::min (buffer.getNumChannels(), static_cast<int> (state.size()));
        const int n = buffer.getNumSamples();
        for (int ch = 0; ch < nch; ++ch)
        {
            auto* x = buffer.getWritePointer (ch);
            double z1 = state[static_cast<size_t> (ch)][0], z2 = state[static_cast<size_t> (ch)][1];
            for (int i = 0; i < n; ++i)
            {
                const double in = x[i];
                const double out = b0 * in + z1;
                z1 = b1 * in - a1 * out + z2;
                z2 = b2 * in - a2 * out;
                x[i] = static_cast<float> (out);
            }
            // flush denormals in the recursive state (silent tails)
            if (std::abs (z1) < 1.0e-30) z1 = 0.0;
            if (std::abs (z2) < 1.0e-30) z2 = 0.0;
            state[static_cast<size_t> (ch)] = { z1, z2 };
        }
    }

private:
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    std::array<std::array<double, 2>, 2> state {};
};

} // namespace monolith::v2
