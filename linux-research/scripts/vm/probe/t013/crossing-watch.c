// Print the Enter/Leave events of one X window (another client's) for N seconds, with the time of day.
// gcc -o crossing-watch crossing-watch.c -lX11 ; crossing-watch <window id> <seconds>
#include <X11/Xlib.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

int main(int argc, char** argv)
{
  if (argc < 3)
  {
    fprintf(stderr, "usage: crossing-watch <window id> <seconds>\n");
    return 2;
  }
  Display* d = XOpenDisplay(NULL);
  if (!d)
    return 1;
  Window w = strtoul(argv[1], NULL, 0);
  XSelectInput(d, w, EnterWindowMask | LeaveWindowMask);
  time_t end = time(NULL) + atoi(argv[2]);
  struct pollfd p = { ConnectionNumber(d), POLLIN, 0 };
  while (time(NULL) < end)
  {
    while (XPending(d))
    {
      XEvent e;
      XNextEvent(d, &e);
      if (e.type != EnterNotify && e.type != LeaveNotify)
        continue;
      struct timeval tv;
      gettimeofday(&tv, NULL);
      char hms[16];
      strftime(hms, sizeof(hms), "%H:%M:%S", localtime(&tv.tv_sec));
      printf("%s.%03ld %s mode %d detail %d at %d,%d\n", hms, (long) tv.tv_usec / 1000,
             e.type == EnterNotify ? "Enter" : "Leave", e.xcrossing.mode, e.xcrossing.detail,
             e.xcrossing.x_root, e.xcrossing.y_root);
      fflush(stdout);
    }
    poll(&p, 1, 200);
  }
  return 0;
}
