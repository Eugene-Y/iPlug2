#pragma once

#include <IControls.h>

namespace hvoya::ui {

// A tab-switch-style control that renders each option as a thin outlined
// rectangle with centered text. Supports both vertical and horizontal stacks.
// Selected option gets a filled background; unselected options are outlines only.
class SelectControl : public IVTabSwitchControl {
public:
    SelectControl (const IRECT& bounds,
                   int paramIdx,
                   const std::vector<const char*>& options = {},
                   const char* label = "",
                   const IVStyle& style = DEFAULT_STYLE,
                   EDirection direction = EDirection::Vertical,
                   float frameThickness = 1.f)
    : IVTabSwitchControl (bounds, paramIdx, options, label, style,
                          EVShape::Rectangle, direction)
    , _frameThickness (frameThickness)
    {}

    SelectControl (const IRECT& bounds,
                   IActionFunction aF,
                   const std::vector<const char*>& options,
                   const char* label = "",
                   const IVStyle& style = DEFAULT_STYLE,
                   EDirection direction = EDirection::Vertical,
                   float frameThickness = 1.f)
    : IVTabSwitchControl (bounds, aF, options, label, style,
                          EVShape::Rectangle, direction)
    , _frameThickness (frameThickness)
    {}

    void DrawButton (IGraphics& g, const IRECT& r,
                     bool pressed, bool mouseOver,
                     ETabSegment, bool disabled) override
    {
        const float opacity = disabled ? 0.35f : 1.f;
        if (pressed || mouseOver) {
            const IColor fill = pressed ? GetColor (kPR) : GetColor (kHL);
            g.FillRect (fill.WithOpacity (opacity), r, &mBlend);
        }
        const IColor outline = GetColor (kX1);
        if (outline.A > 0)
            g.DrawRect (outline.WithOpacity (outline.A / 255.f * opacity * 0.6f), r, &mBlend, _frameThickness);
    }

private:
    float _frameThickness;
};

} // ns hvoya::ui
