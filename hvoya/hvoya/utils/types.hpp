// types.hpp

#pragma once

#include <limits>

namespace hvoya {

    typedef double sample_t;
    typedef size_t n_chan_t;
    typedef size_t n_frames_t;

    typedef int CC_t;
    typedef int PId_t;

    // Sentinels marking a "not yet initialized" cached value — e.g. audio-thread
    // param caches that must fire every setter on the first ProcessBlock.
    namespace uninit {
		inline constexpr CC_t	  cc     = -1;
		inline constexpr PId_t	  pid    = -1;
        inline constexpr sample_t sample = std::numeric_limits<sample_t>::quiet_NaN();
        inline constexpr int      flag   = -1;  // tristate bool / unset enum
    }

}
