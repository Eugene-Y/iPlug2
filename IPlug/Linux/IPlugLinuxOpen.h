/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#pragma once

#include <cerrno>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "IPlugPlatform.h"

extern char** environ;

BEGIN_IPLUG_NAMESPACE

/** Opens a file, folder or URL with the desktop's default handler (xdg-open).
 * xdg-open can block until the launched app exits, so `sh -c '... &'` backgrounds it and exits at once:
 * we only reap that shell, leaving no zombie and nothing of ours to outlive a plug-in unload.
 * The target is passed as $1, so the shell never parses it. */
inline bool LinuxOpenWithDefaultApp(const char* pathOrURL)
{
  if (!pathOrURL || !pathOrURL[0])
    return false;

  char shell[] = "/bin/sh";
  char dashC[] = "-c";
  char script[] = "xdg-open \"$1\" >/dev/null 2>&1 </dev/null &";
  char scriptName[] = "sh";
  char* argv[] = { shell, dashC, script, scriptName, const_cast<char*>(pathOrURL), nullptr };

  pid_t pid = 0;
  if (posix_spawn(&pid, shell, nullptr, nullptr, argv, environ) != 0)
    return false;

  int status = 0;
  while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
  return true;
}

END_IPLUG_NAMESPACE
