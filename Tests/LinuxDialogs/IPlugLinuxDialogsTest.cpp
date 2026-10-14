// LinuxDialogs and LinuxBackgroundProcess against fake zenity/kdialog scripts, pumped on this thread by a fake
// host loop. Covers argv (no shell parsing), the answer mapping, the fallback chain, the save extension and its
// overwrite question, dismissal (process group killed, no handler), a handler that destroys the dialogs, the child's
// locale, and a session bus without a portal. Needs no X display.
#include "IGraphicsLinuxDialogs.h"
#include "Linux/IPlugLinuxHostLoop.h"
#include "Linux/IPlugLinuxSubprocess.h"

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <clocale>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace iplug;
using namespace iplug::igraphics;
using EKind = LinuxDialogRequest::EKind;

namespace
{
int gFailures = 0;
int gStaleWatches = 0;

void Check(bool condition, const std::string& label)
{
  if (!condition)
  {
    std::cerr << "FAIL " << label << "\n";
    ++gFailures;
  }
  else
    std::cerr << "ok   " << label << "\n";
}

/** Runs registered timers and readable fds when Pump() is called, on the calling thread, like a host's loop. */
class FakeBackend final : public HostLoopBackend
{
public:
  using HostLoopBackend::HostLoopBackend;
  const char* GetName() const override { return "fake"; }
  bool IsHostDriven() const override { return true; }
  bool Register(HostLoopEntry& entry) override
  {
    entry.mHandle = ++mNextHandle;
    struct stat info {};
    if (entry.mIsFd)
      fstat(entry.mFd, &info);
    mEntries.push_back({ entry.mIsFd, entry.mHandle, entry.mFd, info.st_dev, info.st_ino });
    return true;
  }
  void Unregister(HostLoopEntry& entry) override
  {
    for (size_t i = 0; i < mEntries.size(); i++)
    {
      if (mEntries[i].isFd == entry.mIsFd && mEntries[i].handle == entry.mHandle)
      {
        mEntries.erase(mEntries.begin() + i);
        return;
      }
    }
  }
  void Pump()
  {
    const std::vector<Registration> entries = mEntries; // callbacks add and remove entries
    for (const Registration& r : entries)
    {
      if (!IsRegistered(r))
        continue;
      if (!r.isFd)
      {
        mLoop.Dispatch(false, r.handle);
        continue;
      }
      // Like hosts that key watches by fd number: a watch whose fd was closed (and maybe reused) is a bug.
      struct stat info {};
      if (fstat(r.fd, &info) != 0 || info.st_dev != r.dev || info.st_ino != r.ino)
      {
        gStaleWatches++;
        continue;
      }
      pollfd p { r.fd, POLLIN, 0 };
      if (poll(&p, 1, 0) > 0)
        mLoop.Dispatch(true, r.handle, (p.revents & POLLIN ? HostLoop::kFdRead : 0) | (p.revents & (POLLHUP | POLLERR) ? HostLoop::kFdError : 0));
    }
  }
  size_t GetCount() const { return mEntries.size(); }

private:
  struct Registration;
  bool IsRegistered(const Registration& r) const
  {
    for (const Registration& e : mEntries)
    {
      if (e.isFd == r.isFd && e.handle == r.handle)
        return true;
    }
    return false;
  }

  struct Registration { bool isFd; uintptr_t handle; int fd; dev_t dev; ino_t ino; };
  std::vector<Registration> mEntries;
  uintptr_t mNextHandle = 0;
};

std::string gTempDir;
std::string gBinDir;
std::string gEmptyDir;

void WriteScript(const std::string& name, const std::string& body)
{
  const std::string path = gBinDir + "/" + name;
  std::ofstream(path) << "#!/bin/sh\n" << body;
  chmod(path.c_str(), 0755);
}

void RemoveScript(const std::string& name) { unlink((gBinDir + "/" + name).c_str()); }

std::string ReadFile(const std::string& path)
{
  std::ifstream file(path);
  std::stringstream text;
  text << file.rdbuf();
  return text.str();
}

bool FileExists(const std::string& path)
{
  struct stat info;
  return stat(path.c_str(), &info) == 0;
}

struct Fixture
{
  HostLoop loop { "test" };
  FakeBackend* pBackend = nullptr;
  std::unique_ptr<LinuxDialogs> dialogs;

  Fixture()
  {
    auto backend = std::make_unique<FakeBackend>(loop);
    pBackend = backend.get();
    loop.SetBackend(std::move(backend));
    dialogs = std::make_unique<LinuxDialogs>(loop);
  }
  ~Fixture()
  {
    dialogs.reset();
    loop.SetBackend(nullptr);
  }

