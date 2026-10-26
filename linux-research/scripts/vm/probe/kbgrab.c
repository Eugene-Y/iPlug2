#include <X11/Xlib.h>
#include <stdio.h>
int main(void)
{
  Display* d = XOpenDisplay(NULL);
  if (!d) return 2;
  Window root, child; int rx, ry, x, y; unsigned mask = 0;
  XQueryPointer(d, DefaultRootWindow(d), &root, &child, &rx, &ry, &x, &y, &mask);
  printf("core pointer at %d,%d mask 0x%x (Button1 %s)\n", rx, ry, mask, (mask & Button1Mask) ? "DOWN" : "up");
  int r = XGrabKeyboard(d, DefaultRootWindow(d), False, GrabModeAsync, GrabModeAsync, CurrentTime);
  printf("keyboard grab by a third client: %s\n", r == GrabSuccess ? "free (GrabSuccess)" : r == AlreadyGrabbed ? "AlreadyGrabbed" : "other");
  if (r == GrabSuccess) XUngrabKeyboard(d, CurrentTime);
  XCloseDisplay(d);
  return 0;
}
