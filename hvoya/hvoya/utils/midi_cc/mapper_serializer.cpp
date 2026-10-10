#include <hvoya/utils/log/logger.hpp>
#include "mapper_serializer.hpp"
#include <algorithm>
#include <cmath>


namespace hvoya::midi_cc::mapper_serializer {

    #ifndef CCMAP_LOGD
        #define CCMAP_LOGD LOGD << "CCMapperSerializer: "
    #else
        #error "rename CCMapperSerializer log macros"
    #endif
    
    
    static constexpr int kChannelVersion = 0x00000300;

    void serialize (const Mapper& mapper, iplug::IByteChunk& chunk) {
        const auto& map = mapper.getCCtoParamMap();
        const size_t mapSize = map.size();
        CCMAP_LOGD << "serializing. map size: " << mapSize;
        chunk.Put (&mapSize);
        for (const auto& [cc, params] : map) {
            assert (cc >= 0 && cc < 128);
            assert (!params.empty());
            chunk.Put (&cc);
            const size_t paramsSize = params.size();
            chunk.Put (&paramsSize);
            for (const auto& param : params) {
                chunk.Put (&param.paramId);
                chunk.Put (&param.cc);
                chunk.Put (&param.minVal);
                chunk.Put (&param.maxVal);
                chunk.Put (&param.channel);  // added in kChannelVersion
            }
        }
    }


    int unserialize (Mapper& mapper, const iplug::IByteChunk& chunk, int startPos, int plugVersion, int numParams) {
        // A corrupt or truncated chunk can hold any count and any value. A count may not promise more
        // than the chunk still holds (else a garbage size_t spins the loop or a vector(count) throws), and
        // a binding is kept only if this build can drive it — an unknown param id would reach GetParam()
        // as a nullptr. Structural damage rejects the block (-1) and leaves the live map alone.
        constexpr int kNumCCs = 128;
        const bool hasChannel = plugVersion >= kChannelVersion;
        const size_t bindingBytes = sizeof (PId_t) + sizeof (CC_t) + 2 * sizeof (double)
                                  + (hasChannel ? sizeof (int) : 0);
        const auto fitsInChunk = [&] (size_t count, int pos) {
            return pos >= 0 && count <= size_t (chunk.Size() - pos) / bindingBytes;
        };

        size_t mapSize = 0;
        startPos = chunk.Get (&mapSize, startPos);
        CCMAP_LOGD << "unserializing. map size: " << mapSize << " hasChannel: " << hasChannel;
        if (startPos < 0 || mapSize > size_t (kNumCCs))
            return -1;

        CCtoParamMap_t map;
        for (size_t i = 0; i < mapSize; i++) {
            CC_t cc = 0;
            size_t paramCount = 0;
            startPos = chunk.Get (&cc, startPos);
            startPos = chunk.Get (&paramCount, startPos);
            if (!fitsInChunk (paramCount, startPos))
                return -1;
            ParamCCMappings_t params;
            for (size_t j = 0; j < paramCount; j++) {
                ParamCCMapping pm;
                startPos = chunk.Get (&pm.paramId, startPos);
                startPos = chunk.Get (&pm.cc,      startPos);
                startPos = chunk.Get (&pm.minVal,  startPos);
                startPos = chunk.Get (&pm.maxVal,  startPos);
                if (hasChannel)
                    startPos = chunk.Get (&pm.channel, startPos);
                if (startPos < 0)
                    return -1;
                if (pm.paramId < 0 || pm.paramId >= numParams || cc < 0 || cc >= kNumCCs)
                    continue;
                pm.cc      = cc;
                pm.minVal  = std::isfinite (pm.minVal) ? std::clamp (pm.minVal, 0.0, 1.0) : 0.0;
                pm.maxVal  = std::isfinite (pm.maxVal) ? std::clamp (pm.maxVal, 0.0, 1.0) : 1.0;
                pm.channel = (pm.channel >= 0 && pm.channel <= 16) ? pm.channel : 0;
                params.push_back (pm);
            }
            if (!params.empty())
                map [cc] = std::move (params);
        }
        mapper.setCCtoParamMap (std::move (map));
        return startPos;
    }


    #undef CCMAP_LOGD

}

