/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#include "IGraphicsLinuxDialogs.h"

#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sys/stat.h>
#include <unistd.h>

#include "heapbuf.h"
#include "Linux/IPlugLinuxDBus.h"
#include "Linux/IPlugLinuxHostLoop.h"
#include "Linux/IPlugLinuxSubprocess.h"

using namespace iplug;
using namespace igraphics;
namespace dbus = iplug::linux_dbus;

namespace
{
using EKind = LinuxDialogRequest::EKind;
using Clock = std::chrono::steady_clock;

// Handlers and dialog programs are picked up at this rate even where the host never reports the fds readable.
constexpr uint32_t kPollIntervalMs = 50;
// Long enough for the portal's D-Bus activation on first use; a portal silent for longer is broken.
constexpr auto kPortalReplyTimeout = std::chrono::seconds(10);
// FileChooser's "directory" option exists from version 3 on; older portals would quietly show a file dialog.
constexpr uint32_t kPortalDirectoryVersion = 3;

constexpr const char* kPortalService = "org.freedesktop.portal.Desktop";
constexpr const char* kPortalPath = "/org/freedesktop/portal/desktop";
constexpr const char* kFileChooserInterface = "org.freedesktop.portal.FileChooser";
constexpr const char* kRequestInterface = "org.freedesktop.portal.Request";
constexpr const char* kBusService = "org.freedesktop.DBus";
constexpr const char* kBusPath = "/org/freedesktop/DBus";

bool IsFileKind(EKind kind)
{
  return kind == EKind::OpenFile || kind == EKind::SaveFile || kind == EKind::Directory;
}

const char* DefaultTitle(EKind kind)
{
  switch (kind)
  {
    case EKind::OpenFile: return "Open File";
    case EKind::SaveFile: return "Save File";
    case EKind::Directory: return "Choose a Directory";
    case EKind::Color: return "Choose a Color";
    case EKind::Message: return "";
  }
  return "";
}

#pragma mark - Paths and extensions

/** "fxp", "wav aif", ".wav .aif", "*.wav;*.aif" -> wav, aif */
void SplitExtensions(const char* list, WDL_PtrList<WDL_String>& extensions)
{
  auto isSeparator = [](char c) { return c == ' ' || c == ',' || c == ';' || c == '\t'; };
  for (const char* p = list ? list : ""; *p;)
  {
    while (*p && isSeparator(*p))
      p++;
    const char* pStart = p;
    while (*p && !isSeparator(*p))
      p++;
    while (pStart < p && (*pStart == '*' || *pStart == '.'))
      pStart++;
    if (pStart < p)
      extensions.Add(new WDL_String(pStart, static_cast<int>(p - pStart)));
  }
}

/** Portal and GTK globs are case-sensitive: "fxp" -> "*.[fF][xX][pP]" */
void AppendCaseInsensitiveGlob(WDL_String& glob, const char* extension)
{
  glob.Append("*.");
  for (const char* p = extension; *p; p++)
  {
    const unsigned char c = static_cast<unsigned char>(*p);
    if (c < 0x80 && isalpha(c))
      glob.AppendFormatted(8, "[%c%c]", tolower(c), toupper(c));
    else if (strchr("*?[", *p))
      glob.AppendFormatted(8, "[%c]", *p);
    else
      glob.Append(p, 1);
  }
}

void AppendPlainGlobs(WDL_String& globs, const WDL_PtrList<WDL_String>& extensions, bool bothCases)
{
  for (int i = 0; i < extensions.GetSize(); i++)
  {
    const char* pExtension = extensions.Get(i)->Get();
    globs.AppendFormatted(256, "%s*.%s", globs.GetLength() ? " " : "", pExtension);
    if (bothCases)
    {
      WDL_String upper(pExtension);
      for (char* p = upper.Get(); *p; p++)
        *p = static_cast<char>(toupper(static_cast<unsigned char>(*p)));
      if (strcmp(upper.Get(), pExtension) != 0)
        globs.AppendFormatted(256, " *.%s", upper.Get());
    }
  }
}

bool HasOneOfExtensions(const char* path, const WDL_PtrList<WDL_String>& extensions)
{
  const char* pName = strrchr(path, '/');
  pName = pName ? pName + 1 : path;
  const char* pDot = strrchr(pName, '.');
  if (!pDot || pDot == pName)
    return false;
  for (int i = 0; i < extensions.GetSize(); i++)
  {
    if (strcasecmp(pDot + 1, extensions.Get(i)->Get()) == 0)
      return true;
  }
  return false;
}

/** The name a save dialog proposes, with the first extension if it has none of them */
WDL_String ProposedFileName(const LinuxDialogRequest& request, const WDL_PtrList<WDL_String>& extensions)
{
  WDL_String name(request.fileName.Get());
  if (name.GetLength() && extensions.GetSize() && !HasOneOfExtensions(name.Get(), extensions))
    name.AppendFormatted(256, ".%s", extensions.Get(0)->Get());
  return name;
}

/** The starting folder with a trailing '/'; the home folder if none was given (zenity would use the host's cwd) */
WDL_String StartFolder(const LinuxDialogRequest& request)
{
  WDL_String folder(request.folder.Get());
  if (!folder.GetLength())
  {
    const char* pHome = getenv("HOME");
    folder.Set(pHome && pHome[0] ? pHome : "/");
  }
  if (folder.Get()[folder.GetLength() - 1] != '/')
    folder.Append("/");
  return folder;
}

void EnsureTrailingSlash(WDL_String& path)
{
  if (path.GetLength() && path.Get()[path.GetLength() - 1] != '/')
    path.Append("/");
}

bool PathExists(const char* path)
{
  struct stat info;
  return stat(path, &info) == 0;
}

int HexDigit(char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

/** file:///a%20b/%D0%B6 -> /a b/ж */
bool PathFromFileURI(const char* uri, WDL_String& path)
{
  if (strncmp(uri, "file://", 7) != 0)
    return false;
  const char* p = uri + 7;
  if (*p != '/')
  {
    p = strchr(p, '/'); // file://host/path
    if (!p)
      return false;
  }
  path.Set("");
  for (; *p; p++)
  {
    const int high = p[0] == '%' ? HexDigit(p[1]) : -1;
    const int low = high >= 0 ? HexDigit(p[2]) : -1;
    if (low >= 0)
    {
      const char c = static_cast<char>(high * 16 + low);
      path.Append(&c, 1);
      p += 2;
    }
    else
      path.Append(p, 1);
  }
  return path.GetLength() > 0;
}

/** "rgb(1,2,3)", "rgba(1,2,3,0.5)" (zenity), "#010203" (kdialog) */
bool ParseColor(const char* text, IColor& color)
{
  int r = 0, g = 0, b = 0;
  float a = 1.f;
  unsigned int rgb = 0;
  if (sscanf(text, "rgba(%d,%d,%d,%f)", &r, &g, &b, &a) == 4 || sscanf(text, "rgb(%d,%d,%d)", &r, &g, &b) == 3)
  {
    const int alpha = strncmp(text, "rgba", 4) == 0 ? static_cast<int>(a * 255.f + 0.5f) : color.A;
    color = IColor(Clip(alpha, 0, 255), Clip(r, 0, 255), Clip(g, 0, 255), Clip(b, 0, 255));
    return true;
  }
  if (sscanf(text, "#%6x", &rgb) == 1 && strlen(text) >= 7)
  {
    color = IColor(color.A, (rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
    return true;
  }
  return false;
}

void FirstLine(const char* text, WDL_String& line)
{
  const char* pEnd = strchr(text, '\n');
  line.Set(text, pEnd ? static_cast<int>(pEnd - text) : static_cast<int>(strlen(text)));
}

#pragma mark - Dialog programs

/** Owns the argument strings and lends them out as an argv array */
class ArgList
{
public:
  ~ArgList() { mArgs.Empty(true); }

  void Add(const char* arg) { mArgs.Add(new WDL_String(arg)); }

  void Add(const char* option, const char* value)
  {
    WDL_String* pArg = new WDL_String(option);
    pArg->Append(value);
    mArgs.Add(pArg);
  }

  const char* const* Get()
  {
    mArgv.Resize(0);
    for (int i = 0; i < mArgs.GetSize(); i++)
      mArgv.Add(mArgs.Get(i)->Get());
    mArgv.Add(nullptr);
    return mArgv.Get();
  }

private:
  WDL_PtrList<WDL_String> mArgs;
  WDL_TypedBuf<const char*> mArgv;
};

/** A helper program's exit status that says the program itself failed (not found, crashed, cannot show a window),
 * as opposed to the user's answer */
bool IsProgramFailure(int exitCode)
{
  return exitCode < 0 || exitCode == 126 || exitCode == 127 || exitCode == 255;
}

struct DialogProgram
{
  const char* mName;
  const char* mExecutable;
  void (*mBuildArgs)(const LinuxDialogRequest& request, ArgList& args);
  /** @return false if the program failed: the next one is tried */
  bool (*mParse)(const LinuxDialogRequest& request, int exitCode, const char* output, LinuxDialogResult& result);
};

bool ParseFileAnswer(const LinuxDialogRequest& request, int exitCode, const char* output, LinuxDialogResult& result)
{
  if (exitCode == 0 && output[0] == '/')
  {
    FirstLine(output, result.path);
    if (request.kind == EKind::Directory)
      EnsureTrailingSlash(result.path);
    result.accepted = true;
  }
  return true;
}

void BuildZenityArgs(const LinuxDialogRequest& request, ArgList& args)
{
  args.Add("zenity");
  args.Add("--title=", request.title.Get());
  if (request.parentWindow)
  {
    // zenity 3 makes the dialog modal for the host's window; zenity 4 ignores both
    char windowId[32];
    snprintf(windowId, sizeof(windowId), "%lu", request.parentWindow);
    args.Add("--attach=", windowId);
    args.Add("--modal");
  }

  WDL_PtrList<WDL_String> extensions;
  SplitExtensions(request.extensions.Get(), extensions);

  switch (request.kind)
  {
    case EKind::OpenFile:
    case EKind::SaveFile:
    case EKind::Directory:
    {
      WDL_String start = StartFolder(request);
      args.Add("--file-selection");
      if (request.kind == EKind::SaveFile)
      {
        args.Add("--save");
        args.Add("--confirm-overwrite"); // zenity 3 only; GTK 4 always asks
        start.Append(ProposedFileName(request, extensions).Get());
      }
      if (request.kind == EKind::Directory)
        args.Add("--directory");
      args.Add("--filename=", start.Get());
      if (request.kind != EKind::Directory && extensions.GetSize())
      {
        WDL_String filter;
        AppendPlainGlobs(filter, extensions, false);
        filter.Append(" | ");
        for (int i = 0; i < extensions.GetSize(); i++)
        {
          if (i)
            filter.Append(" ");
          AppendCaseInsensitiveGlob(filter, extensions.Get(i)->Get());
        }
        args.Add("--file-filter=", filter.Get());
      }
      break;
    }
    case EKind::Message:
    {
      switch (request.messageType)
      {
        case kMB_OK: args.Add("--info"); break;
        case kMB_OKCANCEL: args.Add("--question"); args.Add("--ok-label=OK"); args.Add("--cancel-label=Cancel"); break;
        case kMB_YESNO: args.Add("--question"); args.Add("--ok-label=Yes"); args.Add("--cancel-label=No"); break;
        case kMB_RETRYCANCEL: args.Add("--warning"); args.Add("--ok-label=Retry"); args.Add("--extra-button=Cancel"); break;
        // The answer is the button's label on stdout; Escape prints nothing, which reads as Cancel, as on macOS.
        case kMB_YESNOCANCEL:
          args.Add("--question");
          args.Add("--switch");
          args.Add("--extra-button=Yes");
          args.Add("--extra-button=No");
          args.Add("--extra-button=Cancel");
          break;
      }
      args.Add("--no-markup");
      args.Add("--text=", request.text.Get());
      break;
    }
    case EKind::Color:
    {
      char color[64];
      snprintf(color, sizeof(color), "rgba(%d,%d,%d,%.3f)", request.color.R, request.color.G, request.color.B, request.color.A / 255.f);
      args.Add("--color-selection");
      args.Add("--color=", color);
      break;
    }
  }
}

bool ParseZenityAnswer(const LinuxDialogRequest& request, int exitCode, const char* output, LinuxDialogResult& result)
{
  if (IsProgramFailure(exitCode))
    return false;

  switch (request.kind)
  {
    case EKind::OpenFile:
    case EKind::SaveFile:
    case EKind::Directory:
      return ParseFileAnswer(request, exitCode, output, result);
    case EKind::Message:
    {
      const bool ok = exitCode == 0;
      switch (request.messageType)
      {
        case kMB_OK: result.message = kOK; break;
        case kMB_OKCANCEL: result.message = ok ? kOK : kCANCEL; break;
        case kMB_YESNO: result.message = ok ? kYES : kNO; break;
        case kMB_RETRYCANCEL: result.message = ok ? kRETRY : kCANCEL; break;
        case kMB_YESNOCANCEL:
          result.message = strcmp(output, "Yes") == 0 ? kYES : strcmp(output, "No") == 0 ? kNO : kCANCEL;
          break;
      }
      result.accepted = true;
      return true;
    }
    case EKind::Color:
      result.color = request.color;
      result.accepted = exitCode == 0 && ParseColor(output, result.color);
      return true;
  }
  return true;
}

/** kdialog parses arguments that start with '-' as options, also where it expects text */
const char* KDialogText(const char* text, WDL_String& storage)
{
  if (text[0] != '-')
    return text;
  storage.SetFormatted(static_cast<int>(strlen(text)) + 2, " %s", text);
  return storage.Get();
}

void BuildKDialogArgs(const LinuxDialogRequest& request, ArgList& args)
{
  WDL_String storage;
  args.Add("kdialog");
  args.Add("--title");
  args.Add(KDialogText(request.title.Get(), storage));
  if (request.parentWindow)
  {
    char windowId[32];
    snprintf(windowId, sizeof(windowId), "%lu", request.parentWindow);
    args.Add("--attach");
    args.Add(windowId);
  }

  WDL_PtrList<WDL_String> extensions;
  SplitExtensions(request.extensions.Get(), extensions);
  WDL_String filter;
  if (extensions.GetSize())
  {
    AppendPlainGlobs(filter, extensions, true);
    filter.Append("|");
    AppendPlainGlobs(filter, extensions, false);
  }

  WDL_String start = StartFolder(request);
  switch (request.kind)
  {
    case EKind::OpenFile:
      args.Add("--getopenfilename");
      args.Add(start.Get());
      if (filter.GetLength())
        args.Add(filter.Get());
      break;
    case EKind::SaveFile:
      args.Add("--getsavefilename");
      start.Append(ProposedFileName(request, extensions).Get());
      args.Add(start.Get());
      if (filter.GetLength())
        args.Add(filter.Get());
      break;
    case EKind::Directory:
      args.Add("--getexistingdirectory");
      args.Add(start.Get());
      break;
    case EKind::Message:
    {
      const char* pText = KDialogText(request.text.Get(), storage);
      switch (request.messageType)
      {
        case kMB_OK: args.Add("--msgbox"); args.Add(pText); break;
        case kMB_OKCANCEL: args.Add("--yesno"); args.Add(pText); args.Add("--yes-label"); args.Add("OK"); args.Add("--no-label"); args.Add("Cancel"); break;
        case kMB_YESNO: args.Add("--yesno"); args.Add(pText); break;
        case kMB_RETRYCANCEL: args.Add("--warningyesno"); args.Add(pText); args.Add("--yes-label"); args.Add("Retry"); args.Add("--no-label"); args.Add("Cancel"); break;
        case kMB_YESNOCANCEL: args.Add("--yesnocancel"); args.Add(pText); break;
      }
      break;
    }
    case EKind::Color:
    {
      char color[16];
      snprintf(color, sizeof(color), "#%02x%02x%02x", request.color.R, request.color.G, request.color.B);
      args.Add("--getcolor");
      args.Add("--default");
      args.Add(color);
      break;
    }
  }
}

bool ParseKDialogAnswer(const LinuxDialogRequest& request, int exitCode, const char* output, LinuxDialogResult& result)
{
  if (IsProgramFailure(exitCode))
    return false;

  switch (request.kind)
  {
    case EKind::OpenFile:
    case EKind::SaveFile:
    case EKind::Directory:
      return ParseFileAnswer(request, exitCode, output, result);
    case EKind::Message:
      switch (request.messageType)
      {
        case kMB_OK: result.message = kOK; break;
        case kMB_OKCANCEL: result.message = exitCode == 0 ? kOK : kCANCEL; break;
        case kMB_YESNO: result.message = exitCode == 0 ? kYES : kNO; break;
        case kMB_RETRYCANCEL: result.message = exitCode == 0 ? kRETRY : kCANCEL; break;
        case kMB_YESNOCANCEL: result.message = exitCode == 0 ? kYES : exitCode == 1 ? kNO : kCANCEL; break;
      }
      result.accepted = true;
      return true;
    case EKind::Color:
      result.color = request.color;
      result.accepted = exitCode == 0 && ParseColor(output, result.color);
      return true;
  }
  return true;
}

const DialogProgram kZenity { "zenity", "zenity", BuildZenityArgs, ParseZenityAnswer };
const DialogProgram kKDialog { "kdialog", "kdialog", BuildKDialogArgs, ParseKDialogAnswer };

enum class EBackend { Portal, Zenity, KDialog };

/** portal, then zenity and kdialog (kdialog first on KDE). IPLUG_LINUX_DIALOGS="zenity,portal" etc. overrides it. */
void GetBackendOrder(WDL_TypedBuf<EBackend>& order)
{
  const char* pOverride = getenv("IPLUG_LINUX_DIALOGS");
  if (pOverride && pOverride[0])
  {
    WDL_PtrList<WDL_String> names;
    SplitExtensions(pOverride, names);
    for (int i = 0; i < names.GetSize(); i++)
    {
      const char* pName = names.Get(i)->Get();
      if (!strcmp(pName, "portal")) order.Add(EBackend::Portal);
      else if (!strcmp(pName, "zenity")) order.Add(EBackend::Zenity);
      else if (!strcmp(pName, "kdialog")) order.Add(EBackend::KDialog);
    }
    names.Empty(true);
    return;
  }

  const char* pDesktop = getenv("XDG_CURRENT_DESKTOP");
  const bool isKDE = pDesktop && strstr(pDesktop, "KDE");
  order.Add(EBackend::Portal);
  order.Add(isKDE ? EBackend::KDialog : EBackend::Zenity);
  order.Add(isKDE ? EBackend::Zenity : EBackend::KDialog);
}

#pragma mark - Portal FileChooser

/** One FileChooser request on a private session-bus connection of its own, so that every Response signal on it is
 * this request's. Fully asynchronous: Hello, the call and the Response are read as they arrive. */
class PortalFileChooser
{
public:
  enum class EState { Running, Done, Unavailable };

  ~PortalFileChooser() { Dismiss(); }

  /** @return false if there is no libdbus or no session bus */
  bool Start(const LinuxDialogRequest& request);
  EState Pump(LinuxDialogResult& result);
  int GetFd() const { return mFd; }
  /** Closes the dialog if it is showing, and the connection */
  void Dismiss();

private:
  enum class EStep { Hello, Version, Call, Response };

  void Send(dbus::Message* pMessage, EStep nextStep);
  void AddMatch(const char* requestPath);
  void SendVersionQuery();
  void SendFileChooserCall();
  EState Handle(dbus::Message* pMessage, LinuxDialogResult& result);
  EState HandleReply(dbus::Message* pMessage);
  void ParseResponse(dbus::Message* pMessage, LinuxDialogResult& result);
  EState LostConnection() const { return mStep == EStep::Response ? EState::Done : EState::Unavailable; }

  void AppendString(dbus::MessageIter* pIter, const char* value);
  void OpenOption(dbus::MessageIter* pOptions, const char* key, const char* signature, dbus::MessageIter* pEntry, dbus::MessageIter* pVariant);
  void CloseOption(dbus::MessageIter* pOptions, dbus::MessageIter* pEntry, dbus::MessageIter* pVariant);

  const dbus::Functions* mDBus = nullptr;
  dbus::Connection* mConnection = nullptr;
  int mFd = -1;
  EStep mStep = EStep::Hello;
  uint32_t mSerial = 0;
  Clock::time_point mSentAt;
  LinuxDialogRequest mRequest;
  WDL_String mToken;
  WDL_String mRequestPath; // where the portal will put the request, from our unique name and the token
  WDL_String mHandle;      // where it says it put it (older portals ignore the token)
};

bool PortalFileChooser::Start(const LinuxDialogRequest& request)
{
  if (!(mDBus = dbus::Functions::Get()))
    return false;
  char address[1024];
  if (!dbus::GetSessionBusAddress(address, sizeof(address)))
    return false;
  if (!(mConnection = mDBus->connection_open_private(address, nullptr)))
    return false;
  mDBus->connection_set_exit_on_disconnect(mConnection, 0);
  if (!mDBus->connection_get_unix_fd(mConnection, &mFd))
  {
    Dismiss();
    return false;
  }

  static std::atomic<int> sRequestCount { 0 };
  mToken.SetFormatted(64, "iplug2_%d_%d", static_cast<int>(getpid()), ++sRequestCount);
  mRequest = request;
  // A private connection: libdbus' blocking dbus_bus_register() would do this Hello on the host thread.
  Send(mDBus->message_new_method_call(kBusService, kBusPath, kBusService, "Hello"), EStep::Hello);
  return mSerial != 0;
}

void PortalFileChooser::Send(dbus::Message* pMessage, EStep nextStep)
{
  mSerial = 0;
  if (!pMessage)
    return;
  mDBus->connection_send(mConnection, pMessage, &mSerial);
  mDBus->message_unref(pMessage);
  mDBus->connection_read_write(mConnection, 0);
  mStep = nextStep;
  mSentAt = Clock::now();
}

void PortalFileChooser::AddMatch(const char* requestPath)
{
  WDL_String rule;
  rule.SetFormatted(512, "type='signal',interface='%s',member='Response',path='%s'", kRequestInterface, requestPath);
  dbus::Message* pMessage = mDBus->message_new_method_call(kBusService, kBusPath, kBusService, "AddMatch");
  if (!pMessage)
    return;
  dbus::MessageIter args;
  mDBus->message_iter_init_append(pMessage, &args);
  AppendString(&args, rule.Get());
  mDBus->message_set_no_reply(pMessage, 1);
  mDBus->connection_send(mConnection, pMessage, nullptr);
  mDBus->message_unref(pMessage);
}

void PortalFileChooser::SendVersionQuery()
{
  dbus::Message* pMessage = mDBus->message_new_method_call(kPortalService, kPortalPath, "org.freedesktop.DBus.Properties", "Get");
  if (pMessage)
  {
    dbus::MessageIter args;
    mDBus->message_iter_init_append(pMessage, &args);
    AppendString(&args, kFileChooserInterface);
    AppendString(&args, "version");
  }
  Send(pMessage, EStep::Version);
}

void PortalFileChooser::AppendString(dbus::MessageIter* pIter, const char* value)
{
  mDBus->message_iter_append_basic(pIter, dbus::kTypeString, &value);
}

void PortalFileChooser::OpenOption(dbus::MessageIter* pOptions, const char* key, const char* signature, dbus::MessageIter* pEntry, dbus::MessageIter* pVariant)
{
  mDBus->message_iter_open_container(pOptions, dbus::kTypeDictEntry, nullptr, pEntry);
  AppendString(pEntry, key);
  mDBus->message_iter_open_container(pEntry, dbus::kTypeVariant, signature, pVariant);
}

void PortalFileChooser::CloseOption(dbus::MessageIter* pOptions, dbus::MessageIter* pEntry, dbus::MessageIter* pVariant)
{
  mDBus->message_iter_close_container(pEntry, pVariant);
  mDBus->message_iter_close_container(pOptions, pEntry);
}

void PortalFileChooser::SendFileChooserCall()
{
  const bool save = mRequest.kind == EKind::SaveFile;
  dbus::Message* pMessage = mDBus->message_new_method_call(kPortalService, kPortalPath, kFileChooserInterface, save ? "SaveFile" : "OpenFile");
  if (!pMessage)
  {
    Send(nullptr, EStep::Call);
    return;
  }

  WDL_PtrList<WDL_String> extensions;
  SplitExtensions(mRequest.extensions.Get(), extensions);

  char parent[64] = "";
  if (mRequest.parentWindow)
    snprintf(parent, sizeof(parent), "x11:%lx", mRequest.parentWindow);

  dbus::MessageIter args, options, entry, variant;
  mDBus->message_iter_init_append(pMessage, &args);
  AppendString(&args, parent);
  AppendString(&args, mRequest.title.Get());
  mDBus->message_iter_open_container(&args, dbus::kTypeArray, "{sv}", &options);

  OpenOption(&options, "handle_token", "s", &entry, &variant);
  AppendString(&variant, mToken.Get());
  CloseOption(&options, &entry, &variant);

  const dbus::Bool yes = 1;
  OpenOption(&options, "modal", "b", &entry, &variant);
  mDBus->message_iter_append_basic(&variant, dbus::kTypeBoolean, &yes);
  CloseOption(&options, &entry, &variant);

  if (mRequest.kind == EKind::Directory)
  {
    OpenOption(&options, "directory", "b", &entry, &variant);
    mDBus->message_iter_append_basic(&variant, dbus::kTypeBoolean, &yes);
    CloseOption(&options, &entry, &variant);
  }

  if (save)
  {
    const WDL_String name = ProposedFileName(mRequest, extensions);
    if (name.GetLength())
    {
      OpenOption(&options, "current_name", "s", &entry, &variant);
      AppendString(&variant, name.Get());
      CloseOption(&options, &entry, &variant);
    }
  }

  if (mRequest.folder.GetLength())
  {
    dbus::MessageIter bytes;
    const unsigned char* pFolder = reinterpret_cast<const unsigned char*>(mRequest.folder.Get());
    OpenOption(&options, "current_folder", "ay", &entry, &variant);
    mDBus->message_iter_open_container(&variant, dbus::kTypeArray, "y", &bytes);
    // A NUL-terminated byte string, by the portal's definition
    mDBus->message_iter_append_fixed_array(&bytes, dbus::kTypeByte, &pFolder, mRequest.folder.GetLength() + 1);
    mDBus->message_iter_close_container(&variant, &bytes);
    CloseOption(&options, &entry, &variant);
  }

  if (mRequest.kind != EKind::Directory && extensions.GetSize())
  {
    WDL_String name;
    AppendPlainGlobs(name, extensions, false);
    dbus::MessageIter filters, filter, patterns, pattern;
    OpenOption(&options, "filters", "a(sa(us))", &entry, &variant);
    mDBus->message_iter_open_container(&variant, dbus::kTypeArray, "(sa(us))", &filters);
    mDBus->message_iter_open_container(&filters, dbus::kTypeStruct, nullptr, &filter);
    AppendString(&filter, name.Get());
    mDBus->message_iter_open_container(&filter, dbus::kTypeArray, "(us)", &patterns);
    for (int i = 0; i < extensions.GetSize(); i++)
    {
      const uint32_t kGlob = 0;
      WDL_String glob;
      AppendCaseInsensitiveGlob(glob, extensions.Get(i)->Get());
      mDBus->message_iter_open_container(&patterns, dbus::kTypeStruct, nullptr, &pattern);
      mDBus->message_iter_append_basic(&pattern, dbus::kTypeUInt32, &kGlob);
      AppendString(&pattern, glob.Get());
      mDBus->message_iter_close_container(&patterns, &pattern);
    }
    mDBus->message_iter_close_container(&filter, &patterns);
    mDBus->message_iter_close_container(&filters, &filter);
    mDBus->message_iter_close_container(&variant, &filters);
    CloseOption(&options, &entry, &variant);
  }

  mDBus->message_iter_close_container(&args, &options);
  extensions.Empty(true);
  Send(pMessage, EStep::Call);
}

PortalFileChooser::EState PortalFileChooser::Pump(LinuxDialogResult& result)
{
  if (!mConnection)
    return LostConnection();

  const bool connected = mDBus->connection_read_write(mConnection, 0);
  while (dbus::Message* pMessage = mDBus->connection_pop_message(mConnection))
  {
    const EState state = Handle(pMessage, result);
    mDBus->message_unref(pMessage);
    if (state != EState::Running)
      return state;
  }
  if (!connected || !mDBus->connection_get_is_connected(mConnection))
    return LostConnection();

  if (mStep != EStep::Response && Clock::now() - mSentAt > kPortalReplyTimeout)
  {
    fprintf(stderr, "iPlug2: the desktop portal did not answer, falling back to a dialog program\n");
    return EState::Unavailable;
  }
  return EState::Running;
}

PortalFileChooser::EState PortalFileChooser::Handle(dbus::Message* pMessage, LinuxDialogResult& result)
{
  const int type = mDBus->message_get_type(pMessage);
  if (type == dbus::kMessageMethodReturn || type == dbus::kMessageError)
  {
    if (mDBus->message_get_reply_serial(pMessage) != mSerial)
      return EState::Running;
    if (type == dbus::kMessageError)
    {
      const char* pError = mDBus->message_get_error_name(pMessage);
      // ServiceUnknown: no portal on this bus, the usual case without a desktop
      if (!pError || strcmp(pError, "org.freedesktop.DBus.Error.ServiceUnknown") != 0)
        fprintf(stderr, "iPlug2: the desktop portal's FileChooser failed (%s), falling back to a dialog program\n", pError ? pError : "?");
      return EState::Unavailable;
    }
    return HandleReply(pMessage);
  }

  if (type == dbus::kMessageSignal)
  {
    const char* pInterface = mDBus->message_get_interface(pMessage);
    const char* pMember = mDBus->message_get_member(pMessage);
    const char* pPath = mDBus->message_get_path(pMessage);
    if (!pInterface || !pMember)
      return EState::Running;
    if (!strcmp(pInterface, "org.freedesktop.DBus.Local") && !strcmp(pMember, "Disconnected"))
      return LostConnection();
    const bool callSent = mStep == EStep::Call || mStep == EStep::Response;
    const bool ours = pPath && (!strcmp(pPath, mRequestPath.Get()) || (mHandle.GetLength() && !strcmp(pPath, mHandle.Get())));
    if (callSent && ours && !strcmp(pInterface, kRequestInterface) && !strcmp(pMember, "Response"))
    {
      ParseResponse(pMessage, result);
      return EState::Done;
    }
  }
  return EState::Running;
}

PortalFileChooser::EState PortalFileChooser::HandleReply(dbus::Message* pMessage)
{
  dbus::MessageIter args, value;
  const bool hasArgs = mDBus->message_iter_init(pMessage, &args);
  const int argType = hasArgs ? mDBus->message_iter_get_arg_type(&args) : dbus::kTypeInvalid;

  switch (mStep)
  {
    case EStep::Hello:
    {
      const char* pUniqueName = nullptr;
      if (argType != dbus::kTypeString)
        return EState::Unavailable;
      mDBus->message_iter_get_basic(&args, &pUniqueName);
      // ":1.42" -> "1_42", the request path's sender element
      WDL_String sender(pUniqueName[0] == ':' ? pUniqueName + 1 : pUniqueName);
      for (char* p = sender.Get(); *p; p++)
      {
        if (*p == '.')
          *p = '_';
      }
      mRequestPath.SetFormatted(512, "%s/request/%s/%s", kPortalPath, sender.Get(), mToken.Get());
      // Before the call, so a Response cannot come before we listen for it
      AddMatch(mRequestPath.Get());
      if (mRequest.kind == EKind::Directory)
        SendVersionQuery();
      else
        SendFileChooserCall();
      break;
    }
    case EStep::Version:
    {
      uint32_t version = 0;
      if (argType == dbus::kTypeVariant)
      {
        mDBus->message_iter_recurse(&args, &value);
        if (mDBus->message_iter_get_arg_type(&value) == dbus::kTypeUInt32)
          mDBus->message_iter_get_basic(&value, &version);
      }
      if (version < kPortalDirectoryVersion)
      {
        fprintf(stderr, "iPlug2: the desktop portal's FileChooser (version %u) cannot choose folders, falling back to a dialog program\n", version);
        return EState::Unavailable;
      }
      SendFileChooserCall();
      break;
    }
    case EStep::Call:
    {
      const char* pHandle = nullptr;
      if (argType != dbus::kTypeObjectPath)
        return EState::Unavailable;
      mDBus->message_iter_get_basic(&args, &pHandle);
      mHandle.Set(pHandle);
      if (strcmp(pHandle, mRequestPath.Get()) != 0)
        AddMatch(pHandle);
      mStep = EStep::Response;
      break;
    }
    case EStep::Response:
      break;
  }
  return mSerial || mStep == EStep::Response ? EState::Running : EState::Unavailable;
}

void PortalFileChooser::ParseResponse(dbus::Message* pMessage, LinuxDialogResult& result)
{
  dbus::MessageIter args, results, entry, value, uris;
  uint32_t response = 2;
  if (!mDBus->message_iter_init(pMessage, &args) || mDBus->message_iter_get_arg_type(&args) != dbus::kTypeUInt32)
    return;
  mDBus->message_iter_get_basic(&args, &response);
  if (response != 0)
  {
    if (response != 1)
      fprintf(stderr, "iPlug2: the desktop portal ended the file dialog without an answer (%u)\n", response);
    return;
  }
  if (!mDBus->message_iter_next(&args) || mDBus->message_iter_get_arg_type(&args) != dbus::kTypeArray)
    return;

  for (mDBus->message_iter_recurse(&args, &results); mDBus->message_iter_get_arg_type(&results) == dbus::kTypeDictEntry; mDBus->message_iter_next(&results))
  {
    const char* pKey = nullptr;
    mDBus->message_iter_recurse(&results, &entry);
    if (mDBus->message_iter_get_arg_type(&entry) != dbus::kTypeString)
      continue;
    mDBus->message_iter_get_basic(&entry, &pKey);
    if (strcmp(pKey, "uris") != 0 || !mDBus->message_iter_next(&entry) || mDBus->message_iter_get_arg_type(&entry) != dbus::kTypeVariant)
      continue;
    mDBus->message_iter_recurse(&entry, &value);
    if (mDBus->message_iter_get_arg_type(&value) != dbus::kTypeArray)
      continue;
    mDBus->message_iter_recurse(&value, &uris);
    if (mDBus->message_iter_get_arg_type(&uris) != dbus::kTypeString)
      continue;
    const char* pURI = nullptr;
    mDBus->message_iter_get_basic(&uris, &pURI);
    if (PathFromFileURI(pURI, result.path))
    {
      if (mRequest.kind == EKind::Directory)
        EnsureTrailingSlash(result.path);
      result.accepted = true;
    }
    else
      fprintf(stderr, "iPlug2: the desktop portal returned a location that is not a local file: %s\n", pURI);
    return;
  }
}

void PortalFileChooser::Dismiss()
{
  if (!mConnection)
    return;
  const bool callSent = mStep == EStep::Call || mStep == EStep::Response;
  if (callSent && mDBus->connection_get_is_connected(mConnection))
  {
    // The portal also closes it when our connection goes, but not every version does so at once.
    const char* pPath = mHandle.GetLength() ? mHandle.Get() : mRequestPath.Get();
    if (dbus::Message* pClose = mDBus->message_new_method_call(kPortalService, pPath, kRequestInterface, "Close"))
    {
      mDBus->message_set_no_reply(pClose, 1);
      mDBus->connection_send(mConnection, pClose, nullptr);
      mDBus->message_unref(pClose);
      mDBus->connection_flush(mConnection);
    }
  }
  mDBus->connection_close(mConnection);
  mDBus->connection_unref(mConnection);
  mConnection = nullptr;
  mFd = -1;
}

void WarnNoDialog(const LinuxDialogRequest& request)
{
  static bool sWarned = false;
  if (!sWarned)
  {
    fprintf(stderr, "iPlug2: no dialog available: needs xdg-desktop-portal (files and folders), zenity or kdialog\n");
    sWarned = true;
  }
  if (request.kind == EKind::Message)
    fprintf(stderr, "iPlug2: message not shown: %s: %s\n", request.title.Get(), request.text.Get());
}
} // namespace

#pragma mark - LinuxDialogs::Job

/** One dialog from Show() to its handler: the backends in order until one answers, then, for a save whose name
 * got its extension added, an overwrite question if that file exists. */
struct LinuxDialogs::Job
{
  enum class EStage { Ask, ConfirmOverwrite };

  LinuxDialogRequest mRequest;
  LinuxDialogRequest mConfirmRequest;
  Handler mHandler;
  EStage mStage = EStage::Ask;
  WDL_TypedBuf<EBackend> mBackends;
  int mNextBackend = 0;

  std::unique_ptr<PortalFileChooser> mPortal;
  LinuxBackgroundProcess mProcess;
  const DialogProgram* mProgram = nullptr;

  LinuxDialogResult mResult;
  bool mDone = false;

  int mFdId = 0;
  int mWatchedFd = -1;
  bool mFdFailed = false;

  const LinuxDialogRequest& Asked() const { return mStage == EStage::Ask ? mRequest : mConfirmRequest; }

  void StartNextBackend()
  {
    while (mNextBackend < mBackends.GetSize())
    {
      const EBackend backend = mBackends.Get()[mNextBackend++];
      if (backend == EBackend::Portal)
      {
        if (mStage != EStage::Ask || !IsFileKind(mRequest.kind))
          continue;
        auto portal = std::make_unique<PortalFileChooser>();
        if (portal->Start(mRequest))
        {
          mPortal = std::move(portal);
          return;
        }
        continue;
      }

      const DialogProgram* pProgram = backend == EBackend::Zenity ? &kZenity : &kKDialog;
      if (!LinuxBackgroundProcess::IsExecutableAvailable(pProgram->mExecutable))
        continue;
      ArgList args;
      pProgram->mBuildArgs(Asked(), args);
      if (mProcess.Start(args.Get()))
      {
        mProgram = pProgram;
        return;
      }
    }

    WarnNoDialog(Asked());
    mResult = LinuxDialogResult();
    if (mStage == EStage::Ask && mRequest.kind == EKind::Message && mRequest.messageType == kMB_OK)
      mResult.message = kOK;
    mDone = true;
  }

  void Advance()
  {
    if (mDone)
      return;

    LinuxDialogResult answer;
    if (mPortal)
    {
      const PortalFileChooser::EState state = mPortal->Pump(answer);
      if (state == PortalFileChooser::EState::Running)
        return;
      mPortal.reset();
      if (state == PortalFileChooser::EState::Unavailable)
        StartNextBackend();
      else
        Answered(answer);
      return;
    }

    if (mProgram)
    {
      if (!mProcess.Pump())
        return;
      const DialogProgram* pProgram = mProgram;
      mProgram = nullptr;
      if (!pProgram->mParse(Asked(), mProcess.GetExitCode(), mProcess.GetOutput(), answer))
      {
        fprintf(stderr, "iPlug2: %s could not show the dialog (exit status %d)\n", pProgram->mName, mProcess.GetExitCode());
        StartNextBackend();
        return;
      }
      Answered(answer);
    }
  }

  void Answered(const LinuxDialogResult& answer)
  {
    if (mStage == EStage::ConfirmOverwrite)
    {
      if (answer.message == kYES)
      {
        mResult.accepted = true;
        mResult.path.Set(mConfirmRequest.fileName.Get());
      }
      mDone = true;
      return;
    }

    mResult = answer;
    mDone = true;
    if (mRequest.kind != EKind::SaveFile || !answer.accepted)
      return;

    // Like the save panels on macOS and Windows: the name gets the dialog's extension if it has none of them.
    WDL_PtrList<WDL_String> extensions;
    SplitExtensions(mRequest.extensions.Get(), extensions);
    if (extensions.GetSize() && !HasOneOfExtensions(answer.path.Get(), extensions))
    {
      mResult.path.AppendFormatted(256, ".%s", extensions.Get(0)->Get());
      if (PathExists(mResult.path.Get()))
        AskOverwrite();
    }
    extensions.Empty(true);
  }

  void AskOverwrite()
  {
    const char* pName = strrchr(mResult.path.Get(), '/');
    mConfirmRequest = LinuxDialogRequest();
    mConfirmRequest.kind = EKind::Message;
    mConfirmRequest.messageType = kMB_YESNO;
    mConfirmRequest.title.Set(mRequest.title.Get());
    mConfirmRequest.text.SetFormatted(1024, "\xE2\x80\x9C%s\xE2\x80\x9D already exists. Do you want to replace it?", pName ? pName + 1 : mResult.path.Get());
    mConfirmRequest.fileName.Set(mResult.path.Get());
    mConfirmRequest.parentWindow = mRequest.parentWindow;
    mResult = LinuxDialogResult();
    mStage = EStage::ConfirmOverwrite;
    mNextBackend = 0;
    mDone = false;
    StartNextBackend();
  }

  int GetFd() const
  {
    if (mPortal)
      return mPortal->GetFd();
    return mProgram ? mProcess.GetOutputFd() : -1;
  }

  void Dismiss()
  {
    mPortal.reset();
    mProcess.Kill();
    mProgram = nullptr;
    mDone = true;
  }
};

#pragma mark - LinuxDialogs

LinuxDialogs::LinuxDialogs(HostLoop& loop)
: mLoop(loop)
, mAlive(std::make_shared<bool>(true))
{
}

LinuxDialogs::~LinuxDialogs()
{
  DismissAll();
  *mAlive = false;
}

void LinuxDialogs::Show(const LinuxDialogRequest& request, Handler handler)
{
  Job* pJob = new Job();
  pJob->mRequest = request;
  if (!pJob->mRequest.title.GetLength())
    pJob->mRequest.title.Set(DefaultTitle(request.kind));
  pJob->mHandler = std::move(handler);
  GetBackendOrder(pJob->mBackends);
  pJob->StartNextBackend();
  mJobs.Add(pJob);
  UpdateWatches();
}

void LinuxDialogs::DismissAll()
{
  while (mJobs.GetSize())
  {
    std::unique_ptr<Job> job(mJobs.Get(0));
    mJobs.Delete(0);
    if (job->mFdId)
      mLoop.RemoveFd(job->mFdId);
    job->Dismiss();
  }
  UpdateWatches();
}

void LinuxDialogs::UpdateWatches()
{
  if (mJobs.GetSize() && !mTimerId)
    mTimerId = mLoop.AddTimer(kPollIntervalMs, [this]() { Poll(); });
  else if (!mJobs.GetSize() && mTimerId)
  {
    mLoop.RemoveTimer(mTimerId);
    mTimerId = 0;
  }

  for (int i = 0; i < mJobs.GetSize(); i++)
  {
    Job* pJob = mJobs.Get(i);
    const int fd = pJob->mDone || pJob->mFdFailed ? -1 : pJob->GetFd();
    if (fd == pJob->mWatchedFd)
      continue;
    if (pJob->mFdId)
      mLoop.RemoveFd(pJob->mFdId);
    pJob->mFdId = 0;
    pJob->mWatchedFd = fd;
    if (fd < 0)
      continue;
    pJob->mFdId = mLoop.AddFd(fd, HostLoop::kFdRead, [this, pJob](int, int flags) {
      if ((flags & HostLoop::kFdError) && mJobs.Find(pJob) >= 0)
      {
        // Hung up: the timer reads the end of it; watching on would fire forever.
        mLoop.RemoveFd(pJob->mFdId);
        pJob->mFdId = 0;
        pJob->mFdFailed = true;
      }
      Poll();
    });
  }
}

void LinuxDialogs::Poll()
{
  const std::shared_ptr<bool> alive = mAlive;

  for (int i = 0; i < mJobs.GetSize(); i++)
    mJobs.Get(i)->Advance();

  for (int i = 0; i < mJobs.GetSize();)
  {
    if (!mJobs.Get(i)->mDone)
    {
      i++;
      continue;
    }
    std::unique_ptr<Job> job(mJobs.Get(i));
    mJobs.Delete(i);
    if (job->mFdId)
      mLoop.RemoveFd(job->mFdId);
    UpdateWatches();

    const Handler handler = std::move(job->mHandler);
    const LinuxDialogResult result = job->mResult;
    job.reset();
    if (handler)
      handler(result);
    if (!*alive)
      return;
    // The handler may have opened or dismissed dialogs
    i = 0;
  }
  UpdateWatches();
}
