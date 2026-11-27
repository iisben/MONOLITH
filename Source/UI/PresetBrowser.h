#pragma once

#include "../PluginProcessor.h"
#include "CachedLayer.h"
#include "MeterComponents.h"
#include "MonolithLookAndFeel.h"
#include <functional>
#include <vector>
#include <juce_gui_basics/juce_gui_basics.h>

namespace monolith::ui
{

/**
    Preset Browser overlay (covers the body under the top bar): free-text search across name / category /
    tags / author / description, category column, favourites filter, result list with favourite stars,
    detail pane with description and tags. Double-click / Return / LOAD loads; Escape closes.
*/
class PresetBrowserOverlay final : public juce::Component,
                                   private juce::ListBoxModel
{
public:
    explicit PresetBrowserOverlay (MonolithAudioProcessor& p);

    void open();
    void close();
    std::function<void()> onClosed, onLoaded;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& k) override;

    /** Test / snapshot hooks. */
    void setSearchText (const juce::String& t);
    void selectCategory (const juce::String& c);

private:
    // Results list
    int getNumRows() override { return static_cast<int> (results.size()); }
    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent& e) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void returnKeyPressed (int row) override;
    void selectedRowsChanged (int row) override;

    void rebuild();
    void loadRow (int row);
    [[nodiscard]] const PresetRecord* selectedRecord() const;

    struct CategoryModel final : public juce::ListBoxModel
    {
        explicit CategoryModel (PresetBrowserOverlay& o) : owner (o) {}
        int getNumRows() override { return owner.categories.size(); }
        void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override;
        void selectedRowsChanged (int row) override;
        PresetBrowserOverlay& owner;
    };

    MonolithAudioProcessor& proc;
    juce::TextEditor search;
    GlyphButton searchIcon { "searchIcon", GlyphButton::Glyph::search, false }; // decorative, sits above the editor
    juce::ToggleButton favOnly { "FAVOURITES" };
    GlyphButton closeBtn { "close", GlyphButton::Glyph::close };
    CategoryModel catModel { *this };
    juce::ListBox categoryList { "categories", &catModel };
    juce::ListBox resultList { "results", this };
    juce::TextButton loadBtn { "LOAD" }, deleteBtn { "DELETE" };
    juce::StringArray categories;
    juce::String category;
    std::vector<int> results;
    CachedLayer chromeLayer;
};

/** Save-preset dialog (modal scrim). */
class SavePresetOverlay final : public juce::Component
{
public:
    explicit SavePresetOverlay (MonolithAudioProcessor& p);
    void open();
    void close();
    std::function<void()> onClosed;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& k) override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    [[nodiscard]] juce::Rectangle<int> panelBounds() const;
    void save();

    MonolithAudioProcessor& proc;
    juce::TextEditor nameEd, authorEd, tagsEd, descEd;
    juce::ComboBox categoryBox;
    juce::TextButton cancelBtn { "CANCEL" }, saveBtn { "SAVE PRESET" };
    juce::String status;
    bool statusIsError = false;
    CachedLayer chromeLayer;
};

} // namespace monolith::ui
