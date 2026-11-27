#include "PresetLibrary.h"
#include "V2Presets.h" // juce_add_binary_data(MonolithV2Presets): assets/v2_presets/*.mnlpreset
#include <algorithm>

namespace monolith
{

namespace preset_schema
{

namespace
{
    [[nodiscard]] juce::String exactFloat (float v)
    {
        return juce::String::formatted ("%.9g", static_cast<double> (v)); // round-trips every float exactly
    }

    [[nodiscard]] juce::StringArray tagsForCategory (const juce::String& category)
    {
        juce::StringArray t;
        t.addTokens (category.toLowerCase(), " &,/", "");
        t.removeEmptyStrings();
        t.removeString ("and");
        t.add ("factory");
        t.add ("v1");
        return t;
    }
}

std::unique_ptr<juce::XmlElement> toXml (const PresetRecord& r)
{
    auto xml = std::make_unique<juce::XmlElement> (kRootTag);
    xml->setAttribute ("version", kVersion);
    xml->setAttribute ("name", r.name);
    xml->setAttribute ("author", r.author);
    xml->setAttribute ("category", r.category);
    xml->setAttribute ("tags", r.tags.joinIntoString (","));
    xml->setAttribute ("description", r.description);
    xml->setAttribute ("favorite", r.favorite ? 1 : 0);

    auto* labels = xml->createNewChildElement ("MacroLabels");
    for (size_t i = 0; i < r.macroLabels.size(); ++i)
        labels->setAttribute ("m" + juce::String (static_cast<int> (i) + 1), r.macroLabels[i]);

    auto* ps = xml->createNewChildElement ("Params");
    for (const auto& [id, v] : r.params)
    {
        auto* p = ps->createNewChildElement ("Param");
        p->setAttribute ("id", id);
        p->setAttribute ("value", exactFloat (v));
    }
    if (r.control.isValid())
        if (auto c = r.control.createXml())
            xml->addChildElement (c.release());
    if (r.midiMap.isValid())
        if (auto m = r.midiMap.createXml())
            xml->addChildElement (m.release());
    return xml;
}

std::optional<PresetRecord> fromXml (const juce::XmlElement& xml)
{
    PresetRecord r;
    if (xml.hasTagName (kRootTag))
    {
        r.sourceVersion = xml.getIntAttribute ("version", 1);
        if (r.sourceVersion > kVersion)
            return std::nullopt; // written by a newer MONOLITH: refuse rather than guess
        r.name        = xml.getStringAttribute ("name", "Untitled");
        r.author      = xml.getStringAttribute ("author");
        r.category    = xml.getStringAttribute ("category", "User");
        r.description = xml.getStringAttribute ("description");
        r.favorite    = xml.getBoolAttribute ("favorite", false);
        r.tags.addTokens (xml.getStringAttribute ("tags"), ",", "");
        r.tags.trim();
        r.tags.removeEmptyStrings();
        if (auto* labels = xml.getChildByName ("MacroLabels"))
            for (size_t i = 0; i < r.macroLabels.size(); ++i)
                r.macroLabels[i] = labels->getStringAttribute ("m" + juce::String (static_cast<int> (i) + 1), r.macroLabels[i]);
        if (auto* ps = xml.getChildByName ("Params"))
            for (auto* p : ps->getChildWithTagNameIterator ("Param"))
                r.params.emplace_back (p->getStringAttribute ("id"), static_cast<float> (p->getDoubleAttribute ("value")));
        if (auto* c = xml.getChildByName ("V2Control"))
            r.control = juce::ValueTree::fromXml (*c);
        if (auto* m = xml.getChildByName ("MidiMap"))
            r.midiMap = juce::ValueTree::fromXml (*m);
        return r;
    }

    if (xml.hasTagName ("MONOLITH_STATE")) // legacy V1: raw APVTS state
    {
        r.sourceVersion = 1;
        r.name = "Legacy Session";
        r.category = "User";
        r.author = "Legacy";
        for (auto* p : xml.getChildWithTagNameIterator ("PARAM"))
            r.params.emplace_back (p->getStringAttribute ("id"), static_cast<float> (p->getDoubleAttribute ("value")));
        return r;
    }
    return std::nullopt;
}

PresetRecord fromFactory (const PresetManager& pm, int index)
{
    const auto& fp = pm.getPreset (index);
    PresetRecord r;
    r.name = fp.name;
    r.author = "Monolith Audio";
    r.category = fp.category;
    r.description = fp.description;
    r.tags = tagsForCategory (fp.category);
    r.factoryIndex = index;
    r.sourceVersion = 1;
    for (const auto& kv : pm.getResolvedParams (index))
        r.params.emplace_back (kv.id, kv.value);
    return r;
}

namespace
{
    /** Parses (fileName, xmlText) pairs, drops anything that is not a valid V2 record, sorts by file name. */
    std::vector<PresetRecord> parseFlagship (std::vector<std::pair<juce::String, juce::String>> sources)
    {
        std::sort (sources.begin(), sources.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });
        std::vector<PresetRecord> out;
        for (const auto& [fileName, text] : sources)
        {
            auto xml = juce::parseXML (text);
            if (xml == nullptr)
                continue;
            auto rec = fromXml (*xml);
            if (! rec || rec->sourceVersion != kVersion)
                continue;
            rec->flagshipIndex = static_cast<int> (out.size());
            rec->factoryIndex = -1;
            out.push_back (std::move (*rec));
        }
        return out;
    }
}

std::vector<PresetRecord> loadFlagshipPresets()
{
    std::vector<std::pair<juce::String, juce::String>> sources;
    for (int i = 0; i < V2Presets::namedResourceListSize; ++i)
    {
        int size = 0;
        if (const char* data = V2Presets::getNamedResource (V2Presets::namedResourceList[i], size); data != nullptr && size > 0)
            sources.emplace_back (V2Presets::originalFilenames[i], juce::String::fromUTF8 (data, size));
    }
    return parseFlagship (std::move (sources));
}

std::vector<PresetRecord> loadFlagshipPresets (const juce::File& directory)
{
    std::vector<std::pair<juce::String, juce::String>> sources;
    for (const auto& f : directory.findChildFiles (juce::File::findFiles, false, juce::String ("*") + kFileExtension))
        sources.emplace_back (f.getFileName(), f.loadFileAsString());
    return parseFlagship (std::move (sources));
}

} // namespace preset_schema

