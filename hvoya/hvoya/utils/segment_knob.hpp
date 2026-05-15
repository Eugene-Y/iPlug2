#pragma once

#include <IControls.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>


namespace hvoya::ui {


// IVKnobControl variant for morphing between N named options.
//
// The IRECT passed to the constructor is the knob area (= mWidgetBounds).
// The full mRECT is expanded symmetrically around it to give ticks and labels
// room so they aren't clipped; the widget stays at the caller-supplied position.
//
// Geometry (n = option count, arc = a2 - a1):
//   tick max half-width  = arc / (2n)              (full ticks fill the arc edge-to-edge at norm=1)
//   tick half-width      = norm * arc / (2n)
//   tick centers inset   = a1 + halfArc + i/(n-1) * (arc - 2*halfArc)
//                          (end ticks shifted inward by halfArc so all fit inside [a1, a2])
//   label anchor         = on circle of radius (knobRadius + labelPadding) at the tick center
//   label H-align        = Center if |angle| < epsilon, else Far for angle<0, Near otherwise
//   label V-align        = Bottom if cos>0.5 (top), Top if cos<-0.5 (bottom), Middle otherwise
class SegmentKnob : public IVKnobControl {
public:
    SegmentKnob (const IRECT& knobBounds,
                 int paramIdx,
                 std::vector<std::string> optionNames,
                 const IVStyle& style = DEFAULT_STYLE,
                 float tickThickness = 4.f,
                 float tickWidthNorm = 0.25f,
                 float labelPaddingPx = -1.f,
                 float centerEpsilonDeg = 7.f,
                 bool  showValue = false,
                 bool  valueIsEditable = true,
                 bool  valueInWidget = false,
                 float valuePaddingPx = 4.f,
                 const char* paramName = "",
                 bool  paramNameOnTop = false,
                 float paramNamePaddingPx = 4.f,
                 float a1 = -135.f,
                 float a2 = 135.f)
        : IVKnobControl (expandedFullBounds (knobBounds, style, optionNames,
                                             labelPaddingPx,
                                             paramNameOnTop, paramNamePaddingPx),
                         paramIdx,
                         paramName,
                         styleFor (style, showValue, paramName),
                         valueIsEditable, valueInWidget,
                         a1, a2, a1,
                         EDirection::Vertical, DEFAULT_GEARING,
                         /*trackSize*/ 2.f)
        , _knobW (knobBounds.W())
        , _knobH (knobBounds.H())
        , _optionNames (std::move (optionNames))
        , _tickThickness (tickThickness)
        , _tickWidthNorm (std::clamp (tickWidthNorm, 0.f, 1.f))
        , _labelPaddingPx (labelPaddingPx < 0 ? float (style.labelText.mSize) : labelPaddingPx)
        , _centerEpsilonDeg (centerEpsilonDeg)
        , _valuePaddingPx (valuePaddingPx)
        , _paramNameOnTop (paramNameOnTop)
        , _paramNamePaddingPx (paramNamePaddingPx)
    {
    }

    void setOptionNames (std::vector<std::string> names) {
        _optionNames = std::move (names);
        recomputeGeometry();
        SetDirty (false);
    }

    void setTickWidthNorm (float n)  { _tickWidthNorm = std::clamp (n, 0.f, 1.f); recomputeGeometry(); SetDirty (false); }
    void setTickThickness (float px) { _tickThickness = px;                       SetDirty (false); }
    void setTick (float thickness, float widthNorm) {
        _tickThickness = thickness;
        _tickWidthNorm = std::clamp (widthNorm, 0.f, 1.f);
        recomputeGeometry();
        SetDirty (false);
    }

    void setLabelPadding (float px) { _labelPaddingPx = px; recomputeGeometry(); SetDirty (false); }

    void setShowValue     (bool show)     { mStyle.showValue = show;   SetDirty (false); }
    void setValueEditable (bool editable) { DisablePrompt (!editable); SetDirty (false); }
    void setValuePadding  (float px)      { _valuePaddingPx = px;      OnResize(); }

