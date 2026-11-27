#include "PresetBrowser.h"

namespace monolith::ui
{

namespace
{
    constexpr int kRowH = 32;
    const char* const kAll = "All Presets";
    const char* const kFav = "Favourites";

    void styleEditor (juce::TextEditor& e, const juce::String& placeholder, bool multi = false)
    {
        e.setMultiLine (multi, true);
        e.setReturnKeyStartsNewLine (multi);
        e.setFont (uiFont (13.0f, Weight::medium));
        e.setIndents (10, multi ? 8 : 8);
        e.setTextToShowWhenEmpty (placeholder, palette::ash);
        e.setJustification (multi ? juce::Justification::topLeft : juce::Justification::centredLeft);
    }
}

// ============================================================================
PresetBrowserOverlay::PresetBrowserOverlay (MonolithAudioProcessor& p) : proc (p)
{
    setWantsKeyboardFocus (true);
    styleEditor (search, "Search name, tag, category, author ...");
    search.setIndents (32, 8);
    search.onTextChange = [this] { rebuild(); };
    search.onReturnKey = [this] { loadRow (resultList.getSelectedRow() >= 0 ? resultList.getSelectedRow() : 0); };
    search.onEscapeKey = [this] { close(); };
    addAndMakeVisible (search);
    searchIcon.setInterceptsMouseClicks (false, false);
    searchIcon.setWantsKeyboardFocus (false);
    addAndMakeVisible (searchIcon); // added after the editor so it paints above it

    favOnly.setTooltip ("Show favourites only");
    favOnly.onClick = [this] { rebuild(); };
    addAndMakeVisible (favOnly);

    closeBtn.setTooltip ("Close (Esc)");
    closeBtn.onClick = [this] { close(); };
    addAndMakeVisible (closeBtn);

    for (auto* lb : { &categoryList, &resultList })
    {
        lb->setRowHeight (kRowH);
        lb->setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        lb->setOutlineThickness (0);
        addAndMakeVisible (lb);
    }

    loadBtn.getProperties().set (MonolithLookAndFeel::accentPropertyId, "amber");
    loadBtn.setToggleState (true, juce::dontSendNotification);
    loadBtn.onClick = [this] { loadRow (resultList.getSelectedRow()); };
    deleteBtn.getProperties().set (MonolithLookAndFeel::accentPropertyId, "crimson");
    deleteBtn.onClick = [this] {
        if (const auto* r = selectedRecord(); r != nullptr && ! r->isFactory())
        {
            const int idx = proc.getPresetLibrary().indexOfKey (r->key());
            if (idx >= 0 && proc.getPresetLibrary().deleteUserPreset (idx))
                rebuild();
        }
    };
    addAndMakeVisible (loadBtn);
    addAndMakeVisible (deleteBtn);
    setVisible (false);
}

void PresetBrowserOverlay::open()
{
    proc.getPresetLibrary().rescan();
    categories.clear();
    categories.add (kAll);
    categories.add (kFav);
    categories.addArray (proc.getPresetLibrary().getCategories());
    categoryList.updateContent();
    if (category.isEmpty())
        category = kAll;
    categoryList.selectRow (std::max (0, categories.indexOf (category)), true, true);
    rebuild();

    // Select the current preset
    const int cur = proc.getPresetLibrary().indexOfKey (proc.getCurrentPresetMeta().key());
    for (size_t i = 0; i < results.size(); ++i)
        if (results[i] == cur)
        {
            resultList.selectRow (static_cast<int> (i));
            break;
        }
    setVisible (true);
    toFront (true);
    search.grabKeyboardFocus();
}

void PresetBrowserOverlay::close()
{
    setVisible (false);
    if (onClosed)
        onClosed();
}

void PresetBrowserOverlay::setSearchText (const juce::String& t)
{
    search.setText (t, true);
}

void PresetBrowserOverlay::selectCategory (const juce::String& c)
{
    const int idx = categories.indexOf (c);
    if (idx >= 0)
        categoryList.selectRow (idx);
}

void PresetBrowserOverlay::rebuild()
{
    auto& lib = proc.getPresetLibrary();
    const bool favs = favOnly.getToggleState() || category == kFav;
    const juce::String cat = (category == kAll || category == kFav) ? juce::String() : category;
    results = lib.search (search.getText().trim(), cat, favs);
    resultList.updateContent();
    if (resultList.getSelectedRow() >= static_cast<int> (results.size()))
        resultList.deselectAllRows();
    selectedRowsChanged (resultList.getSelectedRow());
    repaint();
}

const PresetRecord* PresetBrowserOverlay::selectedRecord() const
{
    const int row = resultList.getSelectedRow();
    if (row < 0 || row >= static_cast<int> (results.size()))
        return nullptr;
    return &proc.getPresetLibrary().get (results[static_cast<size_t> (row)]);
}

void PresetBrowserOverlay::loadRow (int row)
{
    if (row < 0 || row >= static_cast<int> (results.size()))
        return;
    proc.loadPresetRecord (proc.getPresetLibrary().get (results[static_cast<size_t> (row)]));
    if (onLoaded)
        onLoaded();
    close();
}

void PresetBrowserOverlay::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (row < 0 || row >= static_cast<int> (results.size()))
        return;
    const auto& r = proc.getPresetLibrary().get (results[static_cast<size_t> (row)]);
    const bool isCurrent = r.key() == proc.getCurrentPresetMeta().key();
    const auto b = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height));
    if (selected)
    {
        g.setColour (palette::panelRaised);
        g.fillRoundedRectangle (b.reduced (0.0f, 1.0f), 3.0f);
        g.setColour (palette::amber);
        g.fillRect (0.0f, 6.0f, 2.0f, static_cast<float> (height) - 12.0f);
    }
    else if (row % 2 == 1)
    {
        g.setColour (juce::Colours::white.withAlpha (0.012f));
        g.fillRect (b);
    }
    drawGlyph (g, GlyphButton::Glyph::star, { 4.0f, 4.0f, 24.0f, 24.0f }, r.favorite ? palette::amber : palette::graphite, r.favorite);
    g.setColour (isCurrent ? palette::amber : palette::ivory);
    g.setFont (uiFont (13.0f, isCurrent || selected ? Weight::demibold : Weight::medium));
    g.drawText (r.name, juce::Rectangle<float> (36.0f, 0.0f, 236.0f, b.getHeight()), juce::Justification::centredLeft, true);
    g.setColour (palette::stone);
    g.setFont (uiFont (12.0f, Weight::regular));
    g.drawText (r.category, juce::Rectangle<float> (280.0f, 0.0f, 168.0f, b.getHeight()), juce::Justification::centredLeft, true);
    g.setColour (palette::ash.brighter (0.2f));
    g.setFont (capsFont (9.0f, Weight::medium));
    g.drawText (r.tags.joinIntoString (juce::String::fromUTF8 ("  \xc2\xb7  ")).toUpperCase(),
                juce::Rectangle<float> (456.0f, 0.0f, b.getWidth() - 464.0f, b.getHeight()), juce::Justification::centredLeft, true);
}

