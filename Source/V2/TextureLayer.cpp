#include "TextureLayer.h"
#include "../State/ParameterIDs.h"
#include "V2Assets.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <algorithm>
#include <cmath>

namespace monolith::v2
{

namespace
{
    constexpr float kLayerTrim   = 0.25f;  // -12 dB: assets sit at -12 dBFS RMS, so a 4-note chord at 100 % lands near -12 dBFS
    constexpr float kEnvFloor    = 1.0e-4f; // -80 dB release end
    constexpr float kUnpitchedTrack = 0.25f;
    constexpr int   kCtrlStep    = 32;      // filter coefficient refresh (samples)

    inline float hermite (const float* d, float t) noexcept
    {
        // d points at x[-1]; 4-point, 3rd-order Hermite (Catmull-Rom)
        const float xm1 = d[0], x0 = d[1], x1 = d[2], x2 = d[3];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
    }

    struct SvfCoeffs { float a1, a2, a3; };

    inline SvfCoeffs svfCoeffs (float fc, double sr) noexcept
    {
        constexpr float k = 1.41421356f; // Q = 0.707 (Butterworth 12 dB/oct)
        const float g = static_cast<float> (std::tan (juce::MathConstants<double>::pi * std::min<double> (fc, 0.45 * sr) / sr));
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        return { a1, a2, g * a2 };
    }

    inline float onePoleCoef (double seconds, double sr) noexcept
    {
        return static_cast<float> (std::exp (-1.0 / (seconds * sr)));
    }

    std::unique_ptr<juce::AudioFormatReader> makeReader (const char* data, int size)
    {
        static juce::FlacAudioFormat flac; // decoder is stateless; readers own their streams
        return std::unique_ptr<juce::AudioFormatReader> (
            flac.createReaderFor (new juce::MemoryInputStream (data, static_cast<size_t> (size), false), true));
    }
} // namespace

// ============================================================================
TextureBank::TextureBank()
{
    samples.reserve (static_cast<size_t> (kNumTextureAssets));
    for (const auto& info : kTextureAssets)
    {
        TextureSample s;
        s.looped = info.looped;
        s.pitched = info.pitched;
        s.rootNote = info.rootNote;

        int size = 0;
        const char* data = V2Assets::getNamedResource (info.resourceName, size);
        auto reader = (data != nullptr) ? makeReader (data, size) : nullptr;
        if (reader != nullptr && reader->lengthInSamples > 8)
        {
            const int len = static_cast<int> (reader->lengthInSamples);
            const int nch = static_cast<int> (std::min<unsigned int> (2, reader->numChannels));
            juce::AudioBuffer<float> tmp (nch, len);
            reader->read (&tmp, 0, len, 0, true, nch > 1);
            s.length = len;
            s.sampleRate = reader->sampleRate;
            for (int c = 0; c < 2; ++c)
            {
                const float* src = tmp.getReadPointer (std::min (c, nch - 1));
                auto& dst = s.ch[static_cast<size_t> (c)];
                dst.assign (static_cast<size_t> (len + 4), 0.0f);
                std::copy (src, src + len, dst.begin() + 1);
                if (s.looped) // wrap guards: x[-1] = x[len-1], x[len..len+2] = x[0..2]
                {
                    dst[0] = src[len - 1];
                    for (int k = 0; k < 3; ++k)
                        dst[static_cast<size_t> (len + 1 + k)] = src[k];
                }
            }
        }
        else
        {
            // Missing/corrupt resource: keep a silent placeholder so indices stay stable
            s.length = 8;
            for (auto& c : s.ch) c.assign (12, 0.0f);
        }
        samples.push_back (std::move (s));
    }
}

const TextureBank& TextureBank::get()
{
    static const TextureBank bank; // thread-safe one-time decode, shared by all instances
    return bank;
}

size_t TextureBank::bytesResident() const noexcept
{
    size_t b = 0;
    for (const auto& s : samples)
        for (const auto& c : s.ch) b += c.size() * sizeof (float);
    return b;
}

// ============================================================================
TextureLayer::TextureLayer() : bank (TextureBank::get()) {}

void TextureLayer::bindParameters (juce::AudioProcessorValueTreeState& apvts)
{
    using namespace monolith::params;
    pMix      = apvts.getRawParameterValue (v2TexMix);
    pAsset    = apvts.getRawParameterValue (v2TexAsset);
    pTune     = apvts.getRawParameterValue (v2TexTune);
    pKeytrack = apvts.getRawParameterValue (v2TexKeytrack);
    pAttack   = apvts.getRawParameterValue (v2TexAttack);
    pRelease  = apvts.getRawParameterValue (v2TexRelease);
    pVelSens  = apvts.getRawParameterValue (v2TexVelSens);
    pHighPass = apvts.getRawParameterValue (v2TexHighPass);
    pLowPass  = apvts.getRawParameterValue (v2TexLowPass);
    pWidth    = apvts.getRawParameterValue (v2TexWidth);
    pCc64Freeze = apvts.getRawParameterValue (cc64Freeze);
    jassert (pMix && pAsset && pTune && pKeytrack && pAttack && pRelease && pVelSens && pHighPass && pLowPass && pWidth && pCc64Freeze);
}

void TextureLayer::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    chunkCapacity = std::max (64, maxBlockSize);
    scratchL.assign (static_cast<size_t> (chunkCapacity), 0.0f);
    scratchR.assign (static_cast<size_t> (chunkCapacity), 0.0f);
    ringL.assign (static_cast<size_t> (kAlignRingLen), 0.0f);
    ringR.assign (static_cast<size_t> (kAlignRingLen), 0.0f);
    mixCoef = onePoleCoef (0.020, sr);
    cutoffCoef = onePoleCoef (0.030, sr / kCtrlStep);
    reset();
}

