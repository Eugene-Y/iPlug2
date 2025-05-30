#pragma once

#include <atomic>

#include "IPlug_include_in_plug_hdr.h"
#include "plug_build_info.hpp"
#include "utils/log/logger.hpp"
#include "utils/host_time_info.hpp"
#include "utils/host_info_model.hpp"
#include "utils/host_info_view.hpp"
#include "utils/soft_limiter.hpp"
#include "utils/midi_cc/mediator.hpp"


namespace hvoya {

    const int kNumPresets = 1;

    enum EParams {
        par_gain_L = 0,
        par_gain_R,
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
        
        bool OnMessage (int msgTag, int ctrlTag, int dataSize, const void* pData) override;
        
        void OnParamChange (int paramIdx, EParamSource, int sampleOffset = -1) override;
        void ProcessMidiMsg (const IMidiMsg&) override;
        
        bool SerializeState(IByteChunk&) const override;
        int UnserializeState(const IByteChunk&, int startPos) override;

        static size_t vMajor (version_hex_t v) { return (v & 0xFFFF0000) >> 16; }
        static size_t vMinor (version_hex_t v) { return (v & 0x0000FF00) >> 8; }
        static size_t vPatch (version_hex_t v) { return (v & 0x000000FF); }

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
        
        sample _gainL { 0 };
        sample _gainR { 0 };
        
        hvoya::midi_cc::CCMediator <IPlugEffect> _midiCCMediator;

};
