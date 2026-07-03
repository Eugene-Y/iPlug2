#pragma once

#include <IControls.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>

namespace hvoya::ui {

// Knob-style control that shows a single integer value.
// Drag vertically to change; normalized range is divided into N equal steps.
//
// Color mapping (via IVStyle):
//   kBG  — normal background
//   kFG  — normal text
//   kHL  — hover background
//   kX1  — hover text
//
// style.valueText — font/size/alignment of the number
// style.labelText — font/size/alignment/color of the optional side label
//
// The control can draw its own side label on one of four sides (or not at all). The
// label and value split the control's bounds, so the label is drawn INSIDE the control
// and fades together with it when disabled. The label text honours the horizontal AND
// vertical alignment of style.labelText (it is NOT forced to centre).
class IntDragControl : public IKnobControlBase
                     , public IVectorBase
{
public:
    // Where the label sits relative to the value, splitting the control's bounds.
    enum class LabelPos { None, Left, Right, Above, Below };

    IntDragControl(const IRECT& bounds,
                   int paramIdx,
                   const IVStyle& style = DEFAULT_STYLE,
                   std::string_view label = "",
                   LabelPos labelPos = LabelPos::None,
                   float labelFraction = 0.4f,
                   double gearing = DEFAULT_GEARING)
    : IKnobControlBase(bounds, paramIdx, EDirection::Vertical, gearing)
    , IVectorBase(style)
    , _label(label)
    , _labelPos(labelPos)
    , _labelFraction(std::clamp(labelFraction, 0.f, 1.f))
    {
        AttachIControl(this, "");
    }

    // Declarative label setters — the label draws inside the control's bounds.
    void setLabel(std::string_view text)   { _label = text;  SetDirty(false); }
    void setLabelPos(LabelPos pos)         { _labelPos = pos; SetDirty(false); }
    void setLabelFraction(float fraction)  { _labelFraction = std::clamp(fraction, 0.f, 1.f); SetDirty(false); }

    void Draw(IGraphics& g) override {
        const bool over = mMouseIsOver;

        g.FillRect(GetColor(over ? kHL : kBG), mRECT, &mBlend);

        const IParam* p = GetParam();
        if (!p) return;

        const bool hasLabel = _labelPos != LabelPos::None && !_label.empty();
        IRECT valueRect = mRECT;
        IRECT labelRect;
        if (hasLabel) splitBounds(mRECT, valueRect, labelRect);

        // GetDisplay is an IPlug API boundary that fills a WDL_String — used here only.
        // GetDisplay so enum labels (e.g. "all") and custom DisplayFuncs work; fall back
        // to %d only if GetDisplay leaves the string empty.
        WDL_String str;
        const double rawVal = p->FromNormalized(GetValue());
        p->GetDisplay(rawVal, false, str, true);
        if (str.GetLength() == 0)
            str.SetFormatted(32, "%d", static_cast<int>(std::round(rawVal)));

        g.DrawText(mStyle.valueText.WithFGColor(GetColor(over ? kX1 : kFG)),
                   str.Get(), valueRect, &mBlend);

        if (hasLabel)
            g.DrawText(mStyle.labelText, _label.c_str(), labelRect, &mBlend);
    }

private:
    // Carve the control bounds into a value rect and a label rect on the chosen side.
    void splitBounds(const IRECT& b, IRECT& value, IRECT& label) const {
        switch (_labelPos) {
            case LabelPos::Left:
                label = b.GetFromLeft   (b.W() * _labelFraction);
                value = b.GetFromRight  (b.W() * (1.f - _labelFraction));
                break;
            case LabelPos::Right:
                label = b.GetFromRight  (b.W() * _labelFraction);
                value = b.GetFromLeft   (b.W() * (1.f - _labelFraction));
                break;
            case LabelPos::Above:
                label = b.GetFromTop    (b.H() * _labelFraction);
                value = b.GetFromBottom (b.H() * (1.f - _labelFraction));
                break;
            case LabelPos::Below:
                label = b.GetFromBottom (b.H() * _labelFraction);
                value = b.GetFromTop    (b.H() * (1.f - _labelFraction));
                break;
            case LabelPos::None:
                value = b;
                break;
        }
    }

    std::string _label;
    LabelPos    _labelPos      = LabelPos::None;
    float       _labelFraction = 0.4f;
};

} // ns hvoya::ui