void TextureLayer::reset() noexcept
{
    for (auto& v : voices) v = Voice {};
    std::fill (ringL.begin(), ringL.end(), 0.0f);
    std::fill (ringR.begin(), ringR.end(), 0.0f);
    ringWrite = 0;
    for (auto& f : hp) f.reset();
    for (auto& f : lp) f.reset();
    mixSmoothed = 0.0f;
    sustainPedal = false;
    active = false;
    activeVoiceCount.store (0, std::memory_order_relaxed);
}

// ── Voice management ─────────────────────────────────────────────────────────
double TextureLayer::voiceIncrement (const Voice& v) const noexcept
{
    const auto& s = bank[v.asset];
    const float track = pKeytrack->load (std::memory_order_relaxed) * (s.pitched ? 1.0f : kUnpitchedTrack);
    const float semis = std::clamp (pTune->load (std::memory_order_relaxed) + track * static_cast<float> (v.note - s.rootNote),
                                    -36.0f, 24.0f);
    return (s.sampleRate / sr) * std::exp2 (static_cast<double> (semis) / 12.0);
}

void TextureLayer::noteOn (int note, float velocity01) noexcept
{
    // Retrigger of a ringing note: fade the old instance so repeated notes never click
    int audible = 0;
    for (auto& v : voices)
    {
        if (v.stage == Stage::idle) continue;
        if (v.note == note && v.stage != Stage::kill) { v.stage = Stage::kill; v.held = false; }
        if (v.stage != Stage::kill) ++audible;
    }
    if (audible >= kMaxVoices) // steal the oldest audible voice with a 3 ms fade
    {
        Voice* oldest = nullptr;
        for (auto& v : voices)
            if (v.stage != Stage::idle && v.stage != Stage::kill && (oldest == nullptr || v.age < oldest->age))
                oldest = &v;
        if (oldest != nullptr) oldest->stage = Stage::kill;
    }

    Voice* slot = nullptr;
    for (auto& v : voices)
        if (v.stage == Stage::idle) { slot = &v; break; }
    if (slot == nullptr) // all slots fading: reuse the quietest
    {
        for (auto& v : voices)
            if (slot == nullptr || v.env < slot->env) slot = &v;
    }

    const int nAssets = bank.size();
    const int asset = std::clamp (static_cast<int> (std::lround (pAsset->load (std::memory_order_relaxed))), 0, nAssets - 1);
    const float sens = std::clamp (pVelSens->load (std::memory_order_relaxed), 0.0f, 1.0f);
    const float vel = std::clamp (velocity01, 0.0f, 1.0f);

    Voice& v = *slot;
    v = Voice {};
    v.stage = Stage::attack;
    v.note = note;
    v.asset = asset;
    v.held = true;
    v.pos = 0.0;
    v.env = 0.0f;
    v.velGain = (1.0f - sens) + sens * std::pow (vel, 1.6f);
    v.attackStep = 1.0f / static_cast<float> (std::max (1.0, pAttack->load (std::memory_order_relaxed) * sr));
    v.killStep = 1.0f / static_cast<float> (std::max (1.0, 0.003 * sr));
    v.age = ++ageCounter;
    v.inc = voiceIncrement (v);
}

void TextureLayer::noteOff (int note) noexcept
{
    for (auto& v : voices)
    {
        if (v.note != note || ! v.held) continue;
        v.held = false;
        if (! sustainPedal && (v.stage == Stage::attack || v.stage == Stage::sustain))
            v.stage = Stage::release;
    }
}

void TextureLayer::allNotesOff (bool hard) noexcept
{
    for (auto& v : voices)
    {
        if (v.stage == Stage::idle) continue;
        v.held = false;
        v.stage = hard ? Stage::kill : (v.stage == Stage::kill ? Stage::kill : Stage::release);
    }
    sustainPedal = false;
}

