#pragma once

#include "../V2/AudioTap.h"
#include <array>
#include <vector>
#include <juce_dsp/juce_dsp.h>

namespace monolith::ui
{

/**
    Message-thread analysis of the post-bus output tap (no audio-thread work beyond the tap copy):
      * sample-peak per channel with 1.2 s hold, latched clip (|x| >= -0.005 dBFS)
      * BS.1770 K-weighted momentary (400 ms) and short-term (3 s) loudness, ungated
      * 2048-point Hann spectrum on 160 log bands (40 Hz - 16 kHz) with ballistic smoothing,
        a 1/3-octave spectral envelope and up to four envelope peaks (formant contour F1-F4)
    All buffers are allocated in prepare(); process() runs at the editor frame rate.
*/
class OutputAnalyser
{
public:
    static constexpr int kFftOrder = 11;
    static constexpr int kFftSize  = 1 << kFftOrder;
    static constexpr int kNumBands = 160;
    static constexpr float kMinHz = 40.0f, kMaxHz = 16000.0f;
    static constexpr float kFloorDb = -96.0f;

    struct Formant { float hz = 0.0f; float db = kFloorDb; };

    OutputAnalyser();
    void prepare (double sampleRate);
    /** Drains the tap and updates every measurement. dtSeconds = time since the previous call. */
    void process (v2::AudioTap& tap, double dtSeconds);
    void resetClip() noexcept { clipLatched = false; }

    [[nodiscard]] float getBarDb (int ch) const noexcept { return barDb[static_cast<size_t> (ch & 1)]; }
    [[nodiscard]] float getHoldDb (int ch) const noexcept { return holdDb[static_cast<size_t> (ch & 1)]; }
    [[nodiscard]] float getMaxPeakDb() const noexcept { return maxPeakDb; }
    [[nodiscard]] bool isClipped() const noexcept { return clipLatched; }
    [[nodiscard]] float getMomentaryLufs() const noexcept { return momentary; }
    [[nodiscard]] float getShortTermLufs() const noexcept { return shortTerm; }

    [[nodiscard]] const std::array<float, kNumBands>& getSpectrum() const noexcept { return spectrum; }
    [[nodiscard]] const std::array<float, kNumBands>& getEnvelope() const noexcept { return envelope; }
    [[nodiscard]] static float bandHz (int band) noexcept;
    [[nodiscard]] static float hzToNorm (float hz) noexcept; // 0..1 across the log band axis
    [[nodiscard]] const std::array<Formant, 4>& getFormants() const noexcept { return formants; }
    [[nodiscard]] int getNumFormants() const noexcept { return numFormants; }
    [[nodiscard]] bool hasSignal() const noexcept { return signalPresent; }

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        double process (double x) noexcept
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    void analyseSpectrum (double dtSeconds);
    void findFormants();

    double sr = 48000.0;
    std::vector<float> scratchL, scratchR;
    std::vector<float> history; // mono, kFftSize ring
    int historyPos = 0;
    int64_t framesSeen = 0;
    bool freshAudio = false;

    juce::dsp::FFT fft { kFftOrder };
    std::vector<float> window, fftData;
    std::array<std::pair<int, int>, kNumBands> bandBins {};

    std::array<Biquad, 2> kShelf, kHighPass;
    double blockAccum = 0.0;
    int blockCount = 0, blockLen = 4800;
    std::array<double, 30> blockMs {};
    int blockWrite = 0, blocksFilled = 0;

    std::array<float, 2> barDb { kFloorDb, kFloorDb }, holdDb { kFloorDb, kFloorDb };
    std::array<double, 2> holdTimer {};
    float maxPeakDb = kFloorDb;
    bool clipLatched = false;
    float momentary = kFloorDb, shortTerm = kFloorDb;
    double silentFor = 10.0;
    bool signalPresent = false;

    std::array<float, kNumBands> spectrum {}, envelope {};
    std::array<Formant, 4> formants {};
    int numFormants = 0;
};

} // namespace monolith::ui
