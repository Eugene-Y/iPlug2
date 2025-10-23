#pragma once

#include <string>
#include <cassert>
#include <IControl.h>
#include "host_info_UI_tags.hpp"
#include "host_time_info.hpp"


namespace hvoya {
    
    template <typename Host>
    class HostInfoView {
        public:
        
            HostInfoView (Host* p) : _pHost (p) { assert (p); }
            

            bool updateSampleRateView (size_t newSR) {
				_lastSampleRate = newSR;
                auto pG = _pHost->GetUI();
                if (!pG)
                    return false;
                auto pC = pG->GetControlWithTag (hvoya::tag_SampleRate);
                if (!pC)
                    return false;
                
                _sampleRateStr = "sr:        " + std::to_string (_lastSampleRate);
                auto pTC = pC-> template As <iplug::igraphics::ITextControl>();
                pTC->SetStr (_sampleRateStr.c_str());
                pTC->SetDirty();
                return true;
            }


            bool updateBufSizeView (size_t newSz) {
				_lastBufSize = newSz;
                auto pG = _pHost->GetUI();
                if (!pG)
                    return false;
                
                auto pC = pG->GetControlWithTag (hvoya::tag_BufSize);
                if (!pC)
                    return false;
                
                auto pTC = pC-> template As <iplug::igraphics::ITextControl>();
                _bufSizeStr = "buf size:  " + std::to_string (_lastBufSize);
                pTC->SetStr (_bufSizeStr.c_str());
                pTC->SetDirty();
                return true;
            }


			bool updateSRBufSizeAboutInfo (size_t newSR, size_t newSz) {
				_lastSampleRate = newSR;
				_lastBufSize = newSz;
				auto pG = _pHost->GetUI();
				if (!pG)
					return false;

				auto pC = pG->GetControlWithTag (hvoya::tag_SRateBufSzInfo);
				if (!pC)
					return false;

				auto pTC = pC-> template As <iplug::igraphics::ITextControl>();
				std::string str = std::format ("sr {} buf size {}", _lastSampleRate, _lastBufSize);
				pTC->SetStr (str.c_str());
				pTC->SetDirty();
			}


            bool updateChansView (size_t in, size_t out) {
				_lastInChans = in;
				_lastOutChans = out;
                auto pG = _pHost->GetUI();
                if (!pG)
                    return false;
                
                auto pC = pG->GetControlWithTag (hvoya::tag_Chans);
                if (!pC)
                    return false;
                
                _chanStr = "I/O:       " + std::to_string (_lastInChans)
										 + "-" + std::to_string (_lastOutChans);
                auto pTC = pC-> template As <iplug::igraphics::ITextControl>();
                pTC->SetStr (_chanStr.c_str());
                pTC->SetDirty();
                return true;
            }


            bool updateHostPositionView (const TimeInfo& i) {
				_lastTimeInfo = i;
                auto pG = _pHost->GetUI();
                if (!pG)
                    return false;
                
                auto pC = pG->GetControlWithTag (hvoya::tag_HostPos);
                if (!pC)
                    return false;

                _hostPositionStr = "host time: " + buildTimeString (_lastTimeInfo);
                auto pTC = pC-> template As <iplug::igraphics::ITextControl>();
                pTC->SetStr (_hostPositionStr.c_str());
                pTC->SetDirty();
                return true;
            }
        
        private:
        
            Host* _pHost;

			size_t _lastBufSize		{ 0 };
			size_t _lastSampleRate 	{ 0 };
			size_t _lastInChans 	{ 0 };
			size_t _lastOutChans 	{ 0 };

			TimeInfo _lastTimeInfo;

			std::string _hostPositionStr { "host time: ---" };
			std::string _sampleRateStr   { "sr:        ---" };
			std::string _bufSizeStr      { "buf size:  ---" };
			std::string _chanStr         { "I/O:       ---" };

    };
    
}
        
