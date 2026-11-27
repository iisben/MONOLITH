#include "PluginProcessor.h"
#if ! MONOLITH_HEADLESS_QA
 #include "PluginEditor.h"
#endif

namespace monolith
{

namespace
{
    /** Session-state children that belong to MONOLITH, not to the APVTS parameter tree. */
    constexpr std::array<const char*, 7> kNonParamChildren { "SlotA", "SlotB", "V2Control", "MidiMap", "V2AB", "V2PresetMeta", "V2UI" };

    void stripNonParamChildren (juce::ValueTree& tree)
    {
        for (const auto* name : kNonParamChildren)
            for (auto c = tree.getChildWithName (name); c.isValid(); c = tree.getChildWithName (name))
                tree.removeChild (c, nullptr);
    }

    void writeMeta (juce::XmlElement& e, const PresetRecord& r)
    {
        e.setAttribute ("name", r.name);
        e.setAttribute ("author", r.author);
        e.setAttribute ("category", r.category);
        e.setAttribute ("tags", r.tags.joinIntoString (","));
        e.setAttribute ("description", r.description);
        e.setAttribute ("factoryIndex", r.factoryIndex);
        e.setAttribute ("flagshipIndex", r.flagshipIndex);
    }

    void readMeta (const juce::XmlElement& e, PresetRecord& r)
    {
        r.name = e.getStringAttribute ("name", r.name);
        r.author = e.getStringAttribute ("author", r.author);
        r.category = e.getStringAttribute ("category", r.category);
        r.tags.clear();
        r.tags.addTokens (e.getStringAttribute ("tags"), ",", "");
        r.tags.removeEmptyStrings();
        r.description = e.getStringAttribute ("description", r.description);
        r.factoryIndex = e.getIntAttribute ("factoryIndex", r.factoryIndex);
        r.flagshipIndex = e.getIntAttribute ("flagshipIndex", -1);
    }
}

MonolithAudioProcessor::MonolithAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MONOLITH_STATE", params::createParameterLayout())
{
    controlPlane.bind (apvts);
    expression.bindParameters (apvts);
    engine.setExpressionHook (&expression);
    textureLayer.bindParameters (apvts, [this] (const char* id) { return resolveShadow (id); });
    presetManager.initializeAB (apvts);
    currentMeta = preset_schema::fromFactory (presetManager, presetManager.getCurrentPresetIndex());
    currentMeta.params.clear();

   #if ! MONOLITH_HEADLESS_QA
    presetLibrary.setUserDirectory (PresetLibrary::defaultUserDirectory());
    (void) midiLearn.loadGlobalMap (v2::MidiLearn::defaultGlobalMapFile(), *this);
   #endif

    undoHistory.reset();
    startTimerHz (50); // PDC poll, MIDI-Learn drain and undo commit (message thread, atomic reads only)
}

MonolithAudioProcessor::~MonolithAudioProcessor()
{
    stopTimer();
}

void MonolithAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    telemetry.reset();
    controlPlane.prepare (sampleRate); // shadows valid before the engine binds to them / PDC is queried
    expression.reset();
    engine.prepare (sampleRate, std::max (64, samplesPerBlock), apvts,
                    [this] (const char* id) { return resolveShadow (id); });
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

    // V2 control plane (pre-engine): velocity curve in place, MIDI-Learn capture, macro / XY / expressive
    // routing into the shadow parameters the engine and texture layer read. Identity while idle.
    expression.beginBlock();
    controlPlane.process (midiMessages, buffer.getNumSamples(), &midiLearn);

    engine.processBlock (buffer,
                         midiMessages,
                         lastKnownBpm.load (std::memory_order_relaxed),
                         telemetry);

    // V2 post-core bus: additive texture layer (returns untouched while bypassed at 0 % mix)
    textureLayer.process (buffer, midiMessages, engine.getLatencySamples());
    expression.publishTelemetry();
    outputTap.push (buffer); // V2 Stage 4: observer only (one relaxed load while no editor is open)
}

juce::AudioProcessorEditor* MonolithAudioProcessor::createEditor()
{
   #if MONOLITH_HEADLESS_QA
    return nullptr; // Headless QA binary: no editor / GUI linkage
   #else
    return new MonolithAudioProcessorEditor (*this);
   #endif
}

// Host programs: 0-29 are the V1 factory presets (indices unchanged for existing sessions / program changes),
// 30-49 the MONOLITH II flagship presets.
int MonolithAudioProcessor::getNumPrograms()
{
    return presetManager.getNumPresets() + presetLibrary.getNumFlagship();
}

int MonolithAudioProcessor::getCurrentProgram()
{
    return currentMeta.isFlagship() ? presetManager.getNumPresets() + currentMeta.flagshipIndex
                                    : presetManager.getCurrentPresetIndex();
}

