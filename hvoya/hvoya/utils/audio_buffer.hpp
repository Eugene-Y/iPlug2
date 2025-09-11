#pragma once

#include <array>
#include <vector>
#include <IPlugConstants.h>
#include "types.hpp"
#include <hvoya/utils/log/logger.hpp>


namespace hvoya {
    
    class AudioBuffer final {
        public:
        
            static constexpr n_chan_t MAX_CHANNELS = 16;

			// TODO iterator returning channels

            AudioBuffer (n_chan_t = 2, n_frames_t = 512);
            AudioBuffer (iplug::sample**, n_chan_t, n_frames_t);
            AudioBuffer (const AudioBuffer&);
            AudioBuffer (AudioBuffer&&) noexcept;
            
            ~AudioBuffer() = default;
            
            void wrap (iplug::sample**, n_chan_t, n_frames_t);
            void unwrap(); 
            
			inline bool isWrapper() const { return _isWrapper; }

            void fillFrom (iplug::sample**, n_chan_t, n_frames_t);
            void copyTo   (iplug::sample**, n_chan_t, n_frames_t);
            
            void fillFrom (const AudioBuffer& other) { copyContentFrom (other); }
            
            void setNumChannels (n_chan_t);
            void setNumFrames   (n_frames_t);
            
            inline n_frames_t numFrames() const { return _numFrames; };
            inline n_chan_t   numChans()  const { return _numChans; }
            
            AudioBuffer getChan (n_chan_t);
        
			AudioBuffer& operator=  (const AudioBuffer&);
            AudioBuffer& operator=  (AudioBuffer&&) noexcept;
            AudioBuffer& operator+= (const AudioBuffer&);
            AudioBuffer& operator*= (const AudioBuffer&);
            AudioBuffer& operator*= (sample_t);
        
            sample_t* operator[] (n_chan_t c) { 
                assert (c < _numChans);
                return _channels [c]; 
            }
            const sample_t* operator[] (n_chan_t c) const { 
                assert (c < _numChans);
                return _channels [c]; 
            }
        
            bool isValid (sample_t thresh = 1) const {
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
            
            n_chan_t maxNumChans() const { return MAX_CHANNELS; }

				  sample_t*      * data()       { return _channels.data(); }
			const sample_t* const* data() const { return _channels.data(); }

        private:
        
            bool _isWrapper;
        
            std::vector <sample_t> _data;
            std::array <sample_t*, MAX_CHANNELS> _channels;
        
            n_chan_t   _numChans;
            n_frames_t _numFrames;
        
            void resize();
            void clearUnusedChanPtrs();
			void copyContentFrom (const AudioBuffer&);
            void swap (AudioBuffer&) noexcept;
        };
    
} // namespace hvoya
