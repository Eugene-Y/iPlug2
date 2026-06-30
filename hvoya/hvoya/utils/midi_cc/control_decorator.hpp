#pragma once

#include <algorithm>
#include <cmath>
#include <string>

#include <IGraphicsPopupMenu.h>

#include <hvoya/utils/log/logger.hpp>
#include <hvoya/utils/types.hpp>
#include "message_tags.hpp"
#include "controllable.hpp"


namespace hvoya::midi_cc {


    using iplug::igraphics::IPopupMenu;
    using iplug::igraphics::IColor;
    using iplug::igraphics::IGraphics;
    using iplug::igraphics::IRECT;


    template <typename C>
    class ControlDecorator : public C, public IControllable {

        private:
            enum MenuLayout {
                mtag_learn = 0,
                mtag_clear,
                mtag_separator,
                mtag_invert,
                mtag_setMin,
                mtag_setMax,
                // Appended; present only when enableCombineModeMenu(true) AND mapped.
                mtag_setDepth,       // Modulate only — arms the set-depth gesture
                mtag_bipolar,        // Modulate only — toggles ± (CC center = base)
                mtag_modeSeparator,
                mtag_absolute,
                mtag_modulate,
            };

            std::string _minDisplay;
            std::string _maxDisplay;
            double      _ccMin = 0.0;  // Absolute CC sweep range (normalized), pushed via setParamMinMax;
            double      _ccMax = 1.0;  // shown as min/max ticks outside the knob when restricted.
            PId_t       _paramId;
            CC_t        _cc;
            int         _midiBaseTag = 0; // VST3: flat tag of the first MIDI CC submenu item
            bool        _combineModeMenu = false; // opt-in (modulatable params only)
            int         _combineMode = 0;         // 0 = Absolute, 1 = Modulate (display stash)

            // Presence dot: a small constant indicator that a CC is bound to this control
            // (opt-in per plugin via enablePresenceDot). Visual properties are explicit/named
            // (declarative UI rule); callers set them rather than decoding draw internals.
            bool        _presenceDot       = false;
            IColor      _presenceDotColor  = IColor (200, 150, 150, 150);
            IColor      _presenceDotAccentColor = IColor (255, 95, 55, 200); // mouse-down fill (overridden by the plugin's accent)
            IColor      _presenceDotHoverColor   = IColor (255, 235, 235, 235); // mouse-over fill: bright, font-like
            IColor      _presenceDotOutlineColor = IColor (255,  60,  60,  60); // mouse-over ring: dark bg gray
            float       _presenceDotRadius = 2.5f;
            float       _presenceDotInset  = 3.5f; // from the control's bottom-right corner
            float       _presenceDotStroke = 1.0f;
            float       _presenceDotOffsetX = 0.f; // nudge, in dot-diameter units (unusual controls)
            float       _presenceDotOffsetY = 0.f;
            bool        _dotHover = false; // mouse over the dot → bright fill + dark outline
            bool        _dotDown  = false; // mouse pressed on the dot → filled with the accent

            // Relative-CC modulation arc (drawn only in Modulate, knob controls). Source-colored.
            double      _modDepth     = 0.0;   // signed normalized extent from base (pushed by host)
            bool        _modBipolar   = false; // arc spans both sides of base (± direction)
            double      _modLiveNorm  = NAN;   // live modulated position (normalized); NAN = no marker
            IColor      _modLiveDotColor;      // override; else derived (knob contour / slider font)
            bool        _hasLiveDotColor = false;
            bool        _depthGesture = false; // this param authors depth via the drag gesture (non-freq)
            float       _arcRadiusFrac = 0.8f; // knob arc radius as a fraction of the knob radius —
                                               // INSIDE the widget so neighbours can't overpaint it
            IColor      _arcColor     = IColor (220, 110, 170, 230);
            float       _arcThickness = 2.0f;

