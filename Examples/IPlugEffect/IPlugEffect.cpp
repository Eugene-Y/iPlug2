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
        _midiCCMediator (this),
        _master_mix (1.) {
    ++_sNumInstances;
    logBuildInfo();

    resetPrevParamVals();
    initializeParams();

    #if IPLUG_EDITOR // http://bit.ly/2S64BDd
        mMakeGraphicsFunc = [&]() {
            float scale = _uiRescaleIsPending.load (std::memory_order_acquire)
                ? _pendingUIScale
                : GetScaleForScreen (PLUG_WIDTH, PLUG_HEIGHT);
            _uiRescaleIsPending.store (false, std::memory_order_release);
            return MakeGraphics (*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, scale);
        };
		initializeLayout();
    #endif
    
    _softLimiter.setThreshold (12);
    _softLimiter.setSoftness (3);

}


#if IPLUG_DSP

    void IPlugEffect::ProcessBlock (sample** ins, sample** outs, int numFrames) {
		++_bufId;
		_hostInfoModel.updateAll (numFrames);

		const auto numChans = _hostInfoModel.getMinChans();

		hvoya::AudioBuffer input (ins, numChans, numFrames);
		assert (input.isWrapper());

		hvoya::AudioBuffer cleanCopy (input);
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
	if (_needsUIUpdate.exchange (false, std::memory_order_acq_rel))
		tryUpdateLayout();
}


void IPlugEffect::OnActivate (bool enable) {
	//LOGD << "OnActivate " << enable;
    OnIdle();
}


void IPlugEffect::OnReset() {
    resetPrevParamVals();
    _hostInfoModel.reset();
	_needsUIUpdate.store (true, std::memory_order_release);
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
    bool ok = serializeParams (chunk);
    ok &= serializeUIScale (chunk);
    return ok;
}


int IPlugEffect::UnserializeState (const IByteChunk &chunk, int pos) {
    LOGD << "Unserializing state";
    version_hex_t vHex = 0;
    pos = chunk.Get (&vHex, pos);
    LOGD << "version " << vMajor (vHex) << "." << vMinor (vHex) << "." << vPatch (vHex);

    if (pos >= 0) pos = _midiCCMediator.unserialize (chunk, pos);
    pos = unserializeParams (chunk, pos);
    pos = unserializeUIScale (chunk, pos);
    return pos;
}


bool IPlugEffect::serializeParams (IByteChunk& chunk) const {
	bool savedOK = true;
	double v;
	v = hvoya::num_params;
	savedOK &= (chunk.Put(&v) > 0);

	for (int i = 0; i < hvoya::num_params && savedOK; ++i) {
		const IParam* pParam = GetParam (i);
		v = pParam->Value();
		savedOK &= (chunk.Put(&v) > 0);
	}
	return savedOK;
}


int IPlugEffect::unserializeParams (const IByteChunk& chunk, int startPos) {
	auto pos = startPos;
	ENTER_PARAMS_MUTEX

	double numParamsInChunk;
	const int numParamsInCurrentVersion = hvoya::num_params;
	pos = chunk.Get (&numParamsInChunk, pos);

	const int nInChunk = int (numParamsInChunk);
	if (nInChunk < 0 || nInChunk > 1024) {
		LOGE << "unserializeParams: invalid param count in chunk: " << numParamsInChunk;
		LEAVE_PARAMS_MUTEX
		return -1;
	}

	for (int i = 0; i < nInChunk && pos >= 0; ++i) {
		double v = 0.0;
		auto prevPos = pos;
		pos = chunk.Get (&v, pos);
		if (pos >= 0) {
			if (i < numParamsInCurrentVersion) {
				IParam* pParam = GetParam (i);
				pParam->Set(v);
			}
			else {
				LOGD << "skipping param " << i;
			}
		}
		else {
			pos = prevPos;
			break;
		}
	}

	OnParamReset (kPresetRecall);
	LEAVE_PARAMS_MUTEX

	return pos;
}


int IPlugEffect::unserializeUIScale (const IByteChunk &chunk, int pos) {
	double uiScale = 1;
	pos = chunk.Get (&uiScale, pos);
	if (uiScale < 0 || uiScale > 20) {
		LOGE << "UI scale is garbage!";
		uiScale = 1;
	}
	_pendingUIScale = uiScale;
	_uiRescaleIsPending.store (true, std::memory_order_release);
	_needsUIUpdate.store (true, std::memory_order_release);
	return pos;
}


bool IPlugEffect::serializeUIScale (IByteChunk& chunk) const {
	double uiScale = GetUI() ? GetUI()->GetDrawScale() : _pendingUIScale;
	return (chunk.Put (&uiScale) > 0);
}
