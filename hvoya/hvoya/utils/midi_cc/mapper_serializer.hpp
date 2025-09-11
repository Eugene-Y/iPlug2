#pragma once

#include <IPlugStructs.h>
#include "mapper.hpp"


namespace hvoya::midi_cc::mapper_serializer {
    
    void serialize (const Mapper&, iplug::IByteChunk&);
    int unserialize (Mapper&, const iplug::IByteChunk&, int);
    
}