            // set-depth gesture (Modulate): armed via the menu, captures the next drag. The extent is
            // accumulated from drag deltas (not GetValue), so a per-idle writer (e.g. a note glider
            // push) can't corrupt it mid-drag and Shift-fine can toggle without a jump.
            bool        _armDepth      = false;
            bool        _gestureActive = false;
            bool        _learning      = false; // this control is the active MIDI-learn target → blink the dot
            bool        _blinkOn       = false; // current blink-animation state (armed OR learning)
            double      _gestureBaseNorm = 0.0;
            double      _gestureExtent   = 0.0;  // signed normalized extent from base, accumulated
            static constexpr float kGestureRange = 200.f; // px of vertical drag for full [0,1]
            static constexpr double kGestureFine = 0.1;   // Shift-held drag multiplier (10× finer)
            static constexpr int   kArmBlinkMs   = 500;
            static constexpr double kRangeEps    = 1e-4;  // Absolute range counts as "restricted" beyond this

            // Freq depth readout: opt-in for params whose depth is musical pitch (cutoffs). During the
            // set-depth drag the live oct/semi/cent delta floats near the cursor in the shared
            // IBubbleControl (attached top-of-stack by the plugin), so it isn't clipped to mRECT.
            bool        _freqDepthReadout = false;

            // not safe if the Delegate params are not yet initialized
            auto getParam() {
                assert (_paramId != uninit::pid);
                return this->GetDelegate()->GetParam (_paramId);
            }
            
            void updateDisplay (std::string& display) {
                WDL_String s;
                getParam()->GetDisplay (s);
                display = s.Get();
            }
            
            
            void updateDisplay (std::string& display, double normVal) {
                assert (normVal >= 0 && normVal <= 1);
                WDL_String s;
                getParam()->GetDisplay (normVal, true, s);
                display = s.Get();
            }


            void setMinMaxDisplaysToFullRange() {
                auto pP = getParam();
                WDL_String s;
                pP->GetDisplay (pP->GetMin(), false, s);
                _minDisplay = s.Get();
                pP->GetDisplay (pP->GetMax(), false, s);
                _maxDisplay = s.Get();
            }

        
        public:
        
            template <typename... Args>
            ControlDecorator (Args&&... args)
                : C (std::forward <Args> (args)...) {
                    _paramId = this->GetParamIdx();
                    _cc = uninit::cc;
                }
                
                
            void setCCNumber (CC_t cc) override {
                _cc = cc;
                setMinMaxDisplaysToFullRange();
                this->SetDirty (false); // dot appears once a CC binds
            }

            // Opt-in (driven by the mediator for modulatable params): adds an
            // Absolute/Modulate radio pair to the MIDI-CC submenu when mapped.
            void enableCombineModeMenu (bool on) { _combineModeMenu = on; }
            // Opt-in: this param authors its depth via the drag gesture (every modulatable param —
            // freqs convert the captured drag to an octave depth host-side; see Gneiss::setCCModDepth).
            void enableDepthGesture (bool on) { _depthGesture = on; }
            // Opt-in: this param's depth is musical pitch, so the drag shows a live oct/semi/cent readout.
            void enableFreqDepthReadout (bool on) { _freqDepthReadout = on; }

            // Presence dot — declarative, named visual setters (opt-in per plugin).
            void enablePresenceDot   (bool on)          { _presenceDot = on; }
            void setPresenceDotColor (const IColor& c)  { _presenceDotColor = c; }
            void setPresenceDotAccentColor  (const IColor& c) { _presenceDotAccentColor = c; }
            void setPresenceDotHoverColor   (const IColor& c) { _presenceDotHoverColor = c; }
            void setPresenceDotOutlineColor (const IColor& c) { _presenceDotOutlineColor = c; }
            // Nudge the dot in dot-diameter units (declared on IControllable for per-control tweaks).
            void setPresenceDotOffset (float dxDiameters, float dyDiameters) override {
                _presenceDotOffsetX = dxDiameters;
                _presenceDotOffsetY = dyDiameters;
            }
            void setPresenceDotRadius (float r)         { _presenceDotRadius = r; }
            void setPresenceDotInset  (float i)         { _presenceDotInset = i; }
            void setPresenceDotStroke (float w)         { _presenceDotStroke = w; }

            // Modulation arc — declarative, named visual setters.
            void setModArcColor      (const IColor& c)  { _arcColor = c; }
            void setModArcThickness  (float w)          { _arcThickness = w; }
            void setModArcRadiusFrac (float f)          { _arcRadiusFrac = f; }

