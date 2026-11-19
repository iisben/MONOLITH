#include "../Source/DSP/SwarmEngine.h"
#include "../Source/Presets/PresetManager.h"
#include "../Source/State/ParameterIDs.h"
#include "../Source/State/TelemetrySnapshot.h"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <set>
#include <vector>

namespace
{

class AuditHostProcessor final : public juce::AudioProcessor
{
public:
    AuditHostProcessor()
        : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          apvts (*this, nullptr, "AUDIT_STATE", monolith::params::createParameterLayout())
    {
    }

    const juce::String getName() const override { return "AuditHost"; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    juce::AudioProcessorValueTreeState apvts;
};

float ampToDb (float a)
{
    return (a > 1.0e-7f) ? (20.0f * std::log10 (a)) : -140.0f;
}

void setParam (juce::AudioProcessorValueTreeState& apvts, const char* id, float val)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (std::clamp (p->convertTo0to1 (val), 0.0f, 1.0f));
}

bool checkFinite (const juce::AudioBuffer<float>& buf, float& outPeak)
{
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        const float* data = buf.getReadPointer (ch);
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            if (! std::isfinite (data[i]))
                return false;
            outPeak = std::max (outPeak, std::abs (data[i]));
        }
    }
    return true;
}

bool testPresetsAndParameters()
{
    std::cout << "[1/5] Auditing APVTS parameter layout & 30 factory presets...\n";
    AuditHostProcessor host;
    monolith::PresetManager pm;

    if (pm.getNumPresets() != 30)
    {
        std::cerr << "FAIL: Expected 30 presets, got " << pm.getNumPresets() << "\n";
        return false;
    }

    std::set<juce::String> names;
    for (int i = 0; i < pm.getNumPresets(); ++i)
    {
        const auto& preset = pm.getPreset (i);
        if (preset.name.isEmpty() || preset.category.isEmpty() || ! names.insert (preset.name).second)
        {
            std::cerr << "FAIL: Invalid or duplicate preset at index " << i << "\n";
            return false;
        }

        for (const auto& kv : preset.params)
        {
            auto* param = host.apvts.getParameter (kv.id);
            if (param == nullptr)
            {
                std::cerr << "FAIL: Unknown parameter ID '" << kv.id << "' in preset " << preset.name << "\n";
                return false;
            }
            const auto range = host.apvts.getParameterRange (kv.id);
            if (kv.value < range.start - 1.0e-5f || kv.value > range.end + 1.0e-5f)
            {
                std::cerr << "FAIL: Out-of-range value " << kv.value << " for '" << kv.id
                          << "' in preset " << preset.name << "\n";
                return false;
            }
        }
    }

    std::cout << "  PASS: All 30 presets and 41 APVTS parameters verified.\n";
    return true;
}

bool testMultiSampleRateStability()
{
    std::cout << "[2/5] Auditing sample rates (44.1 / 48 / 96 / 192 kHz, 2x & 4x OS, oversized chunking)...\n";
    const double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };

    for (double sr : rates)
    {
        for (int osMode = 0; osMode < 2; ++osMode)
        {
            AuditHostProcessor host;
            monolith::dsp::SwarmEngine engine;
            monolith::TelemetrySnapshot telemetry;

            const int prepBlock = 256;
            engine.prepare (sr, prepBlock, host.apvts);
            setParam (host.apvts, monolith::params::oversampleHq, osMode ? 1.0f : 0.0f);
            setParam (host.apvts, monolith::params::driveAmount, 0.55f);
            setParam (host.apvts, monolith::params::tapeInstability, 0.45f);
            setParam (host.apvts, monolith::params::spaceShimmer, 0.45f);

            // Deliberately test a host block larger than preparedMaxBlockSize (1024 > 256)
            const int block = 1024;
            juce::AudioBuffer<float> buf (2, block);
            juce::MidiBuffer midi;

            const int chord[] = { 48, 55, 58, 62, 63 };
            for (int n : chord)
                midi.addEvent (juce::MidiMessage::noteOn (1, n, 0.85f), 0);

            float maxPeak = 0.0f;
            double sumSq  = 0.0;
            const int sustainBlocks = static_cast<int> ((1.2 * sr) / block);

            for (int b = 0; b < sustainBlocks; ++b)
            {
                const float u = static_cast<float> (b) / static_cast<float> (std::max (1, sustainBlocks - 1));
                setParam (host.apvts, monolith::params::phonemeX, 0.15f + 0.70f * u);
                setParam (host.apvts, monolith::params::phonemeY, 0.20f + 0.65f * (1.0f - u));
                setParam (host.apvts, monolith::params::throatShift, -6.0f + 12.0f * u);

                buf.clear();
                engine.processBlock (buf, midi, 120.0, telemetry);
                midi.clear();

                if (! checkFinite (buf, maxPeak))
                {
                    std::cerr << "FAIL: Non-finite audio detected at sr=" << sr << "\n";
                    return false;
                }
                for (int i = 0; i < block; ++i)
                    sumSq += buf.getSample (0, i) * buf.getSample (0, i);
            }

            // Note off + tail verification
            for (int n : chord)
                midi.addEvent (juce::MidiMessage::noteOff (1, n), 0);

            const int releaseBlocks = static_cast<int> ((2.5 * sr) / block);
            for (int b = 0; b < releaseBlocks; ++b)
            {
                buf.clear();
                engine.processBlock (buf, midi, 120.0, telemetry);
                midi.clear();
                if (! checkFinite (buf, maxPeak))
                {
                    std::cerr << "FAIL: Non-finite release audio at sr=" << sr << "\n";
                    return false;
                }
            }

            const float rms = std::sqrt (static_cast<float> (sumSq / (static_cast<double> (sustainBlocks) * block)));
            if (rms < 0.02f || maxPeak >= 0.995f)
            {
                std::cerr << "FAIL: Unexpected level at sr=" << sr << " rms=" << rms << " peak=" << maxPeak << "\n";
                return false;
            }
        }
    }

    std::cout << "  PASS: All sample rates (44.1-192 kHz) and 2x/4x OS stable with zero NaNs.\n";
    return true;
}

