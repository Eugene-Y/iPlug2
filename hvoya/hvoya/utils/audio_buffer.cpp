#include <cassert>
#include <cmath>
#include <algorithm>
#include <functional>
#include "audio_buffer.hpp"


using hvoya::AudioBuffer;


namespace {

	using namespace hvoya;

    inline std::pair <n_chan_t, n_frames_t> validateAndClamp (n_chan_t ownChans, n_frames_t ownFrames,
                                  n_chan_t otherChans, n_frames_t otherFrames,
                                  const char* opName = "") noexcept {
        if (otherChans != ownChans) {
            LOGW << "AudioBuffer::" << opName << ": own chans " << ownChans 
                 << ", other chans " << otherChans << ", using min";
            ownChans = std::min (ownChans, otherChans);
        }
        if (otherFrames != ownFrames) {
            LOGW << "AudioBuffer::" << opName << ": own frames " << ownFrames 
                 << ", other frames " << otherFrames << ", using min";
            ownFrames = std::min (ownFrames, otherFrames);
        }
        
        return { ownChans, ownFrames };
    }
    
        
    inline auto validateAndClamp (n_chan_t numChans, const char* opName = "") noexcept {
        if (numChans > AudioBuffer::MAX_CHANNELS) {
            LOGW << "AudioBuffer::" << opName << ": too many chans:  " << numChans 
                << ", using " << AudioBuffer::MAX_CHANNELS;
            numChans = AudioBuffer::MAX_CHANNELS;
        }
        return numChans;
    }
    
}


AudioBuffer::AudioBuffer (n_chan_t numChans, n_frames_t numFrames) :
    _isWrapper (false) {
    numChans = validateAndClamp (numChans, "AudioBuffer");
    _numChans = numChans;
    _numFrames = numFrames;
    resize();
}


AudioBuffer::AudioBuffer (iplug::sample** pp, n_chan_t c, n_frames_t f) :
    _isWrapper (true) {
    wrap (pp, c, f);
}


AudioBuffer::AudioBuffer (const AudioBuffer& other) :
    _isWrapper (false),
    _numChans (other._numChans),
    _numFrames (other._numFrames) {
    resize();
    copyContentFrom (other);
}


AudioBuffer::AudioBuffer (AudioBuffer&& other) noexcept : 
    _isWrapper (false),
    _data(),
    _channels{},
    _numChans (0),
    _numFrames (0) {
    swap (other);
}


void AudioBuffer::clearUnusedChanPtrs (chan_ptr_t& chans, n_chan_t n) noexcept {
    std::fill (chans.begin() + n, chans.end(), nullptr);
}


AudioBuffer AudioBuffer::getChan (n_chan_t c) {
    if (c >= _numChans) {
        LOGE << "AudioBuffer::getChan: requested non-existent chan:  " << c << ", returning chan 0";
        c = 0;
        assert (c < _numChans);
    }
	return AudioBuffer (_channels.data() + c, 1, _numFrames);
}


void AudioBuffer::resize() {
    if (isWrapper()) {
        LOGE << "AudioBuffer::resize: wrapper!";
        assert (!isWrapper());
        return;
    }
    _data.resize (_numChans * _numFrames, 0.);
    for (n_chan_t c = 0; c != _numChans; ++c)
        _channels [c] = _data.data() + _numFrames * c;
    clearUnusedChanPtrs();
}


void AudioBuffer::wrap (iplug::sample** in, n_chan_t numChans, n_frames_t frames) {
    _data.clear();
    _isWrapper = true;
    _numFrames = frames;
    _numChans = validateAndClamp (numChans, "wrap");
    for (n_chan_t c = 0; c != _numChans; ++c)
        _channels [c] = in [c];
    clearUnusedChanPtrs();
}


void AudioBuffer::unwrap() {
    if (!isWrapper()) {
        LOGE << "AudioBuffer::unwrap: not a wrapper!";
        return;
    }
        
    _data.resize (_numChans * _numFrames);
    
    for (n_chan_t c = 0; c < _numChans; ++c) {
        sample_t* dest = _data.data() + _numFrames * c;
        std::copy (_channels [c], _channels [c] + _numFrames, dest);
        _channels [c] = dest;
    }
    _isWrapper = false;
}


void AudioBuffer::fillFrom (iplug::sample** in, n_chan_t numChans, n_frames_t frames) noexcept {
    numChans = validateAndClamp (numChans, "fillFrom");
    
    const auto [chansToCopy, framesToCopy] = validateAndClamp (
        _numChans, _numFrames, numChans, frames, "fillFrom");
    // TODO: use std::copy_n
    for (n_chan_t c = 0; c != chansToCopy; ++c)
        for (n_frames_t s = 0; s != framesToCopy; ++s)
            _channels [c][s] = sample_t (in [c][s]);
}


