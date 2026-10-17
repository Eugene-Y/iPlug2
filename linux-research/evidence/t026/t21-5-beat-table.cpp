// T21-5: the beat number HostInfoModel shows (host_info_model.hpp), against the expected one and against the
// formula hvoya had before the fix: hand-written cases (count-in included, fraction checked) x tempos.
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

// Written by hand, independent of the header's formula: a beat is one 1/den note, PPQ counts
// quarter notes; before the bar start (count-in) the bar runs backwards from its last beat.
struct Case { int num, den; double ppq; int beat, fraction; };
static const Case kCases[] = {
  {4, 4, 0, 1, 0}, {4, 4, 0.5, 1, 50}, {4, 4, 1, 2, 0}, {4, 4, 1.25, 2, 25}, {4, 4, 3.75, 4, 75},
  {4, 4, 4, 1, 0}, {4, 4, 5.25, 2, 25}, {4, 4, -1, 4, 0}, {4, 4, -0.5, 4, 50}, {4, 4, -4.25, 4, 75},
  {4, 4, -1e-17, 1, 0},
  {3, 4, 0, 1, 0}, {3, 4, 1, 2, 0}, {3, 4, 2, 3, 0}, {3, 4, 3, 1, 0}, {3, 4, 3.5, 1, 50}, {3, 4, -1, 3, 0},
  {6, 8, 0, 1, 0}, {6, 8, 0.5, 2, 0}, {6, 8, 0.75, 2, 50}, {6, 8, 1, 3, 0}, {6, 8, 2, 5, 0}, {6, 8, 2.5, 6, 0},
  {6, 8, 3, 1, 0}, {6, 8, 3.5, 2, 0}, {6, 8, -0.5, 6, 0},
  {7, 8, 3, 7, 0}, {7, 8, 3.5, 1, 0}, {7, 8, 3.75, 1, 50}, {7, 8, -0.25, 7, 50},
  {5, 4, 4.5, 5, 50}, {5, 4, 5, 1, 0},
  {2, 2, 1, 1, 50}, {2, 2, 2, 2, 0}, {2, 2, 3, 2, 50}, {2, 2, 4, 1, 0},
};

// The formula before the fix, verbatim.
static int oldBeat(const MockPlug& p) {
  float m = p.ppq * 60. / p.tempo * (p.den / 8.) * (4. / p.num) + 1;
  float b = size_t(p.num * (m - 1)) % p.den + 1;
  return int(std::floor(b));
}

static hvoya::TimeInfo headerInfo(const MockPlug& p) {
  // The model skips an update while the sample position is unchanged, and it starts at 0: move away first.
  MockPlug copy = p;
  copy.ppq = p.ppq + 1;
  hvoya::HostInfoModel<MockPlug> model(&copy, &MockPlug::triggerUIUpdate);
  model.updateAll(512);
  copy.ppq = p.ppq;
  model.updateAll(512);
  return model.getInfo().hostTime;
}

static int headerBeat(const MockPlug& p) { return headerInfo(p).beat; }

int main() {
  const double tempos[] = {40, 60, 90, 120, 134, 180};

  printf("4/4, one quarter in (ppq 1): expected beat 2\n  tempo   old  header\n");
  for (double t : tempos) {
    MockPlug p; p.tempo = t; p.ppq = 1;
    printf("  %5.0f   %3d  %6d\n", t, oldBeat(p), headerBeat(p));
  }

  printf("hand-written cases at 97 BPM:\n  sig    ppq     expected  old  header\n");
  int cases = 0, oldWrong = 0, headerWrong = 0;
  for (const auto& c : kCases)
    for (double t : tempos) {
      MockPlug p; p.tempo = t; p.num = c.num; p.den = c.den; p.ppq = c.ppq;
      const auto info = headerInfo(p);
      const bool ok = info.beat == c.beat && info.fraction == c.fraction;
      cases++;
      oldWrong += oldBeat(p) != c.beat;
      headerWrong += !ok;
      if (!ok) printf("  WRONG %d/%d ppq %g tempo %g: beat %d.%02d, expected %d.%02d\n", c.num, c.den, c.ppq, t,
                      int(info.beat), int(info.fraction), c.beat, c.fraction);
    }
  for (const auto& c : kCases) {
    MockPlug p; p.tempo = 97; p.num = c.num; p.den = c.den; p.ppq = c.ppq;
    const auto info = headerInfo(p);
    printf("  %d/%d  %7g   %3d.%02d   %3d  %3d.%02d\n", c.num, c.den, c.ppq, c.beat, c.fraction, oldBeat(p),
           int(info.beat), int(info.fraction));
  }
  printf("all %d cases (%zu hand-written x 6 tempos): old formula's beat wrong in %d, header's beat or fraction wrong in %d\n",
         cases, sizeof(kCases) / sizeof(kCases[0]), oldWrong, headerWrong);
  return headerWrong ? 1 : 0;
}
