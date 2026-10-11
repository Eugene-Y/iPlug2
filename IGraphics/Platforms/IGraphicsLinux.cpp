/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>

#include "IGraphicsLinux.h"
#include "IPlugAPIBase.h"
#include "Linux/IPlugLinuxHostLoop.h"
#include "Linux/IPlugLinuxOpen.h"

#if defined IGRAPHICS_NANOVG && defined IGRAPHICS_GL
  #include "IGraphicsNanoVG.cpp"
#else
  #error IGraphicsLinux needs IGRAPHICS_NANOVG with IGRAPHICS_GL2 or IGRAPHICS_GL3
#endif

// X11 after every iPlug header: its macros (None, Bool, Status, Success, ...) collide with iPlug names.
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <GL/glx.h>

#include "IGraphicsLinuxGLX.h"

using namespace iplug;
using namespace igraphics;

namespace
{
constexpr long kWindowEventMask = ExposureMask | StructureNotifyMask | ButtonPressMask | ButtonReleaseMask
                                | PointerMotionMask | LeaveWindowMask;
constexpr Time kDoubleClickMs = 400;
constexpr float kDoubleClickSlop = 4.f;
constexpr unsigned kWheelUp = Button4;
constexpr unsigned kWheelDown = Button5;
constexpr long kXEmbedVersion = 0;
constexpr long kXEmbedMapped = 1;

bool HasGLXExtension(const char* extensions, const char* name)
{
  const size_t length = strlen(name);
  for (const char* p = extensions; p && (p = strstr(p, name)); p += length)
  {
    const bool startsToken = (p == extensions) || p[-1] == ' ';
    const bool endsToken = p[length] == ' ' || p[length] == '\0';
    if (startsToken && endsToken)
      return true;
  }
  return false;
}

IMouseMod MouseModFromState(unsigned int state)
{
  return IMouseMod((state & Button1Mask) != 0, (state & Button3Mask) != 0,
                   (state & ShiftMask) != 0, (state & ControlMask) != 0, (state & Mod1Mask) != 0);
}

class LinuxFont final : public PlatformFont
{
public:
  explicit LinuxFont(IFontDataPtr&& data)
  : PlatformFont(false)
  , mData(std::move(data))
  {}

  IFontDataPtr GetFontData() override
  {
    return IFontDataPtr(new IFontData(mData->Get(), mData->GetSize(), mData->GetFaceIdx()));
  }

private:
  IFontDataPtr mData;
};

PlatformFontPtr MakeFont(const void* pData, int dataSize)
{
  if (!pData || dataSize <= 0)
    return nullptr;
  IFontDataPtr data(new IFontData(pData, dataSize, 0));
  if (!data->IsValid())
    return nullptr;
  return PlatformFontPtr(new LinuxFont(std::move(data)));
}
} // namespace

struct IGraphicsLinux::Impl
{
  const GLXFunctions* mGLX = nullptr;
  Display* mDisplay = nullptr;
  Window mWindow = 0;
  Window mParent = 0; // 0: top-level, because the host gave no usable parent
  Colormap mColormap = 0;
  GLXContext mGLContext = nullptr;
  bool mViewInitialized = false;
  bool mMapped = false;
  Atom mWMDeleteWindow = 0;

  int mWindowW = 0; // physical size of our X window: last requested, or last seen when someone else resized it
  int mWindowH = 0;
  unsigned long mResizeSerial = 0; // request serial of our last XResizeWindow
  int mParentW = 0;
  int mParentH = 0;

  HostLoop* mLoop = nullptr;
  int mTimerId = 0;
  int mFdId = 0;
  // Shared with the callbacks on the stack: an event handler can make the host close the editor (and so
  // delete this object) synchronously; the callback checks the flag before touching the editor again.
  std::shared_ptr<bool> mOpen;

  int mGLDepth = 0;
  Display* mPrevDisplay = nullptr;
  GLXDrawable mPrevDrawable = 0;
  GLXContext mPrevContext = nullptr;

  float mCursorX = 0.f;
  float mCursorY = 0.f;
  Time mLastClickTime = 0;
  unsigned mLastClickButton = 0;
  float mLastClickX = 0.f;
  float mLastClickY = 0.f;
};

IGraphicsLinux::IGraphicsLinux(IGEditorDelegate& dlg, int w, int h, int fps, float scale)
: IGRAPHICS_DRAW_CLASS(dlg, w, h, fps, scale)
, mImpl(std::make_unique<Impl>())
{
}

