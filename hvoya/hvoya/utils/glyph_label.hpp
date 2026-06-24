#pragma once

/* glyph_label.hpp — multi-font (mixed) text labels for iPlug2 controls.
 *
 * A button label may need glyphs from several fonts on one line — e.g. an arrow
 * from ForkAwesome next to a plug icon from fontaudio. `GlyphLabel` holds a small
 * sequence of runs, each carrying its own font; `drawGlyphLabel` lays them out on
 * one centered line. `GlyphButtonControl` is a ready clickable button drawn with one.
 *
 * The fonts must already be registered via IGraphics::LoadFont; this header does
 * NOT pull in any icon header — the caller passes glyph strings + font family names.
 */

#include <IControl.h>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace hvoya::ui {

using namespace iplug::igraphics;

struct GlyphRun {
    std::string text;
    std::string font;   // registered font family; empty = the surrounding IText's font
};

// One line of text that may switch fonts between runs. Construct from a plain
// string (single default-font run) or compose runs with `add` / `operator+`.
class GlyphLabel {
public:
    GlyphLabel () = default;
    GlyphLabel (std::string_view plain) { if (!plain.empty()) _runs.push_back ({ std::string (plain), {} }); }
    GlyphLabel (const char* plain) : GlyphLabel (std::string_view (plain ? plain : "")) {}

    static GlyphLabel glyph (std::string_view s, std::string_view font) {
        GlyphLabel l;
        l._runs.push_back ({ std::string (s), std::string (font) });
        return l;
    }

    GlyphLabel& add (std::string_view s, std::string_view font = {}) {
        _runs.push_back ({ std::string (s), std::string (font) });
        return *this;
    }

    GlyphLabel& operator+= (const GlyphLabel& o) {
        _runs.insert (_runs.end(), o._runs.begin(), o._runs.end());
        return *this;
    }
    friend GlyphLabel operator+ (GlyphLabel a, const GlyphLabel& b) { a += b; return a; }

    bool empty () const { return _runs.empty(); }
    const std::vector<GlyphRun>& runs () const { return _runs; }

private:
    std::vector<GlyphRun> _runs;
};

// A single-font icon run; `font` is one of the registered icon font families.
inline GlyphLabel icon (std::string_view glyph, std::string_view font) {
    return GlyphLabel::glyph (glyph, font);
}

// Draws `label` centered in `r` (horizontally and vertically), taking size/color
// from `base` and the font from each run (empty run font → base font). `runGap`
// adds horizontal spacing between runs.
inline void drawGlyphLabel (IGraphics& g, const GlyphLabel& label, const IRECT& r,
                            const IText& base, const IBlend* blend, float runGap = 0.f) {
    const auto& runs = label.runs();
    if (runs.empty()) return;

    auto styleFor = [&] (const GlyphRun& run) {
        return run.font.empty() ? base : base.WithFont (run.font.c_str());
    };
    const auto centered = [] (const IText& t) {
        return t.WithAlign (EAlign::Center).WithVAlign (EVAlign::Middle);
    };

    if (runs.size() == 1) {
        g.DrawText (centered (styleFor (runs[0])), runs[0].text.c_str(), r, blend);
        return;
    }

    std::vector<float> widths (runs.size());
    float total = 0.f;
    for (size_t i = 0; i < runs.size(); ++i) {
        IRECT m;
        widths[i] = g.MeasureText (styleFor (runs[i]), runs[i].text.c_str(), m);
        total += widths[i] + (i ? runGap : 0.f);
    }

    float x = r.MW() - total * 0.5f;
    for (size_t i = 0; i < runs.size(); ++i) {
        const IRECT cell (x, r.T, x + widths[i], r.B);
        g.DrawText (centered (styleFor (runs[i])), runs[i].text.c_str(), cell, blend);
        x += widths[i] + runGap;
    }
}

// Clickable button whose label can mix fonts — for things like a "save CC" button
// (a down-arrow glyph + a MIDI-plug glyph). Declarative color setters mirror the
// preset strip's, so callers never decode IVStyle color slots.
class GlyphButtonControl : public IControl, public IVectorBase {
public:
    GlyphButtonControl (const IRECT& bounds,
                        GlyphLabel label,
                        std::function<void(IControl*)> onClick,
                        const IVStyle& style = DEFAULT_STYLE)
        : IControl (bounds)
        , IVectorBase (style)
        , _label (std::move (label))
        , _onClick (std::move (onClick))
    {
        AttachIControl (this, "");
    }

