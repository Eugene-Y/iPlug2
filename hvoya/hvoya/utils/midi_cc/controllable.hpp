#pragma once

#include <hvoya/utils/types.hpp>

namespace hvoya::midi_cc {

    class IControllable {
        public:
            virtual ~IControllable() = default;
            virtual void setCCNumber   (CC_t) = 0;
            virtual void clearCCNumber ()     = 0;
            virtual void setParamMinMax (double, double) = 0;
            // Relative-CC modulation depth, as a signed extent in the control's normalized units
            // (the reachable offset from base at full CC). Drives the modulation arc. Default no-op
            // for controls/plugins that don't visualize it.
            virtual void setModDepth (double /*signedNormExtent*/) {}
            // Bipolar modulation (e.g. a ± freq CC direction): the arc spans both sides of base.
            virtual void setModBipolar (bool /*bipolar*/) {}
            // The live modulated position the source currently produces, as a normalized [0,1] — the
            // control marks it as a dot on the arc/strip (NaN = no marker). Pushed only while modulating;
            // dirty-guarded so a static CC repaints nothing. Default no-op.
            virtual void setModLiveValue (double /*normVal*/) {}
            // Authoritative combine mode pushed by the host (0 = Absolute, 1 = Modulate) so the
            // control's display stays in sync across preset load / clear / external changes.
            virtual void setModeDisplay (int /*mode*/) {}
            // True while a set-depth drag is in progress on this control, so an external per-idle value
            // writer (e.g. a note-mode glider push) can skip it and not fight the gesture.
            virtual bool isAuthoringDepth() const { return false; }
            // Pushed by the mediator: this control is the active MIDI-learn target → signal "listening"
            // (the decorator blinks its presence dot). Default no-op.
            virtual void setLearning (bool /*on*/) {}
            // Nudge the CC presence dot, in dot-DIAMETER units (e.g. for an unusual control whose label
            // the default corner placement overlaps). Default no-op.
            virtual void setPresenceDotOffset (float /*dxDiameters*/, float /*dyDiameters*/) {}
    };

}