// ============================================================================

PresetLibrary::PresetLibrary (const PresetManager& factory)
    : factoryPresets (factory), flagship (preset_schema::loadFlagshipPresets())
{
    rescan(); // built-in presets only until a user directory is set
}

juce::File PresetLibrary::defaultUserDirectory()
{
   #if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
        .getChildFile ("Library/Audio/Presets/Monolith Audio/MONOLITH II");
   #elif JUCE_WINDOWS
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Monolith Audio/MONOLITH II/Presets");
   #else
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
        .getChildFile (".local/share/Monolith Audio/MONOLITH II/Presets");
   #endif
}

void PresetLibrary::setUserDirectory (const juce::File& dir)
{
    userDir = dir;
    loadFavourites();
    rescan();
}

void PresetLibrary::rescan()
{
    records.clear();
    records.insert (records.end(), flagship.begin(), flagship.end()); // MONOLITH II flagship first, then V1, then user
    for (int i = 0; i < factoryPresets.getNumPresets(); ++i)
        records.push_back (preset_schema::fromFactory (factoryPresets, i));

    if (userDir.isDirectory())
    {
        auto files = userDir.findChildFiles (juce::File::findFiles, true, juce::String ("*") + preset_schema::kFileExtension);
        files.sort();
        for (const auto& f : files)
        {
            if (auto xml = juce::XmlDocument::parse (f))
            {
                if (auto rec = preset_schema::fromXml (*xml))
                {
                    rec->file = f;
                    records.push_back (std::move (*rec));
                }
            }
        }
    }

    for (auto& r : records)
        r.favorite = r.favorite || favourites.count (r.key()) > 0;
}