    GlyphButtonControl& setLabel           (GlyphLabel l)     { _label = std::move (l); SetDirty (false); return *this; }
    GlyphButtonControl& setRunGap          (float px)         { _runGap = px;                              return *this; }
    GlyphButtonControl& setBackgroundColor (const IColor& c)  { mStyle.colorSpec.mColors[kBG] = c;         return *this; }
    GlyphButtonControl& setMouseOverColor  (const IColor& c)  { mStyle.colorSpec.mColors[kHL] = c;         return *this; }
    GlyphButtonControl& setPressedColor    (const IColor& c)  { mStyle.colorSpec.mColors[kPR] = c;         return *this; }
    GlyphButtonControl& setFrameColor      (const IColor& c)  { mStyle.colorSpec.mColors[kFR] = c;         return *this; }
    GlyphButtonControl& setTextColor       (const IColor& c)  { mStyle.valueText.mFGColor = c;             return *this; }
    // When set, the glyph (not a background rect) recolors to `c` while hovered.
    GlyphButtonControl& setMouseOverTextColor (const IColor& c) { _mouseOverTextColor = c; _hasMouseOverText = true; return *this; }

    // Opt-in: when set, the button is enabled only while `fn` returns true. It is
    // re-evaluated on a throttle (the predicate may be cheap-ish but not free, e.g. a
    // clipboard probe), so the button polls — IsDirty stays true while a predicate is
    // installed. A disabled button dims its glyph and ignores clicks/hover.
    GlyphButtonControl& setEnabledFn (std::function<bool()> fn) { _enabledFn = std::move (fn); return *this; }

    void Draw (IGraphics& g) override {
        if (_enabledFn && --_enabledThrottle <= 0) {
            _enabledThrottle = kEnabledThrottleFrames;
            _enabled = _enabledFn();
        }
        const IColor bg = GetColor (kBG);
        if (bg.A > 0) g.FillRect (bg, mRECT, &mBlend);
        if (_enabled) {
            if (_pressed)          g.FillRect (GetColor (kPR), mRECT, &mBlend);
            else if (mMouseIsOver) g.FillRect (GetColor (kHL), mRECT, &mBlend);
        }
        const IColor frame = GetColor (kFR);
        if (frame.A > 0) g.DrawRect (frame, mRECT, &mBlend, 1.f);
        IText txt = mStyle.valueText;
        if (mMouseIsOver && _hasMouseOverText && _enabled) txt.mFGColor = _mouseOverTextColor;
        if (!_enabled) txt.mFGColor = txt.mFGColor.WithOpacity (0.35f);
        drawGlyphLabel (g, _label, mRECT, txt, &mBlend, _runGap);
    }

    bool IsDirty () override { return _enabledFn ? true : IControl::IsDirty(); }

    void OnMouseDown (float, float, const IMouseMod&) override { if (_enabled) { _pressed = true; SetDirty (false); } }
    void OnMouseUp (float x, float y, const IMouseMod&) override {
        _pressed = false;
        SetDirty (false);
        if (_enabled && _onClick && mRECT.Contains (x, y)) _onClick (this);
    }

private:
    static constexpr int           kEnabledThrottleFrames = 20;

    GlyphLabel                     _label;
    std::function<void(IControl*)> _onClick;
    float                          _runGap  = 0.f;
    bool                           _pressed = false;
    IColor                         _mouseOverTextColor;
    bool                           _hasMouseOverText = false;
    std::function<bool()>          _enabledFn;
    bool                           _enabled = true;
    int                            _enabledThrottle = 0;
};

// Non-interactive centered glyph label — mouse-transparent, never highlights. For
// row prefixes that name a block of buttons (e.g. a "midi:" tag before CC buttons).
class GlyphLabelControl : public IControl, public IVectorBase {
public:
    GlyphLabelControl (const IRECT& bounds, GlyphLabel label, const IVStyle& style = DEFAULT_STYLE)
        : IControl (bounds)
        , IVectorBase (style)
        , _label (std::move (label))
    {
        AttachIControl (this, "");
        SetIgnoreMouse (true);
    }

    GlyphLabelControl& setLabel           (GlyphLabel l)    { _label = std::move (l); SetDirty (false); return *this; }
    GlyphLabelControl& setRunGap          (float px)        { _runGap = px;                              return *this; }
    GlyphLabelControl& setBackgroundColor (const IColor& c) { mStyle.colorSpec.mColors[kBG] = c;         return *this; }
    GlyphLabelControl& setTextColor       (const IColor& c) { mStyle.valueText.mFGColor = c;             return *this; }

    void Draw (IGraphics& g) override {
        const IColor bg = GetColor (kBG);
        if (bg.A > 0) g.FillRect (bg, mRECT, &mBlend);
        drawGlyphLabel (g, _label, mRECT, mStyle.valueText, &mBlend, _runGap);
    }

private:
    GlyphLabel _label;
    float      _runGap = 0.f;
};

} // namespace hvoya::ui
