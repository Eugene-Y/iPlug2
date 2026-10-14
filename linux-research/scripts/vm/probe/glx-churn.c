// RSS over N cycles of the P3a editor lifecycle without iPlug: [XOpenDisplay], window, GLX 1.2 context,
// make current, clear + swap a few frames, destroy all, [XCloseDisplay]. Tells a driver leak from ours.
// Build: gcc -O1 -o glx-churn glx-churn.c -lX11 -ldl
// Run:   glx-churn <cycles> <per-display|shared-display> [objects]
//        objects: also a 1024x768 RGBA texture + FBO with a stencil renderbuffer, a shader program and a VBO per cycle
//        (what a NanoVG GL2 editor creates), all deleted before the context is destroyed.
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
static void* (*pGetProc)(const char*);
static int sObjects;

static long RssKb(void)
{
  FILE* f = fopen("/proc/self/status", "r"); char line[256]; long kb = -1;
  while (fgets(line, sizeof line, f)) if (!strncmp(line, "VmRSS:", 6)) kb = atol(line + 6);
  fclose(f); return kb;
}

typedef unsigned GLuint;
#define GL_TEXTURE_2D 0x0DE1
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401
#define GL_FRAMEBUFFER 0x8D40
#define GL_RENDERBUFFER 0x8D41
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_STENCIL_ATTACHMENT 0x8D20
#define GL_STENCIL_INDEX8 0x8D48
#define GL_ARRAY_BUFFER 0x8892
#define GL_STREAM_DRAW 0x88E0
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_TRIANGLES 0x0004

static void Objects(int i)
{
  static void (*GenTextures)(int, GLuint*), (*BindTexture)(unsigned, GLuint), (*DeleteTextures)(int, const GLuint*);
  static void (*TexImage2D)(unsigned, int, int, int, int, int, unsigned, unsigned, const void*);
  static void (*GenFramebuffers)(int, GLuint*), (*BindFramebuffer)(unsigned, GLuint), (*DeleteFramebuffers)(int, const GLuint*);
  static void (*FramebufferTexture2D)(unsigned, unsigned, unsigned, GLuint, int);
  static void (*GenRenderbuffers)(int, GLuint*), (*BindRenderbuffer)(unsigned, GLuint), (*DeleteRenderbuffers)(int, const GLuint*);
  static void (*RenderbufferStorage)(unsigned, unsigned, int, int);
  static void (*FramebufferRenderbuffer)(unsigned, unsigned, unsigned, GLuint);
  static void (*GenBuffers)(int, GLuint*), (*BindBuffer)(unsigned, GLuint), (*DeleteBuffers)(int, const GLuint*);
  static void (*BufferData)(unsigned, long, const void*, unsigned);
  static GLuint (*CreateShader)(unsigned), (*CreateProgram)(void);
  static void (*ShaderSource)(GLuint, int, const char* const*, const int*), (*CompileShader)(GLuint);
  static void (*AttachShader)(GLuint, GLuint), (*LinkProgram)(GLuint), (*UseProgram)(GLuint);
  static void (*DeleteShader)(GLuint), (*DeleteProgram)(GLuint);
  static void (*VertexAttribPointer)(GLuint, int, unsigned, unsigned char, int, const void*), (*EnableVertexAttribArray)(GLuint);
  static void (*DrawArrays)(unsigned, int, int);
  if (!GenTextures)
  {
#define L(name) *(void**)&name = pGetProc("gl" #name)
    L(GenTextures); L(BindTexture); L(DeleteTextures); L(TexImage2D); L(GenFramebuffers); L(BindFramebuffer);
    L(DeleteFramebuffers); L(FramebufferTexture2D); L(GenRenderbuffers); L(BindRenderbuffer); L(DeleteRenderbuffers);
    L(RenderbufferStorage); L(FramebufferRenderbuffer); L(GenBuffers); L(BindBuffer); L(DeleteBuffers); L(BufferData);
    L(CreateShader); L(CreateProgram); L(ShaderSource); L(CompileShader); L(AttachShader); L(LinkProgram); L(UseProgram);
    L(DeleteShader); L(DeleteProgram); L(VertexAttribPointer); L(EnableVertexAttribArray); L(DrawArrays);
  }
  static unsigned char pixels[1024 * 768 * 4];
  pixels[i % sizeof pixels] = (unsigned char)i;
  GLuint tex, fbo, rb, vbo;
  GenTextures(1, &tex); BindTexture(GL_TEXTURE_2D, tex);
  TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1024, 768, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  GenFramebuffers(1, &fbo); BindFramebuffer(GL_FRAMEBUFFER, fbo);
  FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
  GenRenderbuffers(1, &rb); BindRenderbuffer(GL_RENDERBUFFER, rb);
  RenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, 1024, 768);
  FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb);
  const char* vs = "attribute vec2 p; void main() { gl_Position = vec4(p, 0.0, 1.0); }";
  const char* fs = "void main() { gl_FragColor = vec4(1.0, 0.5, 0.0, 1.0); }";
  GLuint v = CreateShader(GL_VERTEX_SHADER), f = CreateShader(GL_FRAGMENT_SHADER), prog = CreateProgram();
  ShaderSource(v, 1, &vs, NULL); CompileShader(v); ShaderSource(f, 1, &fs, NULL); CompileShader(f);
  AttachShader(prog, v); AttachShader(prog, f); LinkProgram(prog); UseProgram(prog);
  float tri[] = {-1, -1, 1, -1, 0, 1};
  GenBuffers(1, &vbo); BindBuffer(GL_ARRAY_BUFFER, vbo); BufferData(GL_ARRAY_BUFFER, sizeof tri, tri, GL_STREAM_DRAW);
  VertexAttribPointer(0, 2, 0x1406, 0, 0, 0); EnableVertexAttribArray(0);
  for (int k = 0; k < 5; k++) DrawArrays(GL_TRIANGLES, 0, 3);
  BindFramebuffer(GL_FRAMEBUFFER, 0);
  DrawArrays(GL_TRIANGLES, 0, 3);
  UseProgram(0); BindBuffer(GL_ARRAY_BUFFER, 0); BindTexture(GL_TEXTURE_2D, 0); BindRenderbuffer(GL_RENDERBUFFER, 0);
  DeleteBuffers(1, &vbo); DeleteProgram(prog); DeleteShader(v); DeleteShader(f);
  DeleteRenderbuffers(1, &rb); DeleteFramebuffers(1, &fbo); DeleteTextures(1, &tex);
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
  if (sObjects) Objects(i);
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
  pGetProc = dlsym(gl, "glXGetProcAddressARB");
  sObjects = argc > 3 && !strcmp(argv[3], "objects");
  Display* shared = perDisplay ? NULL : XOpenDisplay(NULL);
  printf("mode=%s%s start rss_kb=%ld\n", perDisplay ? "per-display" : "shared-display", sObjects ? "+objects" : "", RssKb());
  for (int i = 1; i <= n; i++)
  {
    Display* d = perDisplay ? XOpenDisplay(NULL) : shared;
    Cycle(d, i);
    if (perDisplay) XCloseDisplay(d);
    if (i % (n / 8 ? n / 8 : 1) == 0) printf("cycle %d rss_kb=%ld\n", i, RssKb());
  }
  return 0;
}
