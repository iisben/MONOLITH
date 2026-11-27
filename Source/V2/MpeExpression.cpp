#include "MpeExpression.h"
#include "../State/ParameterIDs.h"
#include <algorithm>

namespace monolith::v2
{

void MpeExpression::bindParameters (juce::AudioProcessorValueTreeState& apvts)
{
    using namespace monolith::params;
    pEnable   = apvts.getRawParameterValue (v2MpeEnable);
    pPbRange  = apvts.getRawParameterValue (v2MpePbRange);
    pVibrato  = apvts.getRawParameterValue (v2ExprVibrato);
    pEffort   = apvts.getRawParameterValue (v2ExprEffort);
    pOpenness = apvts.getRawParameterValue (v2ExprOpenness);
    reset();
}

void MpeExpression::reset() noexcept
{
    chBendNorm.fill (0.0f);
    chPressure.fill (0.0f);
    chTimbre.fill (0.5f);
    noteChannel.fill (-1);
    polyPressure.fill (0.0f);
    globalTimbre = 0.5f;
    timbreSeen = false;
    breath = 0.0f;
    breathSeen = false;
}

void MpeExpression::beginBlock() noexcept
{
    const bool on = pEnable != nullptr && pEnable->load (std::memory_order_relaxed) >= 0.5f;
    if (on != mpeOn)
    {
        // Zone (de)activation: drop stale per-channel state so nothing leaks across the mode switch
        chBendNorm.fill (0.0f);
        chPressure.fill (0.0f);
        chTimbre.fill (0.5f);
        noteChannel.fill (-1);
        polyPressure.fill (0.0f);
        mpeOn = on;
    }
    pbRange       = pPbRange  != nullptr ? pPbRange->load (std::memory_order_relaxed)  : 48.0f;
    vibratoDepth  = pVibrato  != nullptr ? pVibrato->load (std::memory_order_relaxed)  : 0.4f;
    effortDepth   = pEffort   != nullptr ? pEffort->load (std::memory_order_relaxed)   : 0.5f;
    opennessDepth = pOpenness != nullptr ? pOpenness->load (std::memory_order_relaxed) : 0.5f;
}

bool MpeExpression::consume (const juce::MidiMessage& msg) noexcept
{
    const int ch = std::clamp (msg.getChannel(), 1, 16);
    const bool member = mpeOn && ch >= 2;

    if (msg.isNoteOn())
    {
        const int n = msg.getNoteNumber();
        noteChannel[static_cast<size_t> (n)] = static_cast<int8_t> (mpeOn ? ch : -1);
        polyPressure[static_cast<size_t> (n)] = 0.0f;
        return false;
    }
    if (msg.isNoteOff())
    {
        // Keep the note -> channel binding so the release tail keeps its per-note bend
        polyPressure[static_cast<size_t> (msg.getNoteNumber())] = 0.0f;
        return false;
    }
    if (msg.isPitchWheel() && member)
    {
        chBendNorm[static_cast<size_t> (ch)] = std::clamp ((static_cast<float> (msg.getPitchWheelValue()) - 8192.0f) / 8192.0f, -1.0f, 1.0f);
        return true;
    }
    if (msg.isChannelPressure() && member)
    {
        chPressure[static_cast<size_t> (ch)] = static_cast<float> (msg.getChannelPressureValue()) / 127.0f;
        return true;
    }
    if (msg.isAftertouch() && mpeOn)
    {
        polyPressure[static_cast<size_t> (msg.getNoteNumber())] = static_cast<float> (msg.getAfterTouchValue()) / 127.0f;
        return true;
    }
    if (msg.isController())
    {
        const int cc = msg.getControllerNumber();
        const float v = static_cast<float> (msg.getControllerValue()) / 127.0f;
        if (cc == 74)
        {
            if (member) chTimbre[static_cast<size_t> (ch)] = v;
            else        { globalTimbre = v; timbreSeen = true; }
            return member;
        }
        if (cc == 2)
        {
            breath = v;
            breathSeen = true;
            return false;
        }
        if (cc == 121) // Reset All Controllers
        {
            if (member) { chBendNorm[static_cast<size_t> (ch)] = 0.0f; chPressure[static_cast<size_t> (ch)] = 0.0f; chTimbre[static_cast<size_t> (ch)] = 0.5f; }
            else        { globalTimbre = 0.5f; breath = 0.0f; }
            return false;
        }
    }
    return false;
}

void MpeExpression::applyToVoice (int midiNote, dsp::VoiceRenderParams& vp) const noexcept
{
    const int note = std::clamp (midiNote, 0, 127);
    const int ch = mpeOn ? noteChannel[static_cast<size_t> (note)] : -1;

    float bendSt = 0.0f, press = 0.0f;
    float timbre = timbreSeen ? globalTimbre : 0.5f;
    if (ch >= 2)
    {
        bendSt = chBendNorm[static_cast<size_t> (ch)] * pbRange;
        press  = chPressure[static_cast<size_t> (ch)];
        timbre = chTimbre[static_cast<size_t> (ch)];
    }
    if (mpeOn)
        press = std::max (press, polyPressure[static_cast<size_t> (note)]);

    const float drive = std::max (press, breathSeen ? breath : 0.0f);
    const float slide = timbre - 0.5f;

    if (bendSt != 0.0f)
        vp.pitchBendSt += bendSt;
    if (drive > 0.0f)
    {
        vp.aftertouch01      = std::max (vp.aftertouch01, std::clamp (drive * effortDepth, 0.0f, 1.0f));
        vp.glottalTension01  = std::clamp (vp.glottalTension01 + 0.15f * effortDepth * drive, 0.0f, 1.0f);
        vp.spectralCrown01   = std::clamp (vp.spectralCrown01 + 0.30f * effortDepth * drive, 0.0f, 1.0f);
        vp.vibratoDepth01    = std::clamp (vp.vibratoDepth01 + 0.60f * vibratoDepth * drive, 0.0f, 1.0f);
    }
    if (slide != 0.0f)
    {
        vp.phonemeY        = std::clamp (vp.phonemeY + 0.70f * opennessDepth * slide, 0.0f, 1.0f);
        vp.spectralCrown01 = std::clamp (vp.spectralCrown01 + 0.25f * opennessDepth * slide, 0.0f, 1.0f);
    }
}

float MpeExpression::getMaxPressure() const noexcept
{
    float m = 0.0f;
    if (mpeOn)
    {
        for (int c = 2; c <= 16; ++c)
            m = std::max (m, chPressure[static_cast<size_t> (c)]);
        for (const float p : polyPressure)
            m = std::max (m, p);
    }
    return m;
}

void MpeExpression::publishTelemetry() noexcept
{
    uiMpeActive.store (mpeOn, std::memory_order_relaxed);
    uiPressure.store (std::max (getMaxPressure(), getBreath()), std::memory_order_relaxed);
    float slide = timbreSeen ? globalTimbre : 0.5f;
    if (mpeOn)
        for (int c = 2; c <= 16; ++c)
            if (std::abs (chTimbre[static_cast<size_t> (c)] - 0.5f) > std::abs (slide - 0.5f))
                slide = chTimbre[static_cast<size_t> (c)];
    uiSlide.store (slide, std::memory_order_relaxed);
}

} // namespace monolith::v2