            // Host pushes the current relative-CC depth (signed normalized extent from base).
            void setModDepth (double signedNormExtent) override {
                if (_modDepth != signedNormExtent) {
                    _modDepth = signedNormExtent;
                    this->SetDirty (false);
                }
            }
            void setModBipolar (bool bipolar) override {
                if (_modBipolar != bipolar) {
                    _modBipolar = bipolar;
                    this->SetDirty (false);
                }
            }
            void setModLiveValue (double normVal) override {
                if (_modLiveNorm != normVal) {   // NAN != x is always true → first finite push repaints
                    _modLiveNorm = normVal;
                    this->SetDirty (false);
                }
            }
            // Explicit override for the live dot's color (declarative). Default: the knob's contour
            // color, or a slider's font color — derived in drawModArc from the control's own style.
            void setModLiveDotColor (const IColor& c) { _modLiveDotColor = c; _hasLiveDotColor = true; }
            void setModeDisplay (int mode) override {
                if (_combineMode != mode) {
                    _combineMode = mode;
                    this->SetDirty (false);
                }
            }
            bool isAuthoringDepth() const override { return _gestureActive; }

            void clearCCNumber() override {
                clearCC();
                this->SetDirty (false); // dot disappears once the mapping is cleared
            }


            void setParamMinMax (double normMin, double normMax) override {
                auto pP = getParam();
                assert (pP);
                updateDisplay (_minDisplay, normMin);
                updateDisplay (_maxDisplay, normMax);
                if (_ccMin != normMin || _ccMax != normMax) {
                    _ccMin = normMin; _ccMax = normMax;
                    this->SetDirty (false);   // redraw the Absolute range ticks
                }
            }


            // Center of the presence dot. Anchored to the widget (e.g. a knob's circle) when the
            // control exposes it — lower-right corner, clear of any centred value text; else mRECT.
            void presenceDotCenter (float& cx, float& cy) const {
                IRECT anchor = this->mRECT;
                // Use the widget rect when the control exposes a real one. Some controls inherit
                // GetWidgetBounds() but never set it (it stays empty at 0,0,0,0) — anchoring the dot
                // there would place it near the window origin and OnResize would then stretch the hit
                // rect across the top-left of the UI (stealing clicks from the tabs). Fall back to mRECT.
                if constexpr (requires (const C& c) { c.GetWidgetBounds(); }) {
                    const IRECT wb = this->GetWidgetBounds();
                    if (wb.W() > 0.f && wb.H() > 0.f) anchor = wb;
                }
                cx = anchor.R - _presenceDotInset - _presenceDotRadius + _presenceDotOffsetX * 2.f * _presenceDotRadius;
                cy = anchor.B - _presenceDotInset - _presenceDotRadius + _presenceDotOffsetY * 2.f * _presenceDotRadius;
            }

            // Some controls shrink their hit area below mRECT (e.g. WrapKnobControl targets just the knob
            // circle, not the label band). A presence dot anchored to the widget — especially once nudged
            // by setPresenceDotOffset — can then fall OUTSIDE that target rect, so the cursor never
            // dispatches a mouseover to this control when it's over the drawn dot. Extend the hit area to
            // cover the dot so it's actually hoverable/clickable.
            void OnResize() override {
                C::OnResize();
                if (_presenceDot) {
                    float cx, cy;
                    presenceDotCenter (cx, cy);
                    const float r = _presenceDotRadius + 4.f;   // matches hitPresenceDot's slack
                    this->SetTargetRECT (this->mTargetRECT.Union (IRECT (cx - r, cy - r, cx + r, cy + r)));
                }
            }

            bool hitPresenceDot (float x, float y) const {
                float cx, cy;
                presenceDotCenter (cx, cy);
                const float hit = _presenceDotRadius + 4.f;
                return std::abs (x - cx) <= hit && std::abs (y - cy) <= hit;
            }

