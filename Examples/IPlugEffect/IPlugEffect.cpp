#include <cassert>
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
      : iplug::Plugin (info, MakeConfig (num_params, kNumPresets)),
        _instanceId (_sNumInstances),
        _logger ([this]() { return _instanceId; },
                [this]() { return _bufId.load(); }),
        _hostInfoModel (this, &IPlugEffect::triggerUIUpdate),
        _hostInfoView (this),
		_uiUpdateWatchdog (this, &IPlugEffect::tryUpdateLayout),
        _midiCCMediator (this),
        _master_mix (1.) {
    ++_sNumInstances;
    logBuildInfo();

    resetPrevParamVals();
    initializeParams();

    #if IPLUG_EDITOR // http://bit.ly/2S64BDd
        mMakeGraphicsFunc = [&]() {
            return MakeGraphics (*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen (PLUG_WIDTH, PLUG_HEIGHT));
        };
		initializeLayout();
		_uiUpdateWatchdog.start();
    #endif
    
    _softLimiter.setThreshold (12);
    _softLimiter.setSoftness (3);

}


#if IPLUG_DSP

    void IPlugEffect::ProcessBlock (sample** ins, sample** outs, int numFrames) {
        ++_bufId;
        
		_hostInfoModel.updateChans();
        if (auto pG = GetUI()) {
            _hostInfoModel.updateAll (numFrames);
            //updateHostInfoView(); // TODO dont do from this thread
        }

		const auto numChans = _hostInfoModel.getMinChans();

		AudioBuffer input (ins, numChans, numFrames);
		assert (input.isWrapper());

		AudioBuffer cleanCopy (input);
		assert (!cleanCopy.isWrapper());
		
		for (int c = 0; c < input.numChans(); c++) {
            //...
		}

        _softLimiter.processBuffer (input);
        input *= _master_mix;
        cleanCopy *= (1. - _master_mix);
        input += cleanCopy;

		input.copyTo (outs, numChans, numFrames);

        //LOGD << " num frames: " << numFrames << " in chans: " << inChans << " out chans: " << outChans;
    }

#endif


bool IPlugEffect::OnMessage (int msgTag, int ctrlTag, int dataSize, const void* pData) {
    if (msgTag >= midi_cc::msg_tags_begin && msgTag < midi_cc::msg_tags_end)
        return _midiCCMediator.OnMessage (msgTag, dataSize, pData);
    return false;
}


void IPlugEffect::initializeParams() {    
    GetParam (par_lim_thresh)   ->InitDouble ("Threshold",   0., -60., 24., 0.1);
    GetParam (par_lim_softness) ->InitDouble ("Softness",    0., 0., 24., 0.1);
    GetParam (par_master_mix)   ->InitDouble ("Master Mix",  1., 0., 1., 0.001);
}


void IPlugEffect::OnIdle() {

}


void IPlugEffect::OnActivate (bool enable) {
	//LOGD << "OnActivate " << enable;
    OnIdle();
}


void IPlugEffect::OnReset() {
    resetPrevParamVals();
    _hostInfoModel.reset();
	_uiUpdateWatchdog.notify();
}


void IPlugEffect::ProcessMidiMsg (const IMidiMsg& msg) {
    const auto status = msg.StatusMsg();
    
    switch (status) {
        case IMidiMsg::kControlChange:
            _midiCCMediator.ProcessMidiCC (msg);
            break;
            
        case IMidiMsg::kNoteOn:
            // do_something (msg.NoteNumber(), msg.Velocity() / 127.);
            break;
              
        case IMidiMsg::kNoteOff:
            // do_something();
            break;
        case IMidiMsg::kPolyAftertouch:
        case IMidiMsg::kProgramChange:
        case IMidiMsg::kChannelAftertouch:
        case IMidiMsg::kPitchWheel:
        default:
            break;
    }
}
  
  
void IPlugEffect::OnParamChange (int pid, EParamSource s, int sampleOffset) {
    const double v = GetParam (pid)->Value();
    if (!isSettingNewVal (pid, v))
        return;
        
    LOGD << "OnParamChange: p " << pid << " v " << v;
    switch (pid) {
		case par_lim_thresh:    _softLimiter.setThreshold (v);    break;
		case par_lim_softness:  _softLimiter.setSoftness (v);     break;
        case par_master_mix:    _master_mix = v;                  break;
        default: LOGD << "unknown param idx: " << pid; break;
    }
}


bool IPlugEffect::SerializeState (IByteChunk &chunk) const {
    LOGD << "Serializing state";
    auto vHex = PLUG_VERSION_HEX;
    chunk.Put (&vHex);
    LOGD << "version " << vMajor (vHex) << "." << vMinor (vHex) << "." << vPatch (vHex);

    _midiCCMediator.serialize (chunk);
    return SerializeParams (chunk);
}


int IPlugEffect::UnserializeState (const IByteChunk &chunk, int startPos) {
    LOGD << "Unserializing state";
    version_hex_t vHex = 0;
    startPos = chunk.Get (&vHex, startPos);
    LOGD << "version " << vMajor (vHex) << "." << vMinor (vHex) << "." << vPatch (vHex);

    startPos = _midiCCMediator.unserialize (chunk, startPos);
    return UnserializeParams (chunk, startPos);
}
