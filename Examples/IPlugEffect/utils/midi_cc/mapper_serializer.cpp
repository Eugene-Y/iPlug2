#include "utils/log/logger.hpp"
#include "mapper_serializer.hpp"


namespace hvoya::midi_cc::mapper_serializer {

    #ifndef CCMAP_LOGD
        #define CCMAP_LOGD LOGD << "CCMapperSerializer: "
    #else
        #error "rename CCMapperSerializer log macros"
    #endif
    
    
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
            }
        }
    }
    
    
    int unserialize (Mapper& mapper, const iplug::IByteChunk& chunk, int startPos) {
        CCtoParamMap_t map;
        size_t mapSize = 0;
        startPos = chunk.Get (&mapSize, startPos);
        CCMAP_LOGD << "unserializing. map size: " << mapSize;
        for (size_t i = 0; i < mapSize; i++) {
            CC_t cc = 0;
            startPos = chunk.Get (&cc, startPos);
            assert (cc >= 0 && cc < 128);
            size_t paramCount = 0;
            startPos = chunk.Get (&paramCount, startPos);
            assert (paramCount > 0);
            ParamССMappings_t params (paramCount);
            for (size_t j = 0; j < paramCount; j++) {
                auto& pm = params [j];
                startPos = chunk.Get (&pm.paramId, startPos);
                startPos = chunk.Get (&pm.cc,      startPos);
                startPos = chunk.Get (&pm.minVal,  startPos);
                startPos = chunk.Get (&pm.maxVal,  startPos);
            }
            map [cc] = params;
        }
        mapper.setCCtoParamMap (std::move (map));
        return startPos;
    }
    
    
    #undef CCMAP_LOGD
    
}
