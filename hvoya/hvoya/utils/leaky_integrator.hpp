#pragma once

#include <numbers>
#include <array>
#include <cassert>
#include <cmath>

namespace hvoya {
    
    template <typename T = double>
    inline T calcLeakyIntegratorCoeffForMs (T timeMs, T sampleRate) noexcept {
        assert (timeMs > 0);
        assert (sampleRate > 0);
        T timeSamples = timeMs * sampleRate / T (1000.);
        return T (1.) - std::exp (T (-1.) / timeSamples);
    }
    
    
	template <typename T = double, size_t numChans = 1>
	class LeakyIntegrator {
        public:

            void reset() noexcept {
                for (auto& v : _state) v = 0;
            }
            
            inline void setCoeff (T c) noexcept {
                assert (c >= 0 && c <= 1);
                _coeff = c;
            }

            void setTimeMs (T timeMs, T sampleRate) noexcept {
                T c = calcLeakyIntegratorCoeffForMs <T> (timeMs, sampleRate);
                setCoeff (c);
            }

            inline void processBlock (T* data, size_t numFrames, size_t chan = 0) noexcept {
                assert (chan < numChans);
                for (size_t n = 0; n < numFrames; ++n)
                    processSample (data [n], chan);
            }

            inline void processSample (T& in, size_t chan = 0) noexcept {
                assert (chan < numChans);
                _state [chan] += _coeff * (in - _state [chan]);
                in = _state [chan];
            }

            T getState (size_t chan = 0) const noexcept {
                assert (chan < numChans);
                return _state [chan];
            }

        private:
            std::array <T, numChans> _state { 0 };
            T _coeff { 0.1 };
	};
    
} // ns hvoya
