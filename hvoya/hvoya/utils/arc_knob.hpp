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
					 bool drawRestOfTheArc = true,
					 float centerOfMassAdjustment = 1.f,
					 bool valueInWidget = false,
                     double gearing = DEFAULT_GEARING) :

                     IVKnobControl (bounds, paramIdx, label, style, true, valueInWidget,
									angleMin, angleMax, aAnchor, EDirection::Vertical, gearing),

					 _thicknessFarFromAnchor (thicknessFarFromAnchor),
					 _thicknessNearAnchor (thicknessNearAnchor),
					 _minArcLenPx (minArcLen),
					 _drawRestOfTheArc (drawRestOfTheArc),
					 _centerOfMassAdjustment (centerOfMassAdjustment) {
						 assert (angleMin < angleMax);
						 assert (angleMin <= aAnchor);
						 assert (aAnchor <= angleMax);
					 }


			void setValueIsEditable (bool e) {
				DisablePrompt (!e);
			}
        
			void setValueInWidget (bool v) { // TODO fix value rect
				mValueInWidget = v;
			}

			void Draw (IGraphics& g) override {
				IVKnobControl::DrawBackground (g, mRECT);
				IVKnobControl::DrawLabel (g);
				DrawWidget (g);
				IVKnobControl::DrawValue (g, mValueMouseOver);
			}


            void DrawWidget (IGraphics& g) override {
				const bool debug = 0;
				if (debug) g.DrawRect (COLOR_YELLOW, mWidgetBounds);

				auto wb = mWidgetBounds;
				const float widgetRadius = std::min (wb.H(), wb.W()) / 2.f;

				if (_centerOfMassAdjustment) {
					const float a1 = DegToRad (std::abs (mAngle1));
					const float a2 = DegToRad (std::abs (mAngle2));

					const bool zeroCross = (mAngle1 * mAngle2 < 0.f); // different sign
					const float fromTopNorm = zeroCross ?
						  0.f
						: 1.f - std::cos (std::min (a1, a2));

					const bool onlyUpperHalf = (a1 <= DegToRad (90.f) && a2 <= DegToRad (90.f));
					const float fromBottomNorm = onlyUpperHalf ?
						  1.f + std::cos (std::max (a1, a2))
						: 1.f + std::cos (std::max (a1, a2)); // TODO fix only lower half case

					float freeSpace = widgetRadius * (fromTopNorm + fromBottomNorm);

					if (debug) {
						auto t = wb.T + fromTopNorm * widgetRadius;
						auto b = wb.B - fromBottomNorm * widgetRadius;
						g.DrawLine (COLOR_RED, wb.L + 10, t, wb.R, t, 0, 2);
						g.DrawLine (COLOR_ORANGE, wb.L, b, wb.R - 10, b, 0, 2);
					}

					wb = mWidgetBounds.GetVShifted (_centerOfMassAdjustment * freeSpace / 2.f);
					if (debug) g.DrawRect (COLOR_ORANGE, wb);
				}

				const float cx = wb.MW();
				const float cy = wb.MH();

				const float handleAngle = mAngle1 + GetValue() * (mAngle2 - mAngle1);
	            const float a1 = handleAngle >= mAnchorAngle ? mAnchorAngle
											  : mAnchorAngle - (mAnchorAngle - handleAngle);
	            const float a2 = handleAngle >= mAnchorAngle ? handleAngle
											  : mAnchorAngle;

				const float distFromAnchorNorm = handleAngle == mAnchorAngle ?
					  0.f
					: handleAngle > mAnchorAngle ?
					(mAngle2 != mAnchorAngle) ? (handleAngle - mAnchorAngle) / (mAngle2 - mAnchorAngle) : 1.f
				  : (mAnchorAngle != mAngle1) ? (mAnchorAngle - handleAngle) / (mAnchorAngle - mAngle1) : 1.f;

				const float nearThick = std::min <float> (_thicknessNearAnchor, widgetRadius - 0.5f);
				const float farThick = std::min <float> (_thicknessFarFromAnchor, widgetRadius - 0.5f);
				// -0.5 is a workaround for strange arc center drawing with widgetRadius thickness
				const float t = (1.f - distFromAnchorNorm) *  nearThick + distFromAnchorNorm * farThick;

				const float radius = widgetRadius - t / 2.f;

				if (_drawRestOfTheArc)
					g.DrawArc (GetColor (kSH), cx, cy, radius, mAngle1, mAngle2, &mBlend, t);

                if (a1 != a2)
					g.DrawArc (GetColor (kX1), cx, cy, radius, a1, a2, &mBlend, t);

				// draw thin radial line to imitate the shortest arc with near-anchor thickness.
				// this keeps angle computation above clearer, and resulting image more accurate.
				if (_minArcLenPx > 0.f)
					g.DrawRadialLine (
						debug ? COLOR_RED : GetColor (kFR),
						cx, cy,
						handleAngle,
						widgetRadius - t,
						widgetRadius,
						&mBlend, _minArcLenPx);

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

			void setDrawRestOfTheArc (bool d) {
				_drawRestOfTheArc = d;
			}

		protected:

			bool _drawRestOfTheArc;
			float _minArcLenPx; // NB: default arc drawing backend is not perfect,
								// with min <= 1 there might be a noticeable gap
			float _thicknessFarFromAnchor;
			float _thicknessNearAnchor;
			float _centerOfMassAdjustment; // normalized to widget radius.
										   // 0 = no adjustment

    };
    
}
