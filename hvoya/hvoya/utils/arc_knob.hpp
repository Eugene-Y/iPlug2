#pragma once

#include <IControls.h>

namespace hvoya {
    
    class ArcKnob : public IVKnobControl {
        public:
            ArcKnob (const IRECT& bounds, int paramIdx,
                     const char* label = "",
                     const IVStyle& style = DEFAULT_STYLE,
					 float thicknessNearAnchor = 25.f, float thicknessFarFromAnchor = 2.f,
					 float minArcLen = 2.f,
                     float angleMin = -135, float angleMax = 135, float aAnchor = -135,
					 bool symmetric = false,
					 bool drawRestOfTheArc = true,
					 float centerOfMassAdjustment = 1.f,
					 bool valueIsEditable = true,
					 bool valueInWidget = false,
                     double gearing = DEFAULT_GEARING) :

                     IVKnobControl (bounds, paramIdx, label, style, valueIsEditable, valueInWidget,
									angleMin, angleMax, aAnchor, EDirection::Vertical, gearing),

					 _thicknessFarFromAnchor (thicknessFarFromAnchor),
					 _thicknessNearAnchor (thicknessNearAnchor),
					 _minArcLenPx (minArcLen),
					 _symmetric (symmetric),
					 _drawRestOfTheArc (drawRestOfTheArc),
					 _centerOfMassAdjustment (centerOfMassAdjustment),
					 _needAdjustWidget (true),
					 _backendArcFix (true),
					 _arcFixLineThickness (2.5f) {
						 assert (angleMin < angleMax);
						 assert (angleMin <= aAnchor);
						 assert (aAnchor <= angleMax);
					 }


			void Draw (IGraphics& g) override {
				IVKnobControl::DrawBackground (g, mRECT);
				IVKnobControl::DrawLabel (g);
				DrawWidget (g);
				IVKnobControl::DrawValue (g, mValueMouseOver);
			}


            void DrawWidget (IGraphics& g) override {
				const bool debug = 0;

				if (_needAdjustWidget) {
					adjustWidgetRect();
					_needAdjustWidget = false;
				}
				const auto& wb = _adjustedWidgetRect;
				if (debug) {
					g.DrawRect (COLOR_ORANGE, mWidgetBounds);
					g.DrawRect (COLOR_YELLOW, wb);
				}
				const float widgetRadius = std::min (wb.H(), wb.W()) / 2.f;

				const float cx = wb.MW();
				const float cy = wb.MH();

				const float handleAngle = mAngle1 + GetValue() * (mAngle2 - mAngle1);

				float a1, a2, distFromAnchorNorm;

				if (_symmetric) {
					const float halfSpread = GetValue() * (mAngle2 - mAngle1) / 2.f;
					a1 = std::max (mAngle1, mAnchorAngle - halfSpread);
					a2 = std::min (mAngle2, mAnchorAngle + halfSpread);
					distFromAnchorNorm = GetValue();
				} else {
					a1 = handleAngle >= mAnchorAngle ? mAnchorAngle
													  : mAnchorAngle - (mAnchorAngle - handleAngle);
					a2 = handleAngle >= mAnchorAngle ? handleAngle
													  : mAnchorAngle;
					distFromAnchorNorm = handleAngle == mAnchorAngle ?
						  0.f
						: handleAngle > mAnchorAngle ?
						(mAngle2 != mAnchorAngle) ? (handleAngle - mAnchorAngle) / (mAngle2 - mAnchorAngle) : 1.f
					  : (mAnchorAngle != mAngle1) ? (mAnchorAngle - handleAngle) / (mAnchorAngle - mAngle1) : 1.f;
				}

				const float nearThick = std::min <float> (_thicknessNearAnchor, widgetRadius - 0.5f);
				const float farThick = std::min <float> (_thicknessFarFromAnchor, widgetRadius - 0.5f);
				// -0.5 is a workaround for strange arc center drawing with widgetRadius thickness
				const float t = (1.f - distFromAnchorNorm) *  nearThick + distFromAnchorNorm * farThick;

				const float radius = widgetRadius - t / 2.f;

				if (_drawRestOfTheArc) {
					const auto& col = GetColor (kSH);
					g.DrawArc (col, cx, cy, radius, mAngle1, mAngle2, &mBlend, t);
					if (_backendArcFix) {
						g.DrawRadialLine (col,
										  cx, cy, mAngle1,
										  widgetRadius - t, widgetRadius,
										  &mBlend, _arcFixLineThickness);
						g.DrawRadialLine (col,
										  cx, cy, mAngle2,
										  widgetRadius - t, widgetRadius,
										  &mBlend, _arcFixLineThickness);
					}
				}

                if (a1 != a2)
					g.DrawArc (GetColor (kX1), cx, cy, radius, a1, a2, &mBlend, t);

				// draw thin radial line to imitate the shortest arc with near-anchor thickness.
				// this keeps angle computation above clearer, and resulting image more accurate.
				if (_minArcLenPx > 0.f) {
					const auto& col = GetColor (kFR);
					if (_backendArcFix)
						g.DrawRadialLine (col,
										  cx, cy, mAnchorAngle,
										  widgetRadius - t, widgetRadius,
										  &mBlend, _arcFixLineThickness);
					const float lw = _backendArcFix ?
						std::max (_minArcLenPx, _arcFixLineThickness)
					  :_minArcLenPx;
					g.DrawRadialLine (debug ? COLOR_RED : col,
									  cx, cy, _symmetric ? mAnchorAngle : handleAngle,
									  widgetRadius - t, widgetRadius,
									  &mBlend, lw);
				}

				if (debug) g.DrawCircle (COLOR_ORANGE, cx, cy, radius);
            }


