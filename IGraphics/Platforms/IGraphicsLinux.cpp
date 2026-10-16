/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <utility>

#include "IGraphicsLinux.h"
#include "IGraphicsLinuxDialogs.h"
#include "IGraphicsLinuxScale.h"
#include "IPlugAPIBase.h"
#include "IPopupMenuControl.h"
#include "ITextEntryControl.h"
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
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <GL/glx.h>

#include "IGraphicsLinuxGLX.h"
#include "IGraphicsLinuxKeys.h"
#include "IGraphicsLinuxClipboard.h"
#include "IGraphicsLinuxCursors.h"
#include "IGraphicsLinuxFontconfig.h"

using namespace iplug;
using namespace igraphics;

// -DIGRAPHICS_TRACE_LINUX_INPUT: keys, focus and their routing on stderr, for probing hosts.
#ifdef IGRAPHICS_TRACE_LINUX_INPUT
  #define TRACE_INPUT(...) fprintf(stderr, "iPlug2 input: " __VA_ARGS__)
#else
  #define TRACE_INPUT(...) do {} while (0)
#endif

namespace
{
// KeyPress on our window gets the keys of our own focus (text entry, the APP), the keys a host forwards with
// XSendEvent (REAPER does for those it does not use itself; it calls no VST3 onKeyDown), and, in hosts whose focus
// window contains ours, the keys typed while the pointer is over the editor. Unused ones go on to the host's window.
constexpr long kWindowEventMask = ExposureMask | StructureNotifyMask | ButtonPressMask | ButtonReleaseMask
                                | PointerMotionMask | LeaveWindowMask | KeyPressMask | KeyReleaseMask
                                | FocusChangeMask | PropertyChangeMask | VisibilityChangeMask;
constexpr Time kDoubleClickMs = 400;
constexpr float kDoubleClickSlop = 4.f;
constexpr unsigned kWheelUp = Button4;
constexpr unsigned kWheelDown = Button5;
constexpr long kXEmbedVersion = 0;
constexpr long kXEmbedMapped = 1;
constexpr long kXEmbedEmbeddedNotify = 0;
constexpr long kXEmbedRequestFocus = 3;
constexpr long kXEmbedFocusOut = 5;
constexpr int kClipboardTimeoutMs = 200;
// After a click, the window manager's click-to-focus and the host's focus proxy (GDK) move the focus within its own
// window: once within ≈25 ms under Mutter, but several times over up to ≈0.8 s in a loaded REAPER under openbox. A
// focus change into the host's window this soon after a click (or after the edit took the keys) is theirs, not the
// user's, and the editor asks again; later ones are left to the host.
constexpr auto kClickFocusEpisode = std::chrono::milliseconds(1500);
// Asking again while they still move it only loses again: the focus must have stayed put this long first.
constexpr auto kFocusQuiet = std::chrono::milliseconds(150);

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

enum class EFocusPlace { Ours, OurAncestor, OurTopLevel, Elsewhere };

/** Where the keyboard focus window is relative to ours: our window, one of its ancestors (the host's own top-level
 * or container), another window inside the same top-level (a host's focus proxy), or another top-level window. */
EFocusPlace LocateFocus(Display* pDisplay, Window ours, Window focus)
{
  if (focus == ours)
    return EFocusPlace::Ours;
  if (focus == None || focus == PointerRoot)
    return EFocusPlace::Elsewhere;

  auto parentOf = [pDisplay](Window window) -> Window {
    Window root = 0, parent = 0;
    Window* pChildren = nullptr;
    unsigned int childCount = 0;
    if (!XQueryTree(pDisplay, window, &root, &parent, &pChildren, &childCount))
      return 0;
    if (pChildren)
      XFree(pChildren);
    return parent == root ? 0 : parent;
  };
  constexpr int kMaxDepth = 64;
  Window ourTopLevel = ours;
  for (int depth = 0; depth < kMaxDepth; depth++)
  {
    const Window parent = parentOf(ourTopLevel);
    if (!parent)
      break;
    if (parent == focus)
      return EFocusPlace::OurAncestor;
    ourTopLevel = parent;
  }
  Window focusTopLevel = focus;
  for (int depth = 0; depth < kMaxDepth; depth++)
  {
    const Window parent = parentOf(focusTopLevel);
    if (!parent)
      break;
    focusTopLevel = parent;
  }
  return focusTopLevel == ourTopLevel ? EFocusPlace::OurTopLevel : EFocusPlace::Elsewhere;
}

/** The root-window area applications may cover, in physical pixels: the current desktop's _NET_WORKAREA (the
 * screen minus panels and docks), else the whole root window. */
void GetWorkArea(Display* pDisplay, int& x, int& y, int& w, int& h)
{
  const int screen = DefaultScreen(pDisplay);
  const Window root = RootWindow(pDisplay, screen);
  x = y = 0;
  w = DisplayWidth(pDisplay, screen);
  h = DisplayHeight(pDisplay, screen);

  auto readCardinals = [&](const char* name, long offset, long count, unsigned long& received) -> unsigned char* {
    Atom type = None;
    int format = 0;
    unsigned long remaining = 0;
    unsigned char* pData = nullptr;
    received = 0;
    const Atom property = XInternAtom(pDisplay, name, True);
    if (property == None || XGetWindowProperty(pDisplay, root, property, offset, count, False, XA_CARDINAL, &type, &format, &received, &remaining, &pData) != Success)
      return nullptr;
    if (format != 32 && pData)
    {
      XFree(pData);
      return nullptr;
    }
    return pData;
  };

  unsigned long received = 0;
  long desktop = 0;
  if (unsigned char* pDesktop = readCardinals("_NET_CURRENT_DESKTOP", 0, 1, received))
  {
    if (received == 1)
      desktop = reinterpret_cast<long*>(pDesktop)[0];
    XFree(pDesktop);
  }
  if (unsigned char* pArea = readCardinals("_NET_WORKAREA", desktop * 4, 4, received))
  {
    const long* pValues = reinterpret_cast<long*>(pArea);
    if (received == 4 && pValues[2] > 0 && pValues[3] > 0)
    {
      x = static_cast<int>(pValues[0]);
      y = static_cast<int>(pValues[1]);
      w = static_cast<int>(pValues[2]);
      h = static_cast<int>(pValues[3]);
    }
    XFree(pArea);
  }
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

PlatformFontPtr MakeFont(const void* pData, int dataSize, int faceIdx = 0)
{
  if (!pData || dataSize <= 0)
    return nullptr;
  IFontDataPtr data(new IFontData(pData, dataSize, faceIdx));
  if (!data->IsValid())
    return nullptr;
  return PlatformFontPtr(new LinuxFont(std::move(data)));
}

PlatformFontPtr MakeFontFromFile(const char* path, int faceIdx)
{
  FILE* pFile = fopen(path, "rb");
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
  return MakeFont(data.Get(), data.GetSize(), faceIdx);
}
} // namespace

float iplug::igraphics::linux_scale::QueryDesktopScale()
{
  Display* pDisplay = XOpenDisplay(nullptr);
  // Xft's own lookup: the server's RESOURCE_MANAGER property, else ~/.Xdefaults
  const char* xftDpi = pDisplay ? XGetDefault(pDisplay, "Xft", "dpi") : nullptr;
  const float scale = DesktopScale(xftDpi, getenv("GDK_SCALE"), getenv("QT_SCALE_FACTOR"));
  if (pDisplay)
    XCloseDisplay(pDisplay);
  return scale;
}

struct IGraphicsLinux::Impl
{
  const GLXFunctions* mGLX = nullptr;
  Display* mDisplay = nullptr;
  Window mWindow = 0;
  Window mParent = 0; // 0: top-level, because the host gave no usable parent
  Colormap mColormap = 0;
  GLXContext mGLContext = nullptr;
  bool mViewInitialized = false;
  bool mMenuFontLoaded = false; // into this view's drawing context
  bool mMapped = false;
  // Destroying an ancestor sends our window DestroyNotify only, never UnmapNotify: without this flag every frame
  // would draw and swap into a dead drawable, which crashes llvmpipe.
  bool mWindowAlive = false;
  Atom mWMDeleteWindow = 0;
  // Compositing window managers (Mutter) leave a minimized window's children mapped, so no UnmapNotify stops the
  // frames: the host's top-level is watched for being minimized, and our window for being covered.
  Window mTopLevel = 0;
  bool mTopLevelHasWMState = false; // else it is a guess: the host's window may not be managed yet
  std::chrono::steady_clock::time_point mTopLevelFoundAt;
  bool mTopLevelHidden = false;
  bool mFullyObscured = false;
  Atom mWMState = None;
  Atom mNetWMState = None;
  Atom mNetWMStateHidden = None;

