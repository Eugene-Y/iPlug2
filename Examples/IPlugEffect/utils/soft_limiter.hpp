#pragma once

#include <cstddef>


namespace hvoya {

    class SoftLimiter {
    public:
        SoftLimiter (float threshold = 0.f,
                     float softness = 1.f);

        bool processBuffer (double**, size_t numFrames, size_t numChans);

        // [0, ...]
        void setThreshold (float db);
        float getThreshold() const;

        // [0, 1]
        void setSoftness (float db);
        float getSoftness() const;

    protected:
        float _threshold;
        float _softness;
        float _a;
        float _b;
        float _l;
    };

}

