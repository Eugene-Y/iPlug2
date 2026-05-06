#pragma once

#include <span>
#include <algorithm>
#include <cassert>

#include <hvoya/utils/types.hpp>
#include <hvoya/utils/peak_envelope_detector.hpp>
#include <hvoya/utils/hilbert_envelope_detector.hpp>


namespace hvoya {


// Combines PeakEnvelopeDetector and HilbertEnvelopeDetector:
//   envelope = max(peak, hilbert)
// Peak provides fast attack and transient response.
// Hilbert provides smooth envelope for sustained content.
// Peak release acts as smoothing.

template <typename T = sample_t>
class CompoundEnvelopeDetector {
public:

	using hilbert_t = HilbertEnvelopeDetector<T>;
	using peak_t    = PeakEnvelopeDetector<T>;

	CompoundEnvelopeDetector (T sampleRate = T (48000),
	                          typename hilbert_t::Quality quality = hilbert_t::Quality::High) noexcept
		: _peak    (sampleRate, T (1), T (20))
		, _hilbert (sampleRate, quality)
	{}

	void setSampleRate (T sr) noexcept {
		_peak.setSampleRate (sr);
		_hilbert.setSampleRate (sr);
	}

	void setAttackMs  (T ms) noexcept { _peak.setAttackMs (ms); }
	void setReleaseMs (T ms) noexcept { _peak.setReleaseMs (ms); }

	void setQuality (typename hilbert_t::Quality q) noexcept {
		_hilbert.setQuality (q);
	}

	void reset() noexcept {
		_peak.reset();
		_hilbert.reset();
	}

	inline T processSample (T x) noexcept {
		const T hilbert  = _hilbert.processSample (x);
		const T combined = std::max (std::abs (x), hilbert);
		return _peak.processRectified (combined);
	}

	void process (std::span<const T> in, std::span<T> out) noexcept {
		assert (in.size() == out.size());
		for (size_t i = 0; i < in.size(); ++i)
			out [i] = processSample (in [i]);
	}

private:
	peak_t    _peak;
	hilbert_t _hilbert;
};


} // namespace hvoya
