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
    };

}
