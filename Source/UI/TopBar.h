#pragma once

#include "../PluginProcessor.h"
#include "CachedLayer.h"
#include "MeterComponents.h"
#include "MonolithLookAndFeel.h"
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

namespace monolith::ui
{

class OutputAnalyser;

/** Preset name plate: favourite star, name, category / author, browse affordance. Click opens the browser. */
class PresetDisplay final : public juce::Component,
                            public juce::SettableTooltipClient
{
public:
    PresetDisplay();
    void set (const juce::String& name, const juce::String& sub, bool favourite, bool canFavourite);
    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    std::function<void()> onOpen, onToggleFavourite;

private:
    GlyphButton star { "favourite", GlyphButton::Glyph::star, false };
    juce::String name, sub;
};

/** MPE status: dim when off, outlined when enabled, glowing while MPE data arrives. Click toggles. */
class MpeBadge final : public juce::Component,
                       public juce::SettableTooltipClient
{
public:
    explicit MpeBadge (MonolithAudioProcessor& p);
    void refresh();
    void paint (juce::Graphics& g) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    MonolithAudioProcessor& proc;
    std::unique_ptr<juce::ParameterAttachment> enableAttach;
    bool enabled = false, active = false;
    int bendRange = 48;
};

class TopBar final : public juce::Component
{
public:
    explicit TopBar (MonolithAudioProcessor& p);

    /** 30 Hz: preset meta, favourite, A/B, undo / redo availability, MPE status. */
    void refresh();
    void updateMeter (const OutputAnalyser& a) { meter.update (a); }
    void setZoomPercent (int pct);

    std::function<void()> onBrowse, onSave, onClipReset;
    std::function<void (float)> onZoom;

    void resized() override;

private:
    void stepPreset (int delta);

    MonolithAudioProcessor& proc;
    GlyphButton prevBtn { "prev", GlyphButton::Glyph::prev }, nextBtn { "next", GlyphButton::Glyph::next };
    PresetDisplay display;
    juce::TextButton saveBtn { "SAVE" }, initBtn { "INIT" };
    juce::TextButton aBtn { "A" }, bBtn { "B" }, copyBtn;
    GlyphButton undoBtn { "undo", GlyphButton::Glyph::undo }, redoBtn { "redo", GlyphButton::Glyph::redo };
    MpeBadge mpe;
    juce::TextButton zoomBtn { "100%" };
    OutputMeter meter;
};

/** Root chassis: brushed obsidian body, top bar strip and the MONOLITH II wordmark (one cached image). */
class ChassisBackground final : public juce::Component
{
public:
    void paint (juce::Graphics& g) override;
    void resized() override { layer.invalidate(); }
    [[nodiscard]] int getStaticRenderCount() const noexcept { return layer.getRenderCount(); }

private:
    CachedLayer layer;
};

} // namespace monolith::ui
