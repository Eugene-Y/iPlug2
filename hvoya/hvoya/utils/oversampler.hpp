#pragma once

// hvoya::Oversampler
// Polyphase IIR oversampling via HIIR FPUUpsampler2x / FPUDownsampler2x.
//
// Supports 1x (bypass), 2x, 4x, 8x, 16x.
// SNR 144dB, 0.99 passband
//
// Usage:
//   Oversampler os;
//   os.setFactor (4);
//
//   // in ProcessBlock:
//   _osBuffer.setNumFrames (numFrames * os.getFactor());
//   os.upsample (input, _osBuffer);
//   sandModule.process (_osBuffer);
//   os.downsample (_osBuffer, output);
//
// Buffers and filters grow automatically to fit incoming data.
// No prepare() call required — just setFactor() and go.
//

// TODO: FIXME 16x does not work correctly

#include <cassert>
#include <vector>

#include "HIIR/FPUDownsampler2x.h"
#include "HIIR/FPUUpsampler2x.h"

#include <hvoya/utils/log/logger.hpp>
#include <hvoya/utils/audio_buffer.hpp>


namespace hvoya {


class Oversampler {

	using T = AudioBuffer::sample_type;

public:
    static constexpr size_t MAX_FACTOR = 16;

	/*
	 motivation behind constexpr NC_*X and coeffs:
	 if one needs resampling, one needs quality.
	 the difference between NC for different SNR is too small to add complexity to the code.
	 the most difference is in 1x->2x resampling

	 tbw2x  = 0.01
	 tbw4x  = 0.25
	 tbw8x  = 0.375
	 tbw16x = 0.4375

	 hiir::PolyphaseIIR2Designer::compute_nbr_coefs_from_proto (144, tbw2x) ...
	 num coeff needed for 144 dB SNR: 17, 5, 4, 3
	 for 120 dB: 14, 5, 3, 2
	 for 96 dB:  12, 4, 3, 2
	 */

    static constexpr int NC_2X  = 17;
    static constexpr int NC_4X  = 5;
    static constexpr int NC_8X  = 4;
    static constexpr int NC_16X = 3;

	/*
	 hiir::PolyphaseIIR2Designer::compute_coefs_spec_order_tbw (_coeffs2x.data(), NC_2X, tbw2x);
	 ...
	 */

	static constexpr std::array <double, NC_2X>  _coeffs2x {
		0.018942752654462831, 0.072985610008303281,
		0.15460915785355853, 0.25366174850405637,
		0.35978656070566989, 0.46429695601183418,
		0.56109869787919453, 0.64674643563239975,
		0.71997165506529659, 0.78102575274895092,
		0.83106642722637059, 0.87168924219311117,
		0.90461999470878962, 0.93154195996318379,
		0.95402098678607827, 0.97349560292731629,
		0.99130873389993557
	};

	static constexpr std::array <double, NC_4X>  _coeffs4x {
		0.028414445279188464, 0.11393344727958085,
		0.25906131103886926, 0.4738722544857808,
		0.78701940115286972
	};

	static constexpr std::array <double, NC_8X>  _coeffs8x {
		0.033467890099398885, 0.14067270887069569,
		0.34665643749448355, 0.71373674368792805
	};

	static constexpr std::array <double, NC_16X> _coeffs16x {
		0.053011480741538136, 0.23472966334459619,
		0.63870867303046996
	};


    Oversampler() = default;


    void setFactor (size_t factor) {
        assert (factor == 1 || factor == 2 || factor == 4 || factor == 8 || factor == 16);
        _factor = factor;
    }

    size_t getFactor() const noexcept { return _factor; }


    void reset() {
        for (auto& f : _up2x)  f.clear_buffers();
        for (auto& f : _up4x)  f.clear_buffers();
        for (auto& f : _up8x)  f.clear_buffers();
        for (auto& f : _up16x) f.clear_buffers();
        for (auto& f : _dn2x)  f.clear_buffers();
        for (auto& f : _dn4x)  f.clear_buffers();
        for (auto& f : _dn8x)  f.clear_buffers();
        for (auto& f : _dn16x) f.clear_buffers();
    }
    
    
    static constexpr size_t latencyForFactor (size_t f) {
        switch (f) {
            default: 
            case 1:  return 0;
            case 2:  return 6;
            case 4:  return 7;
            case 8:  return 8;
            case 16: return 8;
        }
    }
    
    
    size_t getCurrentLatency() const noexcept {
        return latencyForFactor (_factor);
    }


    void upsample (const hvoya::AudioBuffer& src, hvoya::AudioBuffer& dst) {
        assert (src.numChans() == dst.numChans());
        assert (dst.numFrames() == src.numFrames() * _factor);

        if (_factor == 1) {
            dst.fillFrom (src);
            return;
        }

        const auto nFrames = static_cast<long> (src.numFrames());
        const n_chan_t nChans = src.numChans();
        ensureCapacity (src.numFrames(), nChans);

        for (n_chan_t c = 0; c < nChans; ++c) {
            if (_factor == 2) {
                _up2x[c].process_block (dst[c], src[c], nFrames);
            }
            else if (_factor == 4) {
                _up2x[c].process_block (scratch2x (c), src[c], nFrames);
                _up4x[c].process_block (dst[c], scratch2x (c), nFrames * 2);
            }
            else if (_factor == 8) {
                _up2x[c].process_block (scratch2x (c), src[c],        nFrames);
                _up4x[c].process_block (scratch4x (c), scratch2x (c), nFrames * 2);
                _up8x[c].process_block (dst[c],        scratch4x (c), nFrames * 4);
            }
            else { // 16
                _up2x[c].process_block  (scratch2x (c), src[c],        nFrames);
                _up4x[c].process_block  (scratch4x (c), scratch2x (c), nFrames * 2);
                _up8x[c].process_block  (scratch8x (c), scratch4x (c), nFrames * 4);
                _up16x[c].process_block (dst[c],        scratch8x (c), nFrames * 8);
            }
        }
    }


