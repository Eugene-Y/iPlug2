#pragma once

#include "utils/types.hpp"

namespace hvoya::midi_cc {

    class IControllable {
        public:
            virtual ~IControllable() = default;
            virtual void setCCNumber (CC_t) = 0;
            virtual void setParamMinMax (double, double) = 0;
    };

}
