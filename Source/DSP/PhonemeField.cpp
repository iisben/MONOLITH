#include "PhonemeField.h"
#include "DspMath.h"
#include <algorithm>
#include <cmath>

namespace monolith::dsp
{

namespace
{
    constexpr std::array<PhonemeAnchor, PhonemeField::kNumStates> kAnchors {{
        { "AH",      "Open Epic",     0.50f, 0.88f },
        { "OH",      "Liturgical",    0.18f, 0.72f },
        { "OO",      "Hollow Dark",   0.12f, 0.36f },
        { "EH",      "Tense",         0.82f, 0.72f },
        { "EE",      "Piercing",      0.88f, 0.36f },
        { "HUM",     "Closed Sub",    0.30f, 0.12f },
        { "WHISPER", "Breath Cloud",  0.70f, 0.12f }
    }};

    struct BasePhonemeSpec
    {
        std::array<float, 5> freq;
        std::array<float, 5> bw;
        std::array<float, 5> gainDb;
        float voicedMix;
        float breathBoost;
        float nasalDip;
    };

    // Tenor-referenced acoustic 5-formant tables (Hz, Hz, dB)
    constexpr std::array<BasePhonemeSpec, PhonemeField::kNumStates> kTenorFormants {{
        // 0: Ah (Open Epic)
        {{ 730.0f, 1090.0f, 2440.0f, 3250.0f, 3950.0f },
         {  78.0f,   92.0f,  125.0f,  165.0f,  220.0f },
         {   0.0f,   -3.5f,  -10.5f,  -15.5f,  -23.0f },
         1.00f, 0.05f, 0.0f },

        // 1: Oh (Liturgical)
        {{ 510.0f,  840.0f, 2360.0f, 3180.0f, 3850.0f },
         {  68.0f,   86.0f,  118.0f,  160.0f,  215.0f },
         {   0.0f,   -5.0f,  -14.5f,  -19.0f,  -26.0f },
         1.00f, 0.04f, 0.0f },

        // 2: Oo (Hollow Dark)
        {{ 310.0f,  680.0f, 2250.0f, 3050.0f, 3720.0f },
         {  58.0f,   78.0f,  115.0f,  165.0f,  225.0f },
         {   0.0f,   -9.5f,  -21.0f,  -25.0f,  -31.0f },
         0.98f, 0.06f, 0.0f },

        // 3: Eh (Tense)
        {{ 550.0f, 1840.0f, 2560.0f, 3420.0f, 4120.0f },
         {  65.0f,   96.0f,  130.0f,  175.0f,  235.0f },
         {   0.0f,   -3.0f,   -8.5f,  -14.0f,  -21.0f },
         1.00f, 0.06f, 0.0f },

        // 4: Ee (Piercing)
        {{ 285.0f, 2250.0f, 2980.0f, 3680.0f, 4380.0f },
         {  55.0f,  105.0f,  140.0f,  180.0f,  240.0f },
         {   0.0f,   -4.5f,   -9.0f,  -13.5f,  -20.0f },
         1.00f, 0.07f, 0.0f },

        // 5: Hum (Closed Sub)
        {{ 255.0f,  760.0f, 1980.0f, 2850.0f, 3550.0f },
         {  48.0f,   95.0f,  145.0f,  195.0f,  250.0f },
         {   2.5f,  -16.0f,  -24.0f,  -30.0f,  -36.0f },
         0.95f, 0.02f, 0.85f },

        // 6: Whisper (Pure Spectral Breath)
        {{ 640.0f, 1480.0f, 2650.0f, 3520.0f, 4250.0f },
         { 115.0f,  155.0f,  195.0f,  245.0f,  300.0f },
         {  -4.0f,   -1.5f,   -4.0f,   -9.5f,  -15.0f },
         0.10f, 0.95f, 0.0f }
    }};

    struct RegisterScaling
    {
        std::array<float, 5> freqScale;
        std::array<float, 5> bwScale;
        std::array<float, 5> gainOffsetDb;
    };

