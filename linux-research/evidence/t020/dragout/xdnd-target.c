/* T-020: a minimal XDND target (version 5) for drag-out tests: accepts text/uri-list, prints the dropped list and
 * the protocol steps, answers XdndFinished. Usage: xdnd-target <x> <y> <w> <h> <seconds> */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(int argc, char** argv)
{
  Display* d = XOpenDisplay(NULL);
  if (!d) return 2;
  Window w = XCreateSimpleWindow(d, DefaultRootWindow(d), atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), 0, 0, 0x406040);
  XStoreName(d, w, "xdnd-target");
  Atom aware = XInternAtom(d, "XdndAware", False), version = 5;
  XChangeProperty(d, w, aware, XA_ATOM, 32, PropModeReplace, (unsigned char*) &version, 1);
  Atom enter = XInternAtom(d, "XdndEnter", False), position = XInternAtom(d, "XdndPosition", False);
  Atom status = XInternAtom(d, "XdndStatus", False), leave = XInternAtom(d, "XdndLeave", False);
  Atom drop = XInternAtom(d, "XdndDrop", False), finished = XInternAtom(d, "XdndFinished", False);
  Atom selection = XInternAtom(d, "XdndSelection", False), uriList = XInternAtom(d, "text/uri-list", False);
  Atom copy = XInternAtom(d, "XdndActionCopy", False), prop = XInternAtom(d, "XDND_TEST", False);
  XMapWindow(d, w);
  XFlush(d);
  Window source = 0;
  int positions = 0;
  time_t end = time(NULL) + atoi(argv[5]);
  printf("target 0x%lx ready\n", w); fflush(stdout);
  while (time(NULL) < end)
  {
    if (!XPending(d)) { struct timespec ts = { 0, 10000000 }; nanosleep(&ts, NULL); continue; }
    XEvent e; XNextEvent(d, &e);
    if (e.type == ClientMessage)
    {
      Atom t = e.xclient.message_type;
      if (t == enter) { source = e.xclient.data.l[0]; printf("Enter v%ld type0=%s\n", e.xclient.data.l[1] >> 24, e.xclient.data.l[2] ? XGetAtomName(d, e.xclient.data.l[2]) : "-"); }
      else if (t == position)
      {
        if (++positions == 1) printf("Position (first) root %ld,%ld\n", e.xclient.data.l[2] >> 16, e.xclient.data.l[2] & 0xffff);
        XEvent r = { 0 };
        r.xclient.type = ClientMessage; r.xclient.window = source; r.xclient.message_type = status; r.xclient.format = 32;
        r.xclient.data.l[0] = w; r.xclient.data.l[1] = 1; r.xclient.data.l[4] = copy;
        XSendEvent(d, source, False, NoEventMask, &r); XFlush(d);
      }
      else if (t == leave) printf("Leave after %d positions\n", positions);
      else if (t == drop) { printf("Drop after %d positions\n", positions); XConvertSelection(d, selection, uriList, prop, w, e.xclient.data.l[2]); XFlush(d); }
      fflush(stdout);
    }
    else if (e.type == SelectionNotify)
    {
      Atom type; int format; unsigned long n, after; unsigned char* data = NULL;
      if (e.xselection.property != None && XGetWindowProperty(d, w, prop, 0, 65536, True, AnyPropertyType, &type, &format, &n, &after, &data) == Success && data)
        printf("DATA %s: %.*s", XGetAtomName(d, type), (int) n, data);
      else
        printf("DATA none\n");
      XEvent r = { 0 };
      r.xclient.type = ClientMessage; r.xclient.window = source; r.xclient.message_type = finished; r.xclient.format = 32;
      r.xclient.data.l[0] = w; r.xclient.data.l[1] = 1; r.xclient.data.l[2] = copy;
      XSendEvent(d, source, False, NoEventMask, &r); XFlush(d);
      printf("Finished sent\n"); fflush(stdout);
    }
  }
  return 0;
}
