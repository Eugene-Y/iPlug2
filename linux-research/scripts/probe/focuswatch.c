// Scratch probe: prints every change of the X input focus (polled every 2 ms) with a timestamp.
#include <X11/Xlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
int main(int argc, char** argv)
{
  Display* d = XOpenDisplay(NULL);
  double secs = argc > 1 ? atof(argv[1]) : 5;
  Window last = 1234567, f; int r;
  struct timespec t0, t; clock_gettime(CLOCK_MONOTONIC, &t0);
  for (;;) {
    XGetInputFocus(d, &f, &r);
    clock_gettime(CLOCK_MONOTONIC, &t);
    double el = (t.tv_sec - t0.tv_sec) + (t.tv_nsec - t0.tv_nsec) / 1e9;
    if (f != last) { printf("%8.3f focus 0x%lx\n", el, f); fflush(stdout); last = f; }
    if (el > secs) break;
    usleep(2000);
  }
  return 0;
}