  template <class Pred>
  bool PumpUntil(Pred done, int timeoutMs = 5000)
  {
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (!done())
    {
      if (std::chrono::steady_clock::now() > end)
        return false;
      if (pBackend)
        pBackend->Pump();
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return true;
  }

  /** Shows the request and pumps until its handler ran. */
  bool Run(const LinuxDialogRequest& request, LinuxDialogResult& result, int timeoutMs = 5000)
  {
    bool called = false;
    bool calledInsideShow = false;
    bool inShow = true;
    dialogs->Show(request, [&](const LinuxDialogResult& r) {
      calledInsideShow = inShow;
      called = true;
      result = r;
    });
    inShow = false;
    const bool ok = PumpUntil([&] { return called; }, timeoutMs);
    Check(!calledInsideShow, "handler not called from inside Show()");
    return ok;
  }
};

LinuxDialogRequest FileRequest(EKind kind, const char* ext = "fxp")
{
  LinuxDialogRequest request;
  request.kind = kind;
  request.folder.Set((gTempDir + "/папка с пробелами").c_str());
  request.extensions.Set(ext);
  return request;
}

void TestArgvAndAnswer()
{
  // Every argument on its own line, so a shell parse (word splitting, $(), quotes) would show.
  WriteScript("zenity", "for a in \"$@\"; do printf '%s\\n' \"$a\"; done > \"$ARGS_FILE\"\nprintf '%s\\n' \"$ANSWER\"\n");
  setenv("ANSWER", (gTempDir + "/a b/ж.fxp").c_str(), 1);
  const std::string argsFile = gTempDir + "/args.txt";
  setenv("ARGS_FILE", argsFile.c_str(), 1);

  Fixture f;
  LinuxDialogRequest request = FileRequest(EKind::SaveFile);
  request.fileName.Set("preset");
  request.title.Set("T $(touch /tmp/iplug-pwned) \"q\" 'x' `y`");
  request.parentWindow = 0x4a0003;
  LinuxDialogResult result;
  Check(f.Run(request, result), "save: handler called");
  Check(result.accepted && std::string(result.path.Get()) == gTempDir + "/a b/ж.fxp", "save: path with spaces and Cyrillic");

  const std::string args = ReadFile(argsFile);
  Check(args.find("--title=T $(touch /tmp/iplug-pwned) \"q\" 'x' `y`\n") != std::string::npos, "title passed verbatim");
  Check(!FileExists("/tmp/iplug-pwned"), "no shell expansion");
  Check(args.find("--filename=" + gTempDir + "/папка с пробелами/preset.fxp\n") != std::string::npos, "proposed name gets the extension");
  Check(args.find("--save\n") != std::string::npos && args.find("--file-filter=*.fxp | *.[fF][xX][pP]\n") != std::string::npos, "save mode and case-insensitive filter");
  Check(args.find("--attach=4849667\n") != std::string::npos, "parent window id in decimal");
  Check(f.pBackend->GetCount() == 0, "no timer or fd left registered");

  request = FileRequest(EKind::OpenFile, ".bmp .JPG");
  Check(f.Run(request, result), "open: handler called");
  const std::string openArgs = ReadFile(argsFile);
  Check(openArgs.find("--filename=" + gTempDir + "/папка с пробелами/.iplug2-start-folder\n") != std::string::npos, "open starts inside the folder");
  Check(openArgs.find("--file-filter=*.bmp *.JPG | *.[bB][mM][pP] *.[jJ][pP][gG]\n") != std::string::npos, "two extensions, leading dots dropped");

  setenv("ANSWER", (gTempDir + "/a b/папка").c_str(), 1);
  request = FileRequest(EKind::Directory, "");
  Check(f.Run(request, result) && std::string(result.path.Get()) == gTempDir + "/a b/папка/", "directory gets a trailing slash");

  setenv("ANSWER", "", 1);
  WriteScript("zenity", "exit 1\n");
  Check(f.Run(FileRequest(EKind::OpenFile), result) && !result.accepted && !result.path.GetLength(), "cancel: empty answer");
}

void TestOverwriteQuestion()
{
  const std::string existing = gTempDir + "/existing";
  std::ofstream(existing + ".fxp") << "x";
  // The file dialog answers a name without the extension; the question gets $QUESTION_EXIT.
  WriteScript("zenity", "case \"$*\" in *--question*) echo \"$*\" > \"$ARGS_FILE.q\"; exit $QUESTION_EXIT;; esac\nprintf '%s\\n' \"$ANSWER\"\n");
  setenv("ANSWER", existing.c_str(), 1);
  setenv("ARGS_FILE", (gTempDir + "/args.txt").c_str(), 1);

  Fixture f;
  LinuxDialogResult result;
  setenv("QUESTION_EXIT", "0", 1);
  Check(f.Run(FileRequest(EKind::SaveFile), result) && result.accepted && std::string(result.path.Get()) == existing + ".fxp", "overwrite: Yes keeps the path with the added extension");
  Check(ReadFile(gTempDir + "/args.txt.q").find("existing.fxp") != std::string::npos, "overwrite: question names the file");
  setenv("QUESTION_EXIT", "1", 1);
  Check(f.Run(FileRequest(EKind::SaveFile), result) && !result.accepted, "overwrite: No cancels");

  setenv("ANSWER", (gTempDir + "/fresh").c_str(), 1);
  unlink((gTempDir + "/args.txt.q").c_str());
  Check(f.Run(FileRequest(EKind::SaveFile), result) && std::string(result.path.Get()) == gTempDir + "/fresh.fxp", "new name: extension added, no question");
  Check(!FileExists(gTempDir + "/args.txt.q"), "new name: no question asked");
}

void TestMessagesAndColour()
{
  WriteScript("zenity", "printf '%s' \"$ANSWER\"; exit $CODE\n");
  Fixture f;
  LinuxDialogResult result;
  struct Case { EMsgBoxType type; const char* answer; const char* code; EMsgBoxResult expected; } cases[] = {
    { kMB_YESNOCANCEL, "Yes", "1", kYES }, { kMB_YESNOCANCEL, "No", "1", kNO }, { kMB_YESNOCANCEL, "", "0", kCANCEL },
    { kMB_YESNO, "", "0", kYES }, { kMB_YESNO, "", "1", kNO }, { kMB_OKCANCEL, "", "1", kCANCEL },
    { kMB_RETRYCANCEL, "", "0", kRETRY }, { kMB_OK, "", "1", kOK },
  };
  for (const Case& c : cases)
  {
    setenv("ANSWER", c.answer, 1);
    setenv("CODE", c.code, 1);
    LinuxDialogRequest request;
    request.kind = EKind::Message;
    request.messageType = c.type;
    request.text.Set("-starts with a dash <b>&");
    f.Run(request, result);
    Check(result.message == c.expected, std::string("message type ") + std::to_string(c.type) + " '" + c.answer + "' exit " + c.code);
  }

  setenv("ANSWER", "rgba(1,2,3,0.5)", 1);
  setenv("CODE", "0", 1);
  LinuxDialogRequest request;
  request.kind = EKind::Color;
  request.color = IColor(255, 9, 9, 9);
  f.Run(request, result);
  Check(result.accepted && result.color == IColor(128, 1, 2, 3), "colour rgba");
  setenv("ANSWER", "rgb(4,5,6)", 1);
  f.Run(request, result);
  Check(result.accepted && result.color == IColor(255, 4, 5, 6), "colour rgb keeps the alpha");
  setenv("CODE", "1", 1);
  f.Run(request, result);
  Check(!result.accepted, "colour cancelled");
}

void TestFallbackChain()
{
  Fixture f;
  LinuxDialogResult result;
  setenv("ANSWER", "/from/kdialog.fxp", 1);
  WriteScript("kdialog", "printf '%s\\n' \"$ANSWER\"\n");

  RemoveScript("zenity");
  Check(f.Run(FileRequest(EKind::OpenFile), result) && std::string(result.path.Get()) == "/from/kdialog.fxp", "no zenity: kdialog");
  WriteScript("zenity", "exit 255\n");
  Check(f.Run(FileRequest(EKind::OpenFile), result) && std::string(result.path.Get()) == "/from/kdialog.fxp", "zenity fails (255): kdialog");
  WriteScript("zenity", "exec /nonexistent/program\n");
  Check(f.Run(FileRequest(EKind::OpenFile), result) && std::string(result.path.Get()) == "/from/kdialog.fxp", "zenity cannot exec (127): kdialog");
  RemoveScript("kdialog");
  RemoveScript("zenity");

  const char* pPath = getenv("PATH");
  const std::string path = pPath;
  setenv("PATH", gEmptyDir.c_str(), 1);
  Check(f.Run(FileRequest(EKind::OpenFile), result) && !result.accepted, "nothing available: cancelled, asynchronously");
  LinuxDialogRequest request;
  request.kind = EKind::Message;
  request.messageType = kMB_OK;
  Check(f.Run(request, result) && result.message == kOK, "nothing available: an OK box reads as OK");
  setenv("PATH", path.c_str(), 1);
}

void TestExtensionsAndSlowExit()
{
  WriteScript("zenity", "printf '%s\\n' \"$ANSWER\"\n");
  Fixture f;
  LinuxDialogResult result;
  setenv("ANSWER", "/x/a.tar.gz", 1);
  Check(f.Run(FileRequest(EKind::SaveFile, "tar.gz"), result) && std::string(result.path.Get()) == "/x/a.tar.gz", "two-part extension kept");
  setenv("ANSWER", "/x/a.TAR.GZ", 1);
  Check(f.Run(FileRequest(EKind::SaveFile, "tar.gz"), result) && std::string(result.path.Get()) == "/x/a.TAR.GZ", "extension compared without case");
  setenv("ANSWER", "/x.d/a", 1);
  Check(f.Run(FileRequest(EKind::SaveFile, "fxp"), result) && std::string(result.path.Get()) == "/x.d/a.fxp", "a dot in the folder is no extension");

  // Something the program started keeps the pipe open: the status line still ends the dialog.
  WriteScript("zenity", "/bin/sleep 5 &\nprintf '/quick\\n'\n");
  const auto start = std::chrono::steady_clock::now();
  Check(f.Run(FileRequest(EKind::OpenFile), result) && std::string(result.path.Get()) == "/quick", "answer while a child holds the pipe");
  Check(std::chrono::steady_clock::now() - start < std::chrono::seconds(2), "no wait for the pipe's end");
}

void TestColourWithDecimalComma()
{
  if (!setlocale(LC_NUMERIC, "de_DE.UTF-8"))
  {
    std::cerr << "skip decimal comma: no de_DE.UTF-8 locale (locale-gen de_DE.UTF-8)\n";
    return;
  }
  WriteScript("zenity", "for a in \"$@\"; do printf '%s\\n' \"$a\"; done > \"$ARGS_FILE\"\nprintf 'rgba(1,2,3,0.5)\\n'\n");
  Fixture f;
  LinuxDialogResult result;
  LinuxDialogRequest request;
  request.kind = EKind::Color;
  request.color = IColor(255, 10, 200, 30);
  f.Run(request, result);
  Check(ReadFile(getenv("ARGS_FILE")).find("--color=rgba(10,200,30,1.000)\n") != std::string::npos, "decimal comma locale: colour sent with a point");
  Check(result.accepted && result.color == IColor(128, 1, 2, 3), "decimal comma locale: alpha read with a point");
  setlocale(LC_NUMERIC, "C");
}

void TestDismiss()
{
  const std::string pidFile = gTempDir + "/pid";
  WriteScript("zenity", "echo $$ > \"$PID_FILE\"\n/bin/sleep 30\necho /never\n");
  setenv("PID_FILE", pidFile.c_str(), 1);

  for (int destroy = 0; destroy < 2; destroy++)
  {
    unlink(pidFile.c_str());
    Fixture f;
    bool called = false;
    f.dialogs->Show(FileRequest(EKind::OpenFile), [&](const LinuxDialogResult&) { called = true; });
    f.PumpUntil([&] { return ReadFile(pidFile).size() > 0; });
    const pid_t pid = atoi(ReadFile(pidFile).c_str());
    if (destroy)
      f.dialogs.reset();
    else
      f.dialogs->DismissAll();
    // The program was orphaned to init (or a subreaper), which reaps it once killed.
    bool gone = false;
    for (int i = 0; i < 200 && !gone; i++)
    {
      gone = kill(pid, 0) != 0 || ReadFile("/proc/" + std::to_string(pid) + "/stat").find(") Z") != std::string::npos;
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    Check(pid > 0 && gone, destroy ? "destroyed: program killed" : "DismissAll: program killed");
    for (int i = 0; i < 20; i++)
      f.pBackend->Pump();
    Check(!called, destroy ? "destroyed: no handler" : "DismissAll: no handler");
    Check(f.pBackend->GetCount() == 0, "dismissed: nothing left registered");
  }
}

void TestHandlerDestroysDialogs()
{
  WriteScript("zenity", "printf '/x\\n'\n");
  Fixture f;
  int calls = 0;
  for (int i = 0; i < 3; i++)
    f.dialogs->Show(FileRequest(EKind::OpenFile), [&](const LinuxDialogResult&) { calls++; f.dialogs.reset(); });
  f.PumpUntil([&] { return calls > 0; });
  for (int i = 0; i < 20; i++)
    f.pBackend->Pump();
  Check(calls == 1, "a handler that destroys the dialogs: the others never run");
  Check(f.pBackend->GetCount() == 0, "handler destroyed the dialogs: nothing left registered");
}

void TestChildLocale()
{
  const std::string out = gTempDir + "/locale.txt";
  const char* argv[] = { "/bin/sh", "-c", "printf '%s|%s|%s' \"${LC_ALL-unset}\" \"${LC_CTYPE-unset}\" \"${LANG-unset}\"", nullptr };
  auto run = [&]() {
    LinuxBackgroundProcess process;
    process.Start(argv);
    for (int i = 0; i < 500 && !process.Pump(); i++)
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    return std::string(process.GetOutput()) + " exit " + std::to_string(process.GetExitCode());
  };
  unsetenv("LC_ALL"); unsetenv("LC_CTYPE"); setenv("LANG", "C", 1);
  Check(run() == "unset|C.UTF-8|C exit 0", "LANG=C: the child gets a UTF-8 LC_CTYPE");
  setenv("LC_ALL", "C", 1);
  Check(run() == "unset|C.UTF-8|C exit 0", "LC_ALL=C: moved to LANG, UTF-8 LC_CTYPE");
  unsetenv("LC_ALL"); setenv("LANG", "de_DE.UTF-8", 1);
  Check(run() == "unset|unset|de_DE.UTF-8 exit 0", "a UTF-8 locale stays as it is");
  setenv("LANG", "C", 1);

  const char* status[] = { "/bin/sh", "-c", "printf 'no newline'; exit 7", nullptr };
  LinuxBackgroundProcess process;
  process.Start(status);
  for (int i = 0; i < 500 && !process.Pump(); i++)
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  Check(std::string(process.GetOutput()) == "no newline" && process.GetExitCode() == 7, "output without newline, exit status");
  const char* missing[] = { "/nonexistent/program", nullptr };
  process.Start(missing);
  for (int i = 0; i < 500 && !process.Pump(); i++)
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  Check(process.GetExitCode() == 127, "a missing program: 127");
}

void TestPortalUnavailable()
{
  WriteScript("zenity", "printf '/via/zenity\\n'\n");
  unsetenv("IPLUG_LINUX_DIALOGS");
  Fixture f;
  LinuxDialogResult result;
  const char* pBus = getenv("DBUS_SESSION_BUS_ADDRESS");
  const std::string bus = pBus ? pBus : "";

  setenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/nonexistent/bus", 1);
  Check(f.Run(FileRequest(EKind::OpenFile), result) && std::string(result.path.Get()) == "/via/zenity", "no bus: zenity");
  if (bus.size())
  {
    // A session bus without xdg-desktop-portal (dbus-run-session): ServiceUnknown, then zenity
    setenv("DBUS_SESSION_BUS_ADDRESS", bus.c_str(), 1);
    const auto start = std::chrono::steady_clock::now();
    Check(f.Run(FileRequest(EKind::Directory), result) && std::string(result.path.Get()) == "/via/zenity/", "bus without a portal: zenity");
    Check(std::chrono::steady_clock::now() - start < std::chrono::seconds(3), "bus without a portal: no timeout wait");
  }
  else
    std::cerr << "skip bus without a portal: run under dbus-run-session\n";
}
} // namespace

int main()
{
  char dir[] = "/tmp/iplug-dialogs-XXXXXX";
  gTempDir = mkdtemp(dir);
  gBinDir = gTempDir + "/bin";
  gEmptyDir = gTempDir + "/empty";
  mkdir(gBinDir.c_str(), 0755);
  mkdir(gEmptyDir.c_str(), 0755);
  unlink("/tmp/iplug-pwned");
  // Only the fake programs: a real zenity or kdialog must never be reached. The scripts use shell builtins only.
  setenv("PATH", gBinDir.c_str(), 1);
  setenv("IPLUG_LINUX_DIALOGS", "zenity,kdialog", 1);
  WriteScript("kdialog", "exit 255\n");

  TestArgvAndAnswer();
  TestOverwriteQuestion();
  TestMessagesAndColour();
  TestFallbackChain();
  TestExtensionsAndSlowExit();
  TestColourWithDecimalComma();
  TestDismiss();
  TestHandlerDestroysDialogs();
  TestChildLocale();
  TestPortalUnavailable();

  Check(gStaleWatches == 0, "no fd watch outlived its fd (" + std::to_string(gStaleWatches) + " stale)");
  std::cerr << (gFailures ? "FAILED: " : "PASSED") << (gFailures ? std::to_string(gFailures) : "") << "\n";
  return gFailures ? 1 : 0;
}
