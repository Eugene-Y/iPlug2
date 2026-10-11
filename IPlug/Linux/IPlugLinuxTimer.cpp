/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#include <atomic>
#include <cstdio>

#include "IPlugTimer.h"

using namespace iplug;

Timer* Timer::Create(ITimerFunction func, uint32_t intervalMs)
{
  return new Timer_impl(func, intervalMs);
}

Timer_impl::Timer_impl(ITimerFunction func, uint32_t intervalMs)
{
  static std::atomic<bool> sWarned { false };
  if (!sWarned.exchange(true))
    fprintf(stderr, "iPlug2: the Linux IPlugTimer is a stub, timers never fire (no OnIdle)\n");
}
