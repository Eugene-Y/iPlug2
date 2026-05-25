#pragma once

#include <IPlugStructs.h>
#include "mapper.hpp"


namespace hvoya::midi_cc::mapper_serializer {

    void serialize (const Mapper&, iplug::IByteChunk&);

    // plugVersion: the plugin's state version_hex (0 = unknown/old preset).
    // Channel field is read only when plugVersion >= 0x00000300.
    int unserialize (Mapper&, const iplug::IByteChunk&, int startPos, int plugVersion = 0);

}
