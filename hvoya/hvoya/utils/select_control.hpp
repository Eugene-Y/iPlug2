#pragma once

#include <IControls.h>
#include <algorithm>

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

    // Suppress the "selected option" highlight without changing the param value — the grid then
    // reads as having no active choice (e.g. the pass-type grid while the filter shape is a
    // user-blend that no single type expresses). Hover feedback is unaffected.
    SelectControl& setDisplayUnselected (bool b) {
        if (_displayUnselected != b) { _displayUnselected = b; SetDirty (false); }
        return *this;
    }

    // Lay the options out as a `cols` × `rows` grid (row-major) instead of a single
    // row/column. Pass 0 to either to fall back to the linear, direction-based layout.
    SelectControl& setGrid (int cols, int rows) {
        _cols = cols;
        _rows = rows;
        if (GetUI()) OnResize();
        return *this;
    }

    void OnResize () override {
        if (_cols <= 0 || _rows <= 0) { IVTabSwitchControl::OnResize(); return; }

        SetTargetRECT (MakeRects (mRECT));
        mButtons.Resize (0);
        for (int i = 0; i < mNumStates; ++i) {
            const int row = std::min (i / _cols, _rows - 1);
            const int col = i % _cols;
            mButtons.Add (mWidgetBounds.SubRectVertical (_rows, row)
                                       .SubRectHorizontal (_cols, col));
        }
        SetDirty (false);
    }

    void DrawButton (IGraphics& g, const IRECT& r,
                     bool pressed, bool mouseOver,
                     ETabSegment, bool disabled) override
    {
        if (_displayUnselected) pressed = false;   // no option reads as selected
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
    int   _cols = 0;   // 0 = linear (direction-based) layout
    int   _rows = 0;
    bool  _displayUnselected = false;
};

} // ns hvoya::ui
