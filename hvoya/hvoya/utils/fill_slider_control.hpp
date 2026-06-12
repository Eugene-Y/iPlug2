#pragma once

#include <IControls.h>
#include <algorithm>
#include <cmath>
#include <format>
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
                       const IText& labelText, std::string label,
                       double gearing = DEFAULT_GEARING)
        : ISliderControlBase (bounds, paramIdx, EDirection::Horizontal, gearing, 0.f)
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
        std::string label = _label;
        if (_showIntValue) {
            if (const IParam* p = GetParam()) {
                const double v = p->FromNormalized (GetValue());
                const std::string valStr = _valueDecimalPlaces > 0
                    ? std::format ("{:.{}f}", v, _valueDecimalPlaces)
                    : std::to_string (static_cast<int> (std::round (v)));
                if (_valueSuffix.empty()) {
                    label += ' ';
                    label += valStr;
                } else {
                    label = valStr + ' ' + _valueSuffix;
                }
            }
        }
        _widget.draw (g, mRECT, static_cast<float> (GetValue()), label, &mBlend);
    }

    // Delta-based drag with gearing so Shift gives fine control (×10 slower).
    // SnapToMouse on initial click is handled by the base OnMouseDown; here we only
    // apply incremental movement so gearing actually works with mHideCursorOnDrag = false.
    void OnMouseDrag (float, float, float dX, float, const IMouseMod& mod) override {
        const double trackW = mTrackBounds.W();
        if (trackW == 0.) return;
        const double gear = IsFineControl (mod, false) ? mGearing * 10. : mGearing;
        mMouseDragValue = std::clamp (mMouseDragValue + dX / trackW / gear, 0., 1.);
        SetValue (mMouseDragValue);
        SetDirty();
    }

    // Double-click resets to the parameter default, like the other controls.
    void OnMouseDblClick (float, float, const IMouseMod&) override { SetValueToDefault(); }

    // ── declarative appearance ───────────────────────────────────────────────
    void setColors (const IColor& track, const IColor& fill, const IColor& activeFill) {
        _track = track; _fill = fill; _activeFill = activeFill; SetDirty (false);
    }
    void setLabelText (const IText& t)     { _labelText = t; SetDirty (false); }
    void setLabel     (std::string label)  { _label = std::move (label); SetDirty (false); }
    // When true, appends the current integer value (in the param's real unit) to the label.
    // If a suffix was set via setValueSuffix, the format is "value suffix" instead of "label value".
    void setShowIntValue (bool show)              { _showIntValue = show; SetDirty (false); }
    // When non-empty, switches the int-value display to "value suffix" (e.g. "-80 dB").
    void setValueSuffix       (std::string s)     { _valueSuffix = std::move (s); SetDirty (false); }
    // Number of decimal places (0 = integer display, the default).
    void setValueDecimalPlaces (int n)            { _valueDecimalPlaces = n;      SetDirty (false); }

private:
    SliderWidget _widget;
    IColor       _track, _fill, _activeFill;
    IText        _labelText;
    std::string  _label;
    std::string  _valueSuffix;
    bool         _showIntValue      = false;
    int          _valueDecimalPlaces = 0;
};

} // namespace hvoya::ui
