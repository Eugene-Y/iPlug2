/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#pragma once

/** @file
 * @brief A helper program (zenity, kdialog, ...) run in the background, its standard output read without blocking.
 */

#include "IPlugPlatform.h"
#include "wdlstring.h"

BEGIN_IPLUG_NAMESPACE

/** Runs one program in the background and collects what it writes to stdout, without ever blocking the caller.
 *
 * The program is detached from the host: a tiny `sh` that exits at once starts it, so init (or the session's
 * subreaper) reaps it, and its exit status comes back as the last line of the pipe. That keeps two things out of
 * the host's way: no zombie is left when the editor closes while the program runs, and a host whose SIGCHLD
 * handler reaps every child (or ignores SIGCHLD) cannot swallow the status. The arguments reach the program as
 * the shell's positional parameters, so the shell never parses them. Arguments and output are UTF-8: a host
 * running in a non-UTF-8 locale passes the program a UTF-8 LC_CTYPE.
 *
 * Not thread-safe; one owner thread (the host main thread for dialogs). */
class LinuxBackgroundProcess
{
public:
  LinuxBackgroundProcess() = default;
  ~LinuxBackgroundProcess() { Kill(); }
  LinuxBackgroundProcess(const LinuxBackgroundProcess&) = delete;
  LinuxBackgroundProcess& operator=(const LinuxBackgroundProcess&) = delete;

  /** @param argv the program (a path or a name looked up in PATH) and its arguments, nullptr-terminated
   * @return false if it could not be started */
  bool Start(const char* const* argv);

  /** The read end of the stdout pipe, non-blocking; open until CloseOutput(), Kill() or the next Start() */
  int GetOutputFd() const { return mFd; }

  /** Reads whatever the program has written so far. @return true once it has finished */
  bool Pump();

  /** Closes the pipe once finished. Separate from Pump() so that whoever watches the fd can stop watching it first. */
  void CloseOutput();

  bool IsFinished() const { return mFinished; }

  /** The program's exit status once finished: 0..255, 128 + signal if it was killed, 127 if it could not be run,
   * -1 if the status line never came (the helper shell itself died) */
  int GetExitCode() const { return mExitCode; }

  /** Everything the program wrote to stdout, with one trailing newline removed */
  const char* GetOutput() const { return mOutput.Get(); }

  /** Terminates the program (and anything it started) unless it has finished, and closes the pipe */
  void Kill();

  /** @return true if name is an absolute executable path, or an executable found in PATH */
  static bool IsExecutableAvailable(const char* name);

private:
  void Finish();
  bool HasCompleteExitLine() const;

  int mFd = -1;
  int mProcessGroup = 0;
  bool mFinished = false;
  int mExitCode = -1;
  WDL_String mOutput;
};

END_IPLUG_NAMESPACE
