#pragma once

#include <array>
#include <vector>
#include <IPlugConstants.h>
#include "types.hpp"
#include "utils/log/logger.hpp"


namespace hvoya {
    
    class AudioBuffer final {
        public:

			// TODO iterator returning channels

            AudioBuffer();
            AudioBuffer (iplug::sample**, n_chan_t, n_frames_t);
            ~AudioBuffer() = default;
            
            void wrap (iplug::sample**, n_chan_t, n_frames_t);
						bool isWrapper() const { return _channels [0] != _data.data(); }

            void fillFrom (iplug::sample**, n_chan_t, n_frames_t);
            void copyTo   (iplug::sample**, n_chan_t, n_frames_t);
            
            void setNumChannels (n_chan_t);
            void setNumFrames   (n_frames_t);
            
            n_frames_t numFrames() const { return _numFrames; };
            n_chan_t   numChans()  const { return _numChans; }
            
            AudioBuffer getChan (n_chan_t);
        
            AudioBuffer& operator=  (const AudioBuffer&);
            AudioBuffer& operator+= (const AudioBuffer&);
            AudioBuffer& operator*= (const AudioBuffer&);
            AudioBuffer& operator*= (sample_t);
        
                  sample_t* operator[] (n_chan_t c)       { return _channels[c]; }
            const sample_t* operator[] (n_chan_t c) const { return _channels[c]; }
        
            bool isValid (sample_t t = -1) const {
                if (t == -1) t = 1;
                for (n_chan_t c = 0; c < _numChans; ++c)
                    for (n_frames_t i = 0; i < _numFrames; ++i) {
                        const auto s = _channels [c][i];
                        if (!(s > -t && s < t)) {
                            LOGW << "audio buffer overdrive! [" << c << "][" << i << "] " << s;
                            return false;
                        }
                    }
                return true;
            }
            
            n_chan_t maxNumChans() const { return _channels.size(); }

									sample_t** data()       { return _channels.data(); }
						const sample_t** data() const { return const_cast<const sample_t**> (_channels.data()); }

        private:
        
            std::vector <sample_t>    _data;
            std::array <sample_t*, 8> _channels;
        
            n_chan_t   _numChans;
            n_frames_t _numFrames;
        
            void resize();
        };
    
} // namespace hvoya
