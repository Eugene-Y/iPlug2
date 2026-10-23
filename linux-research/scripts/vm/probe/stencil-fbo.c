// Does stencil work in an FBO whose stencil is a separate renderbuffer, as NanoVG's nvgluCreateFramebuffer makes it
// (GL_STENCIL_INDEX8 first, GL_DEPTH24_STENCIL8 only if the first is incomplete)? iPlug's NanoVG draws every frame
// into such an FBO; without a working stencil, concave fills paint their whole bounding box.
// Build: gcc -O1 -o stencil-fbo stencil-fbo.c -lGL -lX11      Run: ./stencil-fbo  (needs DISPLAY)
// Marks the left half in the stencil, then fills everything where stencil == 1: the right half must stay black.
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <string.h>

enum { W = 64, H = 32 };

static void quad(float x0, float x1)
{
  glBegin(GL_QUADS);
  glVertex2f(x0, -1.f); glVertex2f(x1, -1.f); glVertex2f(x1, 1.f); glVertex2f(x0, 1.f);
  glEnd();
}

// Returns 1 if the stencil masked the fill as it should.
static int stencilWorks(const char* label)
{
  GLint bits = -1;
  glGetIntegerv(GL_STENCIL_BITS, &bits);
  glViewport(0, 0, W, H);
  glDisable(GL_SCISSOR_TEST);
  glClearColor(0, 0, 0, 1);
  glClearStencil(0);
  glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  glEnable(GL_STENCIL_TEST);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glStencilFunc(GL_ALWAYS, 1, 0xff);
  glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
  quad(-1.f, 0.f);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glStencilFunc(GL_EQUAL, 1, 0xff);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
  glColor3f(1, 1, 1);
  quad(-1.f, 1.f);
  glDisable(GL_STENCIL_TEST);
  glFinish();
  unsigned char left[4] = {0}, right[4] = {0};
  glReadPixels(W / 4, H / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, left);
  glReadPixels(3 * W / 4, H / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, right);
  int ok = left[0] > 200 && right[0] < 50;
  printf("%-28s GL_STENCIL_BITS=%d left=%u right=%u -> %s\n", label, bits, left[0], right[0], ok ? "OK" : "BROKEN");
  return ok;
}

// NanoVG's nonzero fill: front faces INCR_WRAP, back faces DECR_WRAP (glStencilOpSeparate), no culling, then cover
// where stencil != 0. A clockwise and a counter-clockwise quad over the whole viewport must cancel to 0 in either
// drawing order (with DECR first the count wraps to 255 and back); a single quad must leave 1.
static int windingWorks(const char* label, int backFirst)
{
  glViewport(0, 0, W, H);
  glClearColor(0, 0, 0, 1);
  glClearStencil(0);
  glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  glDisable(GL_CULL_FACE);
  glEnable(GL_STENCIL_TEST);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glStencilMask(0xff);
  glStencilFunc(GL_ALWAYS, 0, 0xff);
  glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP);
  glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP);
  for (int pass = 0; pass < 2; pass++)
  {
    int ccw = (pass == 0) != backFirst; // left half: one CCW and one CW quad; right half: one CCW quad only
    glBegin(GL_QUADS);
    if (ccw) { glVertex2f(-1, -1); glVertex2f(0, -1); glVertex2f(0, 1); glVertex2f(-1, 1); }
    else     { glVertex2f(-1, -1); glVertex2f(-1, 1); glVertex2f(0, 1); glVertex2f(0, -1); }
    glEnd();
  }
  quad(0.f, 1.f);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glStencilFunc(GL_NOTEQUAL, 0, 0xff);
  glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);
  glColor3f(1, 1, 1);
  quad(-1.f, 1.f);
  glDisable(GL_STENCIL_TEST);
  glFinish();
  unsigned char left[4] = {0}, right[4] = {0};
  glReadPixels(W / 4, H / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, left);
  glReadPixels(3 * W / 4, H / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, right);
  int ok = left[0] < 50 && right[0] > 200;
  printf("%-28s winding %s: cancelled=%u single=%u -> %s\n", label, backFirst ? "CW first " : "CCW first",
         left[0], right[0], ok ? "OK" : "BROKEN");
  return ok;
}

static unsigned char pixelAt(float x, float y)
{
  unsigned char px[4] = {0};
  glReadPixels((int) ((x + 1.f) * 0.5f * W), (int) ((y + 1.f) * 0.5f * H), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
  return px[0];
}

// NanoVG fills a path as glDrawArrays(GL_TRIANGLE_FAN, offset, count) from one VBO shared by the whole frame, so the
// offset is rarely 0. Fills an L (concave; its bounding box's top-right quarter is outside) through the stencil, with
// `first` junk vertices before it in the buffer: a fan built around the wrong hub covers that quarter too.
static int fanWorks(const char* label, int first)
{
  static const float L[] = { -0.8f, -0.8f, 0.8f, -0.8f, 0.8f, -0.4f, -0.4f, -0.4f, -0.4f, 0.8f, -0.8f, 0.8f };
  float verts[2 * (16 + 6)];
  for (int i = 0; i < first; i++) { verts[2 * i] = 1.f; verts[2 * i + 1] = 1.f; }
  memcpy(verts + 2 * first, L, sizeof L);
  GLuint vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr) (sizeof(float) * 2 * (first + 6)), verts, GL_STREAM_DRAW);
  glEnableClientState(GL_VERTEX_ARRAY);
  glVertexPointer(2, GL_FLOAT, 0, 0);
  glViewport(0, 0, W, H);
  glClearColor(0, 0, 0, 1);
  glClearStencil(0);
  glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  glDisable(GL_CULL_FACE);
  glEnable(GL_STENCIL_TEST);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glStencilMask(0xff);
  glStencilFunc(GL_ALWAYS, 0, 0xff);
  glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP);
  glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP);
  glDrawArrays(GL_TRIANGLE_FAN, first, 6);
  glDisableClientState(GL_VERTEX_ARRAY);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glStencilFunc(GL_NOTEQUAL, 0, 0xff);
  glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);
  glColor3f(1, 1, 1);
  quad(-1.f, 1.f);
  glDisable(GL_STENCIL_TEST);
  glFinish();
  unsigned char inBottom = pixelAt(0.4f, -0.6f), inLeft = pixelAt(-0.6f, 0.4f), outside = pixelAt(0.4f, 0.4f);
  int ok = inBottom > 200 && inLeft > 200 && outside < 50;
  printf("%-28s fan first=%-2d inside=%u,%u outside=%u -> %s\n", label, first, inBottom, inLeft, outside,
         ok ? "OK" : "BROKEN");
  glDeleteBuffers(1, &vbo);
  return ok;
}