void MonolithAudioProcessor::setCurrentProgram (int index)
{
    if (const auto* fs = presetLibrary.getFlagship (index - presetManager.getNumPresets()))
        loadPresetRecord (*fs);
    else
        applyFactoryPreset (index);
}

const juce::String MonolithAudioProcessor::getProgramName (int index)
{
    if (const auto* fs = presetLibrary.getFlagship (index - presetManager.getNumPresets()))
        return fs->name;
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
    controlPlane.resetToFactory ({ "COLOSSUS", "SPECTER", "ROT", "ABYSS" }); // V1 routing: legacy curves only
    currentMeta = preset_schema::fromFactory (presetManager, presetManager.getCurrentPresetIndex());
    currentMeta.params.clear();
    const int libIdx = presetLibrary.indexOfKey (currentMeta.key());
    currentMeta.favorite = libIdx >= 0 && presetLibrary.get (libIdx).favorite;
    undoHistory.commit();
}

void MonolithAudioProcessor::loadPresetRecord (const PresetRecord& record)
{
    if (record.isFactory() && record.factoryIndex < presetManager.getNumPresets())
    {
        applyFactoryPreset (record.factoryIndex);
        return;
    }

    // Deterministic load: every non-global parameter returns to its default, then the record's values apply.
    // Unknown IDs (from future versions) are ignored; IDs missing from older records keep V2 defaults.
    for (auto* p : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (rp == nullptr || params::isGlobalPerformanceParam (rp->paramID))
            continue;
        float target = rp->getDefaultValue();
        for (const auto& [id, v] : record.params)
            if (id == rp->paramID)
                target = std::clamp (rp->convertTo0to1 (v), 0.0f, 1.0f);
        rp->setValueNotifyingHost (target);
    }

    controlPlane.fromValueTree (record.control);
    for (int m = 0; m < v2::kNumMacros; ++m)
        controlPlane.setMacroLabel (m, record.macroLabels[static_cast<size_t> (m)]);
    if (record.midiMap.isValid() && midiLearn.getScope() == v2::MidiLearn::Scope::preset)
        midiLearn.fromValueTree (record.midiMap, *this, false);

    currentMeta = record;
    currentMeta.params.clear();
    undoHistory.commit();
}

void MonolithAudioProcessor::loadInitPatch()
{
    PresetRecord init;
    init.name = "Init";
    init.category = "Init";
    init.author = "MONOLITH II";
    init.description = "Neutral starting point: every parameter at its default, V1 macro curves.";
    loadPresetRecord (init); // empty param list -> defaults; invalid control tree -> factory routing
}

PresetRecord MonolithAudioProcessor::captureCurrentPreset (const juce::String& name, const juce::String& author,
                                                           const juce::String& category, const juce::StringArray& tags,
                                                           const juce::String& description) const
{
    PresetRecord r;
    r.name = name;
    r.author = author;
    r.category = category;
    r.tags = tags;
    r.description = description;
    r.macroLabels = controlPlane.getMacroLabels();
    r.control = controlPlane.toValueTree();
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p); rp != nullptr && ! params::isGlobalPerformanceParam (rp->paramID))
            r.params.emplace_back (rp->paramID, rp->convertFrom0to1 (rp->getValue()));
    if (midiLearn.getScope() == v2::MidiLearn::Scope::preset && midiLearn.getNumBindings() > 0)
        r.midiMap = midiLearn.toValueTree (*this);
    return r;
}

juce::Result MonolithAudioProcessor::saveUserPreset (const PresetRecord& record)
{
    auto res = presetLibrary.saveUserPreset (record);
    if (res.wasOk())
    {
        currentMeta = record;
        currentMeta.params.clear();
    }
    return res;
}

void MonolithAudioProcessor::toggleABCompare()
{
    undoHistory.commit();
    controlPlane.requestTransition (kStateTransitionMs); // shadows glide: click-free swap
    const bool wasB = presetManager.isSlotBActive();
    auto current = controlPlane.toValueTree();
    presetManager.toggleAB (apvts);
    (wasB ? controlSlotB : controlSlotA) = current;
    const auto& next = wasB ? controlSlotA : controlSlotB;
    if (next.isValid())
        controlPlane.fromValueTree (next);
    undoHistory.commit();
}

void MonolithAudioProcessor::copyActiveToInactiveAB()
{
    presetManager.copyActiveToInactive (apvts);
    (presetManager.isSlotBActive() ? controlSlotA : controlSlotB) = controlPlane.toValueTree();
}

v2::StateSnapshot MonolithAudioProcessor::captureSnapshot() const
{
    v2::StateSnapshot s;
    const auto& ps = getParameters();
    s.values.reserve (static_cast<size_t> (ps.size()));
    for (auto* p : ps)
        s.values.push_back (p->getValue());
    s.control = controlPlane.toValueTree();
    return s;
}

