/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#pragma once

/** @file
 * @brief HostLoop backend on SWELL timers, for a module that lives in a SWELL application (a REAPER extension).
 * Include after swell.h, from such a module only.
 *
 * SWELL runs its timers from the application's message loop, on its main thread. It cannot watch fds: Register()
 * refuses them, and their owners poll from a timer instead.
 */

#include "IPlugLinuxHostLoop.h"
#include "ptrlist.h"

BEGIN_IPLUG_NAMESPACE

class SWELLHostLoopBackend final : public HostLoopBackend
{
public:
  explicit SWELLHostLoopBackend(HostLoop& loop)
  : HostLoopBackend(loop)
  {
    Backends().Add(this);
  }

  ~SWELLHostLoopBackend()
  {
    Backends().DeletePtr(this);
  }

  const char* GetName() const override { return "swell-timers"; }
  bool IsHostDriven() const override { return true; }

  bool Register(HostLoopEntry& entry) override
  {
    if (entry.mIsFd)
      return false;

    const UINT_PTR timerId = SetTimer(nullptr, 0, entry.mIntervalMs, OnTimer);
    if (!timerId)
      return false;
    entry.mHandle = (uintptr_t) timerId;
    return true;
  }

  void Unregister(HostLoopEntry& entry) override
  {
    KillTimer(nullptr, (UINT_PTR) entry.mHandle);
  }

private:
  // A timer without a window passes no context, only its id. The ids are unique among the live SWELL timers, and a
  // loop ignores ids it does not hold, so every loop may be offered every id.
  static void CALLBACK OnTimer(HWND, UINT, UINT_PTR timerId, DWORD)
  {
    WDL_PtrList<SWELLHostLoopBackend>& backends = Backends();
    for (int i = 0; i < backends.GetSize(); i++)
      backends.Get(i)->mLoop.Dispatch(false, (uintptr_t) timerId);
  }

  static WDL_PtrList<SWELLHostLoopBackend>& Backends()
  {
    // Never destroyed: a backend can outlive any static of this module (a REAPER extension's instance is destroyed
    // at exit, after statics constructed later than it).
    static auto* sBackends = new WDL_PtrList<SWELLHostLoopBackend>;
    return *sBackends;
  }
};

END_IPLUG_NAMESPACE