static void fboCase(GLenum stencilFormat, const char* label)
{
  GLuint tex, fbo, rbo;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glGenRenderbuffers(1, &rbo);
  glBindRenderbuffer(GL_RENDERBUFFER, rbo);
  glRenderbufferStorage(GL_RENDERBUFFER, stencilFormat, W, H);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
  if (stencilFormat == GL_DEPTH24_STENCIL8)
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);
  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  GLint size = -1;
  glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                        GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &size);
  printf("%-28s status=0x%x (%s) attachment stencil size=%d\n", label, status,
         status == GL_FRAMEBUFFER_COMPLETE ? "complete" : "INCOMPLETE", size);
  if (status == GL_FRAMEBUFFER_COMPLETE)
  {
    stencilWorks(label);
    windingWorks(label, 0);
    windingWorks(label, 1);
    fanWorks(label, 0);
    fanWorks(label, 7);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDeleteFramebuffers(1, &fbo);
  glDeleteRenderbuffers(1, &rbo);
  glDeleteTextures(1, &tex);
}

// What a new stencil renderbuffer holds before anyone clears it: fill one FBO's stencil with 0x55, delete it, make a
// new FBO the same way nvgluCreateFramebuffer does and read its stencil back uncleared.
static void freshStencil(int round)
{
  GLuint tex[2], fbo[2], rbo[2];
  glGenTextures(2, tex); glGenFramebuffers(2, fbo); glGenRenderbuffers(2, rbo);
  for (int k = 0; k < 2; k++)
  {
    glBindTexture(GL_TEXTURE_2D, tex[k]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo[k]);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo[k]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, W, H);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex[k], 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo[k]);
    if (k == 0)
    {
      glClearStencil(0x55);
      glClear(GL_STENCIL_BUFFER_BIT);
      glFinish();
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      glDeleteFramebuffers(1, &fbo[0]); glDeleteRenderbuffers(1, &rbo[0]); glDeleteTextures(1, &tex[0]);
    }
  }
  unsigned char st[W * H];
  memset(st, 0xee, sizeof st);
  glReadPixels(0, 0, W, H, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, st);
  int nonzero = 0;
  for (int i = 0; i < W * H; i++) nonzero += st[i] != 0;
  printf("fresh FBO stencil, round %d: %d of %d pixels nonzero (first=0x%02x)\n", round, nonzero, W * H, st[0]);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDeleteFramebuffers(1, &fbo[1]); glDeleteRenderbuffers(1, &rbo[1]); glDeleteTextures(1, &tex[1]);
}

int main(void)
{
  Display* d = XOpenDisplay(NULL);
  if (!d) { fprintf(stderr, "no display\n"); return 2; }
  int attrs[] = { GLX_RGBA, GLX_DOUBLEBUFFER, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
                  GLX_STENCIL_SIZE, 8, None };
  XVisualInfo* vi = glXChooseVisual(d, DefaultScreen(d), attrs);
  if (!vi) { fprintf(stderr, "no visual\n"); return 2; }
  Window root = RootWindow(d, vi->screen);
  XSetWindowAttributes swa;
  memset(&swa, 0, sizeof swa);
  swa.colormap = XCreateColormap(d, root, vi->visual, AllocNone);
  Window win = XCreateWindow(d, root, 0, 0, W, H, 0, vi->depth, InputOutput, vi->visual,
                             CWColormap | CWBorderPixel, &swa);
  XMapWindow(d, win);
  XSync(d, False);
  GLXContext ctx = glXCreateContext(d, vi, NULL, True);
  glXMakeCurrent(d, win, ctx);
  printf("GL_RENDERER=%s\nGL_VERSION=%s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));
  stencilWorks("window (default framebuffer)");
  windingWorks("window (default framebuffer)", 0);
  windingWorks("window (default framebuffer)", 1);
  fanWorks("window (default framebuffer)", 0);
  fanWorks("window (default framebuffer)", 7);
  fboCase(GL_STENCIL_INDEX8, "FBO + GL_STENCIL_INDEX8");
  fboCase(GL_DEPTH24_STENCIL8, "FBO + GL_DEPTH24_STENCIL8");
  for (int round = 0; round < 3; round++)
    freshStencil(round);
  glXMakeCurrent(d, None, NULL);
  glXDestroyContext(d, ctx);
  XDestroyWindow(d, win);
  XCloseDisplay(d);
  return 0;
}