void PresetBrowserOverlay::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (e.x < 32 && row >= 0 && row < static_cast<int> (results.size()))
    {
        auto& lib = proc.getPresetLibrary();
        const int idx = results[static_cast<size_t> (row)];
        lib.setFavourite (idx, ! lib.get (idx).favorite);
        if (favOnly.getToggleState() || category == kFav)
            rebuild();
        resultList.repaintRow (row);
    }
}

void PresetBrowserOverlay::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    loadRow (row);
}

void PresetBrowserOverlay::returnKeyPressed (int row)
{
    loadRow (row);
}

void PresetBrowserOverlay::selectedRowsChanged (int)
{
    const auto* r = selectedRecord();
    loadBtn.setEnabled (r != nullptr);
    deleteBtn.setVisible (r != nullptr && ! r->isFactory());
    repaint (864, 72, 288, 560);
}

void PresetBrowserOverlay::CategoryModel::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (row < 0 || row >= owner.categories.size())
        return;
    const auto b = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height));
    if (selected)
    {
        g.setColour (palette::panelRaised);
        g.fillRoundedRectangle (b.reduced (0.0f, 1.0f), 3.0f);
        g.setColour (palette::amber);
        g.fillRect (0.0f, 8.0f, 2.0f, static_cast<float> (height) - 16.0f);
    }
    g.setColour (selected ? palette::ivory : palette::stone);
    g.setFont (uiFont (13.0f, selected ? Weight::demibold : Weight::medium));
    g.drawText (owner.categories[row], b.withTrimmedLeft (14.0f).withTrimmedRight (36.0f), juce::Justification::centredLeft, true);

    // Count badge
    const auto& cat = owner.categories[row];
    auto& lib = owner.proc.getPresetLibrary();
    const int n = static_cast<int> (cat == kAll ? lib.search ({}).size()
                                                : (cat == kFav ? lib.search ({}, {}, true).size() : lib.search ({}, cat).size()));
    g.setColour (palette::ash);
    g.setFont (monoFont (10.0f));
    g.drawText (juce::String (n), b.withTrimmedRight (8.0f), juce::Justification::centredRight, false);
}

