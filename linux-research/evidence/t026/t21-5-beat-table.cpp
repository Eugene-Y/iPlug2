// T21-5: the beat number HostInfoModel shows (host_info_model.hpp), against the expected one and against the
// formula hvoya had before the fix, over tempos x time signatures x positions.
// Build (one line): c++ -std=c++20 -include cstdint -I<iPlug2>/hvoya -I<iPlug2>/hvoya/hvoya/libs/plog/include
//   -I<iPlug2>/IPlug -I<iPlug2>/WDL -I<any plug-in dir, for config.h> -DPLOG_DISABLE_LOGGING
//   t21-5-beat-table.cpp <iPlug2>/hvoya/hvoya/utils/host_time_info.cpp -o beat-table
// Exit status 1 if the header disagrees with the expected beat anywhere.
#include <cmath>
#include <cstdio>
#include <IPlugConstants.h>
#include <hvoya/utils/host_info_model.hpp>

struct MockPlug {
  double ppq = 0, tempo = 120, sr = 48000;
  int num = 4, den = 4;
  double GetSamplePos() const { return ppq * 60. / tempo * sr; }
  double GetPPQPos() const { return ppq; }
  double GetTempo() const { return tempo; }
  double GetSampleRate() const { return sr; }
  void GetTimeSig(int& n, int& d) const { n = num; d = den; }
  int MaxNChannels(iplug::ERoute) const { return 2; }
  bool IsChannelConnected(iplug::ERoute, int) const { return true; }
  void triggerUIUpdate() {}
};

// One beat is one 1/den note; PPQ counts quarter notes whatever the tempo.
static int expectedBeat(const MockPlug& p) {
  const double beats = p.ppq * p.den / 4.;
  return int(std::floor(beats - p.num * std::floor(beats / p.num))) + 1;
}

// The formula before the fix, verbatim.
static int oldBeat(const MockPlug& p) {
  float m = p.ppq * 60. / p.tempo * (p.den / 8.) * (4. / p.num) + 1;
  float b = size_t(p.num * (m - 1)) % p.den + 1;
  return int(std::floor(b));
}

static int headerBeat(const MockPlug& p) {
  // The model skips an update while the sample position is unchanged, and it starts at 0: move away first.
  MockPlug copy = p;
  copy.ppq = p.ppq + 1;
  hvoya::HostInfoModel<MockPlug> model(&copy, &MockPlug::triggerUIUpdate);
  model.updateAll(512);
  copy.ppq = p.ppq;
  model.updateAll(512);
  return model.getInfo().hostTime.beat;
}

int main() {
  const double tempos[] = {40, 60, 90, 120, 134, 180};
  const int sigs[][2] = {{4, 4}, {3, 4}, {6, 8}, {7, 8}, {5, 4}, {2, 2}};
  const double ppqs[] = {0, 0.5, 1, 1.5, 2, 2.75, 3, 4, 5.25, 7, 11.5, 33};

  printf("4/4, one quarter in (ppq 1): expected beat 2\n  tempo   old  header\n");
  for (double t : tempos) {
    MockPlug p; p.tempo = t; p.ppq = 1;
    printf("  %5.0f   %3d  %6d\n", t, oldBeat(p), headerBeat(p));
  }
  printf("3/4 and 6/8 at 120 BPM:\n  sig   ppq  expected  old  header\n");
  for (auto& s : {sigs[1], sigs[2]})
    for (double q : {0.0, 1.0, 2.0, 3.0, 3.5}) {
      MockPlug p; p.num = s[0]; p.den = s[1]; p.ppq = q;
      printf("  %d/%d  %4.1f  %8d  %3d  %6d\n", s[0], s[1], q, expectedBeat(p), oldBeat(p), headerBeat(p));
    }

  int cases = 0, oldWrong = 0, headerWrong = 0;
  for (double t : tempos)
    for (auto& s : sigs)
      for (double q : ppqs) {
        MockPlug p; p.tempo = t; p.num = s[0]; p.den = s[1]; p.ppq = q;
        const int e = expectedBeat(p);
        cases++;
        oldWrong += oldBeat(p) != e;
        headerWrong += headerBeat(p) != e;
      }
  printf("all %d cases (6 tempos x 6 signatures x 12 positions): old formula wrong in %d, header wrong in %d\n",
         cases, oldWrong, headerWrong);
  return headerWrong ? 1 : 0;
}