void MonolithAudioProcessor::restoreSnapshot (const v2::StateSnapshot& s)
{
    controlPlane.requestTransition (kStateTransitionMs);
    const auto& ps = getParameters();
    for (int i = 0; i < ps.size() && i < static_cast<int> (s.values.size()); ++i)
    {
        auto* p = ps[i];
        if (! std::equal_to<float>() (p->getValue(), s.values[static_cast<size_t> (i)])) // exact: skip untouched params
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (s.values[static_cast<size_t> (i)]);
            p->endChangeGesture();
        }
    }
    controlPlane.fromValueTree (s.control);
}

bool MonolithAudioProcessor::undo() { return undoHistory.undo(); }
bool MonolithAudioProcessor::redo() { return undoHistory.redo(); }

void MonolithAudioProcessor::runMessageThreadTasks (double nowMs)
{
    // Latency depends on OS mode and on whether the drive stage is engaged (0 when bypassed)
    const int latency = engine.getLatencySamples();
    if (latency != getLatencySamples())
        setLatencySamples (latency);

    (void) midiLearn.drain (*this);
    undoHistory.poll (nowMs);
}

bool MonolithAudioProcessor::saveGlobalMidiMap() const
{
   #if MONOLITH_HEADLESS_QA
    return false;
   #else
    return midiLearn.saveGlobalMap (v2::MidiLearn::defaultGlobalMapFile(), *this);
   #endif
}

void MonolithAudioProcessor::timerCallback()
{
    runMessageThreadTasks (juce::Time::getMillisecondCounterHiRes());
}

void MonolithAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml != nullptr)
    {
        xml->setAttribute (params::stateVersionAttr, params::stateVersion);
        presetManager.saveStateToXml (*xml);

        // V2 Stage 3 session children (ignored by V1 builds; stripped before APVTS replaceState)
        if (auto c = controlPlane.toValueTree().createXml())
            xml->addChildElement (c.release());
        if (auto m = midiLearn.toValueTree (*this).createXml())
            xml->addChildElement (m.release());
        auto* ab = xml->createNewChildElement ("V2AB");
        if (controlSlotA.isValid()) if (auto a = controlSlotA.createXml()) ab->createNewChildElement ("A")->addChildElement (a.release());
        if (controlSlotB.isValid()) if (auto b = controlSlotB.createXml()) ab->createNewChildElement ("B")->addChildElement (b.release());
        writeMeta (*xml->createNewChildElement ("V2PresetMeta"), currentMeta);
        xml->createNewChildElement ("V2UI")->setAttribute ("scale", static_cast<double> (editorScale));

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

        // V2 routing / MIDI map / A-B routing / preset metadata. Absent in V1 sessions -> factory V1 routing.
        controlPlane.fromValueTree (xmlState->getChildByName ("V2Control") != nullptr
                                        ? juce::ValueTree::fromXml (*xmlState->getChildByName ("V2Control"))
                                        : juce::ValueTree());
        if (auto* m = xmlState->getChildByName ("MidiMap"))
        {
            const auto map = juce::ValueTree::fromXml (*m);
           #if ! MONOLITH_HEADLESS_QA
            if (map.getProperty ("scope").toString() == "global"
                && midiLearn.loadGlobalMap (v2::MidiLearn::defaultGlobalMapFile(), *this))
            {
                // global scope: the user-wide map wins over the session copy
            }
            else
           #endif
                midiLearn.fromValueTree (map, *this);
        }
        controlSlotA = controlSlotB = juce::ValueTree();
        if (auto* ab = xmlState->getChildByName ("V2AB"))
        {
            if (auto* a = ab->getChildByName ("A")) if (auto* inner = a->getFirstChildElement()) controlSlotA = juce::ValueTree::fromXml (*inner);
            if (auto* b = ab->getChildByName ("B")) if (auto* inner = b->getFirstChildElement()) controlSlotB = juce::ValueTree::fromXml (*inner);
        }
        currentMeta = preset_schema::fromFactory (presetManager, presetManager.getCurrentPresetIndex());
        currentMeta.params.clear();
        if (auto* meta = xmlState->getChildByName ("V2PresetMeta"))
            readMeta (*meta, currentMeta);
        if (auto* ui = xmlState->getChildByName ("V2UI"))
            setEditorScale (static_cast<float> (ui->getDoubleAttribute ("scale", 1.0)));

        auto tree = juce::ValueTree::fromXml (*xmlState);
        tree.removeProperty (params::stateVersionAttr, nullptr);
        stripNonParamChildren (tree); // A/B slots + V2 children never nest into the parameter tree
        apvts.replaceState (tree);
        setLatencySamples (engine.getLatencySamples());
        undoHistory.reset();
    }
}

} // namespace monolith

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new monolith::MonolithAudioProcessor();
}