    constexpr std::array<RegisterScaling, 4> kRegisterScales {{
        // 0: Basso Profundo (Long, cavernous vocal tract + chest warmth)
        {{ 0.82f, 0.85f, 0.88f, 0.90f, 0.91f },
         { 0.88f, 0.90f, 0.95f, 1.00f, 1.02f },
         { 1.50f, 0.00f, -1.0f, -1.5f, -2.5f }},

        // 1: Tenor (Baseline + classical singer's formant ring around F3/F4)
        {{ 1.00f, 1.00f, 1.00f, 1.00f, 1.00f },
         { 1.00f, 1.00f, 1.00f, 1.00f, 1.00f },
         { 0.00f, 0.50f, 2.00f, 1.50f, 0.00f }},

        // 2: Alto (Warm female/countertenor vocal tract)
        {{ 1.14f, 1.15f, 1.13f, 1.12f, 1.10f },
         { 1.04f, 1.05f, 1.05f, 1.06f, 1.08f },
         { 0.00f, 0.50f, 1.20f, 1.00f, 0.50f }},

        // 3: Soprano (Bright, soaring open tract)
        {{ 1.26f, 1.28f, 1.24f, 1.22f, 1.20f },
         { 1.08f, 1.10f, 1.10f, 1.12f, 1.15f },
         { -0.5f, 0.80f, 2.20f, 2.00f, 1.50f }}
    }};

    [[nodiscard]] inline float dbToLinear (float db) noexcept
    {
        return std::exp (db * 0.1151292546497022842f);
    }

    struct PrecomputedLogTable
    {
        std::array<std::array<float, 5>, PhonemeField::kNumStates> logFreq {};
        std::array<std::array<float, 5>, PhonemeField::kNumStates> logBw {};

        PrecomputedLogTable()
        {
            for (size_t k = 0; k < PhonemeField::kNumStates; ++k)
            {
                for (size_t f = 0; f < 5; ++f)
                {
                    logFreq[k][f] = std::log (kTenorFormants[k].freq[f]);
                    logBw[k][f]   = std::log (kTenorFormants[k].bw[f]);
                }
            }
        }
    };

