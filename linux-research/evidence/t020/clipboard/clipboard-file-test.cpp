// T-020: LinuxClipboard::SetFilePath served to other X clients. An unmapped window owns CLIPBOARD; xclip (run by
// the driver script) reads TARGETS, text/uri-list, x-special/gnome-copied-files and UTF8_STRING meanwhile.
// Usage: clipboard-file-test <path> <seconds>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include "IPlugPlatform.h"
#include "IGraphicsLinuxClipboard.h"

using namespace iplug::igraphics;

int main(int argc, char** argv)
{
  Display* pDisplay = XOpenDisplay(nullptr);
  if (!pDisplay)
    return 2;
  Window window = XCreateSimpleWindow(pDisplay, DefaultRootWindow(pDisplay), 0, 0, 10, 10, 0, 0, 0);
  XSelectInput(pDisplay, window, PropertyChangeMask);
  LinuxClipboard clipboard;
  clipboard.Init(pDisplay, window);
  const bool owned = clipboard.SetFilePath(argv[1], CurrentTime);
  printf("SetFilePath(%s) -> %d\n", argv[1], owned);
  fflush(stdout);
  const time_t end = time(nullptr) + atoi(argv[2]);
  while (time(nullptr) < end)
  {
    while (XPending(pDisplay))
    {
      XEvent event;
      XNextEvent(pDisplay, &event);
      clipboard.HandleEvent(event);
    }
    struct timespec ts { 0, 10 * 1000 * 1000 };
    nanosleep(&ts, nullptr);
  }
  clipboard.Release();
  XCloseDisplay(pDisplay);
  return owned ? 0 : 1;
}