            void Draw (IGraphics& g) override {
                C::Draw (g);
                drawModArc (g);
                // Drawn for a bound CC, or while this control is the MIDI-learn target (a blinking
                // dot is the "I'm listening" signal even before a CC binds).
                if (_presenceDot && (_cc != uninit::cc || _learning)) {
                    // The dot blinks while the set-depth gesture is armed OR this control is the
                    // learn target (off on each cycle's second half). The animation is free-running,
                    // so progress grows unbounded → take the fractional phase.
                    if (_blinkOn && std::fmod (this->GetAnimationProgress(), 1.0) >= 0.5) return;
                    float cx, cy;
                    presenceDotCenter (cx, cy);
                    // Pressed → accent fill; hovered → bright fill + dark ring; else a hollow ring.
                    // The active states grow by the stroke width so the filled dot never looks shrunk
                    // next to the hollow ring it replaces.
                    const float rActive = _presenceDotRadius + _presenceDotStroke;
                    if (_dotDown)
                        g.FillCircle (_presenceDotAccentColor, cx, cy, rActive, &this->mBlend);
                    else if (_dotHover) {
                        g.FillCircle (_presenceDotHoverColor,   cx, cy, rActive, &this->mBlend);
                        g.DrawCircle (_presenceDotOutlineColor, cx, cy, rActive, &this->mBlend, _presenceDotStroke);
                    }
                    else
                        g.DrawCircle (_presenceDotColor, cx, cy, _presenceDotRadius, &this->mBlend, _presenceDotStroke);
                }
            }

            // A left-click on the presence dot toggles Absolute/Modulate (modulatable + mapped only),
            // a shortcut for the right-click menu's radio pair. Also drives the set-depth gesture.
            void OnMouseDown (float x, float y, const iplug::igraphics::IMouseMod& mod) override {
                if (_armDepth && !mod.R) {
                    if (hitPresenceDot (x, y)) {           // dot-click while armed → cancel
                        _dotDown = true;
                        setArmed (false);
                        return;
                    }
                    _gestureActive   = true;               // otherwise this drag authors the depth
                    _gestureBaseNorm = this->GetValue();
                    _gestureExtent   = 0.0;
                    setArmed (false);
                    return;
                }
                if (_presenceDot && _combineModeMenu && _cc != uninit::cc && !mod.R && hitPresenceDot (x, y)) {
                    _dotDown = true;                       // accent fill until release
                    _combineMode = (_combineMode == 0) ? 1 : 0;
                    sendMidiCCAction (_combineMode == 0 ? MessageTags::mtag_set_cc_absolute
                                                       : MessageTags::mtag_set_cc_modulate);
                    this->SetDirty (false);
                    return;
                }
                C::OnMouseDown (x, y, mod);
            }

            void OnMouseDrag (float x, float y, float dX, float dY, const iplug::igraphics::IMouseMod& mod) override {
                if (_gestureActive) {
                    // Accumulate the extent from drag deltas (up = increase). Shift = fine, and toggling
                    // it mid-drag stays continuous because we never re-derive from absolute Y.
                    _gestureExtent += double (-dY) / kGestureRange * (mod.S ? kGestureFine : 1.0);
                    _gestureExtent  = std::clamp (_gestureExtent, -1.0, 1.0);
                    this->SetValue (std::clamp (_gestureBaseNorm + _gestureExtent, 0.0, 1.0)); // visual only
                    this->SetDirty (false);
                    if (_freqDepthReadout)   // float the live oct/semi/cent delta in the bubble overlay
                        this->GetUI()->ShowBubbleControl (this, x, y, formatPitchDepth (gestureOctaves()).c_str());
                    return;
                }
                C::OnMouseDrag (x, y, dX, dY, mod);
            }

            void OnMouseUp (float x, float y, const iplug::igraphics::IMouseMod& mod) override {
                if (_dotDown) { _dotDown = false; this->SetDirty (false); } // release the press accent
                if (_gestureActive) {
                    // Commit the accumulated extent (held in our own member, immune to a per-idle
                    // writer like a note-mode glider push corrupting GetValue mid-drag).
                    const double v = std::clamp (_gestureBaseNorm + _gestureExtent, 0.0, 1.0);
                    const float delta = float (v - _gestureBaseNorm);
                    SetDepthMsg msg { this->GetParamIdx(), delta };
                    this->GetDelegate()->SendArbitraryMsgFromUI (
                        MessageTags::mtag_set_cc_depth, iplug::kNoTag, sizeof (SetDepthMsg), &msg);
                    this->SetValue (_gestureBaseNorm);   // spring back to base
                    this->SetDirty (false);
                    _gestureActive = false;
                    return;
                }
                C::OnMouseUp (x, y, mod);
            }