  int mWindowW = 0; // physical size of our X window: last requested, or last seen when someone else resized it
  int mWindowH = 0;
  unsigned long mResizeSerial = 0; // request serial of our last XResizeWindow
  bool mHostSizeUnconfirmed = false; // the open changed the size the host was told before it
  int mOpenedW = 0; // the window's size at the end of the open
  int mOpenedH = 0;
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
  float mHiddenCursorX = 0.f;
  float mHiddenCursorY = 0.f;
  ECursor mCursorType = ECursor::ARROW;
  LinuxCursors mCursors;
  Time mLastEventTime = CurrentTime;

  XIM mInputMethod = nullptr;
  XIC mInputContext = nullptr;
  bool mHoldsFocus = false;
  bool mFocusDeclined = false; // the focus left for another window during this edit or menu: do not take it back
  bool mFocusLost = false; // the host's window took it during the episode: ask again once the focus stays put
  bool mFocusChecked = false; // the check one quiet period after our last request is done
  bool mClickedForFocus = false; // a click was released in the editor since the last editing-state update
  bool mFocusBeforeKnown = false;
  std::chrono::steady_clock::time_point mFocusEpisodeAt; // the click, or the take, that began this fight for the keys
  std::chrono::steady_clock::time_point mFocusTakenAt;
  Window mFocusWatched = None; // while lost: where the focus is, and since when
  std::chrono::steady_clock::time_point mFocusQuietSince;
  unsigned mButtonState = 0; // buttons held, as after the last button event
  std::chrono::steady_clock::time_point mLastReleaseAt;
  bool mFocusOnClick = false;
  Window mFocusBefore = None;
  int mRevertBefore = RevertToParent;
  Window mEmbedder = 0; // an XEmbed embedder that announced itself with EMBEDDED_NOTIFY
  Atom mXEmbed = None;
  // Keys whose press the editor kept: their releases are kept too, all other releases go on to the host.
  unsigned char mKeptKeys[32] = {};

  LinuxClipboard mClipboard;
  bool mPasteKeyPending = false; // Ctrl+V of an edit, replayed once the clipboard's text is here
  IKeyPress mPasteKey { "v", 'V' };

  // IGraphics-drawn controls standing in for platform text entry and menus, removed once they are done
  ITextEntryControl* mStandInTextEntry = nullptr;
  IPopupMenuControl* mStandInPopup = nullptr;
  // The popup control whose menus are kept to the on-screen part of the editor (IPopupMenuControl::SetMaxBounds)
  IPopupMenuControl* mScreenFittedPopup = nullptr;

  std::unique_ptr<LinuxDialogs> mDialogs;

  Time mLastClickTime = 0;
  unsigned mLastClickButton = 0;
  float mLastClickX = 0.f;
  float mLastClickY = 0.f;
};

IGraphicsLinux::IGraphicsLinux(IGEditorDelegate& dlg, int w, int h, int fps, float scale)
: IGRAPHICS_DRAW_CLASS(dlg, w, h, fps, scale)
, mImpl(std::make_unique<Impl>())
{
  // Before anything sizes the editor (a restored size, the layout). The host already has the editor's size at this
  // scale (CLAP get_size, VST3 getSize), so it is not told again; nothing is open to resize either.
  mInOpenWindow = true;
  SetScreenScale(dlg.GetEditorScreenScale());
  mInOpenWindow = false;
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

  w.mWindowAlive = w.mWindow != 0;
  w.mXEmbed = XInternAtom(pDisplay, "_XEMBED", False);
  w.mClipboard.Init(pDisplay, w.mWindow);
  w.mCursors.Init(pDisplay);
  // Per connection: key repeat then sends presses only, not release + press pairs.
  XkbSetDetectableAutoRepeat(pDisplay, True, nullptr);
  // Xlib's built-in input method (compose and dead keys, UTF-8 text). Neither the locale nor XMODIFIERS is
  // touched: both are process-wide and belong to the host.
  if ((w.mInputMethod = XOpenIM(pDisplay, nullptr, nullptr, nullptr)))
  {
    w.mInputContext = XCreateIC(w.mInputMethod, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                                XNClientWindow, w.mWindow, XNFocusWindow, w.mWindow, nullptr);
  }

  const long xembedInfo[] = { kXEmbedVersion, kXEmbedMapped };
  const Atom xembedInfoAtom = XInternAtom(pDisplay, "_XEMBED_INFO", False);
  XChangeProperty(pDisplay, w.mWindow, xembedInfoAtom, xembedInfoAtom, 32, PropModeReplace,
                  reinterpret_cast<const unsigned char*>(xembedInfo), 2);

  w.mWMState = XInternAtom(pDisplay, "WM_STATE", False);
  w.mNetWMState = XInternAtom(pDisplay, "_NET_WM_STATE", False);
  w.mNetWMStateHidden = XInternAtom(pDisplay, "_NET_WM_STATE_HIDDEN", False);
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
  WatchTopLevel();
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

  // The standalone app's window is the editor's own, so it takes the keys like a focused view on mac.
  // In a host they stay with the host outside text entry: its shortcuts must keep working.
  w.mFocusOnClick = pPlug->GetAPI() == kAPIAPP;
  w.mOpen = std::make_shared<bool>(true);
  w.mLoop = &pPlug->GetHostLoop();
  w.mFdId = w.mLoop->AddFd(ConnectionNumber(pDisplay), HostLoop::kFdRead,
                           [this](int, int flags) { OnXConnectionReadable(flags); });
  const uint32_t frameMs = static_cast<uint32_t>(std::max(1, 1000 / std::max(1, FPS())));
  w.mTimerId = w.mLoop->AddTimer(frameMs, [this]() { OnDisplayTimer(); });
  w.mDialogs = std::make_unique<LinuxDialogs>(*w.mLoop);
  w.mOpenedW = w.mWindowW;
  w.mOpenedH = w.mWindowH;
  w.mHostSizeUnconfirmed = GetDelegate()->GetEditorWidth() != w.mWindowW || GetDelegate()->GetEditorHeight() != w.mWindowH;

  XFlush(pDisplay);
  mInOpenWindow = false;
  return reinterpret_cast<void*>(static_cast<uintptr_t>(w.mWindow));
}

void IGraphicsLinux::CloseWindow()
{
  Impl& w = *mImpl;
  // Once per open window (not again from the destructor), while the controls are still whole: an open menu's
  // owner began a host gesture on its mouse down, and only the menu's completion ends it.
  if (w.mOpen && *w.mOpen)
    DismissOpenMenu();
  // Their handlers belong to an editor that is going away: the dialogs close and no handler runs.
  w.mDialogs.reset();
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
      w.mMenuFontLoaded = false;
    }
    if (w.mGLContext)
    {
      if (w.mGLX->GetCurrentContext() == w.mGLContext)
        w.mGLX->MakeCurrent(w.mDisplay, None, nullptr);
      w.mGLX->DestroyContext(w.mDisplay, w.mGLContext);
    }
    if (w.mHoldsFocus && w.mWindowAlive)
      ReleaseKeyboardFocus();
    if (w.mInputContext)
      XDestroyIC(w.mInputContext);
    if (w.mInputMethod)
      XCloseIM(w.mInputMethod);
    w.mClipboard.Release();
    w.mCursors.Free();
    if (w.mWindow && w.mWindowAlive)
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
  // server for the drawable's geometry, which fails once the host destroyed our window. Mesa can still
  // return True then, so an X error during the call counts as failure too.
  ScopedXErrorTrap trap(w.mDisplay);
  const bool madeCurrent = w.mWindowAlive && w.mGLX->MakeCurrent(w.mDisplay, w.mWindow, w.mGLContext)
                        && trap.GetErrorCount() == 0;
  // On failure leave no context current, rather than someone else's,
  // so that the GL calls that follow cannot touch another context's objects.
  if (!madeCurrent)
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
  if (!ProcessXEvents())
    return;
  if (mImpl->mHostSizeUnconfirmed && !TellHostTheOpenedSize())
    return;
  // A host that maps its own window after the open (Carla) gets it managed, and WM_STATE set, only later; no event
  // of ours tells when.
  constexpr auto kTopLevelRecheck = std::chrono::seconds(1);
  if (!mImpl->mTopLevelHasWMState && std::chrono::steady_clock::now() - mImpl->mTopLevelFoundAt > kTopLevelRecheck)
    WatchTopLevel();
  // Animations (a menu collapsing, the text cursor) end in IsDirty, outside any X event.
  DrawDirtyControls();
  if (mImpl->mOpen && *mImpl->mOpen)
    UpdateEditingState();
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
    if (mImpl->mInputContext && XFilterEvent(&event, None))
      continue;
    HandleXEvent(event);
    if (*open)
      UpdateEditingState();
  }
  return *open;
}

