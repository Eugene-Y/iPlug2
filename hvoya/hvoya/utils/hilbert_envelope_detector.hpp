#pragma once

#include <span>
#include <cmath>
#include <cassert>
#include <array>
#include <algorithm>

#include "HIIR/PolyphaseIIR2Designer.h"
#include <hvoya/utils/types.hpp>


namespace hvoya {


// IIR Hilbert envelope detector based on hiir PhaseHalfPiTpl logic.
//
// Two complete allpass filter states (_filter[0], _filter[1]) alternate each sample.
// Per sample: out_0 = x[n], out_1 = x[n-1], both run through _filter[_phase].
// Allpass form (negative): y = (x + y_prev) * coef - x_prev
// Envelope = sqrt(out_0² + out_1²)
//
// Quality controls the lowest tracked frequency (independent of sample rate):
//   Low    ~100 Hz
//   Medium  ~40 Hz
//   High    ~20 Hz  (default)
//   Ultra   ~10 Hz
//
// kMaxCoefs was determined by running PolyphaseIIR2Designer::compute_coefs for all
// Quality × SR combinations up to 192 kHz at kAttenuation dB:
//   max nc = 14 (Ultra / 192 kHz).
// For SR > 192 kHz the lower frequency bound shifts up proportionally.

template <typename T = sample_t>
class HilbertEnvelopeDetector {
public:

	enum class Quality { Low, Medium, High, Ultra };

	static constexpr size_t kMaxCoefs  = 14;
	static constexpr size_t kMaxStages = kMaxCoefs + 2; // first 2 are input memory holders

	HilbertEnvelopeDetector (T sampleRate = T (48000),
	                         Quality quality = Quality::High) noexcept
		: _sampleRate (sampleRate), _quality (quality)
	{ _recompute(); }

	void setSampleRate (T sr) noexcept {
		_sampleRate = sr;
		_recompute();
	}

	void setQuality (Quality q) noexcept {
		_quality = q;
		_recompute();
	}

	void reset() noexcept {
		for (auto& f : _filter)
			for (auto& s : f) s.mem = T (0);
		_prev         = T (0);
		_phase        = 0;
		_lastEnvelope = T (0);
	}

	inline T processSample (T x) noexcept {
		T out0 = x;
		T out1 = _prev;

		auto* f = _filter [_phase].data();
		for (size_t k = 0; k + 1 < _nc; k += 2) {
			const size_t cnt = k + 2;
			const T tmp0 = (out0 + f [cnt    ].mem) * f [cnt    ].coef - f [k    ].mem;
			const T tmp1 = (out1 + f [cnt + 1].mem) * f [cnt + 1].coef - f [k + 1].mem;
			f [k    ].mem = out0;
			f [k + 1].mem = out1;
			out0 = tmp0;
			out1 = tmp1;
		}
		if (_nc & 1) {
			// REMAINING=1 in StageProcTpl: process only out0, save out1 and result explicitly
			const size_t k   = _nc - 1;
			const size_t cnt = k + 2;
			const T tmp0 = (out0 + f [cnt].mem) * f [cnt].coef - f [k].mem;
			f [k    ].mem = out0;
			f [k + 1].mem = out1;  // store out1 (spl_1 passthrough)
			out0 = tmp0;
			f [cnt  ].mem = out0;  // update y_prev for this stage
		} else {
			// REMAINING=0 in StageProcTpl: save final outputs as y_prev for last stage pair
			f [_nc    ].mem = out0;
			f [_nc + 1].mem = out1;
		}

		_prev  = x;
		_phase ^= 1;

		_lastEnvelope = std::sqrt (out0 * out0 + out1 * out1);
		return _lastEnvelope;
	}

	void process (std::span<const T> in, std::span<T> out) noexcept {
		assert (in.size() == out.size());
		for (size_t i = 0; i < in.size(); ++i)
			out [i] = processSample (in [i]);
	}

	T       state()       const noexcept { return _lastEnvelope; }
	int     numCoefs()    const noexcept { return int (_nc); }
	T       sampleRate()  const noexcept { return _sampleRate; }
	Quality quality()     const noexcept { return _quality; }

private:
	static constexpr double kAttenuation = 60.0;
	static constexpr double kQualityHz[] = { 100.0, 40.0, 20.0, 10.0 };

	struct Stage { T coef { T (0) };  T mem { T (0) }; };

	T       _sampleRate;
	Quality _quality;
	T       _prev         { T (0) };
	T       _lastEnvelope { T (0) };
	int     _phase        { 0 };
	size_t  _nc           { 0 };

	std::array<Stage, kMaxStages> _filter [2];

	void _recompute() noexcept {
		const double lowestHz   = kQualityHz [static_cast<int> (_quality)];
		const double transition = std::clamp (lowestHz / (double (_sampleRate) * 0.5), 1e-5, 0.499);

		double raw [kMaxCoefs + 4];
		const int nc = hiir::PolyphaseIIR2Designer::compute_coefs (raw, kAttenuation, transition);
		assert (nc > 0 && size_t (nc) <= kMaxCoefs); // increase kMaxCoefs if this fires

		_nc = size_t (nc);
		for (size_t i = 0; i < _nc; ++i) {
			_filter [0] [i + 2].coef = T (raw [i]);
			_filter [1] [i + 2].coef = T (raw [i]);
		}

		reset();
	}
};


} // namespace hvoya
