// RSS over N cycles of the P3a editor lifecycle without iPlug: [XOpenDisplay], window, GLX 1.2 context,
// make current, clear + swap a few frames, destroy all, [XCloseDisplay]. Tells a driver leak from ours.
// Build: gcc -O1 -o glx-churn glx-churn.c -lX11 -ldl     Run: glx-churn <cycles> <per-display|shared-display>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct __GLXcontextRec* GLXContext;
#define GLX_RGBA 4
#define GLX_DOUBLEBUFFER 5
#define GLX_RED_SIZE 8
#define GLX_GREEN_SIZE 9
#define GLX_BLUE_SIZE 10
#define GLX_STENCIL_SIZE 13
#define GL_COLOR_BUFFER_BIT 0x4000

static XVisualInfo* (*pChooseVisual)(Display*, int, int*);
static GLXContext (*pCreateContext)(Display*, XVisualInfo*, GLXContext, Bool);
static Bool (*pMakeCurrent)(Display*, Window, GLXContext);
static void (*pDestroyContext)(Display*, GLXContext);
static void (*pSwapBuffers)(Display*, Window);
static void (*pClearColor)(float, float, float, float);
static void (*pClear)(unsigned);

static long RssKb(void)
{
  FILE* f = fopen("/proc/self/status", "r"); char line[256]; long kb = -1;
  while (fgets(line, sizeof line, f)) if (!strncmp(line, "VmRSS:", 6)) kb = atol(line + 6);
  fclose(f); return kb;
}

static void Cycle(Display* d, int i)
{
  int attrs[] = {GLX_RGBA, GLX_DOUBLEBUFFER, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8, GLX_STENCIL_SIZE, 8, None};
  XVisualInfo* vi = pChooseVisual(d, DefaultScreen(d), attrs);
  if (!vi) { fprintf(stderr, "no visual\n"); exit(1); }
  XSetWindowAttributes swa = {0};
  swa.colormap = XCreateColormap(d, RootWindow(d, vi->screen), vi->visual, AllocNone);
  swa.event_mask = ExposureMask;
  Window w = XCreateWindow(d, RootWindow(d, vi->screen), 0, 0, 400, 300, 0, vi->depth, InputOutput, vi->visual,
                           CWColormap | CWEventMask | CWBorderPixel, &swa);
  GLXContext c = pCreateContext(d, vi, NULL, True);
  pMakeCurrent(d, w, c);
  for (int f = 0; f < 3; f++) { pClearColor((i % 7) / 7.f, 0.3f, 0.5f, 1.f); pClear(GL_COLOR_BUFFER_BIT); pSwapBuffers(d, w); }
  pMakeCurrent(d, None, NULL);
  pDestroyContext(d, c);
  XDestroyWindow(d, w);
  XFreeColormap(d, swa.colormap);
  XFree(vi);
  XSync(d, False);
}

int main(int argc, char** argv)
{
  int n = argc > 1 ? atoi(argv[1]) : 100;
  int perDisplay = !(argc > 2 && !strcmp(argv[2], "shared-display"));
  void* gl = dlopen("libGL.so.1", RTLD_NOW | RTLD_LOCAL);
  if (!gl) { fprintf(stderr, "%s\n", dlerror()); return 1; }
  pChooseVisual = dlsym(gl, "glXChooseVisual"); pCreateContext = dlsym(gl, "glXCreateContext");
  pMakeCurrent = dlsym(gl, "glXMakeCurrent"); pDestroyContext = dlsym(gl, "glXDestroyContext");
  pSwapBuffers = dlsym(gl, "glXSwapBuffers"); pClearColor = dlsym(gl, "glClearColor"); pClear = dlsym(gl, "glClear");
  Display* shared = perDisplay ? NULL : XOpenDisplay(NULL);
  printf("mode=%s start rss_kb=%ld\n", perDisplay ? "per-display" : "shared-display", RssKb());
  for (int i = 1; i <= n; i++)
  {
    Display* d = perDisplay ? XOpenDisplay(NULL) : shared;
    Cycle(d, i);
    if (perDisplay) XCloseDisplay(d);
    if (i % (n / 8 ? n / 8 : 1) == 0) printf("cycle %d rss_kb=%ld\n", i, RssKb());
  }
  return 0;
}
