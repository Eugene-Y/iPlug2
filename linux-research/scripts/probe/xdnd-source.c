// A scripted XDND drag source (protocol version 5) for the harness: drags local files to a target found like
// GTK does (the top-level client window under the pointer: its XdndProxy, else its XdndAware) or like Qt does
// (the deepest XdndAware window), sends positions, then drops or leaves. Prints who answered.
// Build: gcc -O1 -o xdnd-source xdnd-source.c -lX11
// Usage: xdnd-source top|deep drop|leave "x,y[;x,y...]" file...   (root coordinates; the last position is where it drops;
//        a position "R" sends XdndLeave and a new XdndEnter to the current target, as GTK 4 does at times)
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static Display* d;
static Window root, src;
static Atom aAware, aProxy, aEnter, aPosition, aStatus, aLeave, aDrop, aFinished, aSelection, aTypeList, aCopy, aUriList, aWMState;

static long now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000 + t.tv_nsec / 1000000; }

static int get_window_prop(Window w, Atom prop, Atom type, Window* out)
{
  Atom actual; int format; unsigned long n, after; unsigned char* data = NULL;
  if (XGetWindowProperty(d, w, prop, 0, 1, False, type, &actual, &format, &n, &after, &data) != Success || !data) return 0;
  int ok = format == 32 && n == 1;
  if (ok) *out = (Window) ((long*) data)[0];
  XFree(data);
  return ok;
}

static int has_prop(Window w, Atom prop)
{
  Atom actual; int format; unsigned long n, after; unsigned char* data = NULL;
  if (XGetWindowProperty(d, w, prop, 0, 1, False, AnyPropertyType, &actual, &format, &n, &after, &data) != Success) return 0;
  if (data) XFree(data);
  return actual != None;
}

static Window child_at(Window parent, int x, int y)
{
  int cx, cy; Window child = None;
  XTranslateCoordinates(d, root, parent, x, y, &cx, &cy, &child);
  return child;
}

// GTK: the client window (WM_STATE) of the top-level under the pointer; the proxy decides where messages go.
static Window find_top(int x, int y, Window* pAddressed)
{
  Window w = child_at(root, x, y), client = None;
  for (Window cur = w; cur != None; cur = child_at(cur, x, y))
    if (has_prop(cur, aWMState)) { client = cur; break; }
  if (client == None) client = w;
  *pAddressed = client;
  Window proxy;
  if (get_window_prop(client, aProxy, XA_WINDOW, &proxy)) { Window self; if (get_window_prop(proxy, aProxy, XA_WINDOW, &self) && self == proxy) return has_prop(proxy, aAware) ? proxy : None; }
  return has_prop(client, aAware) ? client : None;
}

// Qt-like: the deepest XdndAware window under the pointer.
static Window find_deep(int x, int y, Window* pAddressed)
{
  Window found = None;
  for (Window cur = child_at(root, x, y); cur != None; cur = child_at(cur, x, y))
    if (has_prop(cur, aAware)) found = cur;
  *pAddressed = found;
  return found;
}

static void send(Window to, Window addressed, Atom type, long l1, long l2, long l3, long l4)
{
  XEvent e; memset(&e, 0, sizeof e);
  e.xclient.type = ClientMessage; e.xclient.window = addressed; e.xclient.message_type = type; e.xclient.format = 32;
  e.xclient.data.l[0] = (long) src; e.xclient.data.l[1] = l1; e.xclient.data.l[2] = l2; e.xclient.data.l[3] = l3; e.xclient.data.l[4] = l4;
  XSendEvent(d, to, False, NoEventMask, &e);
  XFlush(d);
}

static char uris[8192];

// Serves selection requests meanwhile; returns the first ClientMessage of the wanted type, or 0 on timeout.
static int wait_for(Atom type, int timeout_ms, XClientMessageEvent* out)
{
  long end = now_ms() + timeout_ms;
  while (now_ms() < end) {
    while (XPending(d)) {
      XEvent e; XNextEvent(d, &e);
      if (e.type == SelectionRequest && e.xselectionrequest.selection == aSelection) {
        XSelectionRequestEvent* r = &e.xselectionrequest;
        XEvent reply; memset(&reply, 0, sizeof reply);
        reply.xselection.type = SelectionNotify; reply.xselection.requestor = r->requestor; reply.xselection.selection = r->selection;
        reply.xselection.target = r->target; reply.xselection.time = r->time; reply.xselection.property = None;
        if (r->target == aUriList) {
          XChangeProperty(d, r->requestor, r->property, aUriList, 8, PropModeReplace, (unsigned char*) uris, strlen(uris));
          reply.xselection.property = r->property;
        }
        printf("selection request for %s from 0x%lx\n", XGetAtomName(d, r->target), r->requestor);
        XSendEvent(d, r->requestor, False, NoEventMask, &reply); XFlush(d);
      } else if (e.type == ClientMessage && e.xclient.message_type == type) {
        *out = e.xclient; return 1;
      }
    }
    struct pollfd p = { ConnectionNumber(d), POLLIN, 0 }; poll(&p, 1, 20);
  }
  return 0;
}

