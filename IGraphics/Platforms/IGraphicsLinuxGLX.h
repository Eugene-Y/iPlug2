/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#pragma once

/** @file
 * @brief GLX entry points loaded from libGL.so.1 at run time, plus a scoped X error trap.
 *
 * libGL is dlopen'ed rather than linked so that the plug-in still loads (and scans, and processes
 * audio) on machines without a GL driver; only opening the editor fails there.
 * Include after glad and <GL/glx.h>. Main thread only, like the rest of IGraphicsLinux.
 */

#include <dlfcn.h>
#include <cstdio>
#include <cstring>
#include <type_traits>

BEGIN_IPLUG_NAMESPACE
BEGIN_IGRAPHICS_NAMESPACE

struct GLXFunctions
{
  decltype(&glXChooseVisual) ChooseVisual = nullptr;
  decltype(&glXChooseFBConfig) ChooseFBConfig = nullptr;
  decltype(&glXGetVisualFromFBConfig) GetVisualFromFBConfig = nullptr;
  decltype(&glXCreateContext) CreateContext = nullptr;
  decltype(&glXDestroyContext) DestroyContext = nullptr;
  decltype(&glXMakeCurrent) MakeCurrent = nullptr;
  decltype(&glXSwapBuffers) SwapBuffers = nullptr;
  decltype(&glXGetCurrentContext) GetCurrentContext = nullptr;
  decltype(&glXGetCurrentDrawable) GetCurrentDrawable = nullptr;
  decltype(&glXGetCurrentDisplay) GetCurrentDisplay = nullptr;
  decltype(&glXQueryExtensionsString) QueryExtensionsString = nullptr;
  decltype(&glXGetProcAddressARB) GetProcAddressARB = nullptr;
  void* mLib = nullptr;

  /** Resolves any GL or GLX function: glad's loader for the GL entry points */
  static void* GetProc(const char* name)
  {
    const GLXFunctions* pGLX = Get();
    if (!pGLX)
      return nullptr;
    void* pProc = reinterpret_cast<void*>(pGLX->GetProcAddressARB(reinterpret_cast<const GLubyte*>(name)));
    return pProc ? pProc : dlsym(pGLX->mLib, name);
  }

  /** The process-wide table, loaded on first use, or nullptr without a usable libGL.
   * Never unloaded: the host and other plug-ins share the same libGL, and Mesa does not survive a dlclose
   * while contexts of the same process exist. */
  static const GLXFunctions* Get()
  {
    static GLXFunctions sGLX;
    static bool sTried = false;
    if (!sTried)
    {
      sTried = true;
      if (!sGLX.Load())
      {
        const char* pError = dlerror(); // NULL when dlclose() after a missing symbol cleared it
        fprintf(stderr, "iPlug2: no usable libGL.so.1 (%s), the editor cannot open\n", pError ? pError : "GLX entry points missing");
      }
    }
    return sGLX.mLib ? &sGLX : nullptr;
  }

private:
  bool Load()
  {
    mLib = dlopen("libGL.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!mLib)
      return false;

    bool ok = true;
    auto load = [&](auto& pFunc, const char* name) {
      pFunc = reinterpret_cast<std::remove_reference_t<decltype(pFunc)>>(dlsym(mLib, name));
      ok = ok && pFunc;
    };
    load(ChooseVisual, "glXChooseVisual");
    load(ChooseFBConfig, "glXChooseFBConfig");
    load(GetVisualFromFBConfig, "glXGetVisualFromFBConfig");
    load(CreateContext, "glXCreateContext");
    load(DestroyContext, "glXDestroyContext");
    load(MakeCurrent, "glXMakeCurrent");
    load(SwapBuffers, "glXSwapBuffers");
    load(GetCurrentContext, "glXGetCurrentContext");
    load(GetCurrentDrawable, "glXGetCurrentDrawable");
    load(GetCurrentDisplay, "glXGetCurrentDisplay");
    load(QueryExtensionsString, "glXQueryExtensionsString");
    load(GetProcAddressARB, "glXGetProcAddressARB");

    if (!ok)
    {
      dlclose(mLib);
      mLib = nullptr;
    }
    return ok;
  }
};

/** Swallows X errors raised on one Display while in scope; errors of other connections go to the previous handler.
 * Xlib's default handler exits the process, and a plug-in's requests can fail through no fault of its own:
 * the host may destroy the parent (and so our window) before it closes the editor. Errors are reported when
 * Xlib reads the connection, so every call that reads it (event drain, swap, close) runs inside a trap. */
class ScopedXErrorTrap
{
public:
  explicit ScopedXErrorTrap(Display* pDisplay)
  : mPrevDisplay(sDisplay)
  , mPrevCount(sErrorCount)
  {
    sDisplay = pDisplay;
    sErrorCount = 0;
    mPrevHandler = XSetErrorHandler(&Handler);
    if (!sOuterHandler || mPrevHandler != &Handler)
      sOuterHandler = mPrevHandler;
  }

  ~ScopedXErrorTrap()
  {
    XSetErrorHandler(mPrevHandler);
    sDisplay = mPrevDisplay;
    sErrorCount = mPrevCount + sErrorCount;
  }

  ScopedXErrorTrap(const ScopedXErrorTrap&) = delete;
  ScopedXErrorTrap& operator=(const ScopedXErrorTrap&) = delete;

  /** Errors seen so far in this scope. Call XSync() first to wait for the replies of earlier requests. */
  int GetErrorCount() const { return sErrorCount; }

  /** Before XCloseDisplay: a later connection at the same address reports its first error again. */
  static void ForgetDisplay(Display* pDisplay)
  {
    if (sReportedDisplay == pDisplay)
      sReportedDisplay = nullptr;
  }

private:
  static int Handler(Display* pDisplay, XErrorEvent* pError)
  {
    if (pDisplay != sDisplay)
      return (sOuterHandler && sOuterHandler != &Handler) ? sOuterHandler(pDisplay, pError) : 0;

    sErrorCount++;
#ifdef NDEBUG
    // Release builds name the first error of each editor connection only.
    if (sReportedDisplay == pDisplay)
      return 0;
    sReportedDisplay = pDisplay;
#endif
    char text[128];
    XGetErrorText(pDisplay, pError->error_code, text, sizeof(text));
    fprintf(stderr, "iPlug2: X error ignored: %s (request %d.%d, resource 0x%lx)\n", text,
            pError->request_code, pError->minor_code, pError->resourceid);
    return 0;
  }

  static inline Display* sDisplay = nullptr;
  static inline int sErrorCount = 0;
  static inline XErrorHandler sOuterHandler = nullptr;
  static inline Display* sReportedDisplay = nullptr;
  Display* mPrevDisplay;
  int mPrevCount;
  XErrorHandler mPrevHandler = nullptr;
};

END_IGRAPHICS_NAMESPACE
END_IPLUG_NAMESPACE
