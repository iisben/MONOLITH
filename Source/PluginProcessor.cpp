#include "PluginProcessor.h"
#if ! MONOLITH_HEADLESS_QA
 #include "PluginEditor.h"
#endif

namespace monolith
{

MonolithAudioProcessor::MonolithAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MONOLITH_STATE", params::createParameterLayout())
{
    textureLayer.bindParameters (apvts);
    presetManager.initializeAB (apvts);
    startTimerHz (20); // PDC follows OS mode / drive engagement (0 when bypassed) via atomic parameter reads
}

MonolithAudioProcessor::~MonolithAudioProcessor()
{
    stopTimer();
}

void MonolithAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    telemetry.reset();
    engine.prepare (sampleRate, std::max (64, samplesPerBlock), apvts);
    textureLayer.prepare (sampleRate, std::max (64, samplesPerBlock));
    setLatencySamples (engine.getLatencySamples()); // texture layer adds 0 (aligned to engine PDC)
}

void MonolithAudioProcessor::releaseResources()
{
    engine.reset();
    textureLayer.reset();
    telemetry.reset();
}

bool MonolithAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& outSet = layouts.getMainOutputChannelSet();
    return outSet == juce::AudioChannelSet::stereo()
        || outSet == juce::AudioChannelSet::mono();
}

void MonolithAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midiMessages)
{
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm())
                lastKnownBpm.store (*bpm, std::memory_order_relaxed);
        }
    }

    engine.processBlock (buffer,
                         midiMessages,
                         lastKnownBpm.load (std::memory_order_relaxed),
                         telemetry);

    // V2 post-core bus: additive texture layer (returns untouched while bypassed at 0 % mix)
    textureLayer.process (buffer, midiMessages, engine.getLatencySamples());
}

juce::AudioProcessorEditor* MonolithAudioProcessor::createEditor()
{
   #if MONOLITH_HEADLESS_QA
    return nullptr; // Headless QA binary: no editor / GUI linkage
   #else
    return new MonolithAudioProcessorEditor (*this);
   #endif
}

int MonolithAudioProcessor::getNumPrograms()
{
    return presetManager.getNumPresets();
}

int MonolithAudioProcessor::getCurrentProgram()
{
    return presetManager.getCurrentPresetIndex();
}

void MonolithAudioProcessor::setCurrentProgram (int index)
{
    applyFactoryPreset (index);
}

const juce::String MonolithAudioProcessor::getProgramName (int index)
{
    return presetManager.getPreset (index).name;
}

void MonolithAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    (void) index;
    (void) newName;
}

void MonolithAudioProcessor::applyFactoryPreset (int index)
{
    presetManager.applyPreset (index, apvts);
}

void MonolithAudioProcessor::toggleABCompare()
{
    presetManager.toggleAB (apvts);
}

void MonolithAudioProcessor::copyActiveToInactiveAB()
{
    presetManager.copyActiveToInactive (apvts);
}

void MonolithAudioProcessor::timerCallback()
{
    // Latency depends on OS mode and on whether the drive stage is engaged (0 when bypassed)
    const int latency = engine.getLatencySamples();
    if (latency != getLatencySamples())
        setLatencySamples (latency);
}

void MonolithAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml != nullptr)
    {
        xml->setAttribute (params::stateVersionAttr, params::stateVersion);
        presetManager.saveStateToXml (*xml);
        copyXmlToBinary (*xml, destData);
    }
}

void MonolithAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
    {
        // V1 sessions carry no version attribute and no v2_* children: APVTS restores every missing
        // parameter to its default, so V2 layers come back bypassed (lossless V1 -> V2 migration).
        presetManager.loadStateFromXml (*xmlState);
        auto tree = juce::ValueTree::fromXml (*xmlState);
        tree.removeProperty (params::stateVersionAttr, nullptr);
        apvts.replaceState (tree);
        setLatencySamples (engine.getLatencySamples());
    }
}

} // namespace monolith

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new monolith::MonolithAudioProcessor();
}
