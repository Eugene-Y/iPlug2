// Counts how many Linux Timer_impl instances (PR #1336 code, compiled as-is) ever fire.
#include "IPlugTimer.h"
#include <atomic>
#include <cstdio>
#include <unistd.h>
using namespace iplug;
int main()
{
  const int N = 200;
  int dead = 0;
  for (int i = 0; i < N; i++)
  {
    std::atomic<int> hits{0};
    Timer* t = Timer::Create([&](Timer&) { hits++; }, 2);
    usleep(30000);
    if (hits == 0) dead++;
    t->Stop();
    delete t;
  }
  printf("IPlugTimer: %d of %d timers never fired\n", dead, N);
}
