#pragma once

#include <algorithm>
#include <string>

#include <IGraphicsPopupMenu.h>

#include <hvoya/utils/log/logger.hpp>
#include <hvoya/utils/types.hpp>
#include "message_tags.hpp"
#include "controllable.hpp"


namespace hvoya::midi_cc {


    using iplug::igraphics::IPopupMenu;


    template <typename C>
    class ControlDecorator : public C, public IControllable {

        private:
            enum MenuLayout {
                mtag_learn = 0,
                mtag_clear,
                mtag_separator,
                mtag_invert,
                mtag_setMin,
                mtag_setMax,
            };
            
            std::string _minDisplay;
            std::string _maxDisplay;
            PId_t       _paramId;
            CC_t        _cc;
            int         _midiBaseTag = 0; // VST3: flat tag of the first MIDI CC submenu item

            // not safe if the Delegate params are not yet initialized
            auto getParam() {
                assert (_paramId != uninit::pid);
                return this->GetDelegate()->GetParam (_paramId);
            }
            
            void updateDisplay (std::string& display) {
                WDL_String s;
                getParam()->GetDisplay (s);
                display = s.Get();
            }
            
            
            void updateDisplay (std::string& display, double normVal) {
                assert (normVal >= 0 && normVal <= 1);
                WDL_String s;
                getParam()->GetDisplay (normVal, true, s);
                display = s.Get();
            }


            void setMinMaxDisplaysToFullRange() {
                auto pP = getParam();
                WDL_String s;
                pP->GetDisplay (pP->GetMin(), false, s);
                _minDisplay = s.Get();
                pP->GetDisplay (pP->GetMax(), false, s);
                _maxDisplay = s.Get();
            }

        
        public:
        
            template <typename... Args>
            ControlDecorator (Args&&... args)
                : C (std::forward <Args> (args)...) {
                    _paramId = this->GetParamIdx();
                    _cc = uninit::cc;
                }
                
                
            void setCCNumber (CC_t cc) override {
                _cc = cc;
                setMinMaxDisplaysToFullRange();
            }

            void clearCCNumber() override {
                clearCC();
            }


            void setParamMinMax (double normMin, double normMax) override {
                auto pP = getParam();
                assert (pP);
                updateDisplay (_minDisplay, normMin);
                updateDisplay (_maxDisplay, normMax);
            }

            
            void CreateContextMenu (IPopupMenu& contextMenu) override {
                IPopupMenu* subMenu = new IPopupMenu ("MIDI CC");
                
                const bool mapped = _cc != uninit::cc;

                subMenu->AddItem ("Learn");
                
                std::string s = "Clear";
                if (mapped) s += " (" + std::to_string (_cc) + ")";
                subMenu->AddItem (s.c_str())->SetEnabled (mapped);
                
                subMenu->AddSeparator();
                subMenu->AddItem ("Invert")->SetEnabled (mapped);
                
                s = "Set Min";
                if (mapped) s += " (" + _minDisplay + ")";
                subMenu->AddItem (s.c_str())->SetEnabled (mapped);
                
                s = "Set Max";
                if (mapped) s += " (" + _maxDisplay + ")";
                subMenu->AddItem (s.c_str())->SetEnabled (mapped);

				contextMenu.AddItem (getParam()->GetName(), -1, IPopupMenu::Item::Flags::kTitle);
				contextMenu.AddSeparator();
                // VST3 flattens submenus into sequential tags; record the offset so
                // OnContextSelection can normalize the tag back to the local submenu index.
                // offset = contextMenu items before us (2: title + sep) + 1 (group-start tag)
                _midiBaseTag = contextMenu.NItems() + 1;
                contextMenu.AddItem ("MIDI CC", subMenu);
                
                C::CreateContextMenu (contextMenu);
            }

            
            void OnContextSelection (int itemSelected) override {
#if defined VST3_API || defined VST3C_API
                // In VST3 the context menu delivers a flat sequential tag, not a
                // per-submenu index. Normalise back to the local submenu index so
                // tryProcessMidiCCMenuSelection sees 0-based values.
                const int localIdx = itemSelected - _midiBaseTag;
#else
                const int localIdx = itemSelected;
#endif
                if (tryProcessMidiCCMenuSelection (localIdx))
                    return;
                C::OnContextSelection (itemSelected);
            }
            
        private:

            void clearCC() {
                _cc = uninit::cc;
                _minDisplay.clear();
                _maxDisplay.clear();
            }

            bool tryProcessMidiCCMenuSelection (int itemSelected) {
                if (itemSelected == mtag_separator 
                    || itemSelected < mtag_learn || itemSelected > mtag_setMax)
                    return false;
                    
                typedef midi_cc::MessageTags MT;
                MT action;

                switch (itemSelected) {
                    case mtag_learn:  
                        action = MT::mtag_learn_cc;  
                        clearCC();
                        break;
                        
                    case mtag_clear:  
                        action = MT::mtag_clear_cc;  
                        clearCC();
                        break;
                    
                    case mtag_invert: 
                        action = MT::mtag_invert_range; 
                        std::swap (_minDisplay, _maxDisplay);
                        break;
                    
                    case mtag_setMin: 
                        action = MT::mtag_set_min; 
                        updateDisplay (_minDisplay);
                        break;
                    
                    case mtag_setMax: 
                        action = MT::mtag_set_max; 
                        updateDisplay (_maxDisplay);
                        break;
                    
                    default: return false;
                }
                
                const PId_t pId = this->GetParamIdx();
                LOGD << "MIDI CC: param " << pId << ": action " << action;
                // SendArbitraryMsgFromUI correctly routes to the processor in all formats
                // (including VST3 where GetDelegate() is the controller, not the processor).
                this->GetDelegate()->SendArbitraryMsgFromUI (MT::mtag_listen_to_pid, iplug::kNoTag, sizeof (PId_t), &pId);
                this->GetDelegate()->SendArbitraryMsgFromUI (action, iplug::kNoTag, sizeof (PId_t), &pId);
                
                return true;
            }
    };


    template <typename C>
    using Learnable = ControlDecorator <C>;

}
