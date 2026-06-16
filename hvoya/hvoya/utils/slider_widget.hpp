#pragma once

#include <algorithm>
#include <string>
#include <IGraphics.h>

namespace hvoya::ui {

using namespace iplug;
using namespace iplug::igraphics;

// A lightweight horizontal slider drawn INLINE by an owning control (not a standalone IControl):
// a flat background, an accent fill growing from an anchor (the left edge by default) to the value
// fraction, and the value label centered on top. Several of these compose inside a custom control (e.g. the morph pad's
// edit overlay), where attaching separate IControls would be awkward. The owner keeps the value
// and rect; this widget provides the look + the hit/fraction helpers, so every such slider is
// visually identical.
//
// The style (background, accent, centered text) is set once; draw() takes the rect, the fill
// fraction [0,1] and the already-formatted label.
class SliderWidget {
public:
    void setStyle (const IColor& bg, const IColor& accent, const IText& text) {
        _bg     = bg;
        _accent = accent;
        _text   = text.WithAlign (EAlign::Center).WithVAlign (EVAlign::Middle);
    }

    // The normalized fraction the fill grows FROM (default 0 = left edge). Set to 0.5 for a
    // bipolar param so the fill spans from the track centre toward the value in either direction.
    void setAnchor (float frac) { _anchor = std::clamp (frac, 0.f, 1.f); }

    void draw (IGraphics& g, const IRECT& rect, float frac,
               const std::string& label, const IBlend* blend = nullptr) const {
        g.FillRect (_bg, rect, blend);                                  // background, no outline
        frac = std::clamp (frac, 0.f, 1.f);
        const float lo = std::min (_anchor, frac);
        const float hi = std::max (_anchor, frac);
        if (hi > lo)
            g.FillRect (_accent,
                        IRECT (rect.L + rect.W() * lo, rect.T, rect.L + rect.W() * hi, rect.B),
                        blend);
        g.DrawText (_text, label.c_str(), rect, blend);                 // value centered
    }

    static bool  hit (const IRECT& rect, float x, float y) { return rect.Contains (x, y); }
    static float fracForX (const IRECT& rect, float x) {
        return rect.W() > 0.f ? std::clamp ((x - rect.L) / rect.W(), 0.f, 1.f) : 0.f;
    }

private:
    IColor _bg     { COLOR_BLACK };
    IColor _accent { COLOR_WHITE };
    IText  _text;
    float  _anchor { 0.f };
};

} // namespace hvoya::ui
