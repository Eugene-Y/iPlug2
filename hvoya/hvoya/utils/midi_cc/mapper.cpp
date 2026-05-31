#include "mapper.hpp"
#include <hvoya/utils/log/logger.hpp>


namespace hvoya::midi_cc {
    
    #ifndef MCCM_LOGD
        #define MCCM_LOGD LOGD << "Midi CC Mapper: "
        #define MCCM_LOGW LOGW << "Midi CC Mapper Warning: "
    #else
        #error "rename log macros"
    #endif
    
            
    auto findMappingIteratorForPId (std::vector <ParamCCMapping>& params, PId_t i) {
        return std::find_if (params.begin(), params.end(), 
                [i](const ParamCCMapping& m) { return m.paramId == i; });
    }
            
    
    void Mapper::setLearningForParam (PId_t i, IControllable* p) {
        _pListeningControllable = p;
        clearMappingForParam (i);
        MCCM_LOGD << "learning param " << i;
        _listeningPId = i;
        _isLearning = true;
    }
            
            
    void Mapper::invertRangeForListeningParam() {
        if (auto pM = findMappingForPId (_listeningPId)) {
            MCCM_LOGD <<  " param " << _listeningPId << " invert range";
            std::swap (pM->minVal, pM->maxVal);
        }
    }
            
            
    void Mapper::setMaxForListeningParam (double v) {
        assert (v >= 0 && v <= 1);
        if (auto pM = findMappingForPId (_listeningPId)) {
            MCCM_LOGD <<  " param " << _listeningPId << " set max " << v;
            pM->maxVal = v;
        }
    }
            
            
    void Mapper::setMinForListeningParam (double v) {
        assert (v >= 0 && v <= 1);
        if (auto pM = findMappingForPId (_listeningPId)) {
            MCCM_LOGD <<  " param " << _listeningPId << " set min " << v;
            pM->minVal = v;
        }
    }
            
            
    void Mapper::clearAllMappings () {
        _ccToParamMap.clear();
        _listeningPId = -1;
        _pListeningControllable = nullptr;
        MCCM_LOGD << "clear all mappings";
    }
            

    void Mapper::clearMappingForParam (PId_t i) {
        MCCM_LOGD << "clear mapping for param " << i;
        for (auto& [cc, params] : _ccToParamMap) {
            auto it = findMappingIteratorForPId (params, i);
            if (it != params.end()) {
                params.erase (it);
                if (params.empty()) {
                    _ccToParamMap.erase (cc);
                }
                return;
            }
        }
    }
            
    
    std::vector <Mapper::ResolvedParam> Mapper::processMidiCC (CC_t cc, double normVal, int midiChannel) {
        //MCCM_LOGD << "process midi cc " << cc;
        if (_isLearning) {
            addMapping (cc, _listeningPId);
            _isLearning = false;
        }

        auto it = _ccToParamMap.find (cc);
        if (it != _ccToParamMap.end()) {
            std::vector <ResolvedParam> mapped;
            for (const auto& pM : it->second) {
                if (midiChannel == 0 || pM.channelMatches (midiChannel))
                    mapped.emplace_back (pM.paramId, pM.mapVal (normVal));
            }
            return mapped;
        }

        return {};
    }


    int Mapper::getCCForParam (PId_t id) const {
        for (const auto& [cc, params] : _ccToParamMap)
            for (const auto& pm : params)
                if (pm.paramId == id)
                    return cc;
        return uninit::cc;
    }


    void Mapper::setChannelForParam (PId_t id, int channel) {
        for (auto& [cc, params] : _ccToParamMap)
            for (auto& pm : params)
                if (pm.paramId == id) { pm.channel = channel; return; }
    }
        
        
    void Mapper::addMapping (CC_t cc, PId_t i) {
        MCCM_LOGD << "add mapping CC " << cc << " -> param " << i;
        if (_pListeningControllable) {
            _pListeningControllable->setCCNumber (cc);
            _pListeningControllable = nullptr;
        }
        auto& params = _ccToParamMap [cc];
        auto it = findMappingIteratorForPId (params, i);
        if (it == params.end()) {
            params.emplace_back (ParamCCMapping (i, cc));
        }
    }
            
            
    ParamCCMapping* Mapper::findMappingForPId (PId_t i) {
        for (auto& [cc, params] : _ccToParamMap) {
            auto it = findMappingIteratorForPId (params, i);
            if (it != params.end()) {
                return &(*it);
            }
        }
        MCCM_LOGW << "no param " << i;
        return nullptr;
    }
    
        
    #undef MCCM_LOGD
    #undef MCCM_LOGW

} // namespace hvoya