            // Hovering the presence dot fills it with its own color; while there, the wrapped control's
            // own mouseover is suppressed so the small dot reads clearly (BACKLOG).
            void OnMouseOver (float x, float y, const iplug::igraphics::IMouseMod& mod) override {
                const bool overDot = _presenceDot && _cc != uninit::cc && hitPresenceDot (x, y);
                if (overDot != _dotHover) { _dotHover = overDot; this->SetDirty (false); }
                if (overDot) {
                    if (this->GetMouseIsOver()) C::OnMouseOut();   // drop the control's hover highlight
                } else {
                    C::OnMouseOver (x, y, mod);
                }
            }

            void OnMouseOut() override {
                if (_dotHover) { _dotHover = false; this->SetDirty (false); }
                C::OnMouseOut();
            }

            // Arm/disarm the set-depth gesture; drives the dot's blink (see updateBlink).
            void setArmed (bool on) { _armDepth = on; updateBlink(); }

            // Pushed by the mediator: this control is the active MIDI-learn target, so blink the dot
            // as an "I'm listening" signal (shares the gesture-arm blink).
            void setLearning (bool on) override { if (_learning != on) { _learning = on; updateBlink(); } }

            // The dot blinks while the set-depth gesture is armed OR this control is the learn target.
            // Either source keeps a free-running blink animation alive (it just keeps the control dirty);
            // Draw derives the on/off phase from its progress. Re-arm only on an actual state change so an
            // unrelated repaint can't reset the blink phase.
            void updateBlink() {
                const bool on = _armDepth || _learning;
                if (on != _blinkOn) {
                    _blinkOn = on;
                    if (on) this->SetAnimation ([](iplug::igraphics::IControl* c) { c->SetDirty (false); }, kArmBlinkMs);
                    else    this->SetAnimation (iplug::igraphics::IAnimationFunction (nullptr));
                }
                this->SetDirty (false);
            }

            
            void CreateContextMenu (IPopupMenu& contextMenu) override {
                IPopupMenu* subMenu = new IPopupMenu ("MIDI CC");
                
                const bool mapped = _cc != uninit::cc;

                subMenu->AddItem ("Learn");
                
                std::string s = "Clear";
                if (mapped) s += " (" + std::to_string (_cc) + ")";
                subMenu->AddItem (s.c_str())->SetEnabled (mapped);
                
                subMenu->AddSeparator();
                subMenu->AddItem ("Invert")->SetEnabled (mapped);
                
                // In Modulate the absolute Set Min/Max are inert (mapper min/max are ignored) →
                // they gray out and the relative Set Depth takes over.
                const bool modulate = _combineModeMenu && _combineMode == 1;

                s = "Set Min";
                if (mapped) s += " (" + _minDisplay + ")";
                subMenu->AddItem (s.c_str())->SetEnabled (mapped && !modulate);

                s = "Set Max";
                if (mapped) s += " (" + _maxDisplay + ")";
                subMenu->AddItem (s.c_str())->SetEnabled (mapped && !modulate);

                if (_combineModeMenu && mapped) {
                    // Set Depth authors the relative depth by drag (every modulatable param, freqs
                    // included — they read out oct/semi/cent live near the cursor).
                    subMenu->AddItem ("Set Depth")->SetEnabled (modulate && _depthGesture);
                    // ± : CC center = base, swings both ways (vs unipolar signed). Checkable toggle.
                    auto* bip = subMenu->AddItem ("Bipolar");
                    bip->SetChecked (_modBipolar);
                    bip->SetEnabled (modulate);
                    subMenu->AddSeparator();
                    subMenu->AddItem ("Absolute")->SetChecked (_combineMode == 0);
                    subMenu->AddItem ("Modulate")->SetChecked (_combineMode == 1);
                }

				contextMenu.AddItem (getParam()->GetName(), -1, IPopupMenu::Item::Flags::kTitle);
				contextMenu.AddSeparator();
                // VST3 flattens submenus into sequential tags; record the offset so
                // OnContextSelection can normalize the tag back to the local submenu index.
                // offset = contextMenu items before us (2: title + sep) + 1 (group-start tag)
                _midiBaseTag = contextMenu.NItems() + 1;
                contextMenu.AddItem ("MIDI CC", subMenu);
                
                C::CreateContextMenu (contextMenu);
            }

            
            void OnContextSelection (int itemSelected) override {
#if defined VST3_API || defined VST3C_API
                // In VST3 the context menu delivers a flat sequential tag, not a
                // per-submenu index. Normalise back to the local submenu index so
                // tryProcessMidiCCMenuSelection sees 0-based values.
                const int localIdx = itemSelected - _midiBaseTag;
#else
                const int localIdx = itemSelected;
#endif
                if (tryProcessMidiCCMenuSelection (localIdx))
                    return;
                C::OnContextSelection (itemSelected);
            }
            