    void downsample (const hvoya::AudioBuffer& src, hvoya::AudioBuffer& dst) {
        assert (src.numChans() == dst.numChans());
        assert (src.numFrames() == dst.numFrames() * _factor);

        if (_factor == 1) {
            dst.fillFrom (src);
            return;
        }

        const auto nOutFrames = static_cast<long> (dst.numFrames());
        const n_chan_t nChans = src.numChans();
        ensureCapacity (dst.numFrames(), nChans);

        for (n_chan_t c = 0; c < nChans; ++c) {
            if (_factor == 16) {
                _dn16x[c].process_block (scratch8x (c), src[c],        nOutFrames * 8);
                _dn8x[c] .process_block (scratch4x (c), scratch8x (c), nOutFrames * 4);
                _dn4x[c] .process_block (scratch2x (c), scratch4x (c), nOutFrames * 2);
                _dn2x[c] .process_block (dst[c],        scratch2x (c), nOutFrames);
            } else if (_factor == 8) {
                _dn8x[c].process_block (scratch4x (c), src[c],        nOutFrames * 4);
                _dn4x[c].process_block (scratch2x (c), scratch4x (c), nOutFrames * 2);
                _dn2x[c].process_block (dst[c],        scratch2x (c), nOutFrames);
            } else if (_factor == 4) {
                _dn4x[c].process_block (scratch2x (c), src[c],        nOutFrames * 2);
                _dn2x[c].process_block (dst[c],        scratch2x (c), nOutFrames);
            } else { // _factor == 2
                _dn2x[c].process_block (dst[c], src[c], nOutFrames);
            }
        }
    }

private:
    size_t     _factor {1};
    n_frames_t _maxBlockFrames {0};
    n_chan_t   _nChans {0};

    // Scratch buffers — one flat vector per stage.
    // Layout: [ch0 samples | ch1 samples | ...], each region = _maxBlockFrames * stageMult.
    std::vector<T> _scratch2x;
    std::vector<T> _scratch4x;
    std::vector<T> _scratch8x;

    T* scratch2x (n_chan_t c) { return _scratch2x.data() + c * _maxBlockFrames * 2; }
    T* scratch4x (n_chan_t c) { return _scratch4x.data() + c * _maxBlockFrames * 4; }
    T* scratch8x (n_chan_t c) { return _scratch8x.data() + c * _maxBlockFrames * 8; }

    // Filters — one per channel per stage. Grow-only to preserve state
    // across channel count changes (smooth mono/stereo transitions).
    std::vector<hiir::Upsampler2xFPU<NC_2X, T>>  _up2x;
    std::vector<hiir::Upsampler2xFPU<NC_4X, T>>  _up4x;
    std::vector<hiir::Upsampler2xFPU<NC_8X, T>>  _up8x;
    std::vector<hiir::Upsampler2xFPU<NC_16X, T>> _up16x;

    std::vector<hiir::Downsampler2xFPU<NC_2X, T>>  _dn2x;
    std::vector<hiir::Downsampler2xFPU<NC_4X, T>>  _dn4x;
    std::vector<hiir::Downsampler2xFPU<NC_8X, T>>  _dn8x;
    std::vector<hiir::Downsampler2xFPU<NC_16X, T>> _dn16x;


    // Grow filters and scratches to fit. Never shrinks.
    void ensureCapacity (n_frames_t nFrames, n_chan_t nChans) {
        bool dirty = false;

        if (nChans > _nChans) {
            _nChans = nChans;
            ensureFilters (nChans);
            dirty = true;
        }

        if (nFrames > _maxBlockFrames) {
            _maxBlockFrames = nFrames;
            dirty = true;
        }

        if (dirty)
            allocateScratches();
    }


    void ensureFilters (n_chan_t nChans) {
        if (nChans <= _up2x.size())
            return;

        auto oldSize = _up2x.size();

        _up2x.resize (nChans);
        _up4x.resize (nChans);
        _up8x.resize (nChans);
        _up16x.resize (nChans);
        _dn2x.resize (nChans);
        _dn4x.resize (nChans);
        _dn8x.resize (nChans);
        _dn16x.resize (nChans);

        for (n_chan_t c = oldSize; c < nChans; ++c) {
            _up2x[c].set_coefs (_coeffs2x.data());
            _up4x[c].set_coefs (_coeffs4x.data());
            _up8x[c].set_coefs (_coeffs8x.data());
            _up16x[c].set_coefs (_coeffs16x.data());
            _dn2x[c].set_coefs (_coeffs2x.data());
            _dn4x[c].set_coefs (_coeffs4x.data());
            _dn8x[c].set_coefs (_coeffs8x.data());
            _dn16x[c].set_coefs (_coeffs16x.data());
        }
    }

    void allocateScratches() {
        const size_t chFrames = static_cast<size_t> (_maxBlockFrames) * _nChans;
        _scratch2x.resize (chFrames * 2);
        _scratch4x.resize (chFrames * 4);
        _scratch8x.resize (chFrames * 8);
    }
};


} // namespace hvoya
