#pragma once

#include <atomic>
#include <array>

#include "IPlug_include_in_plug_hdr.h"
#include "plug_build_info.hpp"
#include <hvoya/utils/log/logger.hpp>
#include <hvoya/utils/host_time_info.hpp>
#include <hvoya/utils/host_info_model.hpp>
#include <hvoya/utils/host_info_view.hpp>
#include <hvoya/utils/soft_limiter.hpp>
#include <hvoya/utils/watchdog.hpp>
#include <hvoya/utils/midi_cc/mediator.hpp>


namespace hvoya {

	typedef decltype (PLUG_VERSION_HEX) version_hex_t;

    const int kNumPresets = 1;

    enum EParams {
        par_gain_L = 0,
        par_gain_R,
        par_lim_thresh,
        par_lim_softness,
        par_master_mix,
        num_params
    };

}


using namespace iplug;
using namespace igraphics;


class IPlugEffect final : public Plugin {
    public:
  
        IPlugEffect (const InstanceInfo&);
        
        void OnIdle() override;
        void OnReset() override;
        void OnActivate (bool enable) override;

        #if IPLUG_DSP // http://bit.ly/2S64BDd
            void ProcessBlock (sample** ins, sample** outs, int nFrames) override;
        #endif
        
        bool OnMessage (int msgTag, int ctrlTag, int dataSize, const void* pData) override;
        
        void OnParamChange (int paramIdx, EParamSource, int sampleOffset = -1) override;
        void ProcessMidiMsg (const IMidiMsg&) override;
        
        bool SerializeState(IByteChunk&) const override;
        int UnserializeState(const IByteChunk&, int startPos) override;

		static size_t vMajor (hvoya::version_hex_t v) { return (v & 0xFFFF0000) >> 16; }
        static size_t vMinor (hvoya::version_hex_t v) { return (v & 0x0000FF00) >> 8; }
        static size_t vPatch (hvoya::version_hex_t v) { return (v & 0x000000FF); }

    private:

        inline static std::atomic <size_t> _sNumInstances { 0 };
        const size_t _instanceId;
        std::atomic <size_t> _bufId { 0 };
        hvoya::StatefulLogger _logger;

        
        std::array <double, hvoya::EParams::num_params> _prevParamVals;
        void resetPrevParamVals() { _prevParamVals.fill (std::numeric_limits <double>::max()); }
        bool isSettingNewVal (hvoya::PId_t pid, double v) {
            assert (pid < (hvoya::num_params));
            if (_prevParamVals [pid] != v) {
                _prevParamVals [pid] = v;
                return true;
            }
            return false;
        }

        void initializeParams();
        void initializeLayout();
		void setTooltips (IGraphics*);

		hvoya::Watchdog <IPlugEffect> _uiUpdateWatchdog;
		bool tryUpdateLayout();
		void triggerUIUpdate() { _uiUpdateWatchdog.notify(); }

        hvoya::HostInfoModel <IPlugEffect> _hostInfoModel; // TODO doesnt update chans
        hvoya::HostInfoView <IPlugEffect> _hostInfoView;
        bool updateHostInfoView();

        hvoya::SoftLimiter _softLimiter;
        hvoya::sample_t _master_mix;

        sample _gainL { 0 };
        sample _gainR { 0 };
        
        hvoya::midi_cc::CCMediator <IPlugEffect> _midiCCMediator;

};
