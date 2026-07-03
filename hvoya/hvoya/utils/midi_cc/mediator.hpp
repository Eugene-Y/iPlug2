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

            // Accent color for the CC presence dot's mouse-down fill — set once by the UI so the dot
            // matches the plugin's accent. Applied to every decorator that gets a dot in createLearnable.
            void setPresenceDotAccent (const IColor& c) { _presenceDotAccent = c; _hasPresenceDotAccent = true; }

            // Modulation arc/strip color — set once by the UI (typically a paler tint of the accent).
            // Applied to every modulatable decorator in createLearnable.
            void setModArcColor (const IColor& c) { _modArcColor = c; _hasModArcColor = true; }
            
            
            template <typename Control, typename... Args>
            Control* createLearnable (Args&&... args) const {
                auto* dec = new Learnable <Control> (std::forward <Args> (args)...);
                // Opt-in Absolute/Modulate menu for plugins that expose modulatability
                // (Gneiss). Sand/Scape/DMT lack the hook → the menu never appears.
                if constexpr (requires { _plugin->isParamModulatable (PId_t {}); }) {
                    if (_plugin->isParamModulatable (dec->GetParamIdx())) {
                        dec->enableCombineModeMenu (true);
                        if (_hasModArcColor) dec->setModArcColor (_modArcColor);
                    }
                }
                if constexpr (requires { _plugin->ccUsesDepthGesture (PId_t {}); }) {
                    if (_plugin->ccUsesDepthGesture (dec->GetParamIdx()))
                        dec->enableDepthGesture (true);
                }
                // Freq params show a live oct/semi/cent readout during the set-depth drag.
                if constexpr (requires { _plugin->ccDepthIsFreq (PId_t {}); }) {
                    if (_plugin->ccDepthIsFreq (dec->GetParamIdx()))
                        dec->enableFreqDepthReadout (true);
                }
                // Constant CC-presence dot — opt-in per plugin. Sand/Scape/DMT lack the trait
                // → no dot, look unchanged.
                if constexpr (requires { _plugin->wantsCCPresenceDot(); }) {
                    if (_plugin->wantsCCPresenceDot()) {
                        dec->enablePresenceDot (true);
                        if (_hasPresenceDotAccent) dec->setPresenceDotAccentColor (_presenceDotAccent);
                    }
                }
                return dec;
            }
            
            
            bool OnMessage (int msgTag, int dataSize, const void* pData) {
                const int pId = *static_cast <const int*> (pData);
                double normVal = 0;
                if (auto p = _plugin->GetParam (pId))
                    normVal = p->GetNormalized();
                assert (dataSize == sizeof(int));
                switch (msgTag) {
                    case mtag_listen_to_pid:
                        _mapper.setListeningParamId (pId);
                        return true;

                    case mtag_learn_cc:
                        _mapper.setLearningForParam (pId);
                        refreshUI();   // start the learn-target dot blink (moves it off any prior target)
                        return true;

                    case mtag_cancel_learn:
                        _mapper.cancelLearning();
                        refreshUI();   // stop the learn-target dot blink
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
                ProcessMidiCC (msg, [](PId_t) { return false; });
            }

            // skip(pid) → true to NOT drive that param this time (e.g. a cutoff currently
            // owned by MIDI note mode, whose CC is repurposed as a pitch offset). Every other
            // param mapped to the same CC still updates — so one CC can drive several params.
            template <typename SkipFn>
            void ProcessMidiCC (const IMidiMsg& msg, SkipFn&& skip) {
                assert (msg.StatusMsg() == IMidiMsg::kControlChange);
                const auto cc = msg.ControlChangeIdx();
                const double normValue = msg.ControlChange (cc);
                const int channel1Idx = msg.Channel() + 1;  // IMidiMsg::Channel() is 0-indexed
                //LOGD << "ProcessMidiCC " << cc << " val " << normValue << " ch " << channel1Idx;
                const auto mappedParams = _mapper.processMidiCC (cc, normValue, channel1Idx);
                for (auto& p : mappedParams) {
                    if (skip (p.id)) continue;
                    // Relative-CC (Modulate) path — opt-in per plugin via these hooks. Plugins
                    // without them (Sand/Scape/DMT) always take the Absolute base-write below.
                    // The mapper's min/max are ignored in Modulate; the raw CC norm is handed off.
                    if constexpr (requires { _plugin->isRelativeCC (p.id); _plugin->onRelativeCC (p.id, normValue); }) {
                        if (_plugin->isRelativeCC (p.id)) {
                            _plugin->onRelativeCC (p.id, normValue);
                            continue;
                        }
                    }
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


            // UI thread: if a CC was just bound by learning (on the audio thread), returns
            // true once so the caller can refreshUI() to update control CC# indicators.
            // The audio thread never touches controls — call this each OnIdle.
            bool takeUIDirty() { return _mapper.takeUIDirty(); }


            // Arm learning for a param that has no on-screen control (e.g. the morph
            // pad's X/Y): the next CC routed through ProcessMidiCC binds to it.
            void learnForParam (PId_t paramId, bool mostMoved = false) { _mapper.setLearningForParam (paramId, mostMoved); }
            void cancelLearning() { _mapper.cancelLearning(); }
            bool isLearning() const { return _mapper.isLearning(); }

            // Swap which CC drives each of two params (e.g. morph pad X/Y reversed). Refreshes the UI.
            void swapParamMappings (PId_t a, PId_t b) { _mapper.swapParamMappings (a, b); refreshUI(); }

            // Drop the CC mapping for a param (e.g. to clear a learned X/Y).
            void clearMappingForParam (PId_t paramId) { _mapper.clearMappingForParam (paramId); }

            // Drop the ENTIRE CC map and refresh control indicators (the "clear all" action).
            void clearAllMappings() { _mapper.clearAllMappings(); refreshUI(); }


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

                // Clear every learnable control's CC indicator first — otherwise a mapping
                // removed since the last refresh (host reset-to-default, preset/state load,
                // cleared learn) leaves a stale CC# label on its control.
                pG->ForAllControlsFunc ([](iplug::igraphics::IControl* c) {
                    if (auto* ctrl = dynamic_cast <IControllable*> (c))
                        ctrl->clearCCNumber();
                });

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

                // Blink the active learn target's presence dot ("I'm listening"), clearing the flag on
                // every other control (so re-arming on a new param moves the blink, and a completed bind
                // — which refreshes via takeUIDirty — stops it).
                const PId_t learnPid = _mapper.learningParamId();
                pG->ForAllControlsFunc ([learnPid](iplug::igraphics::IControl* c) {
                    if (auto* ctrl = dynamic_cast <IControllable*> (c))
                        ctrl->setLearning (learnPid != uninit::pid && c->GetParamIdx() == learnPid);
                });

                // Opt-in: let the plugin repaint non-IControllable controls that read the map live (e.g.
                // Gneiss's morph-pad X/Y readout). Every map change funnels through refreshUI — learn,
                // clear, swap, file/preset load — so this is the single point that covers them all.
                if constexpr (requires { _plugin->onCCMapRefreshed(); })
                    _plugin->onCCMapRefreshed();
            }
            
            
        private:
        
            PluginType* _plugin;
            Mapper _mapper;
            IColor _presenceDotAccent;
            bool   _hasPresenceDotAccent = false;
            IColor _modArcColor;
            bool   _hasModArcColor = false;

    };
    


} // namespace hvoya::midi_cc