    const PrecomputedLogTable& getLogTable() noexcept
    {
        static const PrecomputedLogTable table;
        return table;
    }
} // namespace

const std::array<PhonemeAnchor, PhonemeField::kNumStates>& PhonemeField::getAnchors() noexcept
{
    return kAnchors;
}

VocalRegister PhonemeField::resolveRegister (VocalRegister mode, int midiNote) noexcept
{
    if (mode != VocalRegister::SatbAuto)
        return mode;

    if (midiNote < 48)  return VocalRegister::BassoProfundo;
    if (midiNote < 60)  return VocalRegister::Tenor;
    if (midiNote < 70)  return VocalRegister::Alto;
    return VocalRegister::Soprano;
}

FormantTarget PhonemeField::evaluate (float x, float y,
                                      VocalRegister reg,
                                      float f0Hz,
                                      float throatShiftSemitones,
                                      float formantFocus01) noexcept
{
    const float px = std::clamp (x, 0.0f, 1.0f);
    const float py = std::clamp (y, 0.0f, 1.0f);

    // Modified-Shepard Gaussian RBF weights for smooth, non-peaky 2D interpolation
    std::array<float, kNumStates> weights {};
    float weightSum = 0.0f;
    constexpr float twoSigmaSq = 2.0f * 0.28f * 0.28f;

    for (size_t k = 0; k < kNumStates; ++k)
    {
        const float dx = px - kAnchors[k].x;
        const float dy = py - kAnchors[k].y;
        const float d2 = dx * dx + dy * dy;
        const float u  = std::exp (-d2 / twoSigmaSq) / (d2 + 0.004f);
        weights[k] = u;
        weightSum += u;
    }

    const float invWeight = (weightSum > 1.0e-8f) ? (1.0f / weightSum) : (1.0f / static_cast<float> (kNumStates));

    std::array<float, 5> logFreqAcc {};
    std::array<float, 5> logBwAcc {};
    std::array<float, 5> gainDbAcc {};
    float voicedAcc = 0.0f;
    float breathAcc = 0.0f;
    float nasalAcc  = 0.0f;

    const auto& logTbl = getLogTable();
    for (size_t k = 0; k < kNumStates; ++k)
    {
        const float w = weights[k] * invWeight;
        const auto& spec = kTenorFormants[k];
        for (size_t f = 0; f < 5; ++f)
        {
            logFreqAcc[f] += w * logTbl.logFreq[k][f];
            logBwAcc[f]   += w * logTbl.logBw[k][f];
            gainDbAcc[f]  += w * spec.gainDb[f];
        }
        voicedAcc += w * spec.voicedMix;
        breathAcc += w * spec.breathBoost;
        nasalAcc  += w * spec.nasalDip;
    }

    const int regIndex = std::clamp (static_cast<int> (reg), 0, 3);
    const auto& regScale = kRegisterScales[static_cast<size_t> (regIndex)];

    const float throatRatio = semitonesToRatio (std::clamp (throatShiftSemitones, -18.0f, 18.0f));
    const float focus = std::clamp (formantFocus01, 0.0f, 1.0f);
    // Focus 0 -> 1.55x wide pad blend; 0.5 -> 1.0x neutral; 1.0 -> 0.58x focused choir formants
    const float bwFocusScale = (focus <= 0.5f)
                             ? (1.55f - 1.10f * focus)
                             : (1.00f - 0.84f * (focus - 0.5f));
    // Partial energy compensation only: full 1/sqrt(bw) make-up on narrow formants turns
    // high Focus settings into ringing, whistling resonances.
    const float focusGainComp = std::clamp (1.0f / std::sqrt (std::max (bwFocusScale, 0.35f)), 0.85f, 1.10f);

    FormantTarget target;
    for (size_t f = 0; f < 5; ++f)
    {
        float freq = std::exp (logFreqAcc[f]) * regScale.freqScale[f] * throatRatio;
        const float bw = std::exp (logBwAcc[f]) * regScale.bwScale[f] * bwFocusScale;
        const float gDb = gainDbAcc[f] + regScale.gainOffsetDb[f];

        // High-pitch vocal tract jaw-tracking rule: prevents Soprano F1 from starving f0
        if (f == 0 && f0Hz > 0.85f * freq)
            freq = std::min (std::max (freq, 1.08f * f0Hz), 1180.0f);

        target.freq[f] = std::clamp (freq, 90.0f, 12000.0f);
        target.bw[f]   = std::clamp (bw, 22.0f, 1600.0f);
        target.gain[f] = dbToLinear (gDb) * focusGainComp;
    }

    target.voicedMix   = std::clamp (voicedAcc, 0.05f, 1.0f);
    target.breathBoost = std::clamp (breathAcc, 0.0f, 1.0f);
    target.nasalDip    = std::clamp (nasalAcc, 0.0f, 1.0f);

    return target;
}

// ============================================================================
void OrganicOrbitEngine::prepare (double sampleRate) noexcept
{
    sr = std::max (8000.0, sampleRate);
    reset();
}

void OrganicOrbitEngine::reset() noexcept
{
    phaseA      = 0.0;
    phaseB      = 1.17;
    phaseC      = 2.63;
    smoothDepth = 0.0f;
}

void OrganicOrbitEngine::advance (int numSamples,
                                  bool enabled,
                                  float rateHz,
                                  float depth01,
                                  bool tempoSync,
                                  double hostBpm,
                                  float baseX,
                                  float baseY) noexcept
{
    const double dt = static_cast<double> (numSamples) / sr;
    const float targetDepth = enabled ? std::clamp (depth01, 0.0f, 1.0f) : 0.0f;
    const float alpha = 1.0f - std::exp (-static_cast<float> (dt) / 0.08f);
    smoothDepth += alpha * (targetDepth - smoothDepth);

    double effRateHz = static_cast<double> (std::clamp (rateHz, 0.02f, 4.0f));
    if (tempoSync)
    {
        // Map rateHz smoothly onto musical bar divisions {8, 4, 2, 1, 0.5, 0.25, 0.125} bars
        static constexpr std::array<double, 7> kBarDivs { 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125 };
        const double norm = std::clamp ((effRateHz - 0.02) / 3.98, 0.0, 1.0);
        const size_t idx = static_cast<size_t> (std::round (norm * 6.0));
        const double bpm = std::clamp (hostBpm, 30.0, 300.0);
        const double barSeconds = (60.0 / bpm) * 4.0 * kBarDivs[idx];
        effRateHz = 1.0 / std::max (barSeconds, 0.05);
    }

    if (enabled || smoothDepth > 1.0e-4f)
    {
        // Golden-ratio & irrational frequency ratios keep Lissajous motion non-repeating
        phaseA += static_cast<double> (kTwoPi) * effRateHz * dt;
        phaseB += static_cast<double> (kTwoPi) * effRateHz * 0.6180339887 * dt;
        phaseC += static_cast<double> (kTwoPi) * effRateHz * 1.4142135623 * dt;

        if (phaseA > 1.0e5) phaseA = std::fmod (phaseA, static_cast<double> (kTwoPi));
        if (phaseB > 1.0e5) phaseB = std::fmod (phaseB, static_cast<double> (kTwoPi));
        if (phaseC > 1.0e5) phaseC = std::fmod (phaseC, static_cast<double> (kTwoPi));
    }

    const float dx = static_cast<float> (0.65 * std::sin (phaseA) + 0.35 * std::cos (phaseC));
    const float dy = static_cast<float> (0.65 * std::cos (phaseB) + 0.35 * std::sin (phaseA * 0.5 + 0.8));

    const float radius = 0.42f * smoothDepth;
    effectiveX = std::clamp (baseX + radius * dx, 0.02f, 0.98f);
    effectiveY = std::clamp (baseY + radius * dy, 0.02f, 0.98f);
}

} // namespace monolith::dsp