// The host asked for the editor's size before the open, which can still change it (a restored size, a plug-in's own
// draw scale). It is told the opened size here, outside the open: the open does not tell it, because a host that
// reacts synchronously could close the editor under it. A window resized since then has the host's size already.
bool IGraphicsLinux::TellHostTheOpenedSize()
{
  Impl& w = *mImpl;
  w.mHostSizeUnconfirmed = false;
  if (w.mWindowW != w.mOpenedW || w.mWindowH != w.mOpenedH)
    return true;
  const std::shared_ptr<bool> open = w.mOpen;
  GetDelegate()->EditorResizeFromUI(w.mOpenedW, w.mOpenedH, true);
  return *open;
}

void IGraphicsLinux::DrawDirtyControls()
{
  Impl& w = *mImpl;
  if (!w.mMapped || !w.mWindowAlive)
    return;

  const std::shared_ptr<bool> open = w.mOpen;
  IRECTList rects;
  if (!IsDirty(rects) || !*open)
    return;
  SetAllControlsClean();
  // Controls and animations go on as on mac and Windows; only drawing stops. Showing again redraws everything.
  if (w.mTopLevelHidden || w.mFullyObscured)
    return;

  ScopedXErrorTrap trap(w.mDisplay);
  ActivateGLContext();
  if (w.mGLX->GetCurrentContext() == w.mGLContext)
  {
    Draw(rects);
    w.mGLX->SwapBuffers(w.mDisplay, w.mWindow);
  }
  DeactivateGLContext();
}

// Like the mac version, a full repaint: callers use it right after Resize + LayoutUI.
void IGraphicsLinux::DrawNow()
{
  if (!mImpl->mOpen || !*mImpl->mOpen || !mImpl->mMapped || !mImpl->mWindowAlive)
    return;
  SetAllControlsDirty();
  DrawDirtyControls();
}

