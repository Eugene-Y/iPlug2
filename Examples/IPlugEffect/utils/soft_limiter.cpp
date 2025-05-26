#include <cassert>
#include <cmath>

#include "soft_limiter.hpp"



using namespace hvoya;

inline float normalizedToDBFS (float n) {
    assert (n >= 0.f);
    return (n < 1e-15) ? -300.f : 20.f * log10f (n);
}

inline float dBFSToNormalized (float db) { // TODO LUT
    return pow (10.f, (db / 20.f));
}

inline void computeParabolaParams (float thresh, float soft,
                                   float& a, float& b, float& l) {
    // https://www.desmos.com/calculator/phkdzum9a8
    l = (1.f - soft) * thresh;
    
    if (soft == 0.f) {
        a = 0.f;
        b = 0.f;
        return;
    }
    
    a = -0.25f / (soft * thresh);
    b = l - 0.5f / a;
}

inline float limitInput (float in, float thresh,
                         float a, float b, float l) {
    float sign = in < 0.f ? -1.f : 1.f;
    float absIn = in * sign;
    
    if (absIn <= l)
        return in;
    
    if (absIn > b)
        return sign * thresh;
    
    in = absIn - b;
    in = sign * (a * in * in + thresh);
    return in;
}

SoftLimiter::SoftLimiter (float threshold,
                          float softness) {
    _threshold = 0.f;
    setThreshold (threshold);
    setSoftness (softness);
}

bool SoftLimiter::processBuffer (double** outs, size_t numFrames, size_t numChans) {
    for (size_t c = 0; c < numChans; ++c) {
        for (size_t i = 0; i < numFrames; ++i) {
            outs [c][i] = limitInput (outs [c][i], _threshold, _a, _b, _l);
        }
    }
    return true;
}

void SoftLimiter::setThreshold (float db) {
    _threshold = dBFSToNormalized(db);
    computeParabolaParams (_threshold, _softness, _a, _b, _l);
}

float SoftLimiter::getThreshold() const {
    return normalizedToDBFS (_threshold);
}

void SoftLimiter::setSoftness (float db) {
    _softness = 1.0 - dBFSToNormalized(-db);
    computeParabolaParams(_threshold, _softness, _a, _b, _l);
}

float SoftLimiter::getSoftness() const {
    return normalizedToDBFS (1.0 - _softness);
}