        private:

            void clearCC() {
                _cc = uninit::cc;
                _minDisplay.clear();
                _maxDisplay.clear();
                _ccMin = 0.0; _ccMax = 1.0;   // drop the Absolute range ticks
                _combineMode = 0;        // unmapped → back to the Absolute default display
                if (_armDepth) setArmed (false);
            }

            // Relative-CC modulation arc — knob controls only (uses the knob's geometry). Drawn in
            // Modulate mode from base toward base+depth; while armed, a faint bipolar preview.
            // Geometry is heuristic (default knob angle range) pending the control geometry descriptor.
            // True when the Absolute CC sweep covers less than the full param span → worth a hint.
            bool ccRangeRestricted() const {
                return std::min (_ccMin, _ccMax) > kRangeEps || std::max (_ccMin, _ccMax) < 1.0 - kRangeEps;
            }

            void drawModArc (IGraphics& g) {
                // CC hints only for modulatable, mapped params. Modulate → the depth arc/strip + live
                // dot; Absolute → min/max range ticks (the CC's reachable sweep), only when restricted.
                if (!_combineModeMenu || _cc == uninit::cc) return;
                const bool modulate = _combineMode == 1;
                const double base = this->GetValue();
                if constexpr (requires (C& c) { c.GetRadius(); }) {
                    // Knobs (always an IVKnobControl-derivative here): arc around the widget circle.
                    // Use the control's OWN angle range so the arc tracks the pointer exactly — the
                    // sat-type wrap knob rebakes mAngle1/mAngle2, so a hardcoded ±135 would be wrong.
                    const IRECT wb = this->GetWidgetBounds();
                    const float cx = wb.MW(), cy = wb.MH();
                    const float r  = this->GetRadius() * _arcRadiusFrac;   // inside the widget
                    const float a1 = this->mAngle1, a2 = this->mAngle2;
                    auto angleOf = [a1, a2](double v) { return a1 + float (std::clamp (v, 0.0, 1.0)) * (a2 - a1); };
                    if (!modulate) {   // Absolute: fixed-size min/max ticks just OUTSIDE the knob ring
                        if (ccRangeRestricted()) {
                            const float r0  = this->GetRadius();              // start at the knob's edge
                            const float len = _presenceDotRadius * 2.f;        // length = MIDI dot diameter
                            const float w   = this->mPointerThickness;         // width = knob handle thickness
                            // DrawRadialLine is the knob's own pointer primitive → identical geometry.
                            g.DrawRadialLine (_arcColor, cx, cy, angleOf (_ccMin), r0, r0 + len, &this->mBlend, w);
                            g.DrawRadialLine (_arcColor, cx, cy, angleOf (_ccMax), r0, r0 + len, &this->mBlend, w);
                        }
                        return;
                    }
                    const float aBase = angleOf (base);
                    if (_gestureActive) {
                        // Live preview while dragging: arc from the gesture's start to the current
                        // position (the existing committed arc is suppressed — this IS the new depth).
                        const float aStart = angleOf (_gestureBaseNorm);
                        const float aCur   = angleOf (_gestureBaseNorm + _gestureExtent);
                        g.DrawArc (_arcColor, cx, cy, r, std::min (aStart, aCur), std::max (aStart, aCur),
                                   &this->mBlend, _arcThickness);
                        return;
                    }
                    if (_armDepth) {
                        // A symmetric "drag either way" hint around base — symmetric in ANGLE (not value),
                        // clamped only at the knob's physical ends, so it reads bidirectional anywhere.
                        const float halfDeg = std::min (35.f, std::abs (a2 - a1) * 0.2f);
                        IColor faint = _arcColor;
                        faint.A = _arcColor.A / 2;
                        g.DrawArc (faint, cx, cy, r, std::max (std::min (a1, a2), aBase - halfDeg),
                                   std::min (std::max (a1, a2), aBase + halfDeg), &this->mBlend, _arcThickness);
                        return;
                    }
                    if (_modDepth == 0.0) return;
                    // The live modulator dot sits on the arc at the value the CC currently produces;
                    // a small opaque circle (diameter = arc thickness) in the knob's contour color.
                    auto drawLiveDot = [&] {
                        if (std::isnan (_modLiveNorm)) return;
                        const float rad = iplug::igraphics::DegToRad (angleOf (_modLiveNorm) - 90.f);
                        IColor dc;
                        if (_hasLiveDotColor) dc = _modLiveDotColor;
                        else if constexpr (requires (C& c) { c.GetColor (iplug::igraphics::kFR); })
                            dc = this->GetColor (iplug::igraphics::kFR);
                        else dc = _arcColor;
                        g.FillCircle (dc, cx + r * std::cos (rad), cy + r * std::sin (rad),
                                      _arcThickness * 0.5f, &this->mBlend);
                    };
                    if (_modBipolar) {   // ± : symmetric span around base
                        g.DrawArc (_arcColor, cx, cy, r, angleOf (base - std::abs (_modDepth)),
                                   angleOf (base + std::abs (_modDepth)), &this->mBlend, _arcThickness);
                        drawLiveDot();
                        return;
                    }
                    const float aEnd = angleOf (base + _modDepth);
                    g.DrawArc (_arcColor, cx, cy, r, std::min (aBase, aEnd), std::max (aBase, aEnd), &this->mBlend, _arcThickness);
                    drawLiveDot();
                } else {
                    // Sliders (and other non-knob controls): a horizontal strip along the bottom edge,
                    // same thickness as the arc, spanning base→base+depth. (Heuristic track = mRECT for
                    // now; refine with the geometry descriptor — see docs/modulation.md Polish.)
                    // Keep the strip a couple px inside mRECT (bottom + sides) so a neighbouring
                    // control's repaint can't overpaint it (same reason knob arcs sit inside).
                    const float y    = this->mRECT.B - _arcThickness - 2.f;
                    const float padX = 2.f;
                    auto xOf = [&](double v) {
                        return this->mRECT.L + padX + float (std::clamp (v, 0.0, 1.0)) * (this->mRECT.W() - 2 * padX);
                    };
                    if (!modulate) {   // Absolute: fixed vertical min/max ticks on the track (no contour)
                        if (ccRangeRestricted()) {
                            const float len = _presenceDotRadius * 2.f;   // length = MIDI dot diameter
                            auto tick = [&](double norm) {
                                const float x = xOf (norm);
                                g.DrawLine (_arcColor, x, y, x, y - len, &this->mBlend, _arcThickness);
                            };
                            tick (_ccMin); tick (_ccMax);
                        }
                        return;
                    }
                    if (_gestureActive) {
                        // Live preview while dragging: line from the gesture's start to the current
                        // position (suppresses the existing committed strip).
                        g.DrawLine (_arcColor, xOf (_gestureBaseNorm), y,
                                    xOf (_gestureBaseNorm + _gestureExtent), y, &this->mBlend, _arcThickness);
                        return;
                    }
                    if (_armDepth) {
                        IColor faint = _arcColor;
                        faint.A = _arcColor.A / 2;
                        g.DrawLine (faint, xOf (base - 0.25), y, xOf (base + 0.25), y, &this->mBlend, _arcThickness);
                        return;
                    }
                    if (_modDepth == 0.0) return;
                    const double lo = _modBipolar ? base - std::abs (_modDepth) : base;
                    const double hi = _modBipolar ? base + std::abs (_modDepth) : base + _modDepth;
                    g.DrawLine (_arcColor, xOf (lo), y, xOf (hi), y, &this->mBlend, _arcThickness);
                    // Live modulator dot on the strip — a slider has no contour, so use its font color
                    // (exposed by the control; falls back to mText if it doesn't advertise one).
                    if (!std::isnan (_modLiveNorm)) {
                        IColor dc;
                        if (_hasLiveDotColor) dc = _modLiveDotColor;
                        else if constexpr (requires (const C& c) { c.labelColor(); }) dc = this->labelColor();
                        else dc = this->mText.mFGColor;
                        g.FillCircle (dc, xOf (_modLiveNorm), y, _arcThickness * 0.5f, &this->mBlend);
                    }
                }
            }

