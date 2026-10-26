/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

// The Linux IWebView: WebKitGTK runs in a helper process (IPlugWebViewLinuxHelper.c), never in the host's, and
// embeds into a container X window of ours with XEmbed. Everything here runs on the host main thread, driven by
// the plug-in's HostLoop: the helper's socket and our own X connection are fd watches on it.

#include "IPlugWebView.h"
#include "IPlugPaths.h"
#include "IPlugWebViewLinuxIPC.h"
#include "Linux/IPlugLinuxHostLoop.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include <dlfcn.h>
#include <fcntl.h>
#include <limits.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>

extern char** environ;

BEGIN_IPLUG_NAMESPACE

namespace
{
// XEmbed protocol messages and flags (freedesktop.org XEmbed spec 0.5)
constexpr long kXEmbedEmbeddedNotify = 0;
constexpr long kXEmbedWindowActivate = 1;
constexpr long kXEmbedWindowDeactivate = 2;
constexpr long kXEmbedRequestFocus = 3;
constexpr long kXEmbedFocusIn = 4;
constexpr long kXEmbedFocusOut = 5;
constexpr long kXEmbedFocusCurrent = 0;
constexpr long kXEmbedVersion = 0;
constexpr long kXEmbedMapped = 1;

// Polls the socket and the X connection for hosts whose loop cannot watch fds (CLAP without posix-fd-support).
constexpr uint32_t kPollIntervalMs = 15;

// After a click the window manager and the host move the focus to the host's window too, some with CurrentTime, so
// they can win over our request. For this long after the click the focus is asked for again once it stays put.
constexpr auto kFocusEpisode = std::chrono::milliseconds(1000);
constexpr auto kFocusQuiet = std::chrono::milliseconds(100);
constexpr uint32_t kFocusCheckMs = 30;

/** Swallows X errors on one connection while in scope, and passes other connections' errors on.
 * Xlib's default handler exits the process, and our requests can fail through no fault of ours: the host may
 * destroy its window, and ours inside it, before it closes the editor. */
class XErrorTrap
{
public:
  explicit XErrorTrap(Display* pDisplay)
  : mPrevDisplay(sDisplay)
  {
    sDisplay = pDisplay;
    mPrevHandler = XSetErrorHandler(&Handler);
    if (mPrevHandler != &Handler)
      sOuterHandler = mPrevHandler;
  }

  ~XErrorTrap()
  {
    XSetErrorHandler(mPrevHandler);
    sDisplay = mPrevDisplay;
  }

  XErrorTrap(const XErrorTrap&) = delete;
  XErrorTrap& operator=(const XErrorTrap&) = delete;

private:
  static int Handler(Display* pDisplay, XErrorEvent* pError)
  {
    if (pDisplay != sDisplay)
      return (sOuterHandler && sOuterHandler != &Handler) ? sOuterHandler(pDisplay, pError) : 0;
    return 0;
  }

  static inline Display* sDisplay = nullptr;
  static inline XErrorHandler sOuterHandler = nullptr;
  Display* mPrevDisplay;
  XErrorHandler mPrevHandler = nullptr;
};

bool IsExecutable(const std::string& path)
{
  struct stat info {};
  return stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode) && access(path.c_str(), X_OK) == 0;
}

/** The helper ships in the module's resource folder: <Name>.resources/ beside a .clap or a standalone, the
 * bundle's Contents/Resources/ for a VST3, and <stem>.resources/ beside any other module (a REAPER extension).
 * Each plug-in carries its own copy, built from the same iPlug2 as the plug-in, so the protocol always matches. */
std::string FindHelper()
{
  if (const char* pOverride = getenv("IPLUG2_WEBVIEW_HELPER"); pOverride && pOverride[0])
    return pOverride;

  WDL_String resources;
  BundleResourcePath(resources, reinterpret_cast<void*>(&FindHelper));
  std::string candidate;
  if (resources.GetLength())
  {
    candidate = std::string(resources.Get()) + IPLUG_WEBVIEW_HELPER_NAME;
    if (IsExecutable(candidate))
      return candidate;
  }

  Dl_info info {};
  char modulePath[PATH_MAX] {};
  if (dladdr(reinterpret_cast<void*>(&FindHelper), &info) && info.dli_fname && realpath(info.dli_fname, modulePath))
  {
    std::string path(modulePath);
    const size_t slash = path.rfind('/');
    std::string stem = path.substr(slash + 1);
    if (const size_t dot = stem.rfind('.'); dot != std::string::npos && dot > 0)
      stem.resize(dot);
    const std::string fallback = path.substr(0, slash + 1) + stem + ".resources/" + IPLUG_WEBVIEW_HELPER_NAME;
    if (IsExecutable(fallback))
      return fallback;
    if (candidate.empty())
      candidate = fallback;
  }
  return candidate;
}

std::string PercentEncodePath(const char* path)
{
  static const char* kHex = "0123456789ABCDEF";
  std::string encoded;
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(path); *p; p++)
  {
    const unsigned char c = *p;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || strchr("/-._~!$&'()*+,;=:@", c))
    {
      encoded.push_back(static_cast<char>(c));
    }
    else
    {
      encoded.push_back('%');
      encoded.push_back(kHex[c >> 4]);
      encoded.push_back(kHex[c & 15]);
    }
  }
  return encoded;
}
} // namespace

