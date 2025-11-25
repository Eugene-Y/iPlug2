#pragma once

#include <numbers>
#include <array>
#include <cassert>

namespace hvoya {
    
	template <typename T = double, size_t numChans = 1>
	class DCBlocker {
		public:

		void reset() noexcept {
			for (auto& v : _prevX) v = 0;
			for (auto& v : _prevY) v = 0;
		}

		void setCutoff (T hz, T sampleRate) noexcept {
			assert (hz > 0);
			assert (hz < sampleRate / 2);
			assert (sampleRate > 0);
			_r = 1. - 2. * std::numbers::pi_v <T> * hz / sampleRate;
		}

		inline void processBlock (T* data, size_t numFrames, size_t chan = 0) noexcept {
			assert (chan < numChans);
			for (size_t n = 0; n < numFrames; ++n)
				processSample (data [n], chan);
		}

		inline void processSample (T& in, size_t chan = 0) noexcept {
			T x = in;
			T y = in - _prevX [chan] + _r * _prevY [chan];
			in = y;
			_prevX [chan] = x;
			_prevY [chan] = y;
		}

		private:
			std::array <T, numChans> _prevX { 0 };
			std::array <T, numChans> _prevY { 0 };
			T _r { 0.995 };
	};
    
} // ns hvoya
