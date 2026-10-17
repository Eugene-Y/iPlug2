// T-024 F3: drives SandModule directly (default params) and reports non-finite output.
// Build (mac): clang++ -std=c++23 -O2 -DNDEBUG -DOS_MAC -I<dir with a stub plug_build_info.hpp and Sand's config.h>
//   -I<Sand> -I<IPlug2>/hvoya -I<IPlug2>/hvoya/hvoya/libs/plog/include -I<IPlug2>/IPlug -I<IPlug2>/WDL -I<IPlug2>/IPlug/Extras
//   f3-sandmodule-harness.cpp <IPlug2>/hvoya/hvoya/utils/{audio_buffer,utils,hxa_pow}.cpp <IPlug2>/hvoya/hvoya/utils/log/logger.cpp
//   <IPlug2>/IPlug/Extras/HIIR/PolyphaseIIR2Designer.cpp
// Usage: harness <sr> [filter position] [filter hz]; env GRIND, SIGN.
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "sand_module.hpp"
using namespace hvoya;

int main(int argc, char** argv) {
  double sr = argc > 1 ? atof(argv[1]) : 768000;
  int fpos = argc > 2 ? atoi(argv[2]) : 1;
  double fhz = argc > 3 ? atof(argv[3]) : 10000;
  static SandModule m;
  m.setSampleRate(sr); m.reset();
  m.setCompAmntNormalized(0.3); m.setCompRange(0); m.setSoftness(1); m.setDerivativeOrder(1);
  m.setMixMode(SandModule::MixMode(0)); m.setJointMix(0.5); m.setDryMix(0.5); m.setWetMix(0.5);
  m.setSignMode(SandModule::SignMode(getenv("SIGN") ? atoi(getenv("SIGN")) : 0)); m.setDebrick(0);
  m.setFilterCutoff(fhz); m.setFilterRes(0.5); m.setFilterPassType(0); m.setFilterPosition(SandModule::FPos(fpos)); m.setFilterMix(1);
  m.setMidSideWet(1); m.setMidSideDry(1); m.setGrind(getenv("GRIND") ? atof(getenv("GRIND")) : 0); m.setGrindCutoff(4000); m.setGrindPosition(SandModule::GPos(1));
  m.setDryHpMode(SandModule::DryHpMode(0)); m.setAmEnvelope(0); m.setAmWave(0); m.setAmRelease(0.5);
  m.setAmEnvGate(-140); m.setAmWaveGate(-140); m.setAmLpCutoff(20000);
  uint64_t rng = 1; auto rnd = [&]() { rng = rng * 6364136223846793005ULL + 1442695040888963407ULL; return (rng >> 11) * (1.0 / 9007199254740992.0); };
  const int B = 512; long pos = 0, bad = 0, firstBad = -1; double peak = 0;
  AudioBuffer buf(2, B);
  for (int b = 0; b < int(sr) / B; b++) {
    buf.setNumFrames(B);
    for (int c = 0; c < 2; c++) for (int i = 0; i < B; i++) {
      double t = (pos + i) / sr;
      buf.getChan(c)[0][i] = (c ? 0.3 : 0.4) * std::sin(2 * M_PI * 110 * t) + 0.1 * (rnd() * 2 - 1);
    }
    m.process(buf);
    for (int c = 0; c < 2; c++) for (int i = 0; i < B; i++) {
      double v = buf.getChan(c)[0][i];
      if (!std::isfinite(v)) { bad++; if (firstBad < 0) firstBad = pos + i; }
      else peak = std::max(peak, std::abs(v));
    }
    pos += B;
  }
  printf("sr=%g fpos=%d fhz=%g bad=%ld firstBad=%ld peak=%g\n", sr, fpos, fhz, bad, firstBad, peak);
}