			void setThicknessFarFromAnchor (float t) {
				assert (t >= 0.f);
				_thicknessFarFromAnchor = t;
			}

			void setThicknessNearAnchor (float t) {
				assert (t >= 0.f);
				_thicknessNearAnchor = t;
			}

			void setMinArcLenPx (float len) {
				assert (len >- 0.f);
				_minArcLenPx = len;
			}

			void setSymmetric (bool s) {
				_symmetric = s;
			}

			void setDrawRestOfTheArc (bool d) {
				_drawRestOfTheArc = d;
			}

			void setBackendArcFix (bool f, float lineThickness = 2.5f) {
				_backendArcFix = f;
				_arcFixLineThickness = lineThickness;
			}

			void setValueIsEditable (bool e) {
				DisablePrompt (!e);
			}

			void setValueInWidget (bool v) { // TODO fix value rect
				mValueInWidget = v;
			}

			void OnResize() override {
				IVKnobControl::OnResize();
				_needAdjustWidget = true;
			}

		protected:

			bool _symmetric;
			bool _drawRestOfTheArc;
			float _minArcLenPx; // NB: default arc drawing backend is not perfect,
								// with min <= 1 there might be a noticeable gap
			float _thicknessFarFromAnchor;
			float _thicknessNearAnchor;
			float _centerOfMassAdjustment; // normalized to widget radius.
										   // 0 = no adjustment
			IRECT _adjustedWidgetRect;
			bool _needAdjustWidget;

			bool _backendArcFix; // NanoVG (default on mac) has bugs in arc drawing
			float _arcFixLineThickness;

			void adjustWidgetRect() {
				_adjustedWidgetRect = mWidgetBounds;
				if (_centerOfMassAdjustment == 0.f)
					return;

				// TODO add horizontal adjustment
				// TODO count max thickness in
				const float a1 = DegToRad (std::abs (mAngle1));
				const float a2 = DegToRad (std::abs (mAngle2));

				auto& wb = _adjustedWidgetRect;
				const bool zeroCross = (mAngle1 * mAngle2 < 0.f); // different sign
				const float fromTopNorm = zeroCross ?
					0.f
				  : 1.f - std::cos (std::min (a1, a2));
				const float fromBottomNorm = 1.f + std::cos (std::max (a1, a2));
				const float widgetRadius = std::min (wb.H(), wb.W()) / 2.f;
				const float vShift = widgetRadius * (-fromTopNorm + fromBottomNorm) / 2.f;
				wb = mWidgetBounds.GetVShifted (vShift);
			}

    }; // class ArcKnob

} // ns hvoya
