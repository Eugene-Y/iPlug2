#pragma once

#include <array>
#include <vector>
#include <span>
#include <IPlugConstants.h>
#include "types.hpp"
#include <hvoya/utils/log/logger.hpp>


namespace hvoya {


	inline bool epsilonCheckVal (sample_t v, sample_t target, sample_t eps = 0.0001) {
		assert (eps > 0);
		auto min = target - eps;
		auto max = target + eps;
		return v > min && v < max;
	}


	inline bool epsilonCheckInRange (sample_t v, sample_t a, sample_t b, sample_t eps = 0.0001) {
		assert (eps > 0);
		if (a > b) std::swap (a, b);
		auto min = a - eps;
		auto max = b + eps;
		return v > min && v < max;
	}


    template<bool IsConst>
    class ChannelIteratorT {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = std::conditional_t<IsConst, const sample_t*, sample_t*>;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;
        
    private:
        using ptr_type = std::conditional_t<IsConst, const sample_t* const*, sample_t**>;
        ptr_type _ptr;
        
    public:
        ChannelIteratorT(ptr_type ptr) noexcept 
            : _ptr(ptr) {}
        
        reference operator*() const noexcept { return *_ptr; }
        pointer operator->() const noexcept { return _ptr; }
        
        ChannelIteratorT& operator++() noexcept { ++_ptr; return *this; }
        
        bool operator==(const ChannelIteratorT& other) const noexcept { return _ptr == other._ptr; }
        bool operator!=(const ChannelIteratorT& other) const noexcept { return _ptr != other._ptr; }
    };
    
    
    using ChannelIterator = ChannelIteratorT<false>;
    using ConstChannelIterator = ChannelIteratorT<true>;
    
    
    template<bool IsConst>
    class ChannelRangeT {
        using ptr_type = std::conditional_t<IsConst, const sample_t* const*, sample_t**>;
        ptr_type _data;
        n_chan_t _count;
        
    public:
        ChannelRangeT(ptr_type data, n_chan_t count) noexcept
            : _data(data), _count(count) {}
        
        ChannelIteratorT<IsConst> begin() const noexcept { 
            return ChannelIteratorT<IsConst>(_data); 
        }
        ChannelIteratorT<IsConst> end() const noexcept { 
            return ChannelIteratorT<IsConst>(_data + _count); 
        }
    };
    
    using ChannelRange = ChannelRangeT<false>;
    using ConstChannelRange = ChannelRangeT<true>;
    
    

    
    class AudioBuffer final {
        public:
        
            static constexpr n_chan_t MAX_CHANNELS = 16;

            AudioBuffer (n_chan_t = 2, n_frames_t = 512);
            AudioBuffer (iplug::sample**, n_chan_t, n_frames_t);
            AudioBuffer (const AudioBuffer&);
            AudioBuffer (AudioBuffer&&) noexcept;
            
            ~AudioBuffer() noexcept = default;
            
            void wrap (iplug::sample**, n_chan_t, n_frames_t);
            void unwrap(); 
            
			inline bool isWrapper() const noexcept { return _isWrapper; }

            void fillFrom (iplug::sample**, n_chan_t, n_frames_t) noexcept;
            void copyTo   (iplug::sample**, n_chan_t, n_frames_t) noexcept;
            
            void fillFrom (const AudioBuffer& other) { copyContentFrom (other); }
            
            AudioBuffer subBuffer (n_frames_t begin, n_frames_t len = 0); // len 0 means full len
            
            void setNumChannels (n_chan_t);
            void setNumFrames   (n_frames_t);
            
            inline n_frames_t numFrames() const noexcept { return _numFrames; }
            inline n_chan_t   numChans()  const noexcept { return _numChans; }


			inline void fillWith (sample_t v = 0, n_chan_t c = 0) noexcept {
				assert (c < _numChans);
				for (n_frames_t i = 0; i < _numFrames; ++i)
					_channels [c][i] = v;
			}


			inline void clear() noexcept {
				for (n_chan_t c = 0; c < _numChans; ++c)
					fillWith (0, c);
			}


			inline void fillWithRamp (sample_t a, sample_t b, n_chan_t c = 0) noexcept {
				assert (c < _numChans);
				sample_t d = (b - a) / _numFrames;
				for (n_frames_t i = 0; i < _numFrames; ++i)
					_channels [c][i] = a + d * (i + 1);
				
				assert (epsilonCheckInRange (back (c), a, b, std::min (std::abs (d), 0.0001)));
			}


			inline void fillWithSmoothStep (sample_t a, sample_t b, n_chan_t c = 0) noexcept {
				assert(c < _numChans);
				for (n_frames_t i = 0; i < _numFrames; ++i) {
					sample_t t = sample_t (i) / (_numFrames - 1);
					sample_t curve = t * t * (3 - 2 * t);
					_channels [c][i] = a + (b - a) * curve;
				}
				assert (epsilonCheckInRange (back (c), a, b));
				assert (epsilonCheckInRange (front (c), a, b));
			}


			inline void fillWithCosineStep (sample_t a, sample_t b, n_chan_t c = 0) noexcept {
				assert (c < _numChans);
				const sample_t d = b - a;
				for (n_frames_t i = 0; i < _numFrames; ++i) {
					sample_t t = sample_t (i) / (_numFrames - 1);
					t = t * t * t * (t * (t * 6 - 15) + 10);
					_channels [c][i] = a + d * t;
				}
				assert (epsilonCheckInRange (back (c), a, b));
				assert (epsilonCheckInRange (front (c), a, b));
			}


