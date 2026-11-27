#pragma once

#include "../PluginProcessor.h"
#include "CachedLayer.h"
#include "MeterComponents.h"
#include "MonolithLookAndFeel.h"
#include <array>
#include <functional>
#include <memory>
#include <juce_gui_basics/juce_gui_basics.h>

namespace monolith::ui
{

/**
    Macro routing editor (modal): per-preset macro name, V1 legacy-curve gate and 8 routes
    (destination, bipolar depth -100..+100 % of the destination range, unipolar / bipolar).
    Writes go straight to the control plane (message thread -> TripleBuffer -> audio thread).
*/
class MacroEditorOverlay final : public juce::Component
{
public:
    explicit MacroEditorOverlay (MonolithAudioProcessor& p);
    void open (int macroIndex);
    void close();
    std::function<void()> onClosed;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& k) override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    struct Row
    {
        juce::ComboBox dest;
        juce::Slider depth;
        juce::ToggleButton bipolar { "BIPOLAR" };
        GlyphButton clear { "clear", GlyphButton::Glyph::close, false };
    };

    [[nodiscard]] juce::Rectangle<int> panelBounds() const;
    void loadFromControlPlane();
    void writeRow (int slot);

    MonolithAudioProcessor& proc;
    int macro = 0;
    bool loading = false;
    juce::TextEditor labelEd;
    juce::ToggleButton legacyBtn { "V1 CURVE" };
    std::array<std::unique_ptr<Row>, 8> rows;
    juce::TextButton clearAllBtn { "CLEAR ROUTES" }, doneBtn { "DONE" };
    CachedLayer chromeLayer;
};

} // namespace monolith::ui
