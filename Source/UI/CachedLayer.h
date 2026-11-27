#pragma once

#include <cmath>
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

namespace monolith::ui
{

/**
    Retina-aware static image layer. paint() asks the graphics context for its physical pixel scale
    (editor zoom x display scale x snapshot scale) and re-renders the static content only when that scale,
    the area size, or an explicit invalidate() changes; every other frame is a single image blit. This keeps
    chassis panels, knob faces, tick marks and grids off the per-frame paint path.
*/
class CachedLayer
{
public:
    using Renderer = std::function<void (juce::Graphics&, juce::Rectangle<float>)>;

    void invalidate() noexcept { dirty = true; }

    void draw (juce::Graphics& g, juce::Rectangle<int> area, const Renderer& render)
    {
        if (area.isEmpty())
            return;
        const float scale = std::max (0.25f, g.getInternalContext().getPhysicalPixelScaleFactor());
        const int pw = std::max (1, juce::roundToInt (static_cast<float> (area.getWidth()) * scale));
        const int ph = std::max (1, juce::roundToInt (static_cast<float> (area.getHeight()) * scale));

        if (dirty || ! image.isValid() || image.getWidth() != pw || image.getHeight() != ph || std::abs (scale - cachedScale) > 1.0e-4f)
        {
            image = juce::Image (juce::Image::ARGB, pw, ph, true);
            juce::Graphics ig (image);
            ig.addTransform (juce::AffineTransform::scale (static_cast<float> (pw) / static_cast<float> (area.getWidth()),
                                                           static_cast<float> (ph) / static_cast<float> (area.getHeight())));
            render (ig, area.withZeroOrigin().toFloat());
            cachedScale = scale;
            dirty = false;
            ++renderCount;
        }

        g.drawImageTransformed (image,
                                juce::AffineTransform::scale (static_cast<float> (area.getWidth()) / static_cast<float> (pw),
                                                              static_cast<float> (area.getHeight()) / static_cast<float> (ph))
                                    .translated (static_cast<float> (area.getX()), static_cast<float> (area.getY())));
    }

    /** Number of static re-renders (diagnostics: stays flat while only live layers animate). */
    [[nodiscard]] int getRenderCount() const noexcept { return renderCount; }

private:
    juce::Image image;
    float cachedScale = 0.0f;
    bool dirty = true;
    int renderCount = 0;
};

} // namespace monolith::ui
