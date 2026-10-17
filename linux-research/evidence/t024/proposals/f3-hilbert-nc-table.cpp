// T-024 F3: coefficient count HilbertEnvelopeDetector needs per quality and sample rate (storage holds 14).
// Build: clang++ -std=c++17 -O2 -I<IPlug2>/IPlug/Extras f3-hilbert-nc-table.cpp <IPlug2>/IPlug/Extras/HIIR/PolyphaseIIR2Designer.cpp
#include "HIIR/PolyphaseIIR2Designer.h"
#include <cstdio>
#include <algorithm>
int main() {
  const double hz[] = {100, 40, 20, 10}; const char* q[] = {"Low","Medium","High","Ultra"};
  const double srs[] = {44100, 48000, 88200, 96000, 176400, 192000, 352800, 384000, 384001, 386000, 705600, 768000, 1411200, 1536000, 3072000};
  double raw[64];
  printf("%-9s", "sr"); for (auto n : q) printf("%8s", n); printf("\n");
  for (double sr : srs) { printf("%-9.0f", sr);
    for (int i = 0; i < 4; i++) { double t = std::clamp(hz[i] / (sr * 0.5), 1e-5, 0.499); printf("%8d", hiir::PolyphaseIIR2Designer::compute_coefs(raw, 60.0, t)); }
    printf("\n"); }
}
