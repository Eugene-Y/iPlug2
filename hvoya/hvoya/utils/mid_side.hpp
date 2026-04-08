#pragma once

#include <cassert>
#include <hvoya/utils/audio_buffer.hpp>


namespace hvoya::utils {


	/// Encode stereo L/R into mid/side representation.
	/// buf must have >= 2 channels. temp must have >= 1 channel with >= buf.numFrames() frames.
	/// After encoding: channel 0 contains mid, channel 1 contains side.
	inline void encodeMidSide (AudioBuffer& buf, AudioBuffer& temp) noexcept {
		assert (buf.numChans() >= 2);
		assert (temp.numFrames() >= buf.numFrames());

		// save left channel to temp
		temp.copyChanFrom (buf, 0, 0);

		const auto* t = temp[0];

		// mid = (L + R) * 0.5 -> channel 0
		buf.transform ([t, r = buf[1]] (auto, auto i) {
			return (t[i] + r[i]) * 0.5;
		}, 0);

		// side = (L_orig - R) * 0.5 -> channel 1
		buf.transform ([t] (auto r, auto i) {
			return (t[i] - r) * 0.5;
		}, 1);
	}


	/// Decode mid/side back to stereo L/R.
	/// buf channel 0 = mid, channel 1 = side.
	/// After decoding: channel 0 = L, channel 1 = R.
	inline void decodeMidSide (AudioBuffer& buf, AudioBuffer& temp) noexcept {
		assert (buf.numChans() >= 2);
		assert (temp.numFrames() >= buf.numFrames());

		// save mid to temp
		temp.copyChanFrom (buf, 0, 0);

		const auto* t = temp[0];

		// L = mid + side -> channel 0
		buf.transform ([t, s = buf[1]] (auto, auto i) {
			return t[i] + s[i];
		}, 0);

		// R = mid - side -> channel 1
		buf.transform ([t] (auto s, auto i) {
			return t[i] - s;
		}, 1);
	}


	/// Apply mid/side balance to a stereo buffer in-place.
	/// balance: 0 = full mid (mono), 1 = neutral (pass-through), 2 = full side.
	/// Gain-neutral: midGain = min(1, 2-balance), sideGain = min(1, balance).
	/// [0,1] fades out side only; [1,2] fades out mid only.
	inline void applyMidSideBalance (AudioBuffer& buf, AudioBuffer& temp, sample_t balance) noexcept {
		assert (buf.numChans() >= 2);
		assert (temp.numFrames() >= buf.numFrames());
		assert (balance >= 0.0 && balance <= 2.0);

		// midGain  = min(1, 2 - balance), sideGain = min(1, balance)
		// M = (L+R)/2, S = (L-R)/2
		// L_out = midGain * M + sideGain * S = L * a + R * c
		// R_out = midGain * M - sideGain * S = L * c + R * a
		const sample_t midGain  = std::min (sample_t (1), sample_t (2) - balance);
		const sample_t sideGain = std::min (sample_t (1), balance);
		const sample_t a = (midGain + sideGain) * sample_t (0.5);
		const sample_t c = (midGain - sideGain) * sample_t (0.5);

		temp.copyChanFrom (buf, 0, 0);

		const auto* t = temp[0];

		buf.transform ([r = buf[1], a, c] (auto L, auto i) {
			return L * a + r[i] * c;
		}, 0);

		buf.transform ([t, a, c] (auto R, auto i) {
			return t[i] * c + R * a;
		}, 1);
	}


	/// Per-sample ramped mid/side balance.
	/// balanceRamp channel 0 holds per-sample balance values in [0, 2].
	/// Same gain-neutral formula as applyMidSideBalance, computed per sample.
	inline void applyMidSideBalanceRamped (AudioBuffer& buf, AudioBuffer& temp,
										   const AudioBuffer& balanceRamp) noexcept {
		assert (buf.numChans() >= 2);
		assert (temp.numFrames() >= buf.numFrames());
		assert (balanceRamp.numFrames() >= buf.numFrames());

		temp.copyChanFrom (buf, 0, 0);

		const auto* t    = temp[0];
		const auto* ramp = balanceRamp[0];

		buf.transform ([r = buf[1], ramp] (auto L, auto i) {
			const auto bal = ramp[i];
			const auto midG  = std::min (sample_t (1), sample_t (2) - bal);
			const auto sideG = std::min (sample_t (1), bal);
			const auto a = (midG + sideG) * sample_t (0.5);
			const auto c = (midG - sideG) * sample_t (0.5);
			return L * a + r[i] * c;
		}, 0);

		buf.transform ([t, ramp] (auto R, auto i) {
			const auto bal = ramp[i];
			const auto midG  = std::min (sample_t (1), sample_t (2) - bal);
			const auto sideG = std::min (sample_t (1), bal);
			const auto a = (midG + sideG) * sample_t (0.5);
			const auto c = (midG - sideG) * sample_t (0.5);
			return t[i] * c + R * a;
		}, 1);
	}


} // namespace hvoya::utils