    void setParamName        (const char* name) { SetLabelStr (name);               SetDirty (false); }
    void setShowParamName    (bool show)        { mStyle.showLabel = show;          SetDirty (false); }
    void setParamNameOnTop   (bool t)           { _paramNameOnTop = t;              OnResize(); }
    void setParamNamePadding (float px)         { _paramNamePaddingPx = px;         OnResize(); }

    void setArcAngles (float a1, float a2) {
        mAngle1 = a1;
        mAngle2 = a2;
        mAnchorAngle = a1;
        recomputeGeometry();
        SetDirty (false);
    }


    void OnResize() override {
        mWidgetBounds = mRECT.GetCentredInside (_knobW, _knobH);
        // Value strip sits just below the actual circle (cy + radius), not below
        // mWidgetBounds.B — the widget rect can be much taller than the diameter.
        // Computed unconditionally so toggling showValue doesn't shift the knob.
        const float valueH = float (mStyle.valueText.mSize) * 1.2f;
        const float circleBottom = mWidgetBounds.MH() + GetRadius();
        const float valueTop = circleBottom + _valuePaddingPx;
        mValueBounds = IRECT (mWidgetBounds.L, valueTop,
                              mWidgetBounds.R, valueTop + valueH);
        SetTargetRECT (mWidgetBounds);
        recomputeGeometry();
        computeParamNameBounds();
        SetDirty (false);
    }


    void DrawWidget (IGraphics& g) override {
        drawKnobWidget (g);
        drawSegmentTicks (g);
        drawOptionLabels (g);
    }


    void DrawIndicatorTrack (IGraphics&, float, float, float, float) override {}


private:
    struct OptionGeometry {
        float   angleDeg   = 0.f;
        float   halfArcDeg = 0.f;
        float   anchorX    = 0.f;
        float   anchorY    = 0.f;
        EAlign  hAlign     = EAlign::Center;
        EVAlign vAlign     = EVAlign::Middle;
    };


    // Call after any change that affects geometry (mWidgetBounds, option count,
    // tick width, label padding). Results are cached in _geoms and reused by draw*.
    void recomputeGeometry() {
        computeRadialAnchors();
        resolveLabelVerticalOverlaps();
    }


    void computeRadialAnchors() {
        _geoms.clear();
        const int n = int (_optionNames.size());
        if (n <= 0)
            return;
        _geoms.reserve (n);

        const float cx      = mWidgetBounds.MW();
        const float cy      = mWidgetBounds.MH();
        const float rAnchor = GetRadius() + _labelPaddingPx;
        const float arcSpan = mAngle2 - mAngle1;
        const float halfArc = _tickWidthNorm * 0.5f * arcSpan / float (n);
        const float insetSpan = std::max (0.f, arcSpan - 2.f * halfArc);

        for (int i = 0; i < n; ++i) {
            const float t   = (n > 1) ? (float (i) / float (n - 1)) : 0.5f;
            const float ang = mAngle1 + halfArc + t * insetSpan;
            const float rad = ang * float (std::numbers::pi) / 180.f;
            const float sn  = std::sin (rad);
            const float cs  = std::cos (rad);

            OptionGeometry g;
            g.angleDeg   = ang;
            g.halfArcDeg = halfArc;
            g.anchorX    = cx + rAnchor * sn;
            g.anchorY    = cy - rAnchor * cs;
            g.hAlign     = pickHAlign (ang);
            g.vAlign     = pickVAlign (cs);
            _geoms.push_back (g);
        }
    }


    // PAV (Pool Adjacent Violators) per side: minimal-L2 spread that guarantees
    // adjacent anchors are at least lineH apart. Side labels whose Y moved switch
    // to Middle valign so spacing is computed at text centers without overlap.
    void resolveLabelVerticalOverlaps() {
        if (_geoms.size() < 2)
            return;
        const float lineH = float (mStyle.labelText.mSize) * 1.2f;

        spreadFarSide (lineH);
        mirrorNearSideFromFar();
    }


