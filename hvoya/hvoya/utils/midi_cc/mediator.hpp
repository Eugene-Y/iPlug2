// utils/midi_cc/mediator.hpp

#pragma once

#include "mapper.hpp"
#include "control_decorator.hpp"
#include "mapper_serializer.hpp"
#include "message_tags.hpp"


using namespace iplug;


namespace hvoya::midi_cc {


    template <typename PluginType>
    class CCMediator {
        
        public:
        
            CCMediator (PluginType* p) : _plugin (p) {}
            ~CCMediator() = default;
            
            
            template <typename Control, typename... Args>
            Control* createLearnable (Args&&... args) const {
                return new Learnable <Control> (std::forward <Args> (args)...);
            }
            
            
            bool OnMessage (int msgTag, int dataSize, const void* pData) {
                const int pId = *static_cast <const int*> (pData);
                double normVal = 0;
                IControllable* pControllable = nullptr;
                if (auto p = _plugin->GetParam (pId)) {
                    normVal = p->GetNormalized();
                    if (auto pG = _plugin->GetUI()) {
                        auto pC = pG->GetControlWithParamIdx (pId);
                        if (pC)
                            pControllable = dynamic_cast <IControllable*> (pC);
                    }
                }
                assert (dataSize == sizeof(int));
                switch (msgTag) {
                    case mtag_listen_to_pid:
                        _mapper.setListeningParamId (pId);
                        return true;
                        
                    case mtag_learn_cc:
                        _mapper.setLearningForParam (pId, pControllable);
                        return true;
                        
                    case mtag_clear_cc:
                        _mapper.clearMappingForParam (pId);
                        return true;
                        
                    case mtag_invert_range:
                        _mapper.invertRangeForListeningParam();
                        return true;
                        
                    case mtag_set_min:
                        _mapper.setMinForListeningParam (normVal);
                        return true;
                        
                    case mtag_set_max:
                        _mapper.setMaxForListeningParam (normVal);
                        return true;
                        
                }
                
                return false;
            }
            
            
            void ProcessMidiCC (const IMidiMsg& msg) {
                assert (msg.StatusMsg() == IMidiMsg::kControlChange);
                const auto cc = msg.ControlChangeIdx();
                const double normValue = msg.ControlChange (cc);
                //LOGD << "ProcessMidiCC " << cc << " val " << normValue;
                const auto mappedParams = _mapper.processMidiCC (cc, normValue);
                for (auto& p : mappedParams) {
                    IParam* param = _plugin->GetParam (p.id);
                    _plugin->BeginInformHostOfParamChange (p.id);
                    param->SetNormalized (p.mappedNormalizedVal);
                    _plugin->OnParamChange (p.id, EParamSource::kDelegate);
                    _plugin->SendParameterValueFromDelegate (p.id, p.mappedNormalizedVal, true);
                    _plugin->EndInformHostOfParamChange (p.id);
                }
            }
            
            
            void serialize (IByteChunk& bc) const {
                mapper_serializer::serialize (_mapper, bc);
            }
            
            
            int unserialize (const IByteChunk& bc, int startPos) {
                startPos = mapper_serializer::unserialize (_mapper, bc, startPos);
                UpdateMidiControllableUI();
                return startPos;
            }
            
            
            void UpdateMidiControllableUI() {
                if (_controlsUpdatedOnLoad) 
                    return;
                    
                auto pG = _plugin->GetUI();
                if (!pG) 
                    return;
            
                const auto& map = _mapper.getCCtoParamMap();
                for (const auto& [cc, params] : map) {
                    for (const auto& param : params) {
                        auto pC = dynamic_cast <IControllable*> (pG->GetControlWithParamIdx (param.paramId));
                        if (pC) {
                            pC->setCCNumber (cc);
                            pC->setParamMinMax (param.minVal, param.maxVal);
                        }
                        else {
                            LOGW << "could not find control for mapped param " << param.paramId << " cc " << cc;
                        }
                    }
                }
                _controlsUpdatedOnLoad = true;
            }
            
            
        private:
        
            PluginType* _plugin;
            Mapper _mapper;
            bool _controlsUpdatedOnLoad = false;
            
    };
    


} // namespace hvoya::midi_cc