int main(int argc, char** argv)
{
  if (argc < 5) { fprintf(stderr, "usage: %s top|deep drop|leave \"x,y[;x,y...]\" file...\n", argv[0]); return 2; }
  d = XOpenDisplay(NULL); if (!d) return 2;
  root = DefaultRootWindow(d);
#define A(n) XInternAtom(d, n, False)
  aAware = A("XdndAware"); aProxy = A("XdndProxy"); aEnter = A("XdndEnter"); aPosition = A("XdndPosition"); aStatus = A("XdndStatus");
  aLeave = A("XdndLeave"); aDrop = A("XdndDrop"); aFinished = A("XdndFinished"); aSelection = A("XdndSelection");
  aTypeList = A("XdndTypeList"); aCopy = A("XdndActionCopy"); aUriList = A("text/uri-list"); aWMState = A("WM_STATE");
  src = XCreateSimpleWindow(d, root, -10, -10, 1, 1, 0, 0, 0);
  for (int i = 4; i < argc; i++) {
    // Escaped like GTK's g_filename_to_uri: everything but unreserved characters and '/'
    strcat(uris, "file://");
    for (const unsigned char* p = (const unsigned char*) argv[i]; *p; p++) {
      char escaped[4];
      if (strchr("-._~/", *p) || (*p < 0x80 && (*p >= 'a' ? *p <= 'z' : *p >= 'A' ? *p <= 'Z' : *p >= '0' && *p <= '9'))) strncat(uris, (const char*) p, 1);
      else { snprintf(escaped, sizeof escaped, "%%%02X", *p); strcat(uris, escaped); }
    }
    strcat(uris, "\r\n");
  }
  XSetSelectionOwner(d, aSelection, src, CurrentTime);
  const int drop = strcmp(argv[2], "drop") == 0;

  int xs[16], ys[16], reenter[16] = { 0 }, n = 0;
  for (char* tok = strtok(argv[3], ";"); tok && n < 16; tok = strtok(NULL, ";")) {
    if (strcmp(tok, "R") == 0 && n > 0) { reenter[n - 1] = 1; continue; }
    if (sscanf(tok, "%d,%d", &xs[n], &ys[n]) == 2) n++;
  }
  if (!n) return 2;

  Window to = None, addressed = None;
  for (int i = 0; i < n; i++) {
    Window a; Window t = strcmp(argv[1], "deep") == 0 ? find_deep(xs[i], ys[i], &a) : find_top(xs[i], ys[i], &a);
    if (t != to) {
      if (to != None) send(to, addressed, aLeave, 0, 0, 0, 0);
      to = t; addressed = a;
      if (to == None) { printf("pos %d,%d: no XDND target\n", xs[i], ys[i]); continue; }
      printf("target 0x%lx (messages to 0x%lx)\n", addressed, to);
      send(to, addressed, aEnter, 5L << 24, (long) aUriList, 0, 0);
    }
    if (to == None) continue;
    send(to, addressed, aPosition, 0, (xs[i] << 16) | ys[i], CurrentTime, (long) aCopy);
    XClientMessageEvent st;
    if (wait_for(aStatus, 2000, &st))
      printf("pos %d,%d: status from 0x%lx accept=%ld action=%s\n", xs[i], ys[i], st.data.l[0], st.data.l[1] & 1, st.data.l[4] ? XGetAtomName(d, st.data.l[4]) : "none");
    else
      printf("pos %d,%d: no status\n", xs[i], ys[i]);
    if (reenter[i]) {
      send(to, addressed, aLeave, 0, 0, 0, 0);
      send(to, addressed, aEnter, 5L << 24, (long) aUriList, 0, 0);
      printf("re-entered 0x%lx\n", addressed);
    }
  }
  if (to == None) { printf("RESULT: no target\n"); return 1; }
  if (!drop) { send(to, addressed, aLeave, 0, 0, 0, 0); printf("RESULT: left\n"); return 0; }
  send(to, addressed, aDrop, 0, CurrentTime, 0, 0);
  XClientMessageEvent fin;
  if (wait_for(aFinished, 3000, &fin)) {
    printf("finished from 0x%lx accepted=%ld\n", fin.data.l[0], fin.data.l[1] & 1);
    printf("RESULT: %s\n", (fin.data.l[1] & 1) ? "accepted" : "refused");
    return 0;
  }
  printf("RESULT: no finished\n");
  return 1;
}
