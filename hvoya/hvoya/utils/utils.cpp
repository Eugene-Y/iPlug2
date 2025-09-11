#include <cstdint>
#include <algorithm>

#include "types.hpp"
#include "hxa_pow.hpp"
#include "utils.hpp"


namespace hvoya::utils {

    const fast_approx::PowFast fastPowf (12);
    
    const float baseChange2_10f (log10f (2.f));
    
    
    // -0.0009 - +0.0001 dBFS error on the range [-140 - +24] dBFS.
    // yields -625 dBFS at 0.f
    // https://github.com/pmineiro/fastapprox
    // rewritten without undefined behavior: https://gcc.godbolt.org/z/5Y89zMavd
    // http://www.machinedlearnings.com/2011/06/fast-approximate-logarithm-exponential.html
    
    float fastLog2f (float x) {
        static_assert (sizeof (float) == sizeof (int32_t), "type size mismatch");
        uint32_t i1 = *reinterpret_cast<uint32_t*> (&x);
        uint32_t i2 = (i1 & 0x007FFFFF) | 0x3f000000;
        float y = i1 * 1.1920928955078125e-7f;

        return  y - 124.22551499f
                  - 1.498030302f * *reinterpret_cast<float*> (&i2)
                  - 1.72587999f / (0.3520887068f + *reinterpret_cast<float*> (&i2));
    }
    
    
    float fastLog10f (float x) {
        return baseChange2_10f * fastLog2f (x);
    }
    
    
    const float minDBLoudness = -140.f;
    float minLinLoudness = powf (10.f, minDBLoudness / 20.f);

    
    float linearToDB (float v) {
        assert (v >= 0.f);
        return (v > minLinLoudness) ? 20.f * log10f (v) : minDBLoudness;
    }
    
    
    float linearToDBFast (float v) {
        assert (v >= 0.f);
        return 20.f * fastLog10f (v); // -0.0009 - +0.0001 dBFS error
    }

    
    float dbToLinear (float db) {
        return db > minDBLoudness ? powf (10.f, db / 20.f) : 0.f;
    }
    
    
    float dbToLinearFast (float db) {
        return fastPowf.ten (db / 20.f); // +- 0.0008 dBFS error
    }


    float msToSamplesFloat (float ms, float sampleRate) {
        return sampleRate * ms / 1000.f;
    }

    
    size_t msToSamples (float ms, float sampleRate) {
        return size_t (std::max (1.f, std::roundf (sampleRate * ms / 1000.f)));
    }


    float samplesToMs (size_t samp, float sampleRate) {
        return 1000.f * samp / sampleRate;
    }
    
    
    float noteToFreqFast (float n, float A4) {
        return A4 * fastPowf.two ((n - 69.f) / 12.f);
    }
    
    
    float centsFactorFast (float c) {
        return fastPowf.two (c / 1200);
    }


    void interleave (const AudioBuffer& src, AudioBuffer& dest) {
        assert (dest.numChans() == 1);
        assert (src.numFrames() * src.numChans() == dest.numFrames());
        for (size_t c = 0; c < src.numChans(); ++c) {
            for (size_t i = 0; i < src.numFrames(); ++i) {
                dest [0][i * src.numChans() + c] = src [c][i];
            }
        }
    }


    void deinterleave (const AudioBuffer& src, AudioBuffer& dest) {
        assert (src.numChans() == 1);
        assert (dest.numFrames() * dest.numChans() == src.numFrames());
        for (size_t c = 0; c < dest.numChans(); ++c) {
            for (size_t i = 0; i < dest.numFrames(); ++i) {
                dest [c][i] = src [0][i * dest.numChans() + c];
            }
        }
    }
    
} // ns hvoya::utils
