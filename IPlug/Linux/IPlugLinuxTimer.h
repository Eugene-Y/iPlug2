/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#pragma once

BEGIN_IPLUG_NAMESPACE

/** STUB: never fires. OnIdle, ISender draining and other timer work do not run on Linux yet.
 * The real timer must run its callback on the host main thread (CLAP timer-support, VST3 IRunLoop,
 * the APP's GLib loop), not on a thread of its own, because OnIdle touches the UI and calls SetLatency. */
class Timer_impl : public Timer
{
public:
  Timer_impl(ITimerFunction func, uint32_t intervalMs);
  ~Timer_impl() = default;
  void Stop() override {}
};

END_IPLUG_NAMESPACE