void PresetBrowserOverlay::CategoryModel::selectedRowsChanged (int row)
{
    if (row >= 0 && row < owner.categories.size())
    {
        owner.category = owner.categories[row];
        owner.rebuild();
    }
}

bool PresetBrowserOverlay::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey)
    {
        close();
        return true;
    }
    return false;
}

void PresetBrowserOverlay::resized()
{
    chromeLayer.invalidate();
    search.setBounds (232, 12, 400, 32);
    searchIcon.setBounds (236, 16, 24, 24);
    favOnly.setBounds (648, 14, 128, 28);
    closeBtn.setBounds (getWidth() - 44, 14, 28, 28);
    categoryList.setBounds (16, 72, 200, getHeight() - 88);
    resultList.setBounds (232, 104, 616, ((getHeight() - 120) / kRowH) * kRowH); // whole rows only, no clipped half-row
    loadBtn.setBounds (864, getHeight() - 56, 136, 40);
    deleteBtn.setBounds (1016, getHeight() - 56, 136, 40);
}

void PresetBrowserOverlay::paint (juce::Graphics& g)
{
    chromeLayer.draw (g, getLocalBounds(), [] (juce::Graphics& cg, juce::Rectangle<float> r) {
        cg.setColour (juce::Colours::black.withAlpha (0.6f));
        cg.fillRoundedRectangle (r.translated (0.0f, 2.0f), 4.0f);
        chrome::drawPlate (cg, r, {}, false);
        chrome::drawCaption (cg, { 16.0f, 0.0f, 200.0f, 56.0f }, "PRESET BROWSER");
        chrome::drawDividerH (cg, 56.0f, 1.0f, r.getRight() - 1.0f);
        chrome::drawDividerV (cg, 224.0f, 64.0f, r.getBottom() - 16.0f);
        chrome::drawDividerV (cg, 856.0f, 64.0f, r.getBottom() - 16.0f);
        cg.setColour (palette::ash);
        cg.setFont (capsFont (9.0f));
        cg.drawText ("NAME", juce::Rectangle<float> (268.0f, 72.0f, 200.0f, 28.0f), juce::Justification::centredLeft, false);
        cg.drawText ("CATEGORY", juce::Rectangle<float> (512.0f, 72.0f, 160.0f, 28.0f), juce::Justification::centredLeft, false);
        cg.drawText ("TAGS", juce::Rectangle<float> (688.0f, 72.0f, 160.0f, 28.0f), juce::Justification::centredLeft, false);
        chrome::drawDividerH (cg, 100.0f, 232.0f, 848.0f);
        cg.setColour (palette::ash);
        cg.drawText ("DETAILS", juce::Rectangle<float> (872.0f, 72.0f, 160.0f, 28.0f), juce::Justification::centredLeft, false);
    });

    g.setColour (palette::stone);
    g.setFont (capsFont (9.5f));
    g.drawText (juce::String (static_cast<int> (results.size())) + (results.size() == 1 ? " PRESET" : " PRESETS"),
                juce::Rectangle<float> (792.0f, 12.0f, 160.0f, 32.0f), juce::Justification::centredLeft, false);

    // Detail pane
    const auto* r = selectedRecord();
    auto d = juce::Rectangle<float> (872.0f, 104.0f, 272.0f, static_cast<float> (getHeight()) - 176.0f);
    if (r == nullptr)
    {
        g.setColour (palette::ash);
        g.setFont (uiFont (13.0f, Weight::medium));
        g.drawText ("Select a preset", d.withHeight (24.0f), juce::Justification::centredLeft, false);
        return;
    }
    g.setColour (palette::ivory);
    const auto nameFont = uiFont (20.0f, Weight::demibold);
    g.setFont (nameFont);
    const bool oneLine = juce::GlyphArrangement::getStringWidth (nameFont, r->name) <= d.getWidth();
    g.drawFittedText (r->name, d.removeFromTop (oneLine ? 30.0f : 54.0f).toNearestInt(), juce::Justification::topLeft, 2, 1.0f);
    g.setColour (palette::amber);
    g.setFont (capsFont (9.5f));
    g.drawText (r->category.toUpperCase() + (r->isFactory() ? juce::String::fromUTF8 ("  \xc2\xb7  FACTORY") : juce::String::fromUTF8 ("  \xc2\xb7  USER")),
                d.removeFromTop (16.0f), juce::Justification::centredLeft, true);
    if (r->author.isNotEmpty())
    {
        g.setColour (palette::stone);
        g.setFont (uiFont (12.0f, Weight::medium));
        g.drawText ("by " + r->author, d.removeFromTop (20.0f), juce::Justification::centredLeft, true);
    }
    d.removeFromTop (12.0f);

    // Tag chips
    float x = d.getX(), y = d.getY();
    g.setFont (capsFont (8.5f));
    for (const auto& t : r->tags)
    {
        const float w = juce::GlyphArrangement::getStringWidth (capsFont (8.5f), t.toUpperCase()) + 16.0f;
        if (x + w > d.getRight()) { x = d.getX(); y += 24.0f; }
        const auto chip = juce::Rectangle<float> (x, y, w, 18.0f);
        g.setColour (palette::panelRaised);
        g.fillRoundedRectangle (chip, 9.0f);
        g.setColour (palette::hairlineHi);
        g.drawRoundedRectangle (chip.reduced (0.5f), 9.0f, 1.0f);
        g.setColour (palette::stone);
        g.drawText (t.toUpperCase(), chip, juce::Justification::centred, false);
        x += w + 6.0f;
    }
    if (! r->tags.isEmpty())
        d.setTop (y + 32.0f);

    g.setColour (palette::ivory.withAlpha (0.82f));
    g.setFont (uiFont (13.0f, Weight::regular));
    g.drawFittedText (r->description, d.toNearestInt(), juce::Justification::topLeft, 12, 1.0f);
}