			void remapRangeFrom (const AudioBuffer& src,
								 sample_t oldMin, sample_t oldMax,
								 sample_t newMin, sample_t newMax, n_chan_t c = 0) noexcept {
				assert (src.numFrames() == _numFrames);
				assert (c < numChans());
				assert (c < src.numChans());
				assert (oldMax != oldMin && "oldMin and oldMax must be different");

				const sample_t oldRange = oldMax - oldMin;
				const sample_t newRange = newMax - newMin;
				const sample_t scale = newRange / oldRange;

				for (n_frames_t i = 0; i < _numFrames; ++i)
					_channels [c][i] = (src [c][i] - oldMin) * scale + newMin;
			}


			void remapRange (sample_t oldMin, sample_t oldMax,
							 sample_t newMin, sample_t newMax, n_chan_t c = 0) noexcept {
				remapRangeFrom (*this, oldMin, oldMax, newMin, newMax, c);
			}


			template <typename Func>
			inline void transform (Func&& func, n_chan_t c = 0) noexcept {
				assert(c < _numChans);
				for (n_frames_t i = 0; i < _numFrames; ++i)
					_channels [c][i] = func (_channels [c][i], i);
			}


            AudioBuffer getChan (n_chan_t);
            
            ChannelRange      channels()       noexcept { return ChannelRange      (_channels.data(), _numChans); }
            ConstChannelRange channels() const noexcept { return ConstChannelRange (_channels.data(), _numChans);}

			sample_t& front (n_chan_t c = 0) noexcept {
				assert (c < _numChans);
				return _channels [c][0];
			}

			sample_t& back (n_chan_t c = 0) noexcept {
				assert (c < _numChans);
				assert (_numFrames > 0);
				return _channels [c][_numFrames - 1];
			}

			AudioBuffer& operator=  (const AudioBuffer&) noexcept;
            AudioBuffer& operator=  (AudioBuffer&&)      noexcept;
            
            AudioBuffer& operator+= (const AudioBuffer&) noexcept;
            AudioBuffer& operator-= (const AudioBuffer&) noexcept;
            
            AudioBuffer& operator*= (const AudioBuffer&) noexcept;
            AudioBuffer& operator*= (sample_t)           noexcept;

			AudioBuffer& operator/= (const AudioBuffer&) noexcept;
			AudioBuffer& operator/= (sample_t)           noexcept;

            friend AudioBuffer operator- (AudioBuffer, const AudioBuffer&);
            friend AudioBuffer operator+ (AudioBuffer, const AudioBuffer&);
            friend AudioBuffer operator* (AudioBuffer, const AudioBuffer&);
        
            inline sample_t* operator[] (n_chan_t c) noexcept {
                assert (c < _numChans);
                return _channels [c]; 
            }
			inline const sample_t* operator[] (n_chan_t c) const noexcept {
                assert (c < _numChans);
                return _channels [c]; 
            }

			inline  	 sample_t*      * data()       noexcept { return _channels.data(); }
			inline const sample_t* const* data() const noexcept { return _channels.data(); }

			inline sample_t* chanData (n_chan_t c = 0) noexcept {
				assert (c < _numChans);
				return _channels [c];
			}

			inline const sample_t* chanData (n_chan_t c = 0) const noexcept {
				assert (c < _numChans);
				return _channels [c];
			}

			n_chan_t maxNumChans() const noexcept { return MAX_CHANNELS; }

			bool isValid (sample_t thresh = 1) const noexcept {
				for (n_chan_t c = 0; c < _numChans; ++c)
					for (n_frames_t i = 0; i < _numFrames; ++i) {
						const auto& s = _channels [c][i];
						if (!(s > -thresh && s < thresh)) {
							LOGW << "audio buffer overdrive! [" << c << "][" << i << "] " << s;
							return false;
						}
					}
				return true;
			}

        private:
        
            bool _isWrapper;
        
            std::vector <sample_t> _data;

			#ifndef NDEBUG
				mutable std::array <std::span <const sample_t>, MAX_CHANNELS> _dataView;
				void updateDebugDataView() const noexcept {
					for (n_chan_t c = 0; c < _numChans; ++c)
						_dataView[c] = std::span <const sample_t> (_channels[c], _numFrames);
					for (n_chan_t c = _numChans; c < MAX_CHANNELS; ++c)
						_dataView[c] = std::span <const sample_t> ();
				}
			#else
				void updateDebugDataView() const noexcept {}
			#endif

            typedef std::array <sample_t*, MAX_CHANNELS> chan_ptr_t;
            chan_ptr_t _channels;
            
            AudioBuffer (chan_ptr_t, n_chan_t, n_frames_t);
        
            n_chan_t   _numChans;
            n_frames_t _numFrames;
        
            void resize();
            void clearUnusedChanPtrs() noexcept { clearUnusedChanPtrs (_channels, _numChans); }
            void clearUnusedChanPtrs (chan_ptr_t&, n_chan_t) noexcept;
			void copyContentFrom (const AudioBuffer&) noexcept;
            void swap (AudioBuffer&) noexcept;
        };
        
        
        
        inline AudioBuffer operator- (AudioBuffer lhs, const AudioBuffer& rhs) {
            assert(lhs.numChans() == rhs.numChans());
            assert(lhs.numFrames() == rhs.numFrames());
            lhs -= rhs;
            return lhs;
        }
        
        
        inline AudioBuffer operator+ (AudioBuffer lhs, const AudioBuffer& rhs) {
            assert(lhs.numChans() == rhs.numChans());
            assert(lhs.numFrames() == rhs.numFrames());
            lhs += rhs;
            return lhs;
        }
        
        
        inline AudioBuffer operator* (AudioBuffer lhs, const AudioBuffer& rhs) {
            assert(lhs.numChans() == rhs.numChans());
            assert(lhs.numFrames() == rhs.numFrames());
            lhs *= rhs;
            return lhs;
        }
        
    
    
} // namespace hvoya
