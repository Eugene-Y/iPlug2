#pragma once

// midi_cc_mapper.hpp

#include <map>
#include <vector>
#include <algorithm>

#include <hvoya/utils/log/logger.hpp>
#include <hvoya/utils/types.hpp>
#include "controllable.hpp"




namespace hvoya::midi_cc {
    
    struct ParamССMapping {
        PId_t paramId;
        CC_t cc;
        double minVal { 0.0 };
        double maxVal { 1.0 };
        
        ParamССMapping (PId_t id = pid_not_set, CC_t cc = cc_not_set)
          : paramId (id), cc (cc) {}

        double mapVal (double normalized) const {
            double mapped = normalized;
            mapped = minVal + mapped * (maxVal - minVal);
            assert (mapped >= 0 && mapped <= 1);
            return mapped;
        }
    };
    
    
    typedef std::vector <ParamССMapping> ParamССMappings_t;
    typedef std::map <CC_t, ParamССMappings_t> CCtoParamMap_t;
    

    class Mapper {
        
        public:

            struct ResolvedParam {
                const PId_t id;
                const double mappedNormalizedVal;
                ResolvedParam (PId_t id, double v) : id (id), mappedNormalizedVal (v) {}
            };
            
            void setListeningParamId (PId_t i) { _listeningPId = i; }
            void setLearningForParam (PId_t, IControllable*);
            
            void setMaxForListeningParam (double);
            void setMinForListeningParam (double);
            void invertRangeForListeningParam();
            
            void clearAllMappings ();
            void clearMappingForParam (PId_t);
            
            std::vector <ResolvedParam> processMidiCC (CC_t, double normVal);
            
            const CCtoParamMap_t& getCCtoParamMap() const { return _ccToParamMap; }
            void setCCtoParamMap (CCtoParamMap_t&& map) { _ccToParamMap = std::move (map); }
            
        private:

            PId_t _listeningPId = pid_not_set;
            IControllable* _pListeningControllable = nullptr;
            bool _isLearning = false;
            CCtoParamMap_t _ccToParamMap;
        
            void addMapping (CC_t, PId_t);
            ParamССMapping* findMappingForPId (PId_t);
            
        };

} // namespace hvoya
