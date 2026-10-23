/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#pragma once

/** @file
 * @brief Keeps a VST3 plug-in's host traffic on the host's UI thread while the fallback thread runs OnIdle.
 *
 * A host that offers no Linux::IRunLoop (several do so only to an open editor) leaves the timers, and with them
 * OnIdle, on the HostLoop's fallback thread. VST3 has no way to ask the host for a call on its UI thread, so:
 * - each host call on the UI thread holds the loop's host-call gate (a VST3HostThread::Call), and the fallback thread
 *   never runs a callback meanwhile, so OnIdle does not overlap the host's calls, as on mac/win;
 * - the notifications OnIdle sends to the host on the fallback thread (restartComponent, parameter edits) are kept
 *   here and sent at the start of the next host call that may send them (kSendDeferred).
 * Calls on other threads (the audio thread, plug-in threads) are left as they are on every platform.
 */

#include <atomic>

#include <pthread.h>

#include "pluginterfaces/vst/ivsteditcontroller.h"

#include "IPlugLinuxHostLoop.h"
#include "heapbuf.h"

BEGIN_IPLUG_NAMESPACE

class VST3HostThread
{
public:
  /** kSendDeferred: the host call may receive the notifications that wait. Not for the processing-state calls
   * (setActive, setupProcessing…): a restartComponent inside one livelocks some hosts. */
  enum EDeferred { kKeepDeferred, kSendDeferred };

  /** Lives for one host call. Off the UI thread it does nothing: a host that calls on its audio thread against the
   * rules must never wait for OnIdle. */
  class Call
  {
  public:
    Call(VST3HostThread& hostThread, EDeferred deferred)
    {
      if (!hostThread.IsUIThread())
        return;
      mHostThread = &hostThread;
      hostThread.mLoop.GetHostCallGate().lock();
      // Only the outermost call: the host makes calls of its own while it handles a restart.
      if (hostThread.mCallDepth++ == 0 && deferred == kSendDeferred)
        hostThread.SendDeferred();
    }

    ~Call()
    {
      if (!mHostThread)
        return;
      mHostThread->mCallDepth--;
      mHostThread->mLoop.GetHostCallGate().unlock();
    }

    Call(const Call&) = delete;
    Call& operator=(const Call&) = delete;

  private:
    VST3HostThread* mHostThread = nullptr;
  };

  /** @param handler the controller's component handler, read when the notifications are sent */
  VST3HostThread(HostLoop& loop, const Steinberg::IPtr<Steinberg::Vst::IComponentHandler>& handler, int nParams)
  : mLoop(loop), mHandler(handler)
  {
    mEditValues.Resize(nParams);
    ClearDeferred();
  }

  /** In initialize(), which VST3 calls on the host's UI thread. */
  void Initialize()
  {
    mUIThread = pthread_self();
    mHasUIThread.store(true, std::memory_order_release);
  }

  /** In terminate(), once the fallback thread has stopped: the host is going away, what waits is dropped. */
  void Terminate()
  {
    mHasUIThread.store(false, std::memory_order_release);
    ClearDeferred();
  }

  /** @return true if the restart was deferred, false if the caller must send it now */
  bool DeferRestart(Steinberg::int32 flags)
  {
    if (!mLoop.IsFallbackThread())
      return false;
    mRestartFlags |= flags;
    return true;
  }

  /** performEdit. @return true if deferred */
  bool DeferEdit(Steinberg::Vst::ParamID tag, Steinberg::Vst::ParamValue value)
  {
    if (!mLoop.IsFallbackThread())
      return false;
    if (tag < (Steinberg::Vst::ParamID) mEditValues.GetSize())
      mEditValues.Get()[tag] = value;
    return true;
  }

  /** beginEdit, endEdit and group edits. Dropped on the fallback thread: each deferred edit is sent as a whole
   * gesture. @return true if dropped */
  bool DeferGesture() const { return mLoop.IsFallbackThread(); }

private:
  static constexpr double kNoEdit = -1.0;

  bool IsUIThread() const
  {
    return mHasUIThread.load(std::memory_order_acquire) && pthread_equal(pthread_self(), mUIThread);
  }

  void ClearDeferred()
  {
    mRestartFlags = 0;
    for (int i = 0; i < mEditValues.GetSize(); i++)
      mEditValues.Get()[i] = kNoEdit;
  }

  void SendDeferred()
  {
    Steinberg::Vst::IComponentHandler* pHandler = mHandler;
    if (!pHandler)
      return;

    int nEdits = 0;
    for (int i = 0; i < mEditValues.GetSize(); i++)
      nEdits += mEditValues.Get()[i] != kNoEdit;

    if (nEdits)
    {
      Steinberg::FUnknownPtr<Steinberg::Vst::IComponentHandler2> handler2(pHandler);
      const bool group = handler2 && nEdits > 1;
      if (group)
        handler2->startGroupEdit();
      for (int i = 0; i < mEditValues.GetSize(); i++)
      {
        const double value = mEditValues.Get()[i];
        if (value == kNoEdit)
          continue;
        mEditValues.Get()[i] = kNoEdit;
        pHandler->beginEdit(i);
        pHandler->performEdit(i, value);
        pHandler->endEdit(i);
      }
      if (group)
        handler2->finishGroupEdit();
    }

    if (const Steinberg::int32 flags = mRestartFlags)
    {
      mRestartFlags = 0;
      pHandler->restartComponent(flags);
    }
  }

  HostLoop& mLoop;
  const Steinberg::IPtr<Steinberg::Vst::IComponentHandler>& mHandler;
  pthread_t mUIThread {};
  std::atomic<bool> mHasUIThread { false }; // any thread asks IsUIThread()
  int mCallDepth = 0;
  // Written on the fallback thread and read on the UI thread, each while holding the host-call gate.
  Steinberg::int32 mRestartFlags = 0;
  WDL_TypedBuf<double> mEditValues;
};

END_IPLUG_NAMESPACE
