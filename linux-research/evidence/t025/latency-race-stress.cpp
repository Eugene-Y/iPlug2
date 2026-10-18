// T-025: SetLatency on one thread while another runs the bypass path (PassThroughBuffers), as a host does when
// a plug-in changes its latency off the audio thread while bypassed. Build with -fsanitize=address or =thread.
// Usage: latency-race-stress [seconds=5] [maxLatency=4096]
// Exit 0 and "passthrough-ok" when every bypassed block is either the delayed input or silence.

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <thread>

#include "IPlugProcessor.h"

using namespace iplug;

class StressProcessor final : public IPlugProcessor
{
public:
  explicit StressProcessor(const Config& config) : IPlugProcessor(config, kAPIVST3)
  {
    SetChannelConnections(ERoute::kInput, 0, MaxNChannels(ERoute::kInput), true);
    SetChannelConnections(ERoute::kOutput, 0, MaxNChannels(ERoute::kOutput), true);
    InitLatencyDelay();
  }

  bool SendMidiMsg(const IMidiMsg&) override { return true; }

  void Bypass(PLUG_SAMPLE_DST** ins, PLUG_SAMPLE_DST** outs, int nFrames)
  {
    AttachBuffers(ERoute::kInput, 0, 2, ins, nFrames);
    AttachBuffers(ERoute::kOutput, 0, 2, outs, nFrames);
    PassThroughBuffers(PLUG_SAMPLE_DST(0.), nFrames);
  }
};

int main(int argc, char** argv)
{
  const double seconds = argc > 1 ? atof(argv[1]) : 5.0;
  const int maxLatency = argc > 2 ? atoi(argv[2]) : 4096;

  Config config(0, 0, "2-2", "Stress", "Stress", "T-025", 0x10000, 'Strs', 'T025', 0,
                false, false, false, false, 0, false, 0, 0, false, 0, 0, 0, 0, "", "");
  StressProcessor processor(config);

  constexpr int kFrames = 256;
  static PLUG_SAMPLE_DST inL[kFrames], inR[kFrames], outL[kFrames], outR[kFrames];
  PLUG_SAMPLE_DST* ins[2] = { inL, inR };
  PLUG_SAMPLE_DST* outs[2] = { outL, outR };

  // Single-threaded: a bypassed ramp comes out delayed by exactly the latency, also after a change.
  for (int latency : { 0, 100, 700, 1 })
  {
    processor.SetLatency(latency);
    long n = 0;
    for (int block = 0; block < 8; ++block)
    {
      for (int s = 0; s < kFrames; ++s)
      {
        inL[s] = double(n + s + 1);
        inR[s] = -double(n + s + 1);
      }
      processor.Bypass(ins, outs, kFrames);
      for (int s = 0; s < kFrames; ++s)
      {
        const long src = n + s - latency;
        const double expected = src < 0 ? 0.0 : double(src + 1);
        if (outL[s] != expected || outR[s] != -expected)
        {
          printf("delay-wrong latency=%d sample=%ld got=%g expected=%g\n", latency, n + s, outL[s], expected);
          return 1;
        }
      }
      n += kFrames;
    }
  }
  printf("delay-exact\n");

  processor.SetLatency(0);
  for (int s = 0; s < kFrames; ++s)
    inL[s] = inR[s] = 1.0;

  std::atomic<bool> stop { false };
  std::atomic<long> changes { 0 };
  long blocks = 0, badBlocks = 0;

  std::thread setter([&] {
    std::mt19937 rng(25);
    std::uniform_int_distribution<int> latency(0, maxLatency);
    while (!stop.load())
    {
      processor.SetLatency(latency(rng));
      changes.fetch_add(1);
    }
  });

  const auto end = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);
  while (std::chrono::steady_clock::now() < end)
  {
    processor.Bypass(ins, outs, kFrames);
    ++blocks;
    for (int s = 0; s < kFrames; ++s)
      if (!(outL[s] == 0.0 || outL[s] == 1.0) || !(outR[s] == 0.0 || outR[s] == 1.0))
      {
        ++badBlocks;
        break;
      }
  }

  stop = true;
  setter.join();
  printf("blocks=%ld latency-changes=%ld bad-blocks=%ld\n", blocks, changes.load(), badBlocks);
  if (badBlocks)
    return 1;
  printf("passthrough-ok\n");
  return 0;
}
