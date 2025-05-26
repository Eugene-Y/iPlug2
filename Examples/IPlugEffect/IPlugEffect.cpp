#include "IPlugEffect.hpp"
#include "IPlug_include_in_plug_src.h"


using namespace hvoya;


void logBuildInfo() {
    LOGD << "PLUG_GIT_BRANCH_NAME " << PLUG_GIT_BRANCH_NAME;
    LOGD << "PLUG_GIT_COMMIT_SHA  " << PLUG_GIT_COMMIT_SHA;
    LOGD << "PLUG_BUILD_DATE      " << PLUG_BUILD_DATE;
    LOGD << "PLUG_PRODUCT         " << PLUG_PRODUCT;
    LOGD << "PLUG_ARCHS           " << PLUG_ARCHS;
}


IPlugEffect::IPlugEffect (const InstanceInfo& info)
    : iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets)),
        _instanceId (_sNumInstances),
        _logger ([this]() { return _instanceId; },
                [this]() { return _bufId.load(); }),
        _hostInfoModel (this),
        _hostInfoView (this)
{
    ++_sNumInstances;
    logBuildInfo();
    initializeParams();

    #if IPLUG_EDITOR // http://bit.ly/2S64BDd
        mMakeGraphicsFunc = [&]() {
            return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
        };
    #endif
    initializeLayout();
    
    _softLimiter.setThreshold (12);
    _softLimiter.setSoftness (3);

}


#if IPLUG_DSP
    void IPlugEffect::ProcessBlock (sample** inputs, sample** outputs, int numFrames) {
        ++_bufId;
        
        if (auto pG = GetUI()) {
            _hostInfoModel.update (numFrames);
            updateHostInfoView();
        }

        size_t chans = _hostInfoModel.getMinChans();
        for (int s = 0; s < numFrames; s++)
            for (int c = 0; c < chans; c++)
                outputs[c][s] = inputs[c][s] * _gain;
                
        _softLimiter.processBuffer (outputs, numFrames, chans);
        
        //LOGD << " num frames: " << numFrames << " in chans: " << inChans << " out chans: " << outChans;
    }
#endif


void IPlugEffect::initializeParams() {
    GetParam (par_gain)->InitDouble ("Gain", 100., 0., 800.0, 0.01, "%");
}


void IPlugEffect::OnIdle() {
    auto pG = GetUI();
    if (!pG)
        return;
    
    updateHostInfoView();
}


void IPlugEffect::OnActivate (bool enable) {
    OnIdle();
}


void IPlugEffect::OnReset() {
    _hostInfoModel.reset();
}


void IPlugEffect::ProcessMidiMsg (const IMidiMsg& msg) {
    const auto status = msg.StatusMsg();
    
    switch (status) {
        case IMidiMsg::kNoteOn:
            // do_something (msg.NoteNumber(), msg.Velocity() / 127.);
            break;
              
        case IMidiMsg::kNoteOff:
            // do_something();
            break;
        case IMidiMsg::kPolyAftertouch:
        case IMidiMsg::kControlChange:
        case IMidiMsg::kProgramChange:
        case IMidiMsg::kChannelAftertouch:
        case IMidiMsg::kPitchWheel:
        default:
            break;
    }
}
  
  
void IPlugEffect::OnParamChange(int pid, EParamSource s, int sampleOffset) {
    const float v = GetParam (pid)->Value();
    
    switch (pid) {
        case par_gain: _gain = v / 100.;                     break;
        default:       LOGD << "unknown param idx: " << pid; break;
    }
}