IGraphicsLinux::~IGraphicsLinux()
{
  CloseWindow();
}

void* IGraphicsLinux::OpenWindow(void* pParent)
{
  CloseWindow();

  auto* pPlug = dynamic_cast<IPlugAPIBase*>(GetDelegate());
  if (!pPlug)
  {
    fprintf(stderr, "iPlug2: IGraphicsLinux needs an IPlugAPIBase delegate for its host loop, the editor cannot open\n");
    return nullptr;
  }

  Impl& w = *mImpl;
  w.mGLX = GLXFunctions::Get();
  if (!w.mGLX)
    return nullptr;

  // A connection of our own: the host's Display is not ours to read from, and two editors must not
  // steal each other's events.
  w.mDisplay = XOpenDisplay(nullptr);
  if (!w.mDisplay)
  {
    fprintf(stderr, "iPlug2: XOpenDisplay failed, the editor cannot open\n");
    return nullptr;
  }

  mInOpenWindow = true;
  auto fail = [this](const char* what) -> void* {
    fprintf(stderr, "iPlug2: IGraphicsLinux::OpenWindow: %s\n", what);
    DestroyWindowResources();
    mInOpenWindow = false;
    return nullptr;
  };

  Display* pDisplay = w.mDisplay;
  ScopedXErrorTrap trap(pDisplay);
  const int screen = DefaultScreen(pDisplay);
  const Window root = RootWindow(pDisplay, screen);

  w.mParent = static_cast<Window>(reinterpret_cast<uintptr_t>(pParent));
  if (w.mParent)
  {
    XWindowAttributes parentAttributes {};
    if (XGetWindowAttributes(pDisplay, w.mParent, &parentAttributes))
    {
      w.mParentW = parentAttributes.width;
      w.mParentH = parentAttributes.height;
    }
    else
    {
      fprintf(stderr, "iPlug2: the host's parent window 0x%lx is not an X window, opening a top-level window\n", w.mParent);
      w.mParent = 0;
    }
  }

  int visualAttributes[] = {
    GLX_RGBA, GLX_DOUBLEBUFFER,
    GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
    GLX_STENCIL_SIZE, 8,
    None
  };
  XVisualInfo* pVisual = w.mGLX->ChooseVisual(pDisplay, screen, visualAttributes);
  if (!pVisual)
    return fail("no GLX visual with RGB8, stencil 8 and double buffering");

  w.mColormap = XCreateColormap(pDisplay, root, pVisual->visual, AllocNone);
  XSetWindowAttributes attributes {};
  attributes.colormap = w.mColormap;
  attributes.event_mask = kWindowEventMask;
  // The GL visual may differ from the parent's; without an explicit border pixel that is a BadMatch.
  attributes.border_pixel = 0;
  attributes.background_pixel = BlackPixel(pDisplay, screen);

  w.mWindowW = std::max(1, static_cast<int>(WindowWidth() * GetScreenScale()));
  w.mWindowH = std::max(1, static_cast<int>(WindowHeight() * GetScreenScale()));
  w.mWindow = XCreateWindow(pDisplay, w.mParent ? w.mParent : root, 0, 0, w.mWindowW, w.mWindowH, 0,
                            pVisual->depth, InputOutput, pVisual->visual,
                            CWColormap | CWEventMask | CWBorderPixel | CWBackPixel, &attributes);

  const long xembedInfo[] = { kXEmbedVersion, kXEmbedMapped };
  const Atom xembedInfoAtom = XInternAtom(pDisplay, "_XEMBED_INFO", False);
  XChangeProperty(pDisplay, w.mWindow, xembedInfoAtom, xembedInfoAtom, 32, PropModeReplace,
                  reinterpret_cast<const unsigned char*>(xembedInfo), 2);

  if (w.mParent)
  {
    // Our selection on the parent belongs to this connection only; the host's own event mask is untouched.
    XSelectInput(pDisplay, w.mParent, StructureNotifyMask);
  }
  else
  {
    w.mWMDeleteWindow = XInternAtom(pDisplay, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(pDisplay, w.mWindow, &w.mWMDeleteWindow, 1);
    XStoreName(pDisplay, w.mWindow, pPlug->GetPluginName());
  }

  w.mGLContext = w.mGLX->CreateContext(pDisplay, pVisual, nullptr, True);
  XFree(pVisual);
  if (!w.mGLContext)
    return fail("glXCreateContext failed");

  XMapWindow(pDisplay, w.mWindow);
  XSync(pDisplay, False);
  if (trap.GetErrorCount() > 0)
    return fail("X errors while creating the window");

  ActivateGLContext();
  if (w.mGLX->GetCurrentContext() != w.mGLContext || !gladLoadGLLoader(&GLXFunctions::GetProc))
  {
    DeactivateGLContext();
    return fail("the GL context cannot be made current, or GL functions are missing");
  }

  // Pacing comes from the host timer; a blocking swap would stall the host's main thread once per editor.
  const char* glxExtensions = w.mGLX->QueryExtensionsString(pDisplay, screen);
  if (HasGLXExtension(glxExtensions, "GLX_EXT_swap_control"))
  {
    if (auto swapInterval = reinterpret_cast<PFNGLXSWAPINTERVALEXTPROC>(GLXFunctions::GetProc("glXSwapIntervalEXT")))
      swapInterval(pDisplay, w.mWindow, 0);
  }
  else if (HasGLXExtension(glxExtensions, "GLX_MESA_swap_control"))
  {
    if (auto swapInterval = reinterpret_cast<PFNGLXSWAPINTERVALMESAPROC>(GLXFunctions::GetProc("glXSwapIntervalMESA")))
      swapInterval(0);
  }

  OnViewInitialized(w.mGLContext);
  w.mViewInitialized = true;
  DrawResize();
  GetDelegate()->LayoutUI(this);
  GetDelegate()->OnUIOpen();
  SetAllControlsDirty();
  DeactivateGLContext();

  w.mOpen = std::make_shared<bool>(true);
  w.mLoop = &pPlug->GetHostLoop();
  w.mFdId = w.mLoop->AddFd(ConnectionNumber(pDisplay), HostLoop::kFdRead,
                           [this](int, int flags) { OnXConnectionReadable(flags); });
  const uint32_t frameMs = static_cast<uint32_t>(std::max(1, 1000 / std::max(1, FPS())));
  w.mTimerId = w.mLoop->AddTimer(frameMs, [this]() { OnDisplayTimer(); });

  XFlush(pDisplay);
  mInOpenWindow = false;
  return reinterpret_cast<void*>(static_cast<uintptr_t>(w.mWindow));
}

void IGraphicsLinux::CloseWindow()
{
  Impl& w = *mImpl;
  if (w.mLoop)
  {
    w.mLoop->RemoveTimer(w.mTimerId);
    w.mLoop->RemoveFd(w.mFdId);
    w.mLoop = nullptr;
    w.mTimerId = w.mFdId = 0;
  }
  if (w.mOpen)
  {
    *w.mOpen = false;
    w.mOpen.reset();
  }
  DestroyWindowResources();
}

void IGraphicsLinux::DestroyWindowResources()
{
  Impl& w = *mImpl;
  if (!w.mDisplay)
    return;

  {
    // The host may have destroyed its parent window, and with it ours, before closing the editor.
    ScopedXErrorTrap trap(w.mDisplay);
    if (w.mViewInitialized)
    {
      ActivateGLContext();
      OnViewDestroyed();
      DeactivateGLContext();
      w.mViewInitialized = false;
    }
    if (w.mGLContext)
    {
      if (w.mGLX->GetCurrentContext() == w.mGLContext)
        w.mGLX->MakeCurrent(w.mDisplay, None, nullptr);
      w.mGLX->DestroyContext(w.mDisplay, w.mGLContext);
    }
    if (w.mWindow)
      XDestroyWindow(w.mDisplay, w.mWindow);
    if (w.mColormap)
      XFreeColormap(w.mDisplay, w.mColormap);
    XSync(w.mDisplay, False);
  }
  ScopedXErrorTrap::ForgetDisplay(w.mDisplay);
  XCloseDisplay(w.mDisplay);

  const GLXFunctions* pGLX = w.mGLX;
  w = Impl();
  w.mGLX = pGLX;
}

void* IGraphicsLinux::GetWindow()
{
  return reinterpret_cast<void*>(static_cast<uintptr_t>(mImpl->mWindow));
}

bool IGraphicsLinux::WindowIsOpen()
{
  return mImpl->mWindow != 0;
}

void IGraphicsLinux::ActivateGLContext()
{
  Impl& w = *mImpl;
  if (w.mGLDepth++ > 0 || !w.mGLContext)
    return;

  w.mPrevContext = w.mGLX->GetCurrentContext();
  w.mPrevDisplay = w.mGLX->GetCurrentDisplay();
  w.mPrevDrawable = w.mGLX->GetCurrentDrawable();
  if (w.mPrevContext == w.mGLContext && w.mPrevDrawable == w.mWindow)
    return;

  // Also reached from host calls outside any drain (set_size, bitmap loads); Mesa's make-current asks the
  // server for the drawable's geometry, which fails once the host destroyed our window.
  ScopedXErrorTrap trap(w.mDisplay);
  // On failure leave no context current, rather than someone else's,
  // so that the GL calls that follow cannot touch another context's objects.
  if (!w.mGLX->MakeCurrent(w.mDisplay, w.mWindow, w.mGLContext))
    w.mGLX->MakeCurrent(w.mDisplay, None, nullptr);
}

void IGraphicsLinux::DeactivateGLContext()
{
  Impl& w = *mImpl;
  if (w.mGLDepth == 0 || --w.mGLDepth > 0 || !w.mGLContext)
    return;
  if (w.mPrevContext == w.mGLContext && w.mPrevDrawable == w.mWindow)
    return;

  ScopedXErrorTrap trap(w.mDisplay);
  if (w.mPrevContext && w.mPrevDisplay)
    w.mGLX->MakeCurrent(w.mPrevDisplay, w.mPrevDrawable, w.mPrevContext);
  else
    w.mGLX->MakeCurrent(w.mDisplay, None, nullptr);
}

void IGraphicsLinux::OnXConnectionReadable(int flags)
{
  if (flags & HostLoop::kFdError)
  {
    // The X server went away. Xlib treats that as fatal on the next read; stop watching the fd so the
    // host does not spin on it, and let whoever reads next take the process down as Xlib intends.
    fprintf(stderr, "iPlug2: the editor's X connection failed\n");
    mImpl->mLoop->RemoveFd(mImpl->mFdId);
    mImpl->mFdId = 0;
    return;
  }
  ProcessXEvents();
}

void IGraphicsLinux::OnDisplayTimer()
{
  if (ProcessXEvents())
    DrawDirtyControls();
}

bool IGraphicsLinux::ProcessXEvents()
{
  const std::shared_ptr<bool> open = mImpl->mOpen;
  if (!open)
    return false;

  Display* pDisplay = mImpl->mDisplay;
  ScopedXErrorTrap trap(pDisplay);
  while (*open && XPending(pDisplay) > 0)
  {
    XEvent event;
    XNextEvent(pDisplay, &event);
    HandleXEvent(event);
  }
  return *open;
}

void IGraphicsLinux::DrawDirtyControls()
{
  Impl& w = *mImpl;
  if (!w.mMapped)
    return;

  const std::shared_ptr<bool> open = w.mOpen;
  IRECTList rects;
  if (!IsDirty(rects) || !*open)
    return;
  SetAllControlsClean();

  ScopedXErrorTrap trap(w.mDisplay);
  ActivateGLContext();
  Draw(rects);
  w.mGLX->SwapBuffers(w.mDisplay, w.mWindow);
  DeactivateGLContext();
}

// Like the mac version, a full repaint: callers use it right after Resize + LayoutUI.
void IGraphicsLinux::DrawNow()
{
  if (!mImpl->mOpen || !*mImpl->mOpen || !mImpl->mMapped)
    return;
  SetAllControlsDirty();
  DrawDirtyControls();
}

// A call into IGraphics may close the editor and delete this object: it is each handler's last statement, or what follows checks the open flag.
void IGraphicsLinux::HandleXEvent(const XEvent& event)
{
  Impl& w = *mImpl;
  switch (event.type)
  {
    case Expose:
      if (event.xexpose.window == w.mWindow && event.xexpose.count == 0)
        SetAllControlsDirty();
      break;
    case MapNotify:
      if (event.xmap.window == w.mWindow)
      {
        w.mMapped = true;
        SetAllControlsDirty();
      }
      break;
    case UnmapNotify:
      if (event.xunmap.window == w.mWindow)
        w.mMapped = false;
      break;
    case ConfigureNotify:
      HandleConfigure(event);
      break;
    case ButtonPress:
    case ButtonRelease:
      HandleButton(event);
      break;
    case MotionNotify:
      HandleMotion(event);
      break;
    case LeaveNotify:
      if (!(event.xcrossing.state & (Button1Mask | Button3Mask)))
        OnMouseOut();
      break;
    case ClientMessage:
      // Only a top-level fallback window gets WM_DELETE_WINDOW. The editor's lifetime belongs to the host,
      // so hide the window instead of closing the editor under it.
      if (w.mWMDeleteWindow && static_cast<Atom>(event.xclient.data.l[0]) == w.mWMDeleteWindow)
        XUnmapWindow(w.mDisplay, w.mWindow);
      break;
    default:
      break;
  }
}

void IGraphicsLinux::HandleButton(const XEvent& event)
{
  Impl& w = *mImpl;
  const XButtonEvent& button = event.xbutton;
  const float scale = GetTotalScale();
  const float x = button.x / scale;
  const float y = button.y / scale;

  // The state is from before the event: a press does not include its own button, a release still does.
  const unsigned buttonMask = button.button == Button1 ? Button1Mask : button.button == Button3 ? Button3Mask : 0;
  const unsigned state = (event.type == ButtonPress) ? (button.state | buttonMask) : (button.state & ~buttonMask);

  IMouseInfo info;
  info.x = x;
  info.y = y;
  info.ms = MouseModFromState(state);

  if (event.type == ButtonPress)
  {
    if (button.button == kWheelUp || button.button == kWheelDown)
    {
      OnMouseWheel(x, y, info.ms, button.button == kWheelUp ? 1.f : -1.f);
      return;
    }
    if (!buttonMask)
      return;

    const bool isDoubleClick = button.button == w.mLastClickButton
                            && button.time - w.mLastClickTime < kDoubleClickMs
                            && std::fabs(x - w.mLastClickX) < kDoubleClickSlop
                            && std::fabs(y - w.mLastClickY) < kDoubleClickSlop;
    w.mLastClickTime = isDoubleClick ? 0 : button.time; // a third click starts a new pair
    w.mLastClickButton = button.button;
    w.mLastClickX = x;
    w.mLastClickY = y;
    w.mCursorX = x;
    w.mCursorY = y;

    if (isDoubleClick)
      OnMouseDblClick(x, y, info.ms);
    else
      OnMouseDown({ info });
  }
  else if (buttonMask)
  {
    w.mCursorX = x;
    w.mCursorY = y;
    OnMouseUp({ info });
  }
}

void IGraphicsLinux::HandleMotion(const XEvent& event)
{
  Impl& w = *mImpl;
  XEvent latest = event;
  // Coalesce only motion that is next in the queue, so that motion never overtakes a button release.
  while (XEventsQueued(w.mDisplay, QueuedAlready) > 0)
  {
    XEvent next;
    XPeekEvent(w.mDisplay, &next);
    if (next.type != MotionNotify || next.xmotion.window != w.mWindow)
      break;
    XNextEvent(w.mDisplay, &latest);
  }

  const float scale = GetTotalScale();
  IMouseInfo info;
  info.x = latest.xmotion.x / scale;
  info.y = latest.xmotion.y / scale;
  info.dX = info.x - w.mCursorX;
  info.dY = info.y - w.mCursorY;
  info.ms = MouseModFromState(latest.xmotion.state);
  w.mCursorX = info.x;
  w.mCursorY = info.y;

  if (info.ms.L || info.ms.R)
    OnMouseDrag({ info });
  else
    OnMouseOver(info.x, info.y, info.ms);
}

void IGraphicsLinux::HandleConfigure(const XEvent& event)
{
  Impl& w = *mImpl;
  const XConfigureEvent& configure = event.xconfigure;

  if (configure.window == w.mWindow)
  {
    // Echoes of earlier XResizeWindow calls have an older serial; the latest one's echo fails the size check.
    if (configure.serial < w.mResizeSerial)
      return;
    if (configure.width == w.mWindowW && configure.height == w.mWindowH)
      return;
    w.mWindowW = configure.width;
    w.mWindowH = configure.height;
    if (IsHostResizeEnabled())
    {
      const std::shared_ptr<bool> open = w.mOpen;
      GetDelegate()->OnParentWindowResize(configure.width, configure.height);
      if (!*open)
        return;
    }
    // When the editor keeps its size (host resize off, constraints), the window goes back to it,
    // or the drawable and the GL viewport no longer match.
    PlatformResize(false);
    return;
  }

  if (configure.window != w.mParent || (configure.width == w.mParentW && configure.height == w.mParentH))
    return;
  w.mParentW = configure.width;
  w.mParentH = configure.height;

  // A host that resizes its embedding window without telling the plug-in (set_size / onSize) still gets
  // a matching editor. The APP's parent also holds its menu bar; the APP reports its client size itself.
  auto* pPlug = dynamic_cast<IPlugAPIBase*>(GetDelegate());
  if (!IsHostResizeEnabled() || pPlug->GetAPI() == kAPIAPP)
    return;
  if (configure.width == w.mWindowW && configure.height == w.mWindowH)
    return;
  GetDelegate()->OnParentWindowResize(configure.width, configure.height);
}

bool IGraphicsLinux::IsHostResizeEnabled()
{
  auto* pPlug = dynamic_cast<IPlugAPIBase*>(GetDelegate());
  return pPlug && pPlug->GetHostResizeEnabled();
}

void IGraphicsLinux::PlatformResize(bool parentHasResized)
{
  Impl& w = *mImpl;
  if (!w.mWindow)
    return;

  const int width = std::max(1, static_cast<int>(WindowWidth() * GetScreenScale()));
  const int height = std::max(1, static_cast<int>(WindowHeight() * GetScreenScale()));
  if (width == w.mWindowW && height == w.mWindowH)
    return;

  ScopedXErrorTrap trap(w.mDisplay);
  w.mResizeSerial = NextRequest(w.mDisplay);
  XResizeWindow(w.mDisplay, w.mWindow, width, height);
  w.mWindowW = width;
  w.mWindowH = height;
  XFlush(w.mDisplay);
}

void IGraphicsLinux::GetMouseLocation(float& x, float& y) const
{
  x = mImpl->mCursorX;
  y = mImpl->mCursorY;
}

PlatformFontPtr IGraphicsLinux::LoadPlatformFont(const char* fontID, const char* fileNameOrResID)
{
  WDL_String fullPath;
  if (LocateResource(fileNameOrResID, "ttf", fullPath, GetBundleID(), GetWinModuleHandle(), GetSharedResourcesSubPath()) != EResourceLocation::kAbsolutePath)
    return nullptr;

  FILE* pFile = fopen(fullPath.Get(), "rb");
  if (!pFile)
    return nullptr;

  WDL_TypedBuf<unsigned char> data;
  if (fseek(pFile, 0, SEEK_END) == 0)
  {
    const long size = ftell(pFile);
    if (size > 0 && fseek(pFile, 0, SEEK_SET) == 0 && data.ResizeOK(static_cast<int>(size)))
    {
      if (fread(data.Get(), 1, static_cast<size_t>(size), pFile) != static_cast<size_t>(size))
        data.Resize(0);
    }
  }
  fclose(pFile);
  return MakeFont(data.Get(), data.GetSize());
}

PlatformFontPtr IGraphicsLinux::LoadPlatformFont(const char* fontID, void* pData, int dataSize)
{
  return MakeFont(pData, dataSize);
}

EMsgBoxResult IGraphicsLinux::ShowMessageBox(const char* str, const char* title, EMsgBoxType type, IMsgBoxCompletionHandlerFunc completionHandler)
{
  const EMsgBoxResult result = EMsgBoxResult::kCANCEL;
  if (completionHandler)
    completionHandler(result);
  return result;
}

// No dialog yet: behave like a cancelled one (mac/win), so async callers still get their callback.
void IGraphicsLinux::PromptForFile(WDL_String& fileName, WDL_String& path, EFileAction action, const char* ext, IFileDialogCompletionHandlerFunc completionHandler)
{
  fileName.Set("");
  if (completionHandler)
    completionHandler(fileName, path);
}

void IGraphicsLinux::PromptForDirectory(WDL_String& dir, IFileDialogCompletionHandlerFunc completionHandler)
{
  dir.Set("");
  if (completionHandler)
  {
    WDL_String fileName;
    completionHandler(fileName, dir);
  }
}

bool IGraphicsLinux::OpenURL(const char* url, const char* msgWindowTitle, const char* confirmMsg, const char* errMsgOnFailure)
{
  return LinuxOpenWithDefaultApp(url);
}

bool IGraphicsLinux::RevealPathInExplorerOrFinder(WDL_String& path, bool select)
{
  WDL_String dir(path.Get());
  if (select)
    dir.remove_filepart();
  return LinuxOpenWithDefaultApp(dir.Get());
}