void AudioBuffer::copyTo (iplug::sample** out, n_chan_t numChans, n_frames_t frames) noexcept {
    const auto [chansToCopy, framesToCopy] = validateAndClamp (
        _numChans, _numFrames, numChans, frames, "copyTo");
        
    // TODO: use std::copy_n
    for (n_chan_t c = 0; c != chansToCopy; ++c)
        for (n_frames_t s = 0; s != framesToCopy; ++s)
            out [c][s] = iplug::sample (_channels [c][s]);
}


AudioBuffer AudioBuffer::subBuffer (n_frames_t begin, n_frames_t len) {
    auto chans = _channels;
    for (n_chan_t c = 0; c != _numChans; ++c) chans [c] += begin;
    return AudioBuffer (chans, _numChans, len == 0 ? _numFrames - begin : len);
}


AudioBuffer::AudioBuffer (chan_ptr_t chans, n_chan_t nc, n_frames_t nf) :
    _isWrapper (true),
    _numChans (nc),
    _numFrames (nf),
    _channels (chans) {
    clearUnusedChanPtrs();
}


void AudioBuffer::setNumChannels (n_chan_t c) {
    if (isWrapper()) {
        LOGE << "AudioBuffer::setNumChannels: wrapper! ignoring";
        return;
    }
    c = validateAndClamp (c, "setNumChannels");
    if (c != _numChans) {
        _numChans = c;
        resize();
    }
}


void AudioBuffer::setNumFrames (n_frames_t s) {
    if (isWrapper()) {
        LOGE << "AudioBuffer::setNumFrames: wrapper! ignoring";
        return;
    }
    if (s != _numFrames) {
        _numFrames = s;
        resize();
    }
}


void AudioBuffer::copyContentFrom (const AudioBuffer& other) noexcept {
    const auto [chansToCopy, framesToCopy] = validateAndClamp (
        _numChans, _numFrames, other._numChans, other._numFrames, "copyContentFrom");
    
    for (n_chan_t c = 0; c < chansToCopy; ++c)
        std::copy (other._channels [c], other._channels [c] + framesToCopy, _channels [c]);
}


AudioBuffer& AudioBuffer::operator= (const AudioBuffer &other) noexcept {
    copyContentFrom (other);
    return *this;
}


AudioBuffer& AudioBuffer::operator= (AudioBuffer&& other) noexcept {
    swap (other);
    return *this;
}


AudioBuffer& AudioBuffer::operator+= (const AudioBuffer &other) noexcept {
    const auto [chansToProcess, framesToProcess] = validateAndClamp (
        _numChans, _numFrames, other._numChans, other._numFrames, "operator+=");
        
    for (n_chan_t c = 0; c < chansToProcess; ++c)
        std::transform (_channels [c], _channels [c] + framesToProcess, 
                        other._channels [c], 
                        _channels [c], std::plus<sample_t>());

    return *this;
}


AudioBuffer& AudioBuffer::operator-= (const AudioBuffer &other) noexcept {
    const auto [chansToProcess, framesToProcess] = validateAndClamp (
        _numChans, _numFrames, other._numChans, other._numFrames, "operator+=");
        
    for (n_chan_t c = 0; c < chansToProcess; ++c)
        std::transform (_channels [c], _channels [c] + framesToProcess, 
                        other._channels [c], 
                        _channels [c], std::minus<sample_t>());

    return *this;
}


AudioBuffer& AudioBuffer::operator*= (sample_t scale) noexcept {
    for (n_chan_t c = 0; c < _numChans; ++c)
        std::transform (_channels [c], _channels [c] + _numFrames, _channels [c], 
                        [scale] (sample_t s) { return s * scale; });
    return *this;
}


AudioBuffer& AudioBuffer::operator*= (const AudioBuffer &other) noexcept {
    const auto [chansToProcess, framesToProcess] = validateAndClamp (
        _numChans, _numFrames, other._numChans, other._numFrames, "operator*=");
    
    for (n_chan_t c = 0; c < chansToProcess; ++c)
        std::transform (_channels [c], _channels [c] + framesToProcess, 
                        other._channels [c], 
                        _channels [c], std::multiplies<sample_t>());

    return *this;
}


void AudioBuffer::swap (AudioBuffer& other) noexcept {
    std::swap (_isWrapper, other._isWrapper);
    std::swap (_data,      other._data);
    std::swap (_channels,  other._channels);
    std::swap (_numChans,  other._numChans);
    std::swap (_numFrames, other._numFrames);
}
