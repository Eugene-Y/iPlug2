/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#include "IPlugLinuxSubprocess.h"

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "heapbuf.h"

extern char** environ;

using namespace iplug;

namespace
{
constexpr const char* kExitMarker = "\nIPLUG_EXIT=";
constexpr int kMaxOutputBytes = 1 << 16;

// Run in the background so this shell exits at once and the program is reparented away from the host; the
// subshell waits for the program and reports its status on the pipe, after anything the program printed.
constexpr const char* kLauncherScript = "{ \"$@\"; printf '\\nIPLUG_EXIT=%d\\n' \"$?\"; } &";

const char* FindLast(const char* haystack, const char* needle)
{
  const char* last = nullptr;
  for (const char* p = haystack; (p = strstr(p, needle)); p++)
    last = p;
  return last;
}
} // namespace

bool LinuxBackgroundProcess::Start(const char* const* argv)
{
  Kill();
  mOutput.Set("");
  mFinished = false;
  mExitCode = -1;

  if (!argv || !argv[0])
    return false;

  int fds[2];
  if (pipe2(fds, O_CLOEXEC) != 0)
    return false;

  WDL_TypedBuf<char*> shellArgv;
  char shell[] = "/bin/sh";
  char dashC[] = "-c";
  char shellName[] = "sh";
  shellArgv.Add(shell);
  shellArgv.Add(dashC);
  shellArgv.Add(const_cast<char*>(kLauncherScript));
  shellArgv.Add(shellName);
  for (const char* const* pArg = argv; *pArg; pArg++)
    shellArgv.Add(const_cast<char*>(*pArg));
  shellArgv.Add(nullptr);

  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
  posix_spawn_file_actions_adddup2(&actions, fds[1], STDOUT_FILENO);
  // GTK and deprecation warnings of the helper would land in the host's log
  posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
#if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 34))
  // The host's own descriptors without O_CLOEXEC (audio devices, sockets) must not stay open in the helper.
  posix_spawn_file_actions_addclosefrom_np(&actions, STDERR_FILENO + 1);
#endif

  // Its own process group, so Kill() reaches the program behind the launcher shell. Signal state reset: a host
  // may block signals on its main thread or ignore SIGCHLD, which would break the subshell's wait.
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
  const int spawnError = posix_spawn(&pid, shell, &actions, &attributes, shellArgv.Get(), environ);
  posix_spawn_file_actions_destroy(&actions);
  posix_spawnattr_destroy(&attributes);
  close(fds[1]);

  if (spawnError != 0)
  {
    close(fds[0]);
    return false;
  }

  // The launcher only forks the background subshell, then exits.
  int status = 0;
  while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}

  fcntl(fds[0], F_SETFL, fcntl(fds[0], F_GETFL) | O_NONBLOCK);
  mFd = fds[0];
  mProcessGroup = pid;
  return true;
}

bool LinuxBackgroundProcess::Pump()
{
  if (mFd < 0)
    return mFinished;

  char buffer[4096];
  for (;;)
  {
    const ssize_t count = read(mFd, buffer, sizeof(buffer));
    if (count > 0)
    {
      if (mOutput.GetLength() < kMaxOutputBytes)
        mOutput.Append(buffer, static_cast<int>(count));
      continue;
    }
    if (count < 0 && errno == EINTR)
      continue;
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
      return false;
    Finish();
    return true;
  }
}

void LinuxBackgroundProcess::Finish()
{
  close(mFd);
  mFd = -1;
  mFinished = true;

  if (const char* pMarker = FindLast(mOutput.Get(), kExitMarker))
  {
    mExitCode = atoi(pMarker + strlen(kExitMarker));
    mOutput.SetLen(static_cast<int>(pMarker - mOutput.Get()));
    const int length = mOutput.GetLength();
    if (length > 0 && mOutput.Get()[length - 1] == '\n')
      mOutput.SetLen(length - 1);
  }
}

void LinuxBackgroundProcess::Kill()
{
  if (mFd < 0)
    return;
  // Still reading means the subshell still holds the pipe, so the group (and its id) still exists.
  kill(-mProcessGroup, SIGTERM);
  close(mFd);
  mFd = -1;
  mFinished = true;
}

bool LinuxBackgroundProcess::IsExecutableAvailable(const char* name)
{
  if (!name || !name[0])
    return false;
  if (strchr(name, '/'))
    return access(name, X_OK) == 0;

  const char* pPath = getenv("PATH");
  if (!pPath || !pPath[0])
    pPath = "/usr/local/bin:/usr/bin:/bin";

  WDL_String candidate;
  while (*pPath)
  {
    const char* pEnd = strchr(pPath, ':');
    const int length = pEnd ? static_cast<int>(pEnd - pPath) : static_cast<int>(strlen(pPath));
    candidate.Set(length ? pPath : ".", length ? length : 1);
    candidate.Append("/");
    candidate.Append(name);
    if (access(candidate.Get(), X_OK) == 0)
      return true;
    pPath += length;
    if (*pPath == ':')
      pPath++;
  }
  return false;
}
