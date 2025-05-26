#pragma once

#include <atomic>

#include "IPlug_include_in_plug_hdr.h"
#include "plug_build_info.hpp"
#include "utils/log/logger.hpp"
#include "utils/host_time_info.hpp"
#include "utils/host_info_model.hpp"
#include "utils/host_info_view.hpp"
#include "utils/soft_limiter.hpp"


namespace hvoya {

    const int kNumPresets = 1;

    enum EParams {
      par_gain = 0,
      kNumParams
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
        
        virtual void OnParamChange (int paramIdx, EParamSource, int sampleOffset = -1) override;
        virtual void ProcessMidiMsg (const IMidiMsg&) override;
        
    private:
        inline static std::atomic <size_t> _sNumInstances { 0 };
        const size_t _instanceId;
        std::atomic <size_t> _bufId { 0 };
        hvoya::StatefulLogger _logger;

        void initializeParams();
        void initializeLayout();
        
        hvoya::HostInfoModel <Plugin> _hostInfoModel;
        hvoya::HostInfoView <Plugin> _hostInfoView;
        void updateHostInfoView();

        hvoya::SoftLimiter _softLimiter;
        
        sample _gain { 0 };
};
