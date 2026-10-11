/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#pragma once

#include <memory>

#include "IPlugLinuxHostLoop.h"

BEGIN_IPLUG_NAMESPACE

/** A timer on a plug-in instance's HostLoop, so the callback runs on the host main thread.
 * Timer::Create() has no instance to take a loop from: such a timer gets a loop of its own with the
 * fallback thread. IPlugAPIBase::CreateTimer() passes the instance's loop. */
class Timer_impl : public Timer
{
public:
  Timer_impl(ITimerFunction func, uint32_t intervalMs, HostLoop* pLoop);
  ~Timer_impl();
  void Stop() override;

private:
  std::unique_ptr<HostLoop> mOwnLoop;
  HostLoop* mLoop;
  ITimerFunction mTimerFunc;
  int mId = 0;
};

END_IPLUG_NAMESPACE
