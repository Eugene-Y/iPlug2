#include <cassert>
#include <cmath>
#include "audio_buffer.hpp"


using hvoya::AudioBuffer;


AudioBuffer::AudioBuffer() {
    _numChans = 2;
    _numFrames = 512;
    resize();
}


AudioBuffer::AudioBuffer (iplug::sample** in, n_chan_t numChans, n_frames_t frames) {
    wrap (in, numChans, frames);
}


AudioBuffer AudioBuffer::getChan (n_chan_t c) {
		assert (c < _numChans);
		return AudioBuffer (_channels.data() + c, 1, _numFrames);
}


void AudioBuffer::resize() {
		assert (!isWrapper());
    _data = std::vector <sample_t> (_numChans * _numFrames, 0.);
    for (n_chan_t c = 0; c != _numChans; ++c)
        _channels[c] = _data.data() + _numFrames * c;
}


void AudioBuffer::wrap (iplug::sample** in, n_chan_t numChans, n_frames_t frames) {
    _data.clear();
    _numChans = numChans;
    _numFrames = frames;
    for (n_chan_t c = 0; c != numChans; ++c)
        _channels [c] = in [c];
}


void AudioBuffer::fillFrom (iplug::sample** in, n_chan_t numChans, n_frames_t len) {
    if (numChans != maxNumChans())
        LOGW << "AudioBuffer::fillFrom: own chans " << maxNumChans() << ", src chans " << numChans;
        
    numChans = std::min <n_chan_t> (_numChans, numChans);
    assert (len <= _numFrames);
    for (n_chan_t c = 0; c != numChans; ++c)
        for (n_frames_t s = 0; s != len; ++s)
            _channels [c][s] = sample_t (in [c][s]);
}


void AudioBuffer::copyTo (iplug::sample** out, n_chan_t numChans, n_frames_t len) {
    if (numChans != maxNumChans())
        LOGW << "AudioBuffer::copyTo: own chans " << maxNumChans() << ", dst chans " << numChans;
    numChans = std::min <n_chan_t> (_numChans, numChans);

    assert (len <= _numFrames);
    for (n_chan_t c = 0; c != numChans; ++c)
        for (n_frames_t s = 0; s != len; ++s)
            out [c][s] = iplug::sample (_channels [c][s]);
}


void AudioBuffer::setNumChannels (n_chan_t c) {
    if (c != _numChans) {
        _numChans = c;
        resize();
    }
}


void AudioBuffer::setNumFrames (n_frames_t s) {
    if (s != _numFrames) {
        _numFrames = s;
        resize();
    }
}


AudioBuffer& AudioBuffer::operator= (const AudioBuffer &other) {
    assert (other._numFrames <= _numFrames);
    assert (other._numChans == _numChans);
    for (n_chan_t c = 0; c < _numChans; ++c)
        for (n_frames_t s = 0; s < _numFrames; ++s)
            _channels [c][s] = other [c][s];
    return *this;
}

AudioBuffer& AudioBuffer::operator+= (const AudioBuffer &other) {
    assert (other._numFrames <= _numFrames);
    assert (other._numChans == _numChans);
    for (n_chan_t c = 0; c < _numChans; ++c)
        for (n_frames_t s = 0; s < _numFrames; ++s)
            _channels [c][s] += other [c][s];
    return *this;
}

AudioBuffer& AudioBuffer::operator*= (sample_t scale) {
    for (n_chan_t c = 0; c < _numChans; ++c)
        for (n_frames_t s = 0; s < _numFrames; ++s)
            _channels [c][s] *= scale;
    return *this;
}

AudioBuffer& AudioBuffer::operator*= (const AudioBuffer &other) {
    assert (other._numFrames <= _numFrames);
    assert (other._numChans == _numChans);
    for (n_chan_t c = 0; c < _numChans; ++c)
        for (n_frames_t s = 0; s < _numFrames; ++s)
            _channels [c][s] *= other [c][s];
    return *this;
}