    // Spread Far-side labels (with top-polar Center joined in) at a uniform vertical
    // step, keeping the natural span when it's larger than lineH. Centroid of the
    // natural Y distribution is preserved so the stack stays anchored to the knob.
    void spreadFarSide (float lineH) {
        const float cy = mWidgetBounds.MH();

        std::vector<int> idx;
        for (int i = 0; i < int (_geoms.size()); ++i) {
            if (_geoms[i].hAlign == EAlign::Far) {
                idx.push_back (i);
            } else if (_geoms[i].hAlign == EAlign::Center && _geoms[i].anchorY < cy) {
                idx.push_back (i);
            }
        }
        if (idx.size() < 2)
            return;

        std::sort (idx.begin(), idx.end(),
                   [this](int a, int b) { return _geoms[a].anchorY < _geoms[b].anchorY; });

        const int n = int (idx.size());
        const float yMin = _geoms[idx[0]].anchorY;
        const float yMax = _geoms[idx[n - 1]].anchorY;
        const float naturalStep = (yMax - yMin) / float (n - 1);
        const float step = std::max (lineH, naturalStep);

        float centroid = 0.f;
        for (int i : idx) centroid += _geoms[i].anchorY;
        centroid /= float (n);

        const float startY = centroid - float (n - 1) * 0.5f * step;

        for (int j = 0; j < n; ++j) {
            const float newY = startY + float (j) * step;
            if (std::abs (newY - _geoms[idx[j]].anchorY) > 0.5f)
                _geoms[idx[j]].vAlign = EVAlign::Middle;
            _geoms[idx[j]].anchorY = newY;
        }
    }


    // mLabelBounds positions the parameter name text. Either below the value strip
    // (bottom position, default) or above the topmost option label edge (top position).
    // Called after recomputeGeometry so _geoms reflects final option label Y values.
    void computeParamNameBounds() {
        if (!mStyle.showLabel)
            return;
        const float lineH = float (mStyle.labelText.mSize) * 1.2f;

        if (_paramNameOnTop) {
            float topmost = mWidgetBounds.MH() - GetRadius();  // circle top fallback
            for (const auto& g : _geoms) {
                float textTop = g.anchorY;
                if      (g.vAlign == EVAlign::Bottom) textTop -= lineH;
                else if (g.vAlign == EVAlign::Middle) textTop -= lineH * 0.5f;
                topmost = std::min (topmost, textTop);
            }
            const float bottom = topmost - _paramNamePaddingPx;
            mLabelBounds = IRECT (mWidgetBounds.L, bottom - lineH,
                                  mWidgetBounds.R, bottom);
        } else {
            const float topRef = mStyle.showValue
                                 ? mValueBounds.B
                                 : mWidgetBounds.MH() + GetRadius();
            const float top = topRef + _paramNamePaddingPx;
            mLabelBounds = IRECT (mWidgetBounds.L, top,
                                  mWidgetBounds.R, top + lineH);
        }
    }


    // For a symmetric arc, copy Y/vAlign from the mirror-index Far label so the
    // right column ends up an exact vertical mirror of the left.
    void mirrorNearSideFromFar() {
        const int n = int (_geoms.size());
        for (int i = 0; i < n; ++i) {
            if (_geoms[i].hAlign != EAlign::Near)
                continue;
            const int mirror = n - 1 - i;
            if (mirror < 0 || mirror >= n) continue;
            if (_geoms[mirror].hAlign != EAlign::Far) continue;
            _geoms[i].anchorY = _geoms[mirror].anchorY;
            _geoms[i].vAlign  = _geoms[mirror].vAlign;
        }
    }


