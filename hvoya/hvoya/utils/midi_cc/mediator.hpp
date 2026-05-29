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
                const int channel1Idx = msg.Channel() + 1;  // IMidiMsg::Channel() is 0-indexed
                //LOGD << "ProcessMidiCC " << cc << " val " << normValue << " ch " << channel1Idx;
                const auto mappedParams = _mapper.processMidiCC (cc, normValue, channel1Idx);
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


            // plugVersion: the plugin's serialized version_hex (0 = old/unknown).
            int unserialize (const IByteChunk& bc, int startPos, int plugVersion = 0) {
                startPos = mapper_serializer::unserialize (_mapper, bc, startPos, plugVersion);
                refreshUI();
                return startPos;
            }


            // ── CC map access (for external serialization) ───────────────────

            const CCtoParamMap_t& getCCtoParamMap() const {
                return _mapper.getCCtoParamMap();
            }

            // Replace the entire CC map and refresh the UI.
            void setCCtoParamMap (CCtoParamMap_t map) {
                _mapper.setCCtoParamMap (std::move (map));
                refreshUI();
            }


            // Returns the CC number mapped to the given param, or -1 if none.
            int getCCForParam (PId_t paramId) const {
                return _mapper.getCCForParam (paramId);
            }


            // Sets the MIDI channel filter for an existing CC mapping (0=all, 1-16=specific).
            void setChannelForParam (PId_t paramId, int channel) {
                _mapper.setChannelForParam (paramId, channel);
            }
            
            
            // Syncs knob CC-number indicators to the current mapper state.
            // Safe to call any time; no-op when UI is not open.
            void refreshUI() {
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
            }
            
            
        private:
        
            PluginType* _plugin;
            Mapper _mapper;
            
    };
    


} // namespace hvoya::midi_cc
