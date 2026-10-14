// Focus request timing under a real window manager, without REAPER or iPlug.
// Mimics REAPER's structure: a GDK-like top-level T (input hint False + WM_TAKE_FOCUS, i.e. "globally active";
// on WM_TAKE_FOCUS it moves the focus to its 1x1 proxy child P with the message's timestamp) holding an "editor"
// child C. A click on C triggers our request to focus C, in one of these modes:
//   none          no request (baseline: where the WM + GDK-like handling leave the focus)
//   now-current   at ButtonRelease, XSetInputFocus(C, CurrentTime)
//   now-ts        at ButtonRelease, with the ButtonRelease's server time (notes/15 option 1)
//   delay-current 150 ms after ButtonRelease, CurrentTime (what P3b does)
//   delay-ts      150 ms after ButtonRelease, with the ButtonRelease's time
// Per click it prints the focus owner 50 ms and 600 ms after the release: C (editor), P (proxy), T, other.
// A second top-level U is the "elsewhere" window the driver clicks between trials.
// Build: gcc -O1 -o focus-ts-probe focus-ts-probe.c -lX11      Run: focus-ts-probe <mode> <seconds>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double Now(void)
{
  struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec / 1e9;
}

int main(int argc, char** argv)
{
  const char* mode = argc > 1 ? argv[1] : "none";
  double runFor = argc > 2 ? atof(argv[2]) : 30;
  Display* d = XOpenDisplay(NULL);
  if (!d) return 1;
  Window root = DefaultRootWindow(d);
  Atom wmProtocols = XInternAtom(d, "WM_PROTOCOLS", False), takeFocus = XInternAtom(d, "WM_TAKE_FOCUS", False);
  Atom deleteWindow = XInternAtom(d, "WM_DELETE_WINDOW", False);

  Window T = XCreateSimpleWindow(d, root, 300, 200, 400, 300, 0, 0, 0x404040);
  Window U = XCreateSimpleWindow(d, root, 760, 200, 200, 150, 0, 0, 0x206020);
  Window P = XCreateSimpleWindow(d, T, -1, -1, 1, 1, 0, 0, 0);
  Window C = XCreateSimpleWindow(d, T, 20, 20, 360, 260, 0, 0, 0x8080c0);
  XStoreName(d, T, "focus-ts-probe T"); XStoreName(d, U, "focus-ts-probe U");
  XSizeHints pos = {0}; pos.flags = USPosition;
  pos.x = 300; pos.y = 200; XSetWMNormalHints(d, T, &pos);
  pos.x = 760; XSetWMNormalHints(d, U, &pos);
  XWMHints hints = {0}; hints.flags = InputHint; hints.input = False;
  XSetWMHints(d, T, &hints);
  Atom protos[] = {takeFocus, deleteWindow};
  XSetWMProtocols(d, T, protos, 2);
  XSetWMProtocols(d, U, &deleteWindow, 1);
  XSelectInput(d, C, ButtonPressMask | ButtonReleaseMask | FocusChangeMask);
  XSelectInput(d, P, KeyPressMask | FocusChangeMask);
  XMapWindow(d, P); XMapWindow(d, C); XMapWindow(d, T); XMapWindow(d, U);
  XSync(d, False);

  double requestAt = 0, check50 = 0, check600 = 0, end = Now() + runFor;
  Time releaseTime = 0;
  int trial = 0;
  printf("mode=%s T=0x%lx C=0x%lx P=0x%lx U=0x%lx\n", mode, T, C, P, U); fflush(stdout);
  while (Now() < end)
  {
    while (XPending(d))
    {
      XEvent e; XNextEvent(d, &e);
      if (e.type == ClientMessage && e.xclient.message_type == wmProtocols && (Atom)e.xclient.data.l[0] == takeFocus)
      {
        XSetInputFocus(d, P, RevertToParent, (Time)e.xclient.data.l[1]);
        printf("  %s: WM_TAKE_FOCUS t=%lu -> proxy\n", mode, (unsigned long)e.xclient.data.l[1]);
      }
      else if (e.type == ButtonRelease && e.xbutton.window == C)
      {
        trial++; releaseTime = e.xbutton.time;
        printf("  %s: trial %d release t=%lu\n", mode, trial, (unsigned long)releaseTime);
        if (!strcmp(mode, "now-current")) XSetInputFocus(d, C, RevertToParent, CurrentTime);
        else if (!strcmp(mode, "now-ts")) XSetInputFocus(d, C, RevertToParent, releaseTime);
        else if (!strncmp(mode, "delay", 5)) requestAt = Now() + 0.15;
        check50 = Now() + 0.05; check600 = Now() + 0.6;
      }
      else if (e.type == FocusIn || e.type == FocusOut)
      {
        printf("  %s: %s on %s mode=%d detail=%d\n", mode, e.type == FocusIn ? "FocusIn" : "FocusOut",
               e.xfocus.window == C ? "C" : e.xfocus.window == P ? "P" : "?", e.xfocus.mode, e.xfocus.detail);
      }
    }
    double t = Now();
    if (requestAt && t >= requestAt)
    {
      XSetInputFocus(d, C, RevertToParent, !strcmp(mode, "delay-ts") ? releaseTime : CurrentTime);
      requestAt = 0;
    }
    for (int k = 0; k < 2; k++)
    {
      double* at = k ? &check600 : &check50;
      if (*at && t >= *at)
      {
        Window f; int r; XGetInputFocus(d, &f, &r);
        const char* who = f == C ? "C(editor)" : f == P ? "P(proxy)" : f == T ? "T" : f == U ? "U" : "other";
        printf("RESULT mode=%s trial=%d after=%s focus=%s\n", mode, trial, k ? "600ms" : "50ms", who);
        *at = 0;
      }
    }
    fflush(stdout);
    XFlush(d);
    struct pollfd p = {ConnectionNumber(d), POLLIN, 0};
    poll(&p, 1, 5);
  }
  XCloseDisplay(d);
  return 0;
}
