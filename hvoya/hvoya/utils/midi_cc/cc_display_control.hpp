#pragma once
#include <IControl.h>

namespace hvoya::midi_cc {

// Display-only text control with CC-learn support via right-click (wrap with createLearnable).
// Call SetStr() from OnIdle to update the displayed CC number.
class CCDisplayControl : public IControl {
public:
    CCDisplayControl(const IRECT& bounds, int paramIdx, const char* text, const IText& style)
        : IControl(bounds, paramIdx), _style(style) {
        _str.Set(text);
    }

    void Draw(IGraphics& g) override {
        g.DrawText(_style, _str.Get(), mRECT, &mBlend);
    }

    void SetStr(const char* str) {
        _str.Set(str);
        SetDirty(false);
    }

private:
    IText _style;
    WDL_String _str;
};

} // namespace hvoya::midi_cc
