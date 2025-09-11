#pragma once

#include <cmath>
#include <cassert>

#include "audio_buffer.hpp"


namespace hvoya::utils {


    float fastLog2f (float);
    float fastLog10f (float);
    
    float linearToDB (float);
    float linearToDBFast (float); // yields -625 dBFS at 0.f
                                  // -0.0009 - +0.0001 dBFS error on the range [-140 - +24] dBFS.
    float dbToLinear (float);
    float dbToLinearFast (float);




    float msToSamplesFloat (float ms, float sampleRate);
    size_t msToSamples (float ms, float sampleRate);
    float samplesToMs (size_t samp, float sampleRate);
    



    float centsFactorFast (float c);
    
    template <typename T>
    T centsFactor (T c) { return std::pow (T (2), T (c) / 1200); }
    
    float noteToFreqFast (float n, float A4 = 440);
    
    template <typename T>
    T noteToFreq (T n, T A4 = 440) { return A4 * std::pow (T (2), T (n - 69) / 12); }
    
	
	template <typename T>
	inline bool smoothValue (T& smoothVal, T exactVal, T a, T eps) {
		assert (a <= 1);
		if (smoothVal == exactVal) {
			return false;
		}
		
		auto old = smoothVal;
		smoothVal = (1. - a) * smoothVal + a * (exactVal + 1e-25);
		if (smoothVal == old || std::abs (smoothVal - exactVal) <= eps)
			smoothVal = exactVal;
		return true;
	}


    template <typename T>
    void smoothAndSetIfNotEqual (T& valSmooth, T val, T a, T eps = 1e-7,
                                 std::function <void()> setter = [](){}) {
        if (valSmooth == val)
            return;
        
        auto old = valSmooth;
        valSmooth = (1. - a) * valSmooth + a * (val + 1e-25);
        if (valSmooth == old || std::abs (valSmooth - val) <= eps)
            valSmooth = val;
        setter();
    }
	
	
	template <typename T>
	T randomVal (T lo = -1, T hi = 1) {
		return lo + (hi - lo) * ((T) rand() / (RAND_MAX));
	}


    template <typename T>
    auto reverseRange (T* first, size_t len) {
        auto last = first + len;
        while (first != last && first != --last) {
            std::swap <T> (*(first++), *last);
        }
    };

    void interleave   (const AudioBuffer& src, AudioBuffer& dest);
    void deinterleave (const AudioBuffer& src, AudioBuffer& dest);
    
} // ns hvoya::utils