class IWebViewImpl
{
public:
  IWebViewImpl(IWebView* owner) : mIWebView(owner) {}
  ~IWebViewImpl() { CloseWebView(); }

  void* OpenWebView(void* pParent, float x, float y, float w, float h, float scale);
  void CloseWebView();
  void HideWebView(bool hide);
  void LoadHTML(const char* html) { SendString(kIPlugWVLoadHTML, html); }
  void LoadURL(const char* url) { SendString(kIPlugWVLoadURL, url); }
  void LoadFile(const char* fileName, const char* bundleID);
  void ReloadPageContent() { SendInts(kIPlugWVReload, {}); }
  void EvaluateJavaScript(const char* scriptStr, IWebView::completionHandlerFunc func);
  void EnableScroll(bool enable) {}
  void EnableInteraction(bool enable);
  void SetWebViewBounds(float x, float y, float w, float h, float scale);
  void GetWebRoot(WDL_String& path) const { path.Set(mWebRoot.Get()); }
  void GetLocalDownloadPathForFile(const char* fileName, WDL_String& localPath);

  void SetHostLoop(HostLoop* pLoop) { mLoop = pLoop; }
  void SetScreenScale(float scale);

private:
  struct Rect { int x, y, w, h; };
  Rect PhysicalBounds() const;
  long ZoomPermille() const { return std::lround(mZoom * mScreenScale * 1000.f); }

  bool StartHelper(const std::string& helperPath, const Rect& bounds);
  void Watch();
  void Unwatch();
  void OnPoll();
  void OnSocketReadable();
  void HandleMessage(uint32_t type, IPlugWVReader& reader);
  void OnHelperGone(const char* reason);
  void ProcessXEvents();
  void HandleXEvent(const XEvent& event);
  void EmbedPlug(Window plug);
  void TakeFocus(Time time);
  void CheckFocus();
  void EndFocusEpisode();
  void LoseFocus();
  Window TopLevelOf(Window window) const;
  void SendXEmbed(Window target, long message, long detail = 0, long data1 = 0, long data2 = 0, Time time = CurrentTime);
  void ShowFailure(const char* text);
  void DrawFailure();

  void SendString(uint32_t type, const char* str);
  void SendInts(uint32_t type, std::initializer_list<int64_t> values);
  void SendIdAndString(uint32_t type, int64_t id, const char* str);
  void Flush();

  IWebView* mIWebView;
  HostLoop* mLoop = nullptr;
  float mScreenScale = 1.f;

  Display* mDisplay = nullptr;
  Window mParent = 0;
  Window mContainer = 0;
  Window mPlug = 0;
  Window mEmbedder = 0;
  Atom mXEmbed = 0;
  Atom mXEmbedInfo = 0;
  bool mHidden = false;
  bool mHasFocus = false;
  int mFocusTimer = 0;
  std::chrono::steady_clock::time_point mFocusEpisodeAt;
  std::chrono::steady_clock::time_point mFocusQuietSince;
  Window mFocusWatched = 0;
  float mX = 0.f, mY = 0.f, mW = 1.f, mH = 1.f, mZoom = 1.f;
  // Where we last put the container. A host may move it itself (the standalone app puts it below its menu bar),
  // so bounds at the same place only resize it.
  int mPlacedX = 0, mPlacedY = 0;
  long mSentZoom = 0;
  bool mInteractionEnabled = true;

  int mSocket = -1;
  bool mHelperReady = false;
  int mSocketWatch = 0;
  int mWriteWatch = 0;
  int mXWatch = 0;
  int mPollTimer = 0;
  IPlugWVBuffer mIn {};
  IPlugWVBuffer mOut {};
  int64_t mNextEvalId = 0;
  std::unordered_map<int64_t, IWebView::completionHandlerFunc> mEvalHandlers;

  // Cleared by CloseWebView(), so that a callback into the owner that closes the web view stops whatever
  // called it from touching the closed one.
  std::shared_ptr<bool> mAlive;

  WDL_String mFailure;
  WDL_String mWebRoot;
};

END_IPLUG_NAMESPACE

using namespace iplug;

