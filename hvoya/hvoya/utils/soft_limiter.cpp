#include <cassert>
#include <cmath>

#include "soft_limiter.hpp"



using namespace hvoya;

inline float normalizedToDBFS (sample_t n) {
    assert (n >= 0);
    return (n < 1e-15) ? -300 : 20 * log10 (n);
}

inline sample_t dBFSToNormalized (sample_t db) { // TODO LUT
    return pow (10, (db / 20));
}

inline void computeParabolaParams (sample_t thresh, sample_t soft,
                                   sample_t& a, sample_t& b, sample_t& l) {
    // https://www.desmos.com/calculator/phkdzum9a8
    l = (1 - soft) * thresh;
    
    if (soft == 0) {
        a = 0;
        b = 0;
        return;
    }
    
    a = -0.25 / (soft * thresh);
    b = l - 0.5 / a;
}

inline sample_t limitInput (sample_t in, sample_t thresh,
                            sample_t a, sample_t b, sample_t l) {
    sample_t sign = in < 0 ? -1 : 1;
    sample_t absIn = in * sign;
    
    if (absIn <= l)
        return in;
    
    if (absIn > b)
        return sign * thresh;
    
    in = absIn - b;
    in = sign * (a * in * in + thresh);
    return in;
}

SoftLimiter::SoftLimiter (sample_t threshold,
                          sample_t softness) {
    _threshold = 0;
    setThreshold (threshold);
    setSoftness (softness);
}

void SoftLimiter::processBuffer (sample_t** outs, n_chan_t chans, n_frames_t frames) {
    for (n_chan_t c = 0; c < chans; ++c)
        for (n_frames_t i = 0; i < frames; ++i)
            outs [c][i] = limitInput (outs [c][i], _threshold, _a, _b, _l);
}


void SoftLimiter::processBuffer (AudioBuffer in) {
		processBuffer (in.data(), in.numChans(), in.numFrames());
}


void SoftLimiter::setThreshold (sample_t db) {
    _threshold = dBFSToNormalized(db);
    computeParabolaParams (_threshold, _softness, _a, _b, _l);
}

sample_t SoftLimiter::getThreshold() const {
    return normalizedToDBFS (_threshold);
}

void SoftLimiter::setSoftness (sample_t db) {
    _softness = 1.0 - dBFSToNormalized (-db);
    computeParabolaParams(_threshold, _softness, _a, _b, _l);
}

sample_t SoftLimiter::getSoftness() const {
    return normalizedToDBFS (1.0 - _softness);
}