// ============================================================================
SavePresetOverlay::SavePresetOverlay (MonolithAudioProcessor& p) : proc (p)
{
    setWantsKeyboardFocus (true);
    styleEditor (nameEd, "Preset name");
    styleEditor (authorEd, "Author");
    styleEditor (tagsEd, "Tags, comma separated");
    styleEditor (descEd, "Description", true);
    nameEd.onReturnKey = [this] { save(); };
    for (auto* e : { &nameEd, &authorEd, &tagsEd, &descEd })
    {
        e->onEscapeKey = [this] { close(); };
        addAndMakeVisible (e);
    }
    categoryBox.setEditableText (true);
    categoryBox.setTooltip ("Pick a category or type a new one");
    addAndMakeVisible (categoryBox);
    cancelBtn.onClick = [this] { close(); };
    saveBtn.getProperties().set (MonolithLookAndFeel::accentPropertyId, "amber");
    saveBtn.setToggleState (true, juce::dontSendNotification);
    saveBtn.onClick = [this] { save(); };
    addAndMakeVisible (cancelBtn);
    addAndMakeVisible (saveBtn);
    setVisible (false);
}

juce::Rectangle<int> SavePresetOverlay::panelBounds() const
{
    return juce::Rectangle<int> (448, 432).withCentre (getLocalBounds().getCentre());
}

void SavePresetOverlay::open()
{
    const auto& meta = proc.getCurrentPresetMeta();
    nameEd.setText (meta.isFactory() || meta.name == "Init" ? meta.name + " Variation" : meta.name, false);
    authorEd.setText (meta.isFactory() ? juce::String() : meta.author, false);
    tagsEd.setText (meta.tags.joinIntoString (", "), false);
    descEd.setText (meta.description, false);
    categoryBox.clear (juce::dontSendNotification);
    const auto cats = proc.getPresetLibrary().getCategories();
    for (int i = 0; i < cats.size(); ++i)
        categoryBox.addItem (cats[i], i + 1);
    categoryBox.setText (meta.isFactory() || meta.category.isEmpty() || meta.category == "Init" ? juce::String ("User") : meta.category,
                         juce::dontSendNotification);
    status = {};
    setVisible (true);
    toFront (true);
    nameEd.grabKeyboardFocus();
    nameEd.selectAll();
}