void* IWebViewImpl::OpenWebView(void* pParent, float x, float y, float w, float h, float scale)
{
  CloseWebView();
  if (!mLoop)
  {
    fprintf(stderr, "iPlug2: the web view has no host loop (SetWebViewHostLoop), it cannot open\n");
    return nullptr;
  }

  mDisplay = XOpenDisplay(nullptr);
  if (!mDisplay)
  {
    fprintf(stderr, "iPlug2: XOpenDisplay failed, the web view cannot open\n");
    return nullptr;
  }

  XErrorTrap trap(mDisplay);
  const int screen = DefaultScreen(mDisplay);
  mParent = static_cast<Window>(reinterpret_cast<uintptr_t>(pParent));
  XWindowAttributes parentAttributes {};
  if (!mParent || !XGetWindowAttributes(mDisplay, mParent, &parentAttributes))
    mParent = RootWindow(mDisplay, screen);

  mX = x; mY = y; mW = w; mH = h; mZoom = scale > 0.f ? scale : 1.f;
  const Rect bounds = PhysicalBounds();
  mXEmbed = XInternAtom(mDisplay, "_XEMBED", False);
  mXEmbedInfo = XInternAtom(mDisplay, "_XEMBED_INFO", False);

  XSetWindowAttributes attributes {};
  attributes.background_pixel = WhitePixel(mDisplay, screen);
  attributes.event_mask = ExposureMask | StructureNotifyMask | SubstructureNotifyMask;
  mContainer = XCreateWindow(mDisplay, mParent, bounds.x, bounds.y, bounds.w, bounds.h, 0, CopyFromParent,
                             InputOutput, CopyFromParent, CWBackPixel | CWEventMask, &attributes);
  mPlacedX = bounds.x;
  mPlacedY = bounds.y;
  // The container is the editor's window when the web view is the whole UI: an XEmbed host embeds it as such.
  const long xembedInfo[] = { kXEmbedVersion, kXEmbedMapped };
  XChangeProperty(mDisplay, mContainer, mXEmbedInfo, mXEmbedInfo, 32, PropModeReplace,
                  reinterpret_cast<const unsigned char*>(xembedInfo), 2);
  if (!mHidden)
    XMapWindow(mDisplay, mContainer);
  XSync(mDisplay, False);

  mAlive = std::make_shared<bool>(true);
  Watch();

  const std::string helper = FindHelper();
  if (!IsExecutable(helper))
  {
    WDL_String text;
    text.SetFormatted(1024, "The web view's helper program is missing:\n%s\nReinstall the plug-in.", helper.c_str());
    ShowFailure(text.Get());
  }
  else if (!StartHelper(helper, bounds))
  {
    ShowFailure("The web view's helper program could not be started.");
  }
  XFlush(mDisplay);
  return reinterpret_cast<void*>(static_cast<uintptr_t>(mContainer));
}

bool IWebViewImpl::StartHelper(const std::string& helperPath, const Rect& bounds)
{
  int fds[2];
  if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds) != 0)
    return false;
  int helperEnd = fds[1];
  // adddup2(fd, fd) would keep FD_CLOEXEC on some C libraries; the helper must find its end open.
  if (helperEnd == IPLUG_WEBVIEW_HELPER_FD)
  {
    helperEnd = fcntl(fds[1], F_DUPFD_CLOEXEC, IPLUG_WEBVIEW_HELPER_FD + 1);
    close(fds[1]);
    if (helperEnd < 0)
    {
      close(fds[0]);
      return false;
    }
  }

  WDL_String cachePath;
  WebViewCachePath(cachePath);
  char parent[32], width[16], height[16], zoom[32];
  snprintf(parent, sizeof(parent), "0x%lx", mContainer);
  snprintf(width, sizeof(width), "%d", bounds.w);
  snprintf(height, sizeof(height), "%d", bounds.h);
  snprintf(zoom, sizeof(zoom), "%ld", ZoomPermille());
  mSentZoom = ZoomPermille();

  // A tiny shell starts the helper in the background and exits, so that init (or the session's subreaper)
  // reaps the helper, not the host, which may reap every child or ignore SIGCHLD.
  const char* argv[] = {
    "/bin/sh", "-c", "\"$@\" &", "sh", helperPath.c_str(),
    "--parent", parent, "--width", width, "--height", height, "--zoom-permille", zoom,
    "--opaque", mIWebView->IsOpaque() ? "1" : "0",
    "--devtools", mIWebView->GetEnableDevTools() ? "1" : "0",
    "--scheme", mIWebView->GetCustomUrlScheme(),
    "--data-dir", cachePath.Get(),
    nullptr
  };

  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
  posix_spawn_file_actions_adddup2(&actions, helperEnd, IPLUG_WEBVIEW_HELPER_FD);
#if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 34))
  // The host's descriptors without O_CLOEXEC (audio devices, sockets) must not stay open in the helper.
  posix_spawn_file_actions_addclosefrom_np(&actions, IPLUG_WEBVIEW_HELPER_FD + 1);
#endif
  // Its own process group, so a terminal's Ctrl-C to the host does not reach it; the host's end of the socket
  // closing ends it. Signal state reset: a host may block or ignore signals the helper relies on.
  posix_spawnattr_t attributes;
  posix_spawnattr_init(&attributes);
  posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF);
  posix_spawnattr_setpgroup(&attributes, 0);
  sigset_t signals;
  sigemptyset(&signals);
  posix_spawnattr_setsigmask(&attributes, &signals);
  sigfillset(&signals);
  posix_spawnattr_setsigdefault(&attributes, &signals);

  pid_t pid = 0;
  const int error = posix_spawn(&pid, "/bin/sh", &actions, &attributes, const_cast<char* const*>(argv), environ);
  posix_spawn_file_actions_destroy(&actions);
  posix_spawnattr_destroy(&attributes);
  close(helperEnd);
  if (error != 0)
  {
    close(fds[0]);
    return false;
  }
  int status = 0;
  while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}

  mSocket = fds[0];
  mHelperReady = false;
  fcntl(mSocket, F_SETFL, fcntl(mSocket, F_GETFL) | O_NONBLOCK);
  mSocketWatch = mLoop->AddFd(mSocket, HostLoop::kFdRead, [this](int, int) { OnSocketReadable(); });
  if (!mInteractionEnabled)
    SendInts(kIPlugWVEnableInteraction, {0});
  return true;
}

