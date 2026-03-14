#pragma once

// hvoya::measureLatency
//
// Measures the latency (in samples) of any processing chain by sending
// an impulse through it and finding the peak in the output.
//
// Usage:
//
//   auto lat = hvoya::measureLatency ([&](const AudioBuffer& in, AudioBuffer& out) {
//       osBuffer.setNumFrames (in.numFrames() * os.getFactor());
//       os.upsample (in, osBuffer);
//       os.downsample (osBuffer, out);
//   });
//
//   SetLatency (lat);
//

#include <cmath>
#include <functional>

#include <hvoya/utils/audio_buffer.hpp>


namespace hvoya {


/// Returns the delay in samples between input impulse and output peak.
/// processFunc receives mono buffer, testLength frames.
/// testLength should be large enough for the impulse response to settle.

inline int measureLatency (std::function<void (const AudioBuffer& in, AudioBuffer& out)> processFunc,
                           n_frames_t testLength = 4096) {
    AudioBuffer in  (1, testLength);
    AudioBuffer out (1, testLength);

    in.clear();
    out.clear();

    // impulse at sample 0
    in[0][0] = 1.0;

    processFunc (in, out);

    // find peak
    int    peakIdx = 0;
    double peakVal = 0.0;
    const auto* outBuf = out[0];

    for (n_frames_t i = 0; i < testLength; ++i) {
        const double v = std::abs (outBuf[i]);
        if (v > peakVal) {
            peakVal = v;
            peakIdx = static_cast<int> (i);
        }
    }

    return peakIdx;
}


} // namespace hvoya
