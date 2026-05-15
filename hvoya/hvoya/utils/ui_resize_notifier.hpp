#pragma once

#include <IControls.h>

namespace hvoya::ui {
    
    class UIResizeNotifier : public IControl {
        public:
			typedef std::function <void (int w, int h, float scale, bool inDrag)> callback_t;

			UIResizeNotifier (callback_t f)
				: IControl (IRECT (0, 0, 2, 2)),
				_cb (f) {}

			void Draw (IGraphics&) override {
				++_cutOffCounter;
			}

			void OnResize() override {
				if (_cutOffCounter > 0)
					call();
			}

		protected:

			callback_t _cb;
			size_t _cutOffCounter { 0 };

			void call() const {
				auto pG = GetUI();
				assert (pG);
				int w = pG->Width();
				int h = pG->Height();
				float scale = pG->GetDrawScale();
				bool inDrag = pG->GetResizingInProcess();
				_cb (w, h, scale, inDrag);
			}

    }; // class UIResizeNotifier

} // ns hvoya::ui