            // The current set-depth drag as a pitch ratio (dragged value vs base) in octaves — matches
            // Gneiss::setCCModDepth, so the readout is the depth that will be committed.
            double gestureOctaves() {
                auto p = getParam();
                if (!p) return 0.0;
                const double baseV = p->FromNormalized (_gestureBaseNorm);
                const double curV  = p->FromNormalized (std::clamp (_gestureBaseNorm + _gestureExtent, 0.0, 1.0));
                return (baseV > 0 && curV > 0) ? std::log2 (curV / baseV) : 0.0;
            }

            // Octaves → a compact signed "+1oct 2st 35ct" label (leading zero units dropped).
            static std::string formatPitchDepth (double octaves) {
                long cents = std::lround (std::abs (octaves) * 1200.0);
                if (cents == 0) return "0";
                const long oct  = cents / 1200; cents %= 1200;
                const long semi = cents / 100;  const long ct = cents % 100;
                std::string s = octaves < 0 ? "-" : "+";
                if (oct)         s += std::to_string (oct)  + "oct ";
                if (oct || semi) s += std::to_string (semi) + "st ";
                s += std::to_string (ct) + "ct";
                return s;
            }

            bool tryProcessMidiCCMenuSelection (int itemSelected) {
                if (itemSelected == mtag_separator || itemSelected == mtag_modeSeparator
                    || itemSelected < mtag_learn || itemSelected > mtag_modulate)
                    return false;
                    
                typedef midi_cc::MessageTags MT;
                MT action;

                // Set Depth arms a one-shot UI gesture (no processor message yet) — the captured
                // drag commits the depth on mouse-up. Re-selecting while armed cancels.
                if (itemSelected == mtag_setDepth) {
                    setArmed (!_armDepth);
                    return true;
                }

                // Bipolar toggles the ± flag; flip the local display so the arc updates immediately,
                // and let the host store it (Gneiss::toggleCCBipolar).
                if (itemSelected == mtag_bipolar) {
                    _modBipolar = !_modBipolar;
                    this->SetDirty (false);
                    sendMidiCCAction (MT::mtag_set_cc_bipolar);
                    return true;
                }

                switch (itemSelected) {
                    case mtag_learn:  
                        action = MT::mtag_learn_cc;  
                        clearCC();
                        break;
                        
                    case mtag_clear:  
                        action = MT::mtag_clear_cc;  
                        clearCC();
                        break;
                    
                    case mtag_invert:
                        if (_combineModeMenu && _combineMode == 1) {   // Modulate → flip the depth direction
                            sendMidiCCAction (MT::mtag_invert_cc_depth);
                            return true;
                        }
                        action = MT::mtag_invert_range;
                        std::swap (_minDisplay, _maxDisplay);
                        std::swap (_ccMin, _ccMax);                    // keep the range ticks in sync
                        this->SetDirty (false);
                        break;

                    case mtag_setMin:
                        action = MT::mtag_set_min;
                        updateDisplay (_minDisplay);
                        _ccMin = getParam()->GetNormalized();          // Set Min captures the current value →
                        this->SetDirty (false);                        // redraw the range tick immediately
                        break;

                    case mtag_setMax:
                        action = MT::mtag_set_max;
                        updateDisplay (_maxDisplay);
                        _ccMax = getParam()->GetNormalized();
                        this->SetDirty (false);
                        break;

                    case mtag_absolute:
                        action = MT::mtag_set_cc_absolute;
                        _combineMode = 0;
                        break;

                    case mtag_modulate:
                        action = MT::mtag_set_cc_modulate;
                        _combineMode = 1;
                        break;

                    default: return false;
                }
                
                sendMidiCCAction (action);
                return true;
            }

            // Routes a MIDI-CC menu/dot action to the processor. SendArbitraryMsgFromUI works in all
            // formats (incl. VST3, where GetDelegate() is the controller, not the processor).
            void sendMidiCCAction (int action) {
                const PId_t pId = this->GetParamIdx();
                LOGD << "MIDI CC: param " << pId << ": action " << action;
                this->GetDelegate()->SendArbitraryMsgFromUI (MessageTags::mtag_listen_to_pid, iplug::kNoTag, sizeof (PId_t), &pId);
                this->GetDelegate()->SendArbitraryMsgFromUI (action, iplug::kNoTag, sizeof (PId_t), &pId);
            }
    };


    template <typename C>
    using Learnable = ControlDecorator <C>;

}