void IWebViewImpl::Watch()
{
  mXWatch = mLoop->AddFd(ConnectionNumber(mDisplay), HostLoop::kFdRead, [this](int, int) { ProcessXEvents(); });
  if (!mLoop->IsFdWatched(mXWatch))
    mPollTimer = mLoop->AddTimer(kPollIntervalMs, [this]() { OnPoll(); });
}

void IWebViewImpl::Unwatch()
{
  if (!mLoop)
    return;
  for (int* pId : { &mSocketWatch, &mWriteWatch, &mXWatch, &mPollTimer, &mFocusTimer })
  {
    if (*pId)
      mLoop->RemoveTimer(*pId);
    *pId = 0;
  }
}

void IWebViewImpl::OnPoll()
{
  const std::shared_ptr<bool> alive = mAlive;
  if (mSocket >= 0)
  {
    Flush();
    OnSocketReadable();
  }
  if (alive && *alive)
    ProcessXEvents();
}

void IWebViewImpl::CloseWebView()
{
  if (mAlive)
    *mAlive = false;
  mAlive.reset();
  Unwatch();
  if (mSocket >= 0)
  {
    // The helper ends when its end of the socket closes.
    close(mSocket);
    mSocket = -1;
  }
  IPlugWVBufferFree(&mIn);
  IPlugWVBufferFree(&mOut);
  mEvalHandlers.clear();
  if (mDisplay)
  {
    XErrorTrap trap(mDisplay);
    if (mPlug)
    {
      // Ends the embedding as the XEmbed spec asks: the plug leaves our window before it is destroyed.
      XUnmapWindow(mDisplay, mPlug);
      XReparentWindow(mDisplay, mPlug, DefaultRootWindow(mDisplay), 0, 0);
    }
    if (mContainer)
      XDestroyWindow(mDisplay, mContainer);
    XSync(mDisplay, False);
    XCloseDisplay(mDisplay);
  }
  mDisplay = nullptr;
  mParent = mContainer = mPlug = mEmbedder = 0;
  mHasFocus = false;
  mFailure.Set("");
}

void IWebViewImpl::HideWebView(bool hide)
{
  mHidden = hide;
  if (!mDisplay || !mContainer)
    return;
  XErrorTrap trap(mDisplay);
  if (hide)
  {
    LoseFocus();
    XUnmapWindow(mDisplay, mContainer);
  }
  else
  {
    XMapWindow(mDisplay, mContainer);
  }
  XFlush(mDisplay);
}

void IWebViewImpl::LoadFile(const char* fileName, const char* bundleID)
{
  if (!fileName || !fileName[0])
    return;

  // A bare name is a file of the plug-in's web UI, in web/ of its resource folder, as in a mac bundle.
  WDL_String fullPath;
  if (fileName[0] == '/')
  {
    fullPath.Set(fileName);
  }
  else
  {
    BundleResourcePath(fullPath, reinterpret_cast<void*>(&FindHelper));
    fullPath.Append("web/");
    fullPath.Append(fileName);
  }

  WDL_String root(fullPath);
  root.remove_filepart();
  mWebRoot.Set(root.Get());

  const char* scheme = mIWebView->GetCustomUrlScheme();
  std::string url = (scheme && scheme[0]) ? std::string(scheme) + ":" : std::string("file://");
  url += PercentEncodePath(fullPath.Get());
  LoadURL(url.c_str());
}

void IWebViewImpl::EvaluateJavaScript(const char* scriptStr, IWebView::completionHandlerFunc func)
{
  if (mSocket < 0 || !scriptStr)
    return;
  int64_t id = 0;
  if (func)
  {
    id = ++mNextEvalId;
    mEvalHandlers[id] = std::move(func);
  }
  SendIdAndString(kIPlugWVEvaluate, id, scriptStr);
}

void IWebViewImpl::EnableInteraction(bool enable)
{
  mInteractionEnabled = enable;
  if (!enable)
    LoseFocus();
  SendInts(kIPlugWVEnableInteraction, {enable ? 1 : 0});
}

IWebViewImpl::Rect IWebViewImpl::PhysicalBounds() const
{
  const float s = mScreenScale;
  const int left = static_cast<int>(std::lround(mX * s));
  const int top = static_cast<int>(std::lround(mY * s));
  const int right = static_cast<int>(std::lround((mX + mW) * s));
  const int bottom = static_cast<int>(std::lround((mY + mH) * s));
  return { left, top, std::max(1, right - left), std::max(1, bottom - top) };
}

