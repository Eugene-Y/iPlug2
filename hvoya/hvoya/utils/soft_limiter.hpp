#pragma once

#include <cstddef>
#include "audio_buffer.hpp"


namespace hvoya {

    class SoftLimiter {
    public:
        SoftLimiter (sample_t threshold = 0,
                     sample_t softness = 1);

		void processBuffer (sample_t**, n_chan_t, n_frames_t);
		void processBuffer (AudioBuffer&);

		// [12, ...]
        void setThreshold (sample_t db);
        sample_t getThreshold() const;

        // [0, ...]
        void setSoftness (sample_t db);
        sample_t getSoftness() const;

    protected:
        sample_t _threshold;
        sample_t _softness;
        sample_t _a;
        sample_t _b;
        sample_t _l;
    };

}