void TextureLayer::handleMidi (const juce::MidiMessage& m) noexcept
{
    if (m.isNoteOn())
        noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
        noteOff (m.getNoteNumber());
    else if (m.isController())
    {
        const int cc = m.getControllerNumber();
        if (cc == 64 && pCc64Freeze->load (std::memory_order_relaxed) < 0.5f) // mirrors engine: CC64 is sustain unless routed to freeze
        {
            const bool down = m.getControllerValue() >= 64;
            if (sustainPedal && ! down)
                for (auto& v : voices)
                    if (! v.held && (v.stage == Stage::attack || v.stage == Stage::sustain))
                        v.stage = Stage::release;
            sustainPedal = down;
        }
        else if (cc == 120 || cc == 123)
            allNotesOff (cc == 120);
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        allNotesOff (m.isAllSoundOff());
}

// ── Rendering ────────────────────────────────────────────────────────────────
void TextureLayer::renderVoices (int start, int num) noexcept
{
    if (num <= 0) return;
    float* outL = scratchL.data() + start;
    float* outR = scratchR.data() + start;

    for (auto& v : voices)
    {
        if (v.stage == Stage::idle) continue;
        const auto& s = bank[v.asset];
        const float* dL = s.ch[0].data();
        const float* dR = s.ch[1].data();
        const double len = static_cast<double> (s.length);

        for (int i = 0; i < num; ++i)
        {
            switch (v.stage)
            {
                case Stage::attack:
                    v.env += v.attackStep;
                    if (v.env >= 1.0f) { v.env = 1.0f; v.stage = Stage::sustain; }
                    break;
                case Stage::release:
                    v.env *= releaseCoef;
                    if (v.env < kEnvFloor) v.stage = Stage::idle;
                    break;
                case Stage::kill:
                    v.env -= v.killStep;
                    if (v.env <= 0.0f) { v.env = 0.0f; v.stage = Stage::idle; }
                    break;
                case Stage::sustain:
                case Stage::idle:
                    break;
            }
            if (v.stage == Stage::idle) break;

            const int idx = static_cast<int> (v.pos);
            const float t = static_cast<float> (v.pos - idx);
            const float g = v.env * v.velGain;
            outL[i] += g * hermite (dL + idx, t);
            outR[i] += g * hermite (dR + idx, t);

            v.pos += v.inc;
            if (v.pos >= len)
            {
                if (s.looped) v.pos -= len;
                else { v.stage = Stage::idle; break; } // one-shot: asset carries its own 50 ms fade-out
            }
        }
        if (v.stage == Stage::idle) v = Voice {};
    }
}

void TextureLayer::finishChunk (juce::AudioBuffer<float>& buffer, int bufferOffset, int num, int alignDelay) noexcept
{
    // Tone (TPT SVF HP -> LP), width (M/S) and mix gain on the summed layer
    const float hpTarget = std::log2 (std::clamp (pHighPass->load (std::memory_order_relaxed), 20.0f, 2000.0f));
    const float lpTarget = std::log2 (std::clamp (pLowPass->load (std::memory_order_relaxed), 400.0f, 20000.0f));
    const float widthTarget = std::clamp (pWidth->load (std::memory_order_relaxed), 0.0f, 2.0f);
    const float mix = std::clamp (pMix->load (std::memory_order_relaxed), 0.0f, 1.0f);
    const float gainTarget = mix * mix * kLayerTrim;
    constexpr float k = 1.41421356f;

    float* sL = scratchL.data();
    float* sR = scratchR.data();
    const int nch = buffer.getNumChannels();
    float* oL = buffer.getWritePointer (0, bufferOffset);
    float* oR = nch > 1 ? buffer.getWritePointer (1, bufferOffset) : nullptr;
    const int mask = kAlignRingLen - 1;
    const int delay = std::clamp (alignDelay, 0, kAlignRingLen - 1);

    for (int base = 0; base < num; base += kCtrlStep)
    {
        hpLog = hpTarget + (hpLog - hpTarget) * cutoffCoef;
        lpLog = lpTarget + (lpLog - lpTarget) * cutoffCoef;
        const auto h = svfCoeffs (std::exp2 (hpLog), sr);
        const auto l = svfCoeffs (std::exp2 (lpLog), sr);
        const float wStep = (widthTarget - widthSmoothed) / static_cast<float> (kCtrlStep);
        const int end = std::min (num, base + kCtrlStep);

        for (int i = base; i < end; ++i)
        {
            float xs[2] = { sL[i], sR[i] };
            for (int c = 0; c < 2; ++c)
            {
                auto& f = hp[static_cast<size_t> (c)];
                const float v3 = xs[c] - f.ic2;
                const float v1 = h.a1 * f.ic1 + h.a2 * v3;
                const float v2 = f.ic2 + h.a2 * f.ic1 + h.a3 * v3;
                f.ic1 = 2.0f * v1 - f.ic1;
                f.ic2 = 2.0f * v2 - f.ic2;
                const float high = xs[c] - k * v1 - v2;

                auto& g = lp[static_cast<size_t> (c)];
                const float u3 = high - g.ic2;
                const float u1 = l.a1 * g.ic1 + l.a2 * u3;
                const float u2 = g.ic2 + l.a2 * g.ic1 + l.a3 * u3;
                g.ic1 = 2.0f * u1 - g.ic1;
                g.ic2 = 2.0f * u2 - g.ic2;
                xs[c] = u2;
            }
            widthSmoothed += wStep;
            mixSmoothed = gainTarget + (mixSmoothed - gainTarget) * mixCoef;
            const float m = 0.5f * (xs[0] + xs[1]);
            const float sd = 0.5f * (xs[0] - xs[1]) * widthSmoothed;

            ringL[static_cast<size_t> (ringWrite)] = (m + sd) * mixSmoothed;
            ringR[static_cast<size_t> (ringWrite)] = (m - sd) * mixSmoothed;
            const int rd = (ringWrite - delay) & mask;
            ringWrite = (ringWrite + 1) & mask;

            if (oR != nullptr)
            {
                oL[i] += ringL[static_cast<size_t> (rd)];
                oR[i] += ringR[static_cast<size_t> (rd)];
            }
            else
                oL[i] += 0.5f * (ringL[static_cast<size_t> (rd)] + ringR[static_cast<size_t> (rd)]);
        }
        widthSmoothed = widthTarget; // linear 32-sample ramp lands exactly on target
    }
    std::fill (sL, sL + num, 0.0f);
    std::fill (sR, sR + num, 0.0f);
}

void TextureLayer::process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, int engineLatencySamples) noexcept
{
    jassert (pMix != nullptr && chunkCapacity > 0);
    const float mix = std::clamp (pMix->load (std::memory_order_relaxed), 0.0f, 1.0f);

    if (! active)
    {
        if (mix <= 0.0f)
            return; // ── BYPASS: buffer untouched, zero cost, zero latency
        // Wake: start filters/width at their targets so the first note never sweeps
        hpLog = std::log2 (std::clamp (pHighPass->load (std::memory_order_relaxed), 20.0f, 2000.0f));
        lpLog = std::log2 (std::clamp (pLowPass->load (std::memory_order_relaxed), 400.0f, 20000.0f));
        widthSmoothed = std::clamp (pWidth->load (std::memory_order_relaxed), 0.0f, 2.0f);
        for (auto& f : hp) f.reset();
        for (auto& f : lp) f.reset();
        std::fill (ringL.begin(), ringL.end(), 0.0f);
        std::fill (ringR.begin(), ringR.end(), 0.0f);
        mixSmoothed = 0.0f;
        active = true;
    }

    juce::ScopedNoDenormals noDenormals;
    releaseCoef = std::pow (kEnvFloor, 1.0f / static_cast<float> (std::max (1.0, pRelease->load (std::memory_order_relaxed) * sr)));
    for (auto& v : voices) // tune / keytrack follow live for ringing voices
        if (v.stage != Stage::idle) v.inc = voiceIncrement (v);

    const int n = buffer.getNumSamples();
    auto it = midi.cbegin();
    const auto end = midi.cend();
    for (int off = 0; off < n; off += chunkCapacity)
    {
        const int len = std::min (chunkCapacity, n - off);
        int cursor = 0;
        while (it != end)
        {
            const auto meta = *it;
            const int p = std::clamp (meta.samplePosition - off, 0, len);
            if (p >= len && off + len < n) break; // belongs to a later chunk
            renderVoices (cursor, p - cursor);
            cursor = p;
            if (mix > 0.0f) handleMidi (meta.getMessage()); // no new voices while fading out to bypass
            ++it;
        }
        renderVoices (cursor, len - cursor);
        finishChunk (buffer, off, len, engineLatencySamples);
    }

    int count = 0;
    for (const auto& v : voices) count += v.stage != Stage::idle ? 1 : 0;
    activeVoiceCount.store (count, std::memory_order_relaxed);

    if (mix <= 0.0f && mixSmoothed < 1.0e-7f)
    {
        // Faded fully out: drop to hard bypass (ringing voices are silent at this gain)
        for (auto& v : voices) v = Voice {};
        activeVoiceCount.store (0, std::memory_order_relaxed);
        mixSmoothed = 0.0f;
        active = false;
    }
}

} // namespace monolith::v2