void IWebViewImpl::SetWebViewBounds(float x, float y, float w, float h, float scale)
{
  mX = x; mY = y; mW = w; mH = h;
  if (scale > 0.f)
    mZoom = scale;
  if (!mDisplay || !mContainer)
    return;

  const Rect bounds = PhysicalBounds();
  XErrorTrap trap(mDisplay);
  if (bounds.x == mPlacedX && bounds.y == mPlacedY)
  {
    XResizeWindow(mDisplay, mContainer, bounds.w, bounds.h);
  }
  else
  {
    XMoveResizeWindow(mDisplay, mContainer, bounds.x, bounds.y, bounds.w, bounds.h);
    mPlacedX = bounds.x;
    mPlacedY = bounds.y;
  }
  if (mPlug)
    XResizeWindow(mDisplay, mPlug, bounds.w, bounds.h);
  XFlush(mDisplay);
  if (ZoomPermille() != mSentZoom)
  {
    mSentZoom = ZoomPermille();
    SendInts(kIPlugWVSetZoom, {mSentZoom});
  }
}

void IWebViewImpl::SetScreenScale(float scale)
{
  if (!(scale > 0.f) || scale == mScreenScale)
    return;
  mScreenScale = scale;
  SetWebViewBounds(mX, mY, mW, mH, mZoom);
}

void IWebViewImpl::GetLocalDownloadPathForFile(const char* fileName, WDL_String& localPath)
{
  // A folder of its own per download, as on mac, so that no existing file is overwritten.
  const char* tmp = getenv("TMPDIR");
  std::string folder = std::string(tmp && tmp[0] == '/' ? tmp : "/tmp") + "/iplug2-download-XXXXXX";
  const char* name = strrchr(fileName, '/');
  name = name ? name + 1 : fileName;
  localPath.Set("");
  if (!mkdtemp(folder.data()) || !name[0])
    return;
  localPath.SetFormatted(PATH_MAX, "%s/%s", folder.c_str(), name);
}

// ----------------------------------------------------------------------------------------------- the helper

void IWebViewImpl::OnSocketReadable()
{
  if (mSocket < 0)
    return;
  for (;;)
  {
    char chunk[65536];
    const ssize_t count = recv(mSocket, chunk, sizeof(chunk), 0);
    if (count > 0)
    {
      IPlugWVBufferAppend(&mIn, chunk, static_cast<size_t>(count));
      continue;
    }
    if (count < 0 && errno == EINTR)
      continue;
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
      break;
    // Before its first message it most likely could not load WebKitGTK at all.
    OnHelperGone(mHelperReady ? "The web view's helper program stopped.\nClose and reopen the editor."
                              : "The web view's helper program could not start.\n"
                                "It needs WebKitGTK 4.1: libwebkit2gtk-4.1-0 (Debian, Ubuntu) or webkit2gtk4.1 (Fedora).");
    return;
  }

  const std::shared_ptr<bool> alive = mAlive;
  size_t frameLength;
  while (alive && *alive && (frameLength = IPlugWVCompleteFrameLength(mIn.data, mIn.length)) > 0)
  {
    // The handler can reach the owner, which may load, evaluate or even close: work on a copy of the frame.
    std::string frame(mIn.data, frameLength);
    IPlugWVBufferConsume(&mIn, frameLength);
    IPlugWVReader reader;
    const uint32_t type = IPlugWVReadFrameType(&reader, frame.data(), frame.size());
    HandleMessage(type, reader);
  }
}

