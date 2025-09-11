#pragma once

#include <cassert>
#include <cmath>
#include <hvoya/utils/log/logger.hpp>
#include "host_time_info.hpp"


namespace hvoya {
    
    
    struct HostInfo {
        size_t sampleRate;
        size_t bufSize;
        size_t chanIn;
        size_t chanOut;
        TimeInfo hostTime;
    };
    
    
    template <typename Host>
    class HostInfoModel {

		#ifndef HIM_DLOG
			#define HIM_DLOG LOGD << "HostInfo "
		#else
			#error change HostInfo log macro
		#endif

        public:
            explicit HostInfoModel (Host* p) : _pHost (p) { assert (p); }
            HostInfoModel() = delete;
            
            
            void updateAll (size_t bufSize) {
                updateBufSize (bufSize);
                updateChans();
                updateHostPosition();
                updateSampleRate();
            }


			void updateChans (){
				int in = numConnectedChans (iplug::ERoute::kInput);
				int out = numConnectedChans (iplug::ERoute::kOutput);

				if (in == _lastChanIn && out == _lastChanOut)
						return;

				HIM_DLOG << "chans: " << in << "-" << out;
				_lastChanIn = in;
				_lastChanOut = out;
				_updChans = true;
			}



			void updateSampleRate() {
				size_t newSR = _pHost->GetSampleRate();
				if (newSR == _lastSampleRate)
						return;

				HIM_DLOG << "sr: " << newSR;
				_lastSampleRate = newSR;
				_updSR = true;
			}


			void updateBufSize (size_t newSz) {
				if (newSz == _lastBufSize)
					return;

				HIM_DLOG << "buf size: " << newSz;
				_lastBufSize = newSz;
				_updBufSz = true;
			}


			void updateHostPosition() {
				float samplePos = _pHost->GetSamplePos();
				if (_lastSamplePos == samplePos)
						return;

				_lastSamplePos = samplePos;
				_hostTime = buildTimeInfo();
				//HIM_DLOG << "host pos: " << buildTimeString (_hostTime);
				_updHostPos = true;
			}

            
            void reset() {
                updateHostPosition();
                updateSampleRate();
                _updSR      = true;
                _updHostPos = true;
                _updBufSz   = true;
            }
            
            
            void clearUpdateFlags() {
                _updHostPos = false;
                _updSR      = false;
                _updBufSz   = false;
                _updChans   = false;
            }
            
            
            bool getUpdateHostPosition() const { return _updHostPos; }
            bool getUpdateSampleRate()   const { return _updSR; }
            bool getUpdateBufSize()      const { return _updBufSz; }
            bool getUpdateChans()        const { return _updChans; }
            
            size_t getMinChans() const { return std::min (_lastChanIn, _lastChanOut); }
            
            HostInfo getInfo() const {
                return {
                    _lastSampleRate,
                    _lastBufSize,
                    _lastChanIn,
                    _lastChanOut,
                    _hostTime
                };
            }
            
        private:
        
            Host* _pHost;
            
            float _lastSamplePos   = 0;
            TimeInfo _hostTime;
            size_t _lastSampleRate = 0;
            size_t _lastChanIn     = 0;
            size_t _lastChanOut    = 0;
            size_t _lastBufSize    = 0;
            
            bool _updHostPos = true;
            bool _updSR      = true;
            bool _updBufSz   = true;
            bool _updChans   = true;

            
            int numConnectedChans (iplug::ERoute dir) const {
                const auto maxChans = _pHost->MaxNChannels (dir);
                int chans = 0;
                for (int i = 0; i < maxChans; ++i) {
                    chans += _pHost->IsChannelConnected (dir, i);
                }
                return chans;
            }
            
            
            TimeInfo buildTimeInfo() const {
                // https://www.libertyparkmusic.com/musical-time-signatures/
                
                TimeInfo i;
                float samplePos = _pHost->GetSamplePos();
                i.samplePos = std::max <float> (0, samplePos);
                
                int num = 1;
                int den = 1;
                _pHost->GetTimeSig (num, den);
                
                i.num = num;
                i.den = den;
                
                using std::floor;

                float m = _pHost->GetPPQPos() * 60. / _pHost->GetTempo() * (den / 8.) * (4. / num) + 1;
                float b = size_t (num * (m - 1)) % den + 1;
                uint f = 100 * (b - uint (b));
                
                i.beat = floor (b);
                i.fraction = f;
                
                float sec = samplePos > 0 ? samplePos / _pHost->GetSampleRate() : 0;
                float hours = sec / 3600.;
                float minutes = (hours - int (hours)) * 60.;
                float seconds = (minutes - int (minutes)) * 60.;
                
                i.hours   = floor (hours);
                i.minutes = floor (minutes);
                i.seconds = floor (seconds);
                i.milliseconds = (seconds - int (seconds)) * 1000;
                
                return i;
            }
            
            #undef HIM_DLOG
    };
    
}

