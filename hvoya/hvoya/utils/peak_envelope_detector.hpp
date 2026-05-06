#pragma once

#include <span>
#include <cmath>
#include <cassert>

#include <hvoya/utils/types.hpp>


namespace hvoya {


	template <typename T = sample_t>
	class PeakEnvelopeDetector {
	public:

		PeakEnvelopeDetector (T sampleRate = T (48000),
							  T attackMs = T (1), T releaseMs = T (100)) noexcept
			: _sampleRate (sampleRate), _attackMs (attackMs), _releaseMs (releaseMs)
		{ _updateCoeffs(); }

		void setSampleRate (T sr) noexcept {
			_sampleRate = sr;
			_updateCoeffs();
		}

		void setAttackMs (T ms) noexcept {
			_attackMs = ms;
			_attackCoeff = _msToCoeff (ms);
		}

		void setReleaseMs (T ms) noexcept {
			_releaseMs = ms;
			_releaseCoeff = _msToCoeff (ms);
		}

		void reset() noexcept { _state = T (0); }

		inline T processSample (T x) noexcept {
			return processRectified (std::abs (x));
		}

		inline T processRectified (T rect) noexcept {
			const T diff = rect - _state;
			_state += (diff > T (0) ? _attackCoeff : _releaseCoeff) * diff;
			return _state;
		}

		void process (std::span<const T> in, std::span<T> out) noexcept {
			assert (in.size() == out.size());
			for (size_t i = 0; i < in.size(); ++i)
				out [i] = processSample (in [i]);
		}

		T state() const noexcept { return _state; }

	private:
		T _sampleRate   { T (48000) };
		T _attackMs     { T (1) };
		T _releaseMs    { T (100) };
		T _attackCoeff  { T (0) };
		T _releaseCoeff { T (0) };
		T _state        { T (0) };

		void _updateCoeffs() noexcept {
			_attackCoeff  = _msToCoeff (_attackMs);
			_releaseCoeff = _msToCoeff (_releaseMs);
		}

		T _msToCoeff (T ms) const noexcept {
			return T (1) - std::exp (T (-1) / (ms * _sampleRate / T (1000)));
		}
	};


} // namespace hvoya