void SavePresetOverlay::close()
{
    setVisible (false);
    if (onClosed)
        onClosed();
}

void SavePresetOverlay::save()
{
    const auto name = nameEd.getText().trim();
    if (name.isEmpty())
    {
        status = "Enter a preset name.";
        statusIsError = true;
        repaint();
        return;
    }
    juce::StringArray tags;
    tags.addTokens (tagsEd.getText(), ",", "");
    tags.trim();
    tags.removeEmptyStrings();
    auto cat = categoryBox.getText().trim();
    if (cat.isEmpty())
        cat = "User";
    const auto rec = proc.captureCurrentPreset (name, authorEd.getText().trim(), cat, tags, descEd.getText().trim());
    const auto res = proc.saveUserPreset (rec);
    if (res.wasOk())
        close();
    else
    {
        status = res.getErrorMessage();
        statusIsError = true;
        repaint();
    }
}

bool SavePresetOverlay::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey)
    {
        close();
        return true;
    }
    return false;
}

void SavePresetOverlay::mouseDown (const juce::MouseEvent& e)
{
    if (! panelBounds().contains (e.getPosition()))
        close();
}

void SavePresetOverlay::resized()
{
    chromeLayer.invalidate();
    const auto p = panelBounds();
    const int x = p.getX() + 112, w = p.getWidth() - 112 - 24;
    nameEd.setBounds (x, p.getY() + 72, w, 32);
    authorEd.setBounds (x, p.getY() + 112, w, 32);
    categoryBox.setBounds (x, p.getY() + 152, w, 32);
    tagsEd.setBounds (x, p.getY() + 192, w, 32);
    descEd.setBounds (x, p.getY() + 232, w, 104);
    cancelBtn.setBounds (p.getRight() - 24 - 136 - 8 - 104, p.getBottom() - 56, 104, 36);
    saveBtn.setBounds (p.getRight() - 24 - 136, p.getBottom() - 56, 136, 36);
}

void SavePresetOverlay::paint (juce::Graphics& g)
{
    const auto p = panelBounds().toFloat();
    chromeLayer.draw (g, getLocalBounds(), [p] (juce::Graphics& cg, juce::Rectangle<float> r) {
        cg.setColour (juce::Colours::black.withAlpha (0.62f));
        cg.fillRect (r);
        cg.setColour (juce::Colours::black.withAlpha (0.5f));
        cg.fillRoundedRectangle (p.translated (0.0f, 4.0f).expanded (2.0f), 6.0f);
        chrome::drawPlate (cg, p, {}, false);
        chrome::drawCaption (cg, { p.getX() + 24.0f, p.getY() + 8.0f, 240.0f, 40.0f }, "SAVE PRESET");
        chrome::drawDividerH (cg, p.getY() + 56.0f, p.getX() + 1.0f, p.getRight() - 1.0f);
        cg.setColour (palette::stone);
        cg.setFont (capsFont (9.5f));
        const char* const labels[] { "NAME", "AUTHOR", "CATEGORY", "TAGS", "DESCRIPTION" };
        for (int i = 0; i < 5; ++i)
            cg.drawText (labels[i], juce::Rectangle<float> (p.getX() + 24.0f, p.getY() + 72.0f + static_cast<float> (i) * 40.0f, 84.0f, 32.0f),
                         juce::Justification::centredLeft, false);
        chrome::drawDividerH (cg, p.getBottom() - 72.0f, p.getX() + 1.0f, p.getRight() - 1.0f);
    });
    g.setFont (uiFont (12.0f, Weight::medium));
    g.setColour (statusIsError ? palette::crimson : palette::stone);
    const auto msg = status.isNotEmpty() ? status : juce::String ("Saved as .mnlpreset (V2 schema) in your user preset folder.");
    g.drawFittedText (msg, juce::Rectangle<int> (static_cast<int> (p.getX()) + 24, static_cast<int> (p.getBottom()) - 56, 136, 36),
                      juce::Justification::centredLeft, 2);
}

} // namespace monolith::ui