// A call into IGraphics may close the editor and delete this object: it is each handler's last statement, or what follows checks the open flag.
void IGraphicsLinux::HandleXEvent(const XEvent& event)
{
  Impl& w = *mImpl;
  if (w.mClipboard.HandleEvent(event))
    return;

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
    case DestroyNotify:
      if (event.xdestroywindow.window == w.mWindow || event.xdestroywindow.window == w.mParent)
      {
        w.mWindowAlive = false;
        w.mMapped = false;
      }
      break;
    case UnmapNotify:
      if (event.xunmap.window == w.mWindow)
        w.mMapped = false;
      break;
    case ReparentNotify:
      if (event.xreparent.window == w.mWindow || event.xreparent.window == w.mParent)
        WatchTopLevel();
      break;
    case PropertyNotify:
      if (event.xproperty.window == w.mTopLevel && (event.xproperty.atom == w.mWMState || event.xproperty.atom == w.mNetWMState))
        UpdateTopLevelHidden();
      break;
    case VisibilityNotify:
      if (event.xvisibility.window == w.mWindow)
      {
        const bool obscured = event.xvisibility.state == VisibilityFullyObscured;
        if (w.mFullyObscured && !obscured)
          SetAllControlsDirty();
        w.mFullyObscured = obscured;
      }
      break;
    case ConfigureNotify:
      HandleConfigure(event);
      break;
    case ButtonPress:
    case ButtonRelease:
      w.mLastEventTime = event.xbutton.time;
      HandleButton(event);
      break;
    case KeyPress:
    case KeyRelease:
      HandleKey(event);
      break;
    case FocusIn:
      TRACE_INPUT("FocusIn mode %d detail %d\n", event.xfocus.mode, event.xfocus.detail);
      if (w.mInputContext && event.xfocus.window == w.mWindow)
        XSetICFocus(w.mInputContext);
      break;
    case FocusOut:
      HandleFocusOut(event);
      break;
    case MotionNotify:
      HandleMotion(event);
      break;
    case LeaveNotify:
      if (!(event.xcrossing.state & (Button1Mask | Button3Mask)))
        OnMouseOut();
      break;
    case ClientMessage:
      HandleClientMessage(event);
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
  w.mButtonState = state & (Button1Mask | Button2Mask | Button3Mask);
  if (event.type == ButtonRelease)
    w.mLastReleaseAt = std::chrono::steady_clock::now();

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

    FinishFadingMenu();
    FitMenusOnScreen();
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
    w.mClickedForFocus = true;
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

  if (!info.ms.L && !info.ms.R)
  {
    OnMouseOver(info.x, info.y, info.ms);
    return;
  }

  const std::shared_ptr<bool> open = w.mOpen;
  OnMouseDrag({ info });
  // A locked cursor goes back after every step. The warp sends a motion event of its own, without a delta;
  // warping again on that one would loop.
  const bool moved = info.dX != 0.f || info.dY != 0.f;
  if (*open && mCursorLock && moved)
    MoveMouseCursor(w.mHiddenCursorX, w.mHiddenCursorY);
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

void IGraphicsLinux::HandleKey(const XEvent& event)
{
  Impl& w = *mImpl;
  XKeyEvent key = event.xkey;
  w.mLastEventTime = key.time;
  const bool isPress = key.type == KeyPress;
  const unsigned keycode = key.keycode & 0xff;
  const unsigned char keyBit = static_cast<unsigned char>(1u << (keycode & 7));
  unsigned char& keptKeys = w.mKeptKeys[keycode >> 3];

  IPopupMenuControl* pPopup = GetPopupMenuControl();
  const bool menuOpen = IsMenuOpen();
  // Keys of an edit or an open menu never reach the host: its Escape or Return would close the window under them.
  const bool modal = IsEditing() || menuOpen;

  if (!isPress && (keptKeys & keyBit))
  {
    TRACE_INPUT("release keycode %u: kept like its press\n", keycode);
    keptKeys &= ~keyBit;
    return;
  }

  // IPopupMenuControl has no key handling of its own
  if (isPress && menuOpen && pPopup->GetExpanded() && XLookupKeysym(&key, 0) == XK_Escape)
  {
    keptKeys |= keyBit;
    DismissOpenMenu();
    return;
  }

  // Ctrl+V in an edit reads the clipboard without blocking, then replays the key. A blocking read cannot get
  // text that the host itself owns: its main thread, which would have to answer, is the one waiting.
  const bool pasteInEdit = isPress && IsEditing() && (key.state & ControlMask)
                        && linux_keys::PhysicalCharacterVirtualKey(w.mDisplay, key.keycode, key.state) == 'V';
  // Another Ctrl+V (key repeat) would start a blocking read on the same property; the replay pastes once.
  if (pasteInEdit && w.mPasteKeyPending)
  {
    keptKeys |= keyBit;
    return;
  }
  if (pasteInEdit && w.mClipboard.BeginAsyncRead())
  {
    keptKeys |= keyBit;
    w.mPasteKeyPending = true;
    w.mPasteKey = IKeyPress("v", 'V', key.state & ShiftMask, true, key.state & Mod1Mask);
    return;
  }

  const std::shared_ptr<bool> open = w.mOpen;
  bool handled = false;
  linux_keys::ForEachKeyPress(w.mDisplay, isPress ? w.mInputContext : nullptr, key, [&](const IKeyPress& keyPress) {
    if (!*open)
      return;
    float x, y;
    GetMouseLocation(x, y);
    const bool keyHandled = isPress ? OnKeyDown(x, y, keyPress) : OnKeyUp(x, y, keyPress);
    handled = handled || keyHandled;
  });
  if (!*open)
    return;

  TRACE_INPUT("%s keycode %u state 0x%x%s: %s\n", isPress ? "press" : "release", keycode, key.state,
              key.send_event ? " (sent by a client)" : "", handled ? "handled" : modal ? "kept (edit or menu)" : key.send_event ? "dropped" : "to host");
  if (isPress && (handled || modal))
    keptKeys |= keyBit;
  // A synthetic key came from a host that forwards keys (REAPER, XEmbed embedders); sending it back would loop.
  else if (!handled && !modal && !key.send_event)
    ForwardKeyToHost(event);
}

void IGraphicsLinux::ForwardKeyToHost(const XEvent& event)
{
  Impl& w = *mImpl;
  if (!w.mParent)
    return;
  XEvent forwarded = event;
  forwarded.xkey.window = w.mParent;
  forwarded.xkey.subwindow = None;
  XSendEvent(w.mDisplay, w.mParent, True, event.type == KeyPress ? KeyPressMask : KeyReleaseMask, &forwarded);
  XFlush(w.mDisplay);
}

void IGraphicsLinux::HandleFocusOut(const XEvent& event)
{
  Impl& w = *mImpl;
  const XFocusChangeEvent& focus = event.xfocus;
  if (focus.window != w.mWindow)
    return;
  TRACE_INPUT("FocusOut mode %d detail %d, holding focus %d\n", focus.mode, focus.detail, w.mHoldsFocus);
  if (w.mInputContext && !w.mHoldsFocus)
    XUnsetICFocus(w.mInputContext);

  if (!w.mHoldsFocus || focus.mode != NotifyNormal) // grabs (NotifyGrab) take the focus only for a while
    return;

  ScopedXErrorTrap trap(w.mDisplay);
  Window current = None;
  int revert = 0;
  XGetInputFocus(w.mDisplay, &current, &revert);
  const EFocusPlace place = LocateFocus(w.mDisplay, w.mWindow, current);
  if (place == EFocusPlace::Ours)
    return;

  // A focus that left for another top-level window (a click elsewhere, alt-tab) ends the edit.
  if (place == EFocusPlace::Elsewhere)
  {
    DeclineKeyboardFocus();
    return;
  }

  // Into the host's own window (its top-level, or GDK's focus proxy inside it) the edit stays open: hosts that
  // forward keys (REAPER) still deliver most of them. During the episode it is the click's own focus change by the
  // window manager and the host, which won the server's timestamp tie or used CurrentTime: ask again later.
  LoseKeyboardFocusToHost(current);
}

void IGraphicsLinux::LoseKeyboardFocusToHost(unsigned long focus)
{
  Impl& w = *mImpl;
  const auto now = std::chrono::steady_clock::now();
  if (now - w.mFocusEpisodeAt > kClickFocusEpisode)
    return;
  TRACE_INPUT("focus lost to the host's 0x%lx, asking again once it stays put\n", focus);
  w.mHoldsFocus = false;
  w.mFocusLost = true;
  w.mFocusWatched = focus;
  w.mFocusQuietSince = now;
}

void IGraphicsLinux::DeclineKeyboardFocus()
{
  Impl& w = *mImpl;
  w.mHoldsFocus = false;
  w.mFocusDeclined = true;
  EndTextEntryOnFocusLoss();
}

void IGraphicsLinux::HandleClientMessage(const XEvent& event)
{
  Impl& w = *mImpl;
  const XClientMessageEvent& message = event.xclient;

  // Only a top-level fallback window gets WM_DELETE_WINDOW. The editor's lifetime belongs to the host,
  // so hide the window instead of closing the editor under it.
  if (w.mWMDeleteWindow && static_cast<Atom>(message.data.l[0]) == w.mWMDeleteWindow)
  {
    XUnmapWindow(w.mDisplay, w.mWindow);
    return;
  }

  if (message.message_type != w.mXEmbed || message.format != 32)
    return;

  // Hosts implement XEmbed partially at best (REAPER and Bitwig not at all), so nothing depends on it:
  // the embedder is remembered for focus requests, and its focus-out ends an edit like a real one.
  switch (message.data.l[1])
  {
    case kXEmbedEmbeddedNotify:
      w.mEmbedder = static_cast<Window>(message.data.l[3]);
      break;
    case kXEmbedFocusOut:
      if (w.mHoldsFocus)
        DeclineKeyboardFocus();
      break;
    default:
      break;
  }
}

void IGraphicsLinux::UpdateEditingState()
{
  Impl& w = *mImpl;
  RemoveFinishedStandIns();
  // IGraphics keeps the edited control when its text entry control goes without ending the edit (a plug-in's
  // RemoveAllControls or RemoveTextEntryControl mid-edit); every key would then count as an edit's forever.
  if (GetControlInTextEntry() && !IsEditing())
    ClearInTextEntryControl();

  const auto now = std::chrono::steady_clock::now();
  // The standalone app's window is the editor's own: a click into it asks for the keys, like a click into a view
  // on mac, and the app keeps them afterwards. In a host, only an edit or an open menu does; an open menu so that
  // Escape closes it rather than (in REAPER) the host's window.
  const bool appClick = w.mFocusOnClick && now - w.mLastReleaseAt < kClickFocusEpisode;
  const bool wantsKeys = IsEditing() || IsMenuOpen() || appClick;
  if (!wantsKeys)
    w.mFocusDeclined = false;
  const bool clicked = std::exchange(w.mClickedForFocus, false) && wantsKeys;
  // Every click begins an episode: window managers focus the top-level again on a click into it (openbox does on
  // every click, from our child too). Not during the press: the window manager and the host answer it with its time.
  if (clicked)
  {
    w.mFocusEpisodeAt = now;
    w.mFocusDeclined = false;
  }
  const Time eventTime = w.mLastEventTime;
  if (clicked && !w.mButtonState)
    TakeKeyboardFocus(eventTime);
  else if (wantsKeys && !w.mHoldsFocus && !w.mFocusLost && !w.mFocusDeclined && !w.mButtonState)
    TakeKeyboardFocus(eventTime);
  else if (wantsKeys && w.mFocusLost && !w.mFocusDeclined && !w.mButtonState)
    AskForKeyboardFocusAgain();
  else if (wantsKeys && w.mHoldsFocus && !w.mFocusChecked && now - w.mFocusTakenAt > kFocusQuiet)
    CheckKeyboardFocus();
  else if (!wantsKeys && (w.mHoldsFocus || w.mFocusLost))
    ReleaseKeyboardFocus();

  if (w.mPasteKeyPending && w.mClipboard.AsyncReadDone())
  {
    w.mPasteKeyPending = false;
    if (IsEditing() && w.mClipboard.HasAsyncText())
    {
      const std::shared_ptr<bool> open = w.mOpen;
      OnKeyDown(w.mCursorX, w.mCursorY, w.mPasteKey);
      if (!*open)
        return;
    }
    w.mClipboard.DropAsyncText();
  }
}

// Text entry asks for the keys while the pointer is elsewhere too (a host may refuse by taking the focus back).
// Outside an edit the host keeps its focus, so its shortcuts work whenever the pointer is not over the editor.
void IGraphicsLinux::TakeKeyboardFocus(unsigned long time)
{
  Impl& w = *mImpl;
  if (!w.mWindowAlive || !w.mMapped)
    return;

  ScopedXErrorTrap trap(w.mDisplay);
  Window focus = None;
  int revert = RevertToParent;
  XGetInputFocus(w.mDisplay, &focus, &revert);
  TRACE_INPUT("take focus from 0x%lx%s\n", focus, time == CurrentTime ? " (again)" : "");
  if (!w.mFocusBeforeKnown)
  {
    if (focus != w.mWindow)
    {
      w.mFocusBefore = focus;
      w.mRevertBefore = revert;
    }
    w.mFocusBeforeKnown = true;
    w.mFocusEpisodeAt = std::chrono::steady_clock::now();
  }
  else
  {
    RememberHostFocusWindow(focus);
  }
  // A request for an event carries its time, normally the click's release. The window manager's click-to-focus and
  // a host's focus proxy carry the press's time, so the server ignores them if they arrive later; a focus change the
  // user made after the click is newer and wins. Asking again follows a focus change seen after our request, which
  // may have used CurrentTime.
  RequestKeyboardFocus(time);
  w.mHoldsFocus = true;
  w.mFocusLost = false;
  w.mFocusChecked = false;
  w.mFocusTakenAt = std::chrono::steady_clock::now();
}

// While the host's window has the focus it took during the episode: ask again once the focus has stayed put for a
// quiet period, so as not to race the rest of its moves. The episode bounds the whole fight.
void IGraphicsLinux::AskForKeyboardFocusAgain()
{
  Impl& w = *mImpl;
  const auto now = std::chrono::steady_clock::now();
  if (now - w.mFocusEpisodeAt > kClickFocusEpisode)
  {
    TRACE_INPUT("focus left with the host\n");
    w.mFocusLost = false;
    w.mFocusDeclined = true;
    return;
  }

  ScopedXErrorTrap trap(w.mDisplay);
  Window focus = None;
  int revert = 0;
  XGetInputFocus(w.mDisplay, &focus, &revert);
  if (focus != w.mFocusWatched)
  {
    switch (LocateFocus(w.mDisplay, w.mWindow, focus))
    {
      case EFocusPlace::Ours:
        w.mFocusLost = false;
        w.mHoldsFocus = true;
        return;
      case EFocusPlace::Elsewhere:
        w.mFocusLost = false;
        DeclineKeyboardFocus();
        return;
      default:
        w.mFocusWatched = focus;
        w.mFocusQuietSince = now;
        return;
    }
  }
  if (now - w.mFocusQuietSince > kFocusQuiet)
    TakeKeyboardFocus(CurrentTime);
}

// One quiet period after a request: with a press and release in the same millisecond (a tap, a synthetic click) the
// host's focus proxy can win the timestamp tie without a FocusOut reaching us first.
void IGraphicsLinux::CheckKeyboardFocus()
{
  Impl& w = *mImpl;
  w.mFocusChecked = true;
  if (!w.mWindowAlive || !w.mMapped)
    return;

  ScopedXErrorTrap trap(w.mDisplay);
  Window focus = None;
  int revert = 0;
  XGetInputFocus(w.mDisplay, &focus, &revert);
  const EFocusPlace place = LocateFocus(w.mDisplay, w.mWindow, focus);
  TRACE_INPUT("check focus: at 0x%lx (%s)\n", focus, place == EFocusPlace::Ours ? "ours" : place == EFocusPlace::Elsewhere ? "elsewhere" : "host");
  if (place == EFocusPlace::OurAncestor || place == EFocusPlace::OurTopLevel)
    LoseKeyboardFocusToHost(focus);
}

// The host's own focus window for the click (GDK's focus proxy) is the best place to give the keys back to; its
// top-level, where the window manager put the focus, is only second best.
void IGraphicsLinux::RememberHostFocusWindow(unsigned long focus)
{
  Impl& w = *mImpl;
  if (LocateFocus(w.mDisplay, w.mWindow, focus) != EFocusPlace::OurTopLevel)
    return;
  w.mFocusBefore = focus;
  w.mRevertBefore = RevertToParent;
}

void IGraphicsLinux::RequestKeyboardFocus(unsigned long time)
{
  Impl& w = *mImpl;
  XSetInputFocus(w.mDisplay, w.mWindow, RevertToParent, time);
  if (w.mInputContext)
    XSetICFocus(w.mInputContext);
  if (w.mEmbedder)
  {
    XEvent request {};
    request.xclient.type = ClientMessage;
    request.xclient.window = w.mEmbedder;
    request.xclient.message_type = w.mXEmbed;
    request.xclient.format = 32;
    request.xclient.data.l[0] = static_cast<long>(w.mLastEventTime);
    request.xclient.data.l[1] = kXEmbedRequestFocus;
    XSendEvent(w.mDisplay, w.mEmbedder, False, NoEventMask, &request);
  }
  XFlush(w.mDisplay);
}

void IGraphicsLinux::ReleaseKeyboardFocus()
{
  Impl& w = *mImpl;
  w.mHoldsFocus = false;
  w.mFocusLost = false;
  w.mFocusBeforeKnown = false;
  // The standalone app keeps the focus, and the input context with it.
  if (w.mFocusOnClick)
    return;
  if (w.mInputContext)
    XUnsetICFocus(w.mInputContext);

  ScopedXErrorTrap trap(w.mDisplay);
  Window focus = None;
  int revert = 0;
  XGetInputFocus(w.mDisplay, &focus, &revert);
  TRACE_INPUT("release focus: now at 0x%lx, before 0x%lx\n", focus, w.mFocusBefore);
  // Someone else moved the focus meanwhile (the window manager, the host): leave it there.
  if (focus != w.mWindow)
    return;

  const bool restorable = w.mFocusBefore != None && w.mFocusBefore != w.mWindow;
  const Window target = restorable ? w.mFocusBefore : (w.mParent ? w.mParent : PointerRoot);
  XSetInputFocus(w.mDisplay, target, restorable ? w.mRevertBefore : RevertToParent, CurrentTime);
  XSync(w.mDisplay, False);
  if (trap.GetErrorCount() > 0) // the window that had the focus is gone
    XSetInputFocus(w.mDisplay, PointerRoot, RevertToPointerRoot, CurrentTime);
  XFlush(w.mDisplay);
}

// Like losing first responder on mac: what was typed is kept. An open menu closes without a choice first, as
// native menus do; it may be the edit's own right-click menu, whose items act on the edit.
void IGraphicsLinux::EndTextEntryOnFocusLoss()
{
  IPopupMenuControl* pPopup = GetPopupMenuControl();
  if (pPopup && pPopup->GetExpanded())
    DismissOpenMenu();
  ITextEntryControl* pTextEntry = GetTextEntryControl();
  if (pTextEntry && pTextEntry->EditInProgress())
    pTextEntry->CommitEdit();
}

void IGraphicsLinux::RemoveFinishedStandIns()
{
  Impl& w = *mImpl;
  // A plug-in may remove them itself (RemoveAllControls in a layout, RemovePopupMenuControl): then they are gone.
  if (w.mStandInTextEntry && GetTextEntryControl() != w.mStandInTextEntry)
    w.mStandInTextEntry = nullptr;
  if (w.mStandInPopup && GetPopupMenuControl() != w.mStandInPopup)
    w.mStandInPopup = nullptr;

  // The edit's own right-click menu refers to its text entry control: that one stays while the menu does.
  const bool textEntryDone = w.mStandInTextEntry && !w.mStandInTextEntry->EditInProgress() && !IsMenuOpen();
  const bool popupDone = w.mStandInPopup && w.mStandInPopup->GetState() == IPopupMenuControl::kCollapsed;
  if (!textEntryDone && !popupDone)
    return;

  // IGraphics may still point at the control as the mouse-over or captured one, and the removal does not clear that.
  IControl* finished[] = { textEntryDone ? w.mStandInTextEntry : nullptr, popupDone ? w.mStandInPopup : nullptr };
  for (IControl* pControl : finished)
  {
    if (pControl && ControlIsCaptured(pControl))
      ReleaseMouseCapture();
  }
  OnMouseOut();

  if (textEntryDone)
  {
    RemoveTextEntryControl();
    w.mStandInTextEntry = nullptr;
  }
  if (popupDone)
  {
    RemovePopupMenuControl();
    w.mStandInPopup = nullptr;
  }
}

bool IGraphicsLinux::IsEditing()
{
  ITextEntryControl* pTextEntry = GetTextEntryControl();
  return GetControlInTextEntry() && pTextEntry && pTextEntry->EditInProgress();
}

bool IGraphicsLinux::IsMenuOpen()
{
  IPopupMenuControl* pPopup = GetPopupMenuControl();
  return pPopup && pPopup->GetState() != IPopupMenuControl::kCollapsed;
}

// As if the menu were dismissed by a click outside it: no choice, and the menu's owner gets its completion.
void IGraphicsLinux::DismissOpenMenu()
{
  IPopupMenuControl* pPopup = GetPopupMenuControl();
  if (!pPopup)
    return;
  switch (pPopup->GetState())
  {
    case IPopupMenuControl::kExpanded:
      pPopup->OnMouseDown(-1.f, -1.f, IMouseMod());
      break;
    // Still fading in: the panels cannot take a click yet, the completion goes straight to the owner.
    case IPopupMenuControl::kExpanding:
    case IPopupMenuControl::kSubMenuAppearing:
      SetControlValueAfterPopupMenu(nullptr);
      break;
    default: // collapsing: the completion has been sent
      break;
  }
}

bool IGraphicsLinux::IsHostResizeEnabled()
{
  auto* pPlug = dynamic_cast<IPlugAPIBase*>(GetDelegate());
  return pPlug && pPlug->GetHostResizeEnabled();
}

void IGraphicsLinux::PlatformResize(bool parentHasResized)
{
  Impl& w = *mImpl;
  if (!w.mWindow || !w.mWindowAlive)
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

void IGraphicsLinux::HideMouseCursor(bool hide, bool lock)
{
  Impl& w = *mImpl;
  if (mCursorHidden == hide)
    return;

  if (hide)
  {
    w.mHiddenCursorX = w.mCursorX;
    w.mHiddenCursorY = w.mCursorY;
    mCursorHidden = true;
    mCursorLock = lock;
  }
  else
  {
    mCursorHidden = false;
    if (mCursorLock)
      MoveMouseCursor(w.mHiddenCursorX, w.mHiddenCursorY);
    mCursorLock = false;
  }

  if (!w.mWindowAlive)
    return;
  ScopedXErrorTrap trap(w.mDisplay);
  XDefineCursor(w.mDisplay, w.mWindow, hide ? w.mCursors.GetBlank(w.mWindow) : w.mCursors.Get(w.mCursorType));
  XFlush(w.mDisplay);
}

void IGraphicsLinux::MoveMouseCursor(float x, float y)
{
  Impl& w = *mImpl;
  if (!w.mWindowAlive)
    return;
  const float scale = GetTotalScale();
  ScopedXErrorTrap trap(w.mDisplay);
  XWarpPointer(w.mDisplay, None, w.mWindow, 0, 0, 0, 0, static_cast<int>(std::lround(x * scale)), static_cast<int>(std::lround(y * scale)));
  XFlush(w.mDisplay);
  w.mCursorX = x;
  w.mCursorY = y;
}

ECursor IGraphicsLinux::SetMouseCursor(ECursor cursorType)
{
  Impl& w = *mImpl;
  const ECursor previous = IGRAPHICS_DRAW_CLASS::SetMouseCursor(cursorType);
  w.mCursorType = cursorType;
  if (w.mWindowAlive && !mCursorHidden)
  {
    ScopedXErrorTrap trap(w.mDisplay);
    XDefineCursor(w.mDisplay, w.mWindow, w.mCursors.Get(cursorType));
    XFlush(w.mDisplay);
  }
  return previous;
}

bool IGraphicsLinux::SetTextInClipboard(const char* str)
{
  Impl& w = *mImpl;
  if (!w.mWindowAlive)
    return false;
  ScopedXErrorTrap trap(w.mDisplay);
  return w.mClipboard.SetText(str, w.mLastEventTime);
}

bool IGraphicsLinux::GetTextFromClipboard(WDL_String& str)
{
  Impl& w = *mImpl;
  str.Set("");
  if (!w.mWindowAlive)
    return false;
  ScopedXErrorTrap trap(w.mDisplay);
  return w.mClipboard.GetText(str, kClipboardTimeoutMs);
}

// X has no text field or menu without a toolkit, and GTK is not allowed in the plug-in: IGraphics' own controls
// stand in, attached for this one edit or menu (IsInPlatformTextEntry() is false meanwhile). Both return at once
// and complete later, which is what IGraphics::CreateTextEntry and DoCreatePopupMenu expect.
void IGraphicsLinux::CreatePlatformTextEntry(int paramIdx, const IText& text, const IRECT& bounds, int length, const char* str)
{
  AttachTextEntryControl();
  mImpl->mStandInTextEntry = GetTextEntryControl();
  mImpl->mStandInTextEntry->CreateTextEntry(paramIdx, text, bounds, length, str);
}

// Platform menus on mac/win draw with the system UI font, and a plug-in need not load DEFAULT_FONT (NanoVG then
// draws no text at all, and asserts in debug builds): the stand-in menu uses the system sans-serif face too.
IText IGraphicsLinux::StandInMenuText()
{
  static constexpr const char* kMenuFontID = "iPlug2-Linux-menu";
  Impl& w = *mImpl;
  if (!w.mMenuFontLoaded)
    w.mMenuFontLoaded = LoadFont(kMenuFontID, "sans-serif", ETextStyle::Normal);
  return w.mMenuFontLoaded ? DEFAULT_TEXT.WithFont(kMenuFontID) : DEFAULT_TEXT;
}

IPopupMenu* IGraphicsLinux::CreatePlatformPopupMenu(IPopupMenu& menu, const IRECT bounds, bool& isAsync)
{
  AttachPopupMenuControl(StandInMenuText());
  mImpl->mStandInPopup = GetPopupMenuControl();
  FitMenusOnScreen();
  mImpl->mStandInPopup->CreatePopupMenu(menu, bounds);
  isAsync = true;
  return nullptr;
}

// A menu that has sent its completion fades out for ≈300 ms, and IPopupMenuControl::CreatePopupMenu ignores requests
// until it has collapsed: a click on a menu's owner meanwhile opened nothing, and left that owner waiting for a
// completion (with its host gesture begun) until the next menu. Such a click ends the fade first.
void IGraphicsLinux::FinishFadingMenu()
{
  IPopupMenuControl* pPopup = GetPopupMenuControl();
  auto isFading = [pPopup]() {
    const IPopupMenuControl::EPopupState state = pPopup->GetState();
    return state == IPopupMenuControl::kFlickering || state == IPopupMenuControl::kCollapsing || state == IPopupMenuControl::kIdling;
  };
  if (!pPopup || !isFading())
    return;
  // Each call is the end of one fade stage: flickering, collapsing, idling, then collapsed.
  constexpr int kFadeStages = 3;
  for (int stage = 0; stage < kFadeStages && isFading(); stage++)
    pPopup->OnEndAnimation();
  // The panels' area is no control's: only a full redraw clears it.
  SetAllControlsDirty();
}

// IPopupMenuControl keeps its menus inside the editor, flipping and scrolling them there. Native menus stay on the
// screen: with the editor dragged partly off it, a dropdown would open into the part nobody can see. Before a menu
// can open (a press, a platform menu), the editor's on-screen part becomes the popup control's limit; with the editor
// wholly on screen the limit goes back to the whole editor, IPopupMenuControl's own default. That overwrites a
// plug-in's own SetMaxBounds for good (the control has no getter), and a menu opened without a press keeps the last
// press's limit. Multi-monitor layouts that are not one rectangle are not considered.
void IGraphicsLinux::FitMenusOnScreen()
{
  Impl& w = *mImpl;
  IPopupMenuControl* pPopup = GetPopupMenuControl();
  if (!pPopup || pPopup->GetState() != IPopupMenuControl::kCollapsed || !w.mWindowAlive)
    return;

  ScopedXErrorTrap trap(w.mDisplay);
  const Window root = RootWindow(w.mDisplay, DefaultScreen(w.mDisplay));
  int windowX = 0, windowY = 0;
  Window child = None;
  if (!XTranslateCoordinates(w.mDisplay, w.mWindow, root, 0, 0, &windowX, &windowY, &child))
    return;
  int areaX, areaY, areaW, areaH;
  GetWorkArea(w.mDisplay, areaX, areaY, areaW, areaH);

  const float scale = GetTotalScale();
  const IRECT editor = GetBounds();
  const IRECT area((areaX - windowX) / scale, (areaY - windowY) / scale,
                   (areaX + areaW - windowX) / scale, (areaY + areaH - windowY) / scale);
  const IRECT onScreen = editor.Intersect(area);
  const bool partlyOffScreen = !onScreen.Empty() && !area.Contains(editor);
  if (partlyOffScreen)
  {
    pPopup->SetMaxBounds(onScreen);
    w.mScreenFittedPopup = pPopup;
  }
  else if (w.mScreenFittedPopup == pPopup)
  {
    pPopup->SetMaxBounds(editor);
    w.mScreenFittedPopup = nullptr;
  }
}

void IGraphicsLinux::ForceEndUserEdit()
{
  ITextEntryControl* pTextEntry = GetTextEntryControl();
  if (pTextEntry && pTextEntry->EditInProgress())
    pTextEntry->DismissEdit();
}

PlatformFontPtr IGraphicsLinux::LoadPlatformFont(const char* fontID, const char* fileNameOrResID)
{
  WDL_String fullPath;
  if (LocateResource(fileNameOrResID, "ttf", fullPath, GetBundleID(), GetWinModuleHandle(), GetSharedResourcesSubPath()) != EResourceLocation::kAbsolutePath)
    return nullptr;
  return MakeFontFromFile(fullPath.Get(), 0);
}

PlatformFontPtr IGraphicsLinux::LoadPlatformFont(const char* fontID, const char* fontName, ETextStyle style)
{
  PlatformFontPtr font;
  linux_fonts::FindSystemFont(fontName, style, [&](const char* path, int faceIdx) {
    font = MakeFontFromFile(path, faceIdx);
    return font != nullptr;
  });
  return font;
}

PlatformFontPtr IGraphicsLinux::LoadPlatformFont(const char* fontID, void* pData, int dataSize)
{
  return MakeFont(pData, dataSize);
}

#pragma mark - Dialogs

// The window manager's WM_STATE (Iconic) or the EWMH _NET_WM_STATE_HIDDEN on the host's top-level: minimized, or its
// owner window minimized (Mutter iconifies REAPER's FX windows with the main window).
void IGraphicsLinux::UpdateTopLevelHidden()
{
  Impl& w = *mImpl;
  bool hidden = false;
  if (w.mTopLevel)
  {
    ScopedXErrorTrap trap(w.mDisplay);
    Atom type = None;
    int format = 0;
    unsigned long count = 0, remaining = 0;
    unsigned char* pData = nullptr;
    if (XGetWindowProperty(w.mDisplay, w.mTopLevel, w.mWMState, 0, 1, False, w.mWMState, &type, &format, &count, &remaining, &pData) == Success && pData)
    {
      hidden = format == 32 && count == 1 && reinterpret_cast<long*>(pData)[0] == IconicState;
      XFree(pData);
    }
    pData = nullptr;
    constexpr long kMaxStates = 32;
    if (!hidden && XGetWindowProperty(w.mDisplay, w.mTopLevel, w.mNetWMState, 0, kMaxStates, False, XA_ATOM, &type, &format, &count, &remaining, &pData) == Success && pData)
    {
      const long* pStates = reinterpret_cast<long*>(pData);
      for (unsigned long i = 0; format == 32 && i < count && !hidden; i++)
        hidden = static_cast<Atom>(pStates[i]) == w.mNetWMStateHidden;
      XFree(pData);
    }
  }
  if (w.mTopLevelHidden && !hidden)
    SetAllControlsDirty();
  w.mTopLevelHidden = hidden;
}

// A host can move our window into another top-level (REAPER docking its FX window); only a reparenting of our window
// or of the host's parent window is seen.
void IGraphicsLinux::WatchTopLevel()
{
  Impl& w = *mImpl;
  ScopedXErrorTrap trap(w.mDisplay);
  bool hasWMState = false;
  const Window topLevel = FindClientTopLevel(&hasWMState);
  w.mTopLevelHasWMState = hasWMState;
  w.mTopLevelFoundAt = std::chrono::steady_clock::now();
  if (topLevel != w.mTopLevel)
  {
    auto watchMask = [&w](Window window, long mask) {
      if (window && window != w.mWindow)
        XSelectInput(w.mDisplay, window, mask | (window == w.mParent ? StructureNotifyMask : NoEventMask));
    };
    watchMask(w.mTopLevel, NoEventMask);
    watchMask(topLevel, PropertyChangeMask);
    w.mTopLevel = topLevel;
  }
  UpdateTopLevelHidden();
}

// Dialogs are made transient for it: the window manager only honours that for top-levels.
unsigned long IGraphicsLinux::GetDialogParentWindow()
{
  return mImpl->mWindowAlive ? FindClientTopLevel(nullptr) : 0;
}

/** The client top-level that contains our window (the one with WM_STATE), else the outermost ancestor below the root */
unsigned long IGraphicsLinux::FindClientTopLevel(bool* pHasWMState)
{
  Impl& w = *mImpl;
  ScopedXErrorTrap trap(w.mDisplay);
  const Atom wmState = w.mWMState;
  Window window = w.mWindow;
  Window outermost = w.mWindow;
  for (int depth = 0; depth < 64; depth++)
  {
    Atom type = None;
    int format = 0;
    unsigned long count = 0, remaining = 0;
    unsigned char* pData = nullptr;
    if (XGetWindowProperty(w.mDisplay, window, wmState, 0, 0, False, AnyPropertyType, &type, &format, &count, &remaining, &pData) == Success)
    {
      if (pData)
        XFree(pData);
      if (type != None)
      {
        if (pHasWMState)
          *pHasWMState = true;
        return window;
      }
    }

    Window root = 0, parent = 0;
    Window* pChildren = nullptr;
    unsigned int childCount = 0;
    if (!XQueryTree(w.mDisplay, window, &root, &parent, &pChildren, &childCount))
      break;
    if (pChildren)
      XFree(pChildren);
    if (!parent || parent == root)
      break;
    window = outermost = parent;
  }
  return trap.GetErrorCount() ? 0 : outermost;
}

bool IGraphicsLinux::ShowDialog(LinuxDialogRequest& request, std::function<void(const LinuxDialogResult&)> handler)
{
  if (!mImpl->mDialogs)
    return false;
  request.parentWindow = GetDialogParentWindow();
  mImpl->mDialogs->Show(request, std::move(handler));
  return true;
}

// Linux dialogs are asynchronous only, like on iOS: a blocking one would stall the host's main thread, whose loop
// is not ours to run nested. A call without a handler gets an empty answer at once.
static void WarnBlockingDialogCall(const char* method, bool& warned)
{
  if (!warned)
    fprintf(stderr, "iPlug2: %s without a completion handler cannot block on Linux, returned empty. Pass a handler.\n", method);
  warned = true;
}

EMsgBoxResult IGraphicsLinux::ShowMessageBox(const char* str, const char* title, EMsgBoxType type, IMsgBoxCompletionHandlerFunc completionHandler)
{
  ReleaseMouseCapture();

  LinuxDialogRequest request;
  request.kind = LinuxDialogRequest::EKind::Message;
  request.text.Set(str ? str : "");
  request.title.Set(title ? title : "");
  request.messageType = type;
  ShowDialog(request, [completionHandler](const LinuxDialogResult& result) {
    if (completionHandler)
      completionHandler(result.message);
  });
  return kNoResult;
}

void IGraphicsLinux::PromptForFile(WDL_String& fileName, WDL_String& path, EFileAction action, const char* ext, IFileDialogCompletionHandlerFunc completionHandler)
{
  if (!WindowIsOpen() || !completionHandler)
  {
    static bool sWarned = false;
    if (WindowIsOpen())
      WarnBlockingDialogCall("PromptForFile", sWarned);
    fileName.Set("");
    return;
  }

  LinuxDialogRequest request;
  request.kind = action == EFileAction::Save ? LinuxDialogRequest::EKind::SaveFile : LinuxDialogRequest::EKind::OpenFile;
  request.folder.Set(path.Get());
  request.fileName.Set(fileName.Get());
  request.extensions.Set(ext ? ext : "");
  fileName.Set("");

  // As on macOS and Windows: the full path as the file name, its folder with a trailing separator as the path.
  ShowDialog(request, [completionHandler](const LinuxDialogResult& result) {
    WDL_String chosenFile, chosenFolder;
    if (result.accepted)
    {
      chosenFile.Set(result.path.Get());
      chosenFolder.Set(result.path.Get());
      chosenFolder.remove_filepart(true);
    }
    completionHandler(chosenFile, chosenFolder);
  });
}

void IGraphicsLinux::PromptForDirectory(WDL_String& dir, IFileDialogCompletionHandlerFunc completionHandler)
{
  if (!WindowIsOpen() || !completionHandler)
  {
    static bool sWarned = false;
    if (WindowIsOpen())
      WarnBlockingDialogCall("PromptForDirectory", sWarned);
    dir.Set("");
    return;
  }

  LinuxDialogRequest request;
  request.kind = LinuxDialogRequest::EKind::Directory;
  request.folder.Set(dir.Get());
  ShowDialog(request, [completionHandler](const LinuxDialogResult& result) {
    WDL_String noFile, chosenFolder;
    if (result.accepted)
      chosenFolder.Set(result.path.Get());
    completionHandler(noFile, chosenFolder);
  });
}

// zenity's or kdialog's colour chooser (the portal has none). The colour arrives through func; false tells a caller
// that expects a synchronous answer that color is unchanged.
bool IGraphicsLinux::PromptForColor(IColor& color, const char* str, IColorPickerHandlerFunc func)
{
  ReleaseMouseCapture();
  if (!func)
  {
    static bool sWarned = false;
    WarnBlockingDialogCall("PromptForColor", sWarned);
    return false;
  }

  LinuxDialogRequest request;
  request.kind = LinuxDialogRequest::EKind::Color;
  request.title.Set(str ? str : "");
  request.color = color;
  ShowDialog(request, [func](const LinuxDialogResult& result) {
    if (result.accepted)
      func(result.color);
  });
  return false;
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