const PresetRecord& PresetLibrary::get (int index) const
{
    return records[static_cast<size_t> (std::clamp (index, 0, size() - 1))];
}

int PresetLibrary::indexOfKey (const juce::String& key) const
{
    for (int i = 0; i < size(); ++i)
        if (records[static_cast<size_t> (i)].key() == key)
            return i;
    return -1;
}

juce::StringArray PresetLibrary::getCategories() const
{
    juce::StringArray cats;
    for (const auto& r : records)
        cats.addIfNotAlreadyThere (r.category);
    return cats;
}

std::vector<int> PresetLibrary::search (const juce::String& text, const juce::String& category, bool favouritesOnly) const
{
    juce::StringArray words;
    words.addTokens (text.toLowerCase(), " ", "\"");
    words.removeEmptyStrings();

    std::vector<int> out;
    for (int i = 0; i < size(); ++i)
    {
        const auto& r = records[static_cast<size_t> (i)];
        if (favouritesOnly && ! r.favorite) continue;
        if (category.isNotEmpty() && category != "All" && r.category != category) continue;
        const auto hay = (r.name + " " + r.category + " " + r.tags.joinIntoString (" ") + " " + r.author + " " + r.description).toLowerCase();
        bool all = true;
        for (const auto& w : words)
            all = all && hay.contains (w);
        if (all)
            out.push_back (i);
    }
    return out;
}

bool PresetLibrary::setFavourite (int index, bool fav)
{
    if (index < 0 || index >= size())
        return false;
    auto& r = records[static_cast<size_t> (index)];
    r.favorite = fav;
    if (fav) favourites.insert (r.key());
    else     favourites.erase (r.key());
    saveFavourites();
    return true;
}

juce::Result PresetLibrary::saveUserPreset (const PresetRecord& record)
{
    if (userDir == juce::File())
        return juce::Result::fail ("No user preset directory");
    const auto safeName = juce::File::createLegalFileName (record.name.isNotEmpty() ? record.name : "Untitled");
    const auto safeCat  = juce::File::createLegalFileName (record.category.isNotEmpty() ? record.category : "User");
    auto file = userDir.getChildFile (safeCat).getChildFile (safeName + preset_schema::kFileExtension);
    if (! file.getParentDirectory().createDirectory())
        return juce::Result::fail ("Cannot create " + file.getParentDirectory().getFullPathName());

    PresetRecord copy = record;
    copy.factoryIndex = -1;
    copy.flagshipIndex = -1;
    auto xml = preset_schema::toXml (copy);
    if (! xml->writeTo (file))
        return juce::Result::fail ("Cannot write " + file.getFullPathName());
    if (copy.favorite)
        favourites.insert ("user:" + file.getFileName());
    saveFavourites();
    rescan();
    return juce::Result::ok();
}

bool PresetLibrary::deleteUserPreset (int index)
{
    if (index < 0 || index >= size() || records[static_cast<size_t> (index)].isBuiltIn())
        return false;
    const auto f = records[static_cast<size_t> (index)].file;
    favourites.erase (records[static_cast<size_t> (index)].key());
    const bool ok = f.moveToTrash() || f.deleteFile();
    saveFavourites();
    rescan();
    return ok;
}

void PresetLibrary::loadFavourites()
{
    favourites.clear();
    const auto f = userDir.getChildFile ("favourites.xml");
    if (auto xml = juce::XmlDocument::parse (f))
        for (auto* e : xml->getChildWithTagNameIterator ("Fav"))
            favourites.insert (e->getStringAttribute ("key"));
}

void PresetLibrary::saveFavourites() const
{
    if (userDir == juce::File())
        return;
    juce::XmlElement xml ("Favourites");
    for (const auto& k : favourites)
        xml.createNewChildElement ("Fav")->setAttribute ("key", k);
    (void) userDir.createDirectory();
    (void) xml.writeTo (userDir.getChildFile ("favourites.xml"));
}

} // namespace monolith