void IWebViewImpl::HandleMessage(uint32_t type, IPlugWVReader& reader)
{
  auto readString = [&reader]() {
    char* raw = IPlugWVReadString(&reader);
    std::string str(raw ? raw : "");
    free(raw);
    return str;
  };

  switch (type)
  {
    case kIPlugWVReady:
    {
      const int64_t version = IPlugWVReadInt(&reader);
      const Window plug = static_cast<Window>(IPlugWVReadInt(&reader));
      if (version != IPLUG_WEBVIEW_PROTOCOL_VERSION)
      {
        OnHelperGone("The web view's helper program is from another version of the plug-in.\nReinstall the plug-in.");
        return;
      }
      mHelperReady = true;
      EmbedPlug(plug);
      mIWebView->OnWebViewReady();
      break;
    }
    case kIPlugWVContentLoaded:
      mIWebView->OnWebContentLoaded();
      break;
    case kIPlugWVScriptMessage:
    {
      const std::string json = readString();
      mIWebView->OnMessageFromWebView(json.c_str());
      break;
    }
    case kIPlugWVEvaluateResult:
    {
      const int64_t id = IPlugWVReadInt(&reader);
      const bool succeeded = IPlugWVReadInt(&reader) != 0;
      const std::string json = readString();
      auto it = mEvalHandlers.find(id);
      if (it == mEvalHandlers.end())
        break;
      IWebView::completionHandlerFunc func = std::move(it->second);
      mEvalHandlers.erase(it);
      if (succeeded && func)
        func(json.c_str());
      break;
    }
    case kIPlugWVNavigationRequest:
    {
      const int64_t id = IPlugWVReadInt(&reader);
      const std::string url = readString();
      const bool allow = mIWebView->OnCanNavigateToURL(url.c_str());
      SendInts(kIPlugWVNavigationReply, {id, allow ? 1 : 0});
      break;
    }
    case kIPlugWVMimeTypeRequest:
    {
      const int64_t id = IPlugWVReadInt(&reader);
      const std::string mimeType = readString();
      const bool download = mIWebView->OnCanDownloadMIMEType(mimeType.c_str());
      SendInts(kIPlugWVMimeTypeReply, {id, download ? 1 : 0});
      break;
    }
    case kIPlugWVDownloadPathRequest:
    {
      const int64_t id = IPlugWVReadInt(&reader);
      const std::string fileName = readString();
      WDL_String path;
      mIWebView->OnGetLocalDownloadPathForFile(fileName.c_str(), path);
      SendIdAndString(kIPlugWVDownloadPathReply, id, path.Get());
      break;
    }
    case kIPlugWVDownloadProgress:
    {
      const int64_t received = IPlugWVReadInt(&reader);
      const int64_t total = IPlugWVReadInt(&reader);
      mIWebView->OnReceivedData(static_cast<size_t>(received), static_cast<size_t>(total));
      break;
    }
    case kIPlugWVDownloadFinished:
    {
      const bool succeeded = IPlugWVReadInt(&reader) != 0;
      const std::string path = readString();
      if (succeeded)
        mIWebView->OnDownloadedFile(path.c_str());
      else
        mIWebView->OnFailedToDownloadFile(path.c_str());
      break;
    }
    case kIPlugWVFocusRequest:
      TakeFocus(static_cast<Time>(IPlugWVReadInt(&reader)));
      break;
    case kIPlugWVWebProcessGone:
      DBGMSG("iPlug2: the web view's web process ended (reason %d)\n", static_cast<int>(IPlugWVReadInt(&reader)));
      break;
    default:
      break;
  }
}

void IWebViewImpl::OnHelperGone(const char* reason)
{
  if (mSocketWatch)
    mLoop->RemoveFd(mSocketWatch);
  if (mWriteWatch)
    mLoop->RemoveFd(mWriteWatch);
  mSocketWatch = mWriteWatch = 0;
  if (mSocket >= 0)
    close(mSocket);
  mSocket = -1;
  IPlugWVBufferFree(&mIn);
  IPlugWVBufferFree(&mOut);
  mEvalHandlers.clear();
  mHasFocus = false;
  ShowFailure(reason);
}

void IWebViewImpl::SendString(uint32_t type, const char* str)
{
  if (mSocket < 0)
    return;
  const size_t frame = IPlugWVBeginFrame(&mOut, type);
  IPlugWVAppendString(&mOut, str);
  IPlugWVEndFrame(&mOut, frame);
  Flush();
}

void IWebViewImpl::SendInts(uint32_t type, std::initializer_list<int64_t> values)
{
  if (mSocket < 0)
    return;
  const size_t frame = IPlugWVBeginFrame(&mOut, type);
  for (int64_t value : values)
    IPlugWVAppendInt(&mOut, value);
  IPlugWVEndFrame(&mOut, frame);
  Flush();
}

void IWebViewImpl::SendIdAndString(uint32_t type, int64_t id, const char* str)
{
  if (mSocket < 0)
    return;
  const size_t frame = IPlugWVBeginFrame(&mOut, type);
  IPlugWVAppendInt(&mOut, id);
  IPlugWVAppendString(&mOut, str);
  IPlugWVEndFrame(&mOut, frame);
  Flush();
}

void IWebViewImpl::Flush()
{
  if (mOut.failed)
  {
    OnHelperGone("The web view ran out of memory.");
    return;
  }
  while (mSocket >= 0 && mOut.length > 0)
  {
    // MSG_NOSIGNAL: a helper that died must not take the host down with SIGPIPE.
    const ssize_t sent = send(mSocket, mOut.data, mOut.length, MSG_NOSIGNAL);
    if (sent > 0)
    {
      IPlugWVBufferConsume(&mOut, static_cast<size_t>(sent));
      continue;
    }
    if (sent < 0 && errno == EINTR)
      continue;
    if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
    {
      if (!mWriteWatch && !mPollTimer)
        mWriteWatch = mLoop->AddFd(mSocket, HostLoop::kFdWrite, [this](int, int) { Flush(); });
      return;
    }
    // The read side sees the end of the socket and reports it.
    return;
  }
  if (mWriteWatch && mOut.length == 0)
  {
    mLoop->RemoveFd(mWriteWatch);
    mWriteWatch = 0;
  }
}

// ---------------------------------------------------------------------------------------------- X11, XEmbed