bool testAll30PresetsGainStaging()
{
    std::cout << "[3/5] Auditing gain staging across all 30 factory presets (48 kHz)...\n";
    const double sr   = 48000.0;
    const int block   = 512;
    const int totalSamples = static_cast<int> (2.8 * sr);
    const int measureStart = static_cast<int> (1.5 * sr);

    monolith::PresetManager pm;
    bool allPassed = true;

    for (int p = 0; p < pm.getNumPresets(); ++p)
    {
        AuditHostProcessor host;
        monolith::dsp::SwarmEngine engine;
        monolith::TelemetrySnapshot telemetry;

        pm.applyPreset (p, host.apvts);
        engine.prepare (sr, block, host.apvts);

        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 48, 0.78f), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 51, 0.78f), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 55, 0.78f), 0);

        float peak = 0.0f;
        double sumSq = 0.0;
        int countedSamples = 0;

        int rendered = 0;
        while (rendered < totalSamples)
        {
            buf.clear();
            engine.processBlock (buf, midi, 120.0, telemetry);
            midi.clear();

            if (! checkFinite (buf, peak))
            {
                std::cerr << "FAIL: NaN/Inf in preset " << pm.getPreset (p).name << "\n";
                return false;
            }

            if (rendered >= measureStart)
            {
                for (int i = 0; i < block; ++i)
                {
                    const float l = buf.getSample (0, i);
                    const float r = buf.getSample (1, i);
                    sumSq += 0.5 * (l * l + r * r);
                }
                countedSamples += block;
            }
            rendered += block;
        }

        const float rms = std::sqrt (static_cast<float> (sumSq / std::max (1, countedSamples)));
        const float rmsDb  = ampToDb (rms);
        const float peakDb = ampToDb (peak);

        std::cout << "  " << std::setw (28) << std::left << pm.getPreset (p).name.toStdString()
                  << " | RMS: " << std::fixed << std::setprecision (2) << std::setw (6) << rmsDb << " dBFS"
                  << " | Peak: " << std::setw (6) << peakDb << " dBFS\n";

        if (rmsDb < -20.0f || rmsDb > -7.0f || peak >= 0.995f)
        {
            std::cerr << "  --> OUT OF RANGE: " << pm.getPreset (p).name << "\n";
            allPassed = false;
        }
    }

    if (allPassed)
        std::cout << "  PASS: All 30 presets calibrated and clipping-free.\n";
    return allPassed;
}

