#pragma once

#include <IControls.h>
#include <string>
#include <utility>

#include "slider_widget.hpp"

namespace hvoya::ui {

using namespace iplug;
using namespace iplug::igraphics;

// A standalone, parameter-bound horizontal slider drawn with SliderWidget: a flat track,
// a fill growing from the left by the parameter's normalized value, and a fixed centered
// label (no numeric readout). ISliderControlBase supplies drag / wheel / fine control; this
// only swaps the look to the flat-fill SliderWidget style.
//
// Two visual states, switched by hover/drag (mouseOver OR mouseDown):
//   idle   — background = track,  fill = fill         (e.g. transparent track, light-gray fill)
//   active — background = fill,   fill = activeFill    (e.g. light-gray track, accent fill)
// so hovering or dragging highlights the slider. Appearance is declarative — the colours and
// label are given explicitly and drawn exactly as given (no style-slot decoding).
class FillSliderControl : public ISliderControlBase {
public:
    FillSliderControl (const IRECT& bounds, int paramIdx,
                       const IColor& track, const IColor& fill, const IColor& activeFill,
                       const IText& labelText, std::string label)
        : ISliderControlBase (bounds, paramIdx, EDirection::Horizontal, DEFAULT_GEARING, 0.f)
        , _track (track), _fill (fill), _activeFill (activeFill), _labelText (labelText)
        , _label (std::move (label))
    {
        mHideCursorOnDrag = false;   // thin horizontal bar — keep the cursor visible while dragging
    }

    void Draw (IGraphics& g) override {
        const bool active = mMouseDown || mMouseIsOver;
        _widget.setStyle (active ? _fill      : _track,    // active: light-gray track; idle: transparent
                          active ? _activeFill : _fill,     // active: accent fill;      idle: light-gray
                          _labelText);
        _widget.draw (g, mRECT, static_cast<float> (GetValue()), _label, &mBlend);
    }

    // Double-click resets to the parameter default, like the other controls.
    void OnMouseDblClick (float, float, const IMouseMod&) override { SetValueToDefault(); }

    // ── declarative appearance ───────────────────────────────────────────────
    void setColors (const IColor& track, const IColor& fill, const IColor& activeFill) {
        _track = track; _fill = fill; _activeFill = activeFill; SetDirty (false);
    }
    void setLabelText (const IText& t)     { _labelText = t; SetDirty (false); }
    void setLabel     (std::string label)  { _label = std::move (label); SetDirty (false); }

private:
    SliderWidget _widget;
    IColor       _track, _fill, _activeFill;
    IText        _labelText;
    std::string  _label;
};

} // namespace hvoya::ui
