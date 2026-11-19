#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace monolith
{

MonolithAudioProcessor::MonolithAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MONOLITH_STATE", params::createParameterLayout())
{
    apvts.addParameterListener (params::oversampleHq, this);
    presetManager.initializeAB (apvts);
}

MonolithAudioProcessor::~MonolithAudioProcessor()
{
    cancelPendingUpdate();
    apvts.removeParameterListener (params::oversampleHq, this);
}

void MonolithAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    telemetry.reset();
    engine.prepare (sampleRate, std::max (64, samplesPerBlock), apvts);
    setLatencySamples (engine.getLatencySamples());
}

void MonolithAudioProcessor::releaseResources()
{
    engine.reset();
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
}

juce::AudioProcessorEditor* MonolithAudioProcessor::createEditor()
{
    return new MonolithAudioProcessorEditor (*this);
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

void MonolithAudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    (void) newValue;
    if (parameterID == params::oversampleHq)
        triggerAsyncUpdate();
}

void MonolithAudioProcessor::handleAsyncUpdate()
{
    setLatencySamples (engine.getLatencySamples());
}

void MonolithAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml != nullptr)
    {
        presetManager.saveStateToXml (*xml);
        copyXmlToBinary (*xml, destData);
    }
}

void MonolithAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
    {
        presetManager.loadStateFromXml (*xmlState);
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
        setLatencySamples (engine.getLatencySamples());
    }
}

} // namespace monolith

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new monolith::MonolithAudioProcessor();
}