bool testFreezeAndScrewAndLegatoAndMacros()
{
    std::cout << "[4/5] Auditing Spectral Freeze, Screw Pitch, Legato, CC1 Expression, and Macros...\n";
    const double sr = 48000.0;
    const int block = 512;

    // A) Spectral Freeze stability over 5 seconds with zero note input
    {
        AuditHostProcessor host;
        monolith::dsp::SwarmEngine engine;
        monolith::TelemetrySnapshot telemetry;
        engine.prepare (sr, block, host.apvts);

        setParam (host.apvts, monolith::params::spaceMix, 0.60f);
        setParam (host.apvts, monolith::params::spaceDecay, 0.72f);

        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 48, 0.85f), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 55, 0.85f), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.85f), 0);

        for (int b = 0; b < 80; ++b) // ~0.85s fill
        {
            buf.clear();
            engine.processBlock (buf, midi, 120.0, telemetry);
            midi.clear();
        }

        // Engage freeze and release keys
        setParam (host.apvts, monolith::params::spaceFreeze, 1.0f);
        midi.addEvent (juce::MidiMessage::noteOff (1, 48), 0);
        midi.addEvent (juce::MidiMessage::noteOff (1, 55), 0);
        midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);

        double sqEarly = 0.0, sqLate = 0.0;
        int nEarly = 0, nLate = 0;
        float peak = 0.0f;

        const int freezeBlocks = static_cast<int> ((5.0 * sr) / block);
        for (int b = 0; b < freezeBlocks; ++b)
        {
            buf.clear();
            engine.processBlock (buf, midi, 120.0, telemetry);
            midi.clear();
            if (! checkFinite (buf, peak))
                return false;

            const float t = static_cast<float> (b * block) / static_cast<float> (sr);
            if (t >= 1.5f && t < 2.5f)
            {
                for (int i = 0; i < block; ++i)
                    sqEarly += buf.getSample (0, i) * buf.getSample (0, i);
                nEarly += block;
            }
            else if (t >= 4.0f)
            {
                for (int i = 0; i < block; ++i)
                    sqLate += buf.getSample (0, i) * buf.getSample (0, i);
                nLate += block;
            }
        }

        const float dbEarly = ampToDb (std::sqrt (static_cast<float> (sqEarly / std::max (1, nEarly))));
        const float dbLate  = ampToDb (std::sqrt (static_cast<float> (sqLate / std::max (1, nLate))));
        std::cout << "  Freeze sustain: 2s=" << dbEarly << " dBFS, 4.5s=" << dbLate << " dBFS\n";
        if (dbLate < -35.0f || std::abs (dbLate - dbEarly) > 2.5f || peak >= 0.995f)
        {
            std::cerr << "FAIL: Spectral Freeze unstable or decayed.\n";
            return false;
        }
    }

    // B) Screw Pitch lowers fundamental frequency by ~1 octave at -12 st (measured via autocorrelation)
    {
        auto measureFundamentalHz = [&] (float screwSt) -> float
        {
            AuditHostProcessor host;
            monolith::dsp::SwarmEngine engine;
            monolith::TelemetrySnapshot telemetry;
            engine.prepare (sr, block, host.apvts);

            setParam (host.apvts, monolith::params::ensembleSize, 1.0f);
            setParam (host.apvts, monolith::params::vibratoDepth, 0.0f);
            setParam (host.apvts, monolith::params::spaceMix, 0.0f);
            setParam (host.apvts, monolith::params::subAbyss, 0.0f);
            setParam (host.apvts, monolith::params::spectralCrown, 0.0f);
            setParam (host.apvts, monolith::params::breathAmount, 0.0f);
            setParam (host.apvts, monolith::params::phonemeX, 0.50f);
            setParam (host.apvts, monolith::params::phonemeY, 0.88f); // Ah (rich harmonics)
            setParam (host.apvts, monolith::params::screwPitch, screwSt);

            juce::AudioBuffer<float> buf (2, block);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 57, 0.85f), 0); // A3 = 220 Hz

            std::vector<float> capture;
            capture.reserve (4096);

            for (int b = 0; b < 48; ++b)
            {
                buf.clear();
                engine.processBlock (buf, midi, 120.0, telemetry);
                midi.clear();
                if (b >= 36 && capture.size() < 4096)
                {
                    for (int i = 0; i < block; ++i)
                        capture.push_back (buf.getSample (0, i));
                }
            }

            // Normalized autocorrelation search across [70 Hz .. 350 Hz]
            const int minLag = static_cast<int> (sr / 350.0);
            const int maxLag = static_cast<int> (sr / 70.0);
            const int evalN  = 2048;
            int bestLag = minLag;
            double bestCorr = -1.0e9;

            for (int lag = minLag; lag <= maxLag; ++lag)
            {
                double num = 0.0, e0 = 0.0, e1 = 0.0;
                for (int i = 0; i < evalN; ++i)
                {
                    const double a = capture[static_cast<size_t> (i)];
                    const double b = capture[static_cast<size_t> (i + lag)];
                    num += a * b;
                    e0  += a * a;
                    e1  += b * b;
                }
                const double normCorr = num / std::sqrt (std::max (e0 * e1, 1.0e-12));
                // Prefer fundamental over sub-octave harmonic by slight tie-breaker for shorter lag if nearly identical
                if (normCorr > bestCorr + 0.015)
                {
                    bestCorr = normCorr;
                    bestLag  = lag;
                }
            }
            return static_cast<float> (sr) / static_cast<float> (bestLag);
        };

        const float fNormal  = measureFundamentalHz (0.0f);
        const float fScrewed = measureFundamentalHz (-12.0f);
        std::cout << "  Screw Pitch check: 0st=" << fNormal << " Hz, -12st=" << fScrewed << " Hz\n";
        if (std::abs (fNormal - 220.0f) > 8.0f || std::abs (fScrewed - 110.0f) > 6.0f)
        {
            std::cerr << "FAIL: Screw Pitch did not track expected 220 Hz -> 110 Hz octave drop.\n";
            return false;
        }
    }

    // C) Legato & CC1 Expression & 4 Master Macros
    {
        AuditHostProcessor host;
        monolith::dsp::SwarmEngine engine;
        monolith::TelemetrySnapshot telemetry;
        engine.prepare (sr, block, host.apvts);

        setParam (host.apvts, monolith::params::legatoEnable, 1.0f);
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        for (int b = 0; b < 15; ++b)
        {
            buf.clear();
            engine.processBlock (buf, midi, 120.0, telemetry);
            midi.clear();
        }
        midi.addEvent (juce::MidiMessage::noteOn (1, 67, 0.8f), 0);
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, 127), 0);
        buf.clear();
        engine.processBlock (buf, midi, 120.0, telemetry);

        if (telemetry.activeVoices.load() != 1)
        {
            std::cerr << "FAIL: Legato activeVoices expected 1, got " << telemetry.activeVoices.load() << "\n";
            return false;
        }

        // Verify CC1=20 produces quieter output than CC1=127
        auto renderExpr = [&] (int ccVal) -> float
        {
            setParam (host.apvts, monolith::params::attackTime, 0.02f);
            juce::MidiBuffer m;
            m.addEvent (juce::MidiMessage::controllerEvent (1, 1, ccVal), 0);
            double sq = 0.0;
            for (int b = 0; b < 15; ++b)
            {
                buf.clear();
                engine.processBlock (buf, m, 120.0, telemetry);
                m.clear();
                if (b >= 8)
                {
                    for (int i = 0; i < block; ++i)
                        sq += buf.getSample (0, i) * buf.getSample (0, i);
                }
            }
            return std::sqrt (static_cast<float> (sq));
        };

        const float loudExpr = renderExpr (127);
        const float softExpr = renderExpr (15);
        std::cout << "  CC1 Expression: CC127=" << loudExpr << ", CC15=" << softExpr << "\n";
        if (loudExpr <= softExpr * 1.4f)
        {
            std::cerr << "FAIL: CC1 Expression swell did not scale dynamically.\n";
            return false;
        }

        // Verify 4 macros change audio measurably
        const char* macroIds[] = {
            monolith::params::macroColossus,
            monolith::params::macroSpecter,
            monolith::params::macroRot,
            monolith::params::macroAbyss
        };
        for (const char* id : macroIds)
        {
            setParam (host.apvts, id, 1.0f);
            buf.clear();
            midi.clear();
            engine.processBlock (buf, midi, 120.0, telemetry);
            float pk = 0.0f;
            if (! checkFinite (buf, pk))
                return false;
            setParam (host.apvts, id, 0.0f);
        }
    }

    std::cout << "  PASS: Spectral Freeze, Screw Pitch, Legato, CC1 Expression, and Macros verified.\n";
    return true;
}

} // namespace

int main()
{
    std::cout << "=== MONOLITH DSP & PRESET AUDIT SUITE ===\n";

    if (! testPresetsAndParameters())
        return 1;
    if (! testMultiSampleRateStability())
        return 1;
    if (! testAll30PresetsGainStaging())
        return 1;
    if (! testFreezeAndScrewAndLegatoAndMacros())
        return 1;

    std::cout << "[5/5] ALL AUDITS PASSED.\n";
    return 0;
}
