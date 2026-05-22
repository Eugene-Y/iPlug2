#pragma once

#include <IControls.h>
#include <cmath>

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
// style.labelText — font/size/alignment of the optional side label
//                   (label rect is always center-aligned regardless of labelText alignment)
//
// The side label is drawn in a rect equal to mRECT shifted by (labelOffsetX, labelOffsetY).
// Pass label="" to disable it.
class IntDragControl : public IKnobControlBase
                     , public IVectorBase
{
public:
    IntDragControl(const IRECT& bounds,
                   int paramIdx,
                   const IVStyle& style = DEFAULT_STYLE,
                   const char* label = "",
                   float labelOffsetX = 0.f,
                   float labelOffsetY = 0.f,
                   double gearing = DEFAULT_GEARING)
    : IKnobControlBase(bounds, paramIdx, EDirection::Vertical, gearing)
    , IVectorBase(style)
    , _labelOffsetX(labelOffsetX)
    , _labelOffsetY(labelOffsetY)
    {
        _label.Set(label);
        AttachIControl(this, "");
    }

    void Draw(IGraphics& g) override {
        const bool over = mMouseIsOver;

        g.FillRect(GetColor(over ? kHL : kBG), mRECT, &mBlend);

        const IParam* p = GetParam();
        if (!p) return;

        WDL_String str;
        str.SetFormatted(32, "%d", static_cast<int>(std::round(p->FromNormalized(GetValue()))));

        g.DrawText(mStyle.valueText.WithFGColor(GetColor(over ? kX1 : kFG)),
                   str.Get(), mRECT, &mBlend);

        if (_label.GetLength() > 0) {
            const IRECT labelRect = mRECT.GetTranslated(_labelOffsetX, _labelOffsetY);
            g.DrawText(mStyle.labelText.WithAlign(EAlign::Center).WithVAlign(EVAlign::Middle),
                       _label.Get(), labelRect, &mBlend);
        }
    }

private:
    WDL_String _label;
    float      _labelOffsetX;
    float      _labelOffsetY;
};

} // ns hvoya::ui
