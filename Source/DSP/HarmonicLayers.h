#pragma once

#include "DspMath.h"

namespace monolith::dsp
{

/**
    Per-voice phase-coherent Sub-Abyss (chest sub-octave with soft saturation
    and low-pass sculpting) and Spectral Crown (detuned airy upper-octave sheen).
*/
class HarmonicLayers
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    /** Updates filter coefficients at control rate (avoids per-sample trig during glide). */
    void updateFilters (float f0Hz, float tension01) noexcept;

    /**
        Generates stereo output additions for Sub-Abyss (centered) and
        Spectral Crown (twin detuned stereo pair) for one sample.
    */
    void processSample (float f0Hz,
                        float subAmount01,
                        float crownAmount01,
                        float expression01,
                        float& outL,
                        float& outR) noexcept;

private:
    double sr          = 48000.0;
    double subPhase    = 0.0;
    double crownPhaseL = 0.0;
    double crownPhaseR = 0.37;

    ZdfSvf subLp;
    ZdfSvf subHp;
    ZdfSvf crownBpL;
    ZdfSvf crownBpR;
};

} // namespace monolith::dsp
