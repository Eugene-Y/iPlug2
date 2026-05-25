#pragma once

#include <cmath>
#include <cassert>
#include <numeric>
#include <span>

#include "audio_buffer.hpp"


namespace hvoya::utils {
    
    template <typename T>
    inline T sign (T v) { return v < T(0) ? T(-1) : T(1); }

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
    T freqToNote (T f, T A4 = 440) { return T (69) + T (12) * std::log2 (f / A4); }
    
	
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
    void smoothAndSetIfNotEqual (T& valSmooth, T valTarget, T a, T eps = 1e-7,
                                 std::function <void()> setter = [](){}) {
        if (valSmooth == valTarget)
            return;
        
        auto old = valSmooth;
        valSmooth = (1. - a) * valSmooth + a * (valTarget + 1e-25);
        if (valSmooth == old || std::abs (valSmooth - valTarget) <= eps)
            valSmooth = valTarget;
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
    }
	



    template <typename T>
    inline void derivative (std::span<const T> in, std::span<T> out, T* pPrevVal) {
		assert (!in.empty());
        assert ((in.data() == out.data() ||
                out.data() + out.size() <= in.data() ||
                in.data() + in.size() <= out.data()) && 
			   "adjacent_difference takes either spans with same start or not overlaping");

		T lastOriginal = in.back();
        std::adjacent_difference (in.begin(), in.end(), out.begin());
        out.front() = in.front() - *pPrevVal;
		*pPrevVal = lastOriginal;
    }


    template <typename T>
    inline void derivativeInplace (std::span<T> data, T* pPrevVal) {
		assert (!data.empty());
		T lastOriginal = data.back();
        std::adjacent_difference (data.begin(), data.end(), data.begin());
        data.front() -= *pPrevVal;
		*pPrevVal = lastOriginal;
    }


    template <typename T>
    inline void derivativeNthInplace (std::span<T> data, size_t order, std::span<T> prevVals) {
		assert (prevVals.size() >= order);
        for (size_t n = 0; n < order; ++n)
            derivativeInplace (data, &(prevVals [n]));
    }


    template <typename T>
    inline void integrateInplace (std::span<T> data, T* pAccum) {
		assert (!data.empty());
        data.front() += *pAccum;
        for (size_t i = 1; i < data.size(); ++i)
            data[i] += data[i - 1];
        *pAccum = data.back();
    }


    template <typename T>
    inline void integrateNthInplace (std::span<T> data, size_t order, std::span<T> accums) {
		assert (accums.size() >= order);
        // reverse order: undo D_n first, then D_{n-1}, ..., then D_1
        for (size_t n = order; n > 0; --n)
            integrateInplace (data, &(accums [n - 1]));
    }


    void interleave   (const AudioBuffer& src, AudioBuffer& dest);
    void deinterleave (const AudioBuffer& src, AudioBuffer& dest);

} // ns hvoya::utils