    void drawKnobWidget (IGraphics& g) {
        const float widgetRadius = GetRadius();
        const float cx = mWidgetBounds.MW();
        const float cy = mWidgetBounds.MH();
        const IRECT handleBounds = mWidgetBounds.GetCentredInside ((widgetRadius - mTrackToHandleDistance) * 2.f);
        const float angle = mAngle1 + float (GetValue()) * (mAngle2 - mAngle1);
        DrawHandle (g, handleBounds);
        DrawPointer (g, angle, cx, cy, handleBounds.W() / 2.f);
    }


    void drawSegmentTicks (IGraphics& g) const {
        if (_geoms.empty())
            return;

        const float cx = mWidgetBounds.MW();
        const float cy = mWidgetBounds.MH();
        const float r  = GetRadius();
        const IColor col = GetColor (kFR);

        for (const auto& og : _geoms) {
            if (og.halfArcDeg <= 0.f)
                continue;
            g.DrawArc (col, cx, cy, r,
                       og.angleDeg - og.halfArcDeg,
                       og.angleDeg + og.halfArcDeg,
                       &mBlend, _tickThickness);
        }
    }


    void drawOptionLabels (IGraphics& g) const {
        const int n = int (_geoms.size());
        for (int i = 0; i < n; ++i) {
            IText txt   = mStyle.labelText;
            txt.mAlign  = _geoms[i].hAlign;
            txt.mVAlign = _geoms[i].vAlign;
            g.DrawText (txt, _optionNames[i].c_str(),
                        _geoms[i].anchorX, _geoms[i].anchorY, &mBlend);
        }
    }


    EAlign pickHAlign (float angleDeg) const {
        if (std::abs (angleDeg) < _centerEpsilonDeg)
            return EAlign::Center;
        return angleDeg < 0.f ? EAlign::Far : EAlign::Near;
    }


    static EVAlign pickVAlign (float cosAngle) {
        if (cosAngle >  0.5f) return EVAlign::Bottom;
        if (cosAngle < -0.5f) return EVAlign::Top;
        return EVAlign::Middle;
    }


    static IVStyle styleFor (IVStyle s, bool showValue, const char* paramName) {
        s.showLabel = (paramName != nullptr && paramName[0] != '\0');
        s.showValue = showValue;
        return s;
    }


    // Rough monospace char-width estimate used only to size the mRECT expansion margin.
    static constexpr float kCharWidthFactor = 0.62f;


    static IRECT expandedFullBounds (const IRECT& knob,
                                     const IVStyle& style,
                                     const std::vector<std::string>& names,
                                     float labelPaddingPx,
                                     bool paramNameOnTop,
                                     float paramNamePaddingPx) {
        const float fontH = float (style.labelText.mSize);
        const float pad   = (labelPaddingPx < 0 ? fontH : labelPaddingPx);

        std::size_t maxLen = 0;
        for (const auto& s : names)
            maxLen = std::max (maxLen, s.size());

        const float textW   = fontH * kCharWidthFactor * float (maxLen);
        const float hMargin = pad + textW;
        float vMarginTop    = pad + fontH;
        float vMarginBottom = pad + fontH;

        // Always reserve room for the parameter name slot, even if not currently
        // shown, so that toggling setShowParamName at runtime doesn't clip.
        const float extra = paramNamePaddingPx + fontH * 1.2f;
        if (paramNameOnTop) vMarginTop    += extra;
        else                vMarginBottom += extra;

        return IRECT (knob.L - hMargin, knob.T - vMarginTop,
                      knob.R + hMargin, knob.B + vMarginBottom);
    }


    float _knobW;
    float _knobH;
    std::vector<std::string> _optionNames;
    float _tickThickness;
    float _tickWidthNorm;
    float _labelPaddingPx;
    float _centerEpsilonDeg;
    float _valuePaddingPx;
    bool  _paramNameOnTop;
    float _paramNamePaddingPx;
    std::vector<OptionGeometry> _geoms;
};


} // namespace hvoya::ui
