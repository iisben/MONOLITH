#include "MidiLearn.h"

namespace monolith::v2
{

namespace
{
    [[nodiscard]] bool isLearnableCc (int cc) noexcept
    {
        // Channel-mode messages (120-127) and the sustain pedal keep their fixed meanings
        return cc >= 0 && cc < 120 && cc != 64;
    }
}

MidiLearn::MidiLearn()
{
    for (auto& c : ccToParam)
        c.store (static_cast<int16_t> (kNone), std::memory_order_relaxed);
}

void MidiLearn::bind (int cc, int paramIndex) noexcept
{
    if (! isLearnableCc (cc) || paramIndex < 0 || paramIndex >= static_cast<int> (touched.size()))
        return;
    clearParam (paramIndex);
    ccToParam[static_cast<size_t> (cc)].store (static_cast<int16_t> (paramIndex), std::memory_order_release);
}

void MidiLearn::clearParam (int paramIndex) noexcept
{
    for (auto& c : ccToParam)
        if (c.load (std::memory_order_relaxed) == paramIndex)
            c.store (static_cast<int16_t> (kNone), std::memory_order_release);
}

void MidiLearn::clearAll() noexcept
{
    for (auto& c : ccToParam)
        c.store (static_cast<int16_t> (kNone), std::memory_order_release);
}

int MidiLearn::getCcForParam (int paramIndex) const noexcept
{
    for (int cc = 0; cc < 128; ++cc)
        if (ccToParam[static_cast<size_t> (cc)].load (std::memory_order_relaxed) == paramIndex)
            return cc;
    return kNone;
}

int MidiLearn::getParamForCc (int cc) const noexcept
{
    return (cc >= 0 && cc < 128) ? ccToParam[static_cast<size_t> (cc)].load (std::memory_order_relaxed) : kNone;
}

int MidiLearn::getNumBindings() const noexcept
{
    int n = 0;
    for (const auto& c : ccToParam)
        n += (c.load (std::memory_order_relaxed) != kNone) ? 1 : 0;
    return n;
}

void MidiLearn::handleCc (int cc, int value) noexcept
{
    if (! isLearnableCc (cc))
        return;

    const int want = armed.load (std::memory_order_acquire);
    if (want != kNone)
    {
        int expected = want;
        if (armed.compare_exchange_strong (expected, kNone, std::memory_order_acq_rel))
        {
            // Unbind the param's previous CC and this CC's previous param (lock-free stores only)
            for (auto& c : ccToParam)
                if (c.load (std::memory_order_relaxed) == want)
                    c.store (static_cast<int16_t> (kNone), std::memory_order_relaxed);
            ccToParam[static_cast<size_t> (cc)].store (static_cast<int16_t> (want), std::memory_order_release);
            learnCompleted.store (true, std::memory_order_release);
        }
    }

    const int param = ccToParam[static_cast<size_t> (cc)].load (std::memory_order_acquire);
    if (param == kNone)
        return;

    const auto scope1 = fifo.write (1);
    if (scope1.blockSize1 > 0)
        events[static_cast<size_t> (scope1.startIndex1)] = { static_cast<int16_t> (param), static_cast<float> (value) / 127.0f };
    // FIFO full: the oldest pending values still apply; this one is dropped (next CC message supersedes it)
}

int MidiLearn::drain (juce::AudioProcessor& processor)
{
    const auto& params = processor.getParameters();
    const int ready = fifo.getNumReady();
    if (ready <= 0)
        return 0;

    // Coalesce: last value per parameter wins, each parameter gets one gesture-bracketed host notification

    const auto read = fifo.read (ready);
    auto consume = [&] (int start, int size) {
        for (int i = 0; i < size; ++i)
        {
            const auto& e = events[static_cast<size_t> (start + i)];
            if (e.param >= 0 && e.param < params.size() && e.param < static_cast<int> (touched.size()))
            {
                touched[static_cast<size_t> (e.param)] = true;
                coalesced[static_cast<size_t> (e.param)] = e.value01;
            }
        }
    };
    consume (read.startIndex1, read.blockSize1);
    consume (read.startIndex2, read.blockSize2);

    int applied = 0;
    for (int i = 0; i < std::min (params.size(), static_cast<int> (touched.size())); ++i)
    {
        if (! touched[static_cast<size_t> (i)])
            continue;
        touched[static_cast<size_t> (i)] = false;
        auto* p = params[i];
        p->beginChangeGesture();
        p->setValueNotifyingHost (coalesced[static_cast<size_t> (i)]);
        p->endChangeGesture();
        ++applied;
    }
    return applied;
}

juce::ValueTree MidiLearn::toValueTree (const juce::AudioProcessor& processor) const
{
    juce::ValueTree t ("MidiMap");
    t.setProperty ("scope", getScope() == Scope::global ? "global" : "preset", nullptr);
    const auto& params = processor.getParameters();
    for (int cc = 0; cc < 128; ++cc)
    {
        const int pi = getParamForCc (cc);
        if (pi < 0 || pi >= params.size())
            continue;
        if (auto* wp = dynamic_cast<juce::AudioProcessorParameterWithID*> (params[pi]))
        {
            juce::ValueTree b ("Bind");
            b.setProperty ("cc", cc, nullptr);
            b.setProperty ("param", wp->paramID, nullptr);
            t.appendChild (b, nullptr);
        }
    }
    return t;
}

void MidiLearn::fromValueTree (const juce::ValueTree& tree, const juce::AudioProcessor& processor, bool applyScope)
{
    if (! tree.hasType ("MidiMap"))
        return;
    if (applyScope)
        setScope (tree.getProperty ("scope").toString() == "global" ? Scope::global : Scope::preset);

    clearAll();
    const auto& params = processor.getParameters();
    for (const auto& b : tree)
    {
        if (! b.hasType ("Bind"))
            continue;
        const int cc = static_cast<int> (b.getProperty ("cc", -1));
        const auto id = b.getProperty ("param").toString();
        for (int i = 0; i < params.size(); ++i)
            if (auto* wp = dynamic_cast<juce::AudioProcessorParameterWithID*> (params[i]); wp != nullptr && wp->paramID == id)
                bind (cc, i);
    }
}

bool MidiLearn::saveGlobalMap (const juce::File& file, const juce::AudioProcessor& processor) const
{
    if (auto xml = toValueTree (processor).createXml())
    {
        (void) file.getParentDirectory().createDirectory();
        return xml->writeTo (file);
    }
    return false;
}

bool MidiLearn::loadGlobalMap (const juce::File& file, const juce::AudioProcessor& processor)
{
    if (! file.existsAsFile())
        return false;
    if (auto xml = juce::XmlDocument::parse (file))
    {
        fromValueTree (juce::ValueTree::fromXml (*xml), processor, false);
        setScope (Scope::global);
        return true;
    }
    return false;
}

juce::File MidiLearn::defaultGlobalMapFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Monolith Audio/MONOLITH II/MidiLearn.xml");
}

} // namespace monolith::v2