void IWebViewImpl::EmbedPlug(Window plug)
{
  if (!mDisplay || !plug)
    return;
  XErrorTrap trap(mDisplay);
  mPlug = plug;
  XSelectInput(mDisplay, mPlug, StructureNotifyMask | FocusChangeMask | PropertyChangeMask);
  const Rect bounds = PhysicalBounds();
  XMoveResizeWindow(mDisplay, mPlug, 0, 0, bounds.w, bounds.h);
  SendXEmbed(mPlug, kXEmbedEmbeddedNotify, 0, static_cast<long>(mContainer), kXEmbedVersion);
  XMapWindow(mDisplay, mPlug);
  XFlush(mDisplay);
}

void IWebViewImpl::SendXEmbed(Window target, long message, long detail, long data1, long data2, Time time)
{
  XEvent event {};
  event.xclient.type = ClientMessage;
  event.xclient.window = target;
  event.xclient.message_type = mXEmbed;
  event.xclient.format = 32;
  event.xclient.data.l[0] = static_cast<long>(time);
  event.xclient.data.l[1] = message;
  event.xclient.data.l[2] = detail;
  event.xclient.data.l[3] = data1;
  event.xclient.data.l[4] = data2;
  XSendEvent(mDisplay, target, False, NoEventMask, &event);
}

// The keys go straight to the plug's window, where GTK reads them; the XEmbed messages tell GTK that it is
// active and focused, which it otherwise learns only from an embedder that forwards keys (GtkSocket).
// The helper asks on every button release, with its time: the window manager's click-to-focus carries the
// press's, so the server ignores it if it comes later.
void IWebViewImpl::TakeFocus(Time time)
{
  if (!mDisplay || !mPlug || mHidden || !mInteractionEnabled)
    return;
  XErrorTrap trap(mDisplay);
  XSetInputFocus(mDisplay, mPlug, RevertToParent, time);
  if (!mHasFocus)
  {
    SendXEmbed(mPlug, kXEmbedWindowActivate);
    SendXEmbed(mPlug, kXEmbedFocusIn, kXEmbedFocusCurrent);
  }
  mHasFocus = true;
  if (mEmbedder)
    SendXEmbed(mEmbedder, kXEmbedRequestFocus, 0, 0, 0, time);
  XFlush(mDisplay);

  const auto now = std::chrono::steady_clock::now();
  if (time != CurrentTime)
    mFocusEpisodeAt = now;
  mFocusWatched = 0;
  mFocusQuietSince = now;
  if (!mFocusTimer)
    mFocusTimer = mLoop->AddTimer(kFocusCheckMs, [this]() { CheckFocus(); });
}

void IWebViewImpl::CheckFocus()
{
  const auto now = std::chrono::steady_clock::now();
  if (!mDisplay || !mPlug || !mHasFocus || now - mFocusEpisodeAt > kFocusEpisode)
  {
    EndFocusEpisode();
    return;
  }
  XErrorTrap trap(mDisplay);
  Window focus = None;
  int revert = 0;
  XGetInputFocus(mDisplay, &focus, &revert);
  if (focus == mPlug)
    return;
  // Another top-level window took it (alt-tab, a click elsewhere): the user moved on.
  if (focus == None || focus == PointerRoot || TopLevelOf(focus) != TopLevelOf(mContainer))
  {
    EndFocusEpisode();
    LoseFocus();
    return;
  }
  if (focus != mFocusWatched)
  {
    mFocusWatched = focus;
    mFocusQuietSince = now;
    return;
  }
  if (now - mFocusQuietSince > kFocusQuiet)
    TakeFocus(CurrentTime);
}

void IWebViewImpl::EndFocusEpisode()
{
  if (mFocusTimer)
    mLoop->RemoveTimer(mFocusTimer);
  mFocusTimer = 0;
}

Window IWebViewImpl::TopLevelOf(Window window) const
{
  const Window root = DefaultRootWindow(mDisplay);
  for (int depth = 0; window && window != root && depth < 64; depth++)
  {
    Window windowRoot = 0, parent = 0;
    Window* pChildren = nullptr;
    unsigned int count = 0;
    if (!XQueryTree(mDisplay, window, &windowRoot, &parent, &pChildren, &count))
      return 0;
    if (pChildren)
      XFree(pChildren);
    if (parent == root)
      return window;
    window = parent;
  }
  return window;
}

void IWebViewImpl::LoseFocus()
{
  EndFocusEpisode();
  if (!mHasFocus || !mDisplay || !mPlug)
    return;
  mHasFocus = false;
  XErrorTrap trap(mDisplay);
  SendXEmbed(mPlug, kXEmbedFocusOut);
  SendXEmbed(mPlug, kXEmbedWindowDeactivate);
  XFlush(mDisplay);
}

void IWebViewImpl::ProcessXEvents()
{
  if (!mDisplay)
    return;
  const std::shared_ptr<bool> alive = mAlive;
  XErrorTrap trap(mDisplay);
  while (alive && *alive && XPending(mDisplay) > 0)
  {
    XEvent event;
    XNextEvent(mDisplay, &event);
    HandleXEvent(event);
  }
}

