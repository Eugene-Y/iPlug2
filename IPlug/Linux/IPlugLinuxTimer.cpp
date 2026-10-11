/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#include "IPlugTimer.h"

using namespace iplug;

Timer* Timer::Create(ITimerFunction func, uint32_t intervalMs)
{
  return new Timer_impl(func, intervalMs, nullptr);
}

Timer_impl::Timer_impl(ITimerFunction func, uint32_t intervalMs, HostLoop* pLoop)
: mLoop(pLoop)
, mTimerFunc(func)
{
  if (!mLoop)
  {
    mOwnLoop = std::make_unique<HostLoop>("Timer::Create");
    mOwnLoop->SetBackend(MakeFallbackHostLoopBackend(*mOwnLoop));
    mLoop = mOwnLoop.get();
  }

  mId = mLoop->AddTimer(intervalMs, [this]() { mTimerFunc(*this); });
}

Timer_impl::~Timer_impl()
{
  Stop();
}

void Timer_impl::Stop()
{
  if (mId)
    mLoop->RemoveTimer(mId);
  mId = 0;
}
