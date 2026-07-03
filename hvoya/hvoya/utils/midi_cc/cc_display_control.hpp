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

    // The control keeps its colour in _style (it draws from that, not IControl::mText). Advertise it so
    // the CC decorator's live-modulator dot uses this text colour instead of the default-black mText.
    IColor labelColor() const { return _style.mFGColor; }

private:
    IText _style;
    WDL_String _str;
};

} // namespace hvoya::midi_cc