void IWebViewImpl::HandleXEvent(const XEvent& event)
{
  switch (event.type)
  {
    case Expose:
      if (event.xexpose.window == mContainer && event.xexpose.count == 0)
        DrawFailure();
      break;
    case ClientMessage:
      if (event.xclient.message_type == mXEmbed && event.xclient.window == mContainer)
      {
        const long message = event.xclient.data.l[1];
        if (message == kXEmbedRequestFocus)
          TakeFocus(static_cast<Time>(event.xclient.data.l[0]));
        else if (message == kXEmbedEmbeddedNotify)
          mEmbedder = static_cast<Window>(event.xclient.data.l[3]);
      }
      break;
    case FocusOut:
      // Grabs (a page's own popup) move the focus only for a moment; NotifyInferior stays inside the plug. During
      // a click's episode CheckFocus() decides.
      if (event.xfocus.window == mPlug && event.xfocus.mode != NotifyGrab && event.xfocus.mode != NotifyUngrab
          && event.xfocus.detail != NotifyInferior && !mFocusTimer)
        LoseFocus();
      break;
    case PropertyNotify:
      if (event.xproperty.window == mPlug && event.xproperty.atom == mXEmbedInfo)
      {
        Atom type = None;
        int format = 0;
        unsigned long count = 0, remaining = 0;
        unsigned char* pData = nullptr;
        if (XGetWindowProperty(mDisplay, mPlug, mXEmbedInfo, 0, 2, False, mXEmbedInfo, &type, &format, &count,
                               &remaining, &pData) == Success && pData && format == 32 && count >= 2)
        {
          const long flags = reinterpret_cast<long*>(pData)[1];
          (flags & kXEmbedMapped) ? XMapWindow(mDisplay, mPlug) : XUnmapWindow(mDisplay, mPlug);
        }
        if (pData)
          XFree(pData);
      }
      break;
    case DestroyNotify:
      if (event.xdestroywindow.window == mPlug)
      {
        mPlug = 0;
        mHasFocus = false;
      }
      break;
    case ReparentNotify:
      if (event.xreparent.window == mPlug && event.xreparent.parent != mContainer)
      {
        mPlug = 0;
        mHasFocus = false;
      }
      break;
    default:
      break;
  }
}

void IWebViewImpl::ShowFailure(const char* text)
{
  fprintf(stderr, "iPlug2: %s\n", text);
  mFailure.Set(text);
  if (!mDisplay || !mContainer)
    return;
  XErrorTrap trap(mDisplay);
  XClearArea(mDisplay, mContainer, 0, 0, 0, 0, True);
  XFlush(mDisplay);
}

void IWebViewImpl::DrawFailure()
{
  if (!mFailure.GetLength() || !mDisplay || !mContainer)
    return;
  const int screen = DefaultScreen(mDisplay);
  GC gc = XCreateGC(mDisplay, mContainer, 0, nullptr);
  XSetForeground(mDisplay, gc, BlackPixel(mDisplay, screen));
  XFontStruct* pFont = XLoadQueryFont(mDisplay, "fixed");
  if (pFont)
    XSetFont(mDisplay, gc, pFont->fid);
  const int lineHeight = pFont ? pFont->ascent + pFont->descent + 4 : 16;
  int y = 24;
  const char* line = mFailure.Get();
  while (*line)
  {
    const char* end = strchr(line, '\n');
    const int length = end ? static_cast<int>(end - line) : static_cast<int>(strlen(line));
    XDrawString(mDisplay, mContainer, gc, 12, y, line, length);
    y += lineHeight;
    line += length + (end ? 1 : 0);
  }
  if (pFont)
    XFreeFont(mDisplay, pFont);
  XFreeGC(mDisplay, gc);
  XFlush(mDisplay);
}

// --------------------------------------------------------------------------------------------- IWebView glue

void IWebView::SetWebViewHostLoop(HostLoop* pLoop)
{
  mpImpl->SetHostLoop(pLoop);
}

void IWebView::SetWebViewScreenScale(float scale)
{
  mpImpl->SetScreenScale(scale);
}

float IWebView::GetLinuxDesktopScale()
{
  constexpr double kReferenceDpi = 96.0;
  double scale = 0.0;
  if (Display* pDisplay = XOpenDisplay(nullptr))
  {
    // Xft.dpi, where GNOME and KDE publish their UI scale (96 × scale), read as Xft itself reads it
    if (const char* resources = XResourceManagerString(pDisplay))
    {
      XrmInitialize();
      if (XrmDatabase database = XrmGetStringDatabase(resources))
      {
        char* pType = nullptr;
        XrmValue value {};
        if (XrmGetResource(database, "Xft.dpi", "Xft.Dpi", &pType, &value) && value.addr)
          scale = strtod(value.addr, nullptr) / kReferenceDpi;
        XrmDestroyDatabase(database);
      }
    }
    XCloseDisplay(pDisplay);
  }
  if (!(scale > 0.0))
  {
    const char* gdkScale = getenv("GDK_SCALE");
    scale = gdkScale ? strtod(gdkScale, nullptr) : 1.0;
  }
  // Quarters as the desktops offer them; a DPI a point or two off 96 stays 1.
  scale = std::round(scale * 8.0) / 8.0;
  return static_cast<float>(std::clamp(scale, 1.0, 4.0));
}

#include "IPlugWebView.cpp"
