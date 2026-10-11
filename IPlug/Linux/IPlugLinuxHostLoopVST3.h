/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#pragma once

/** @file
 * @brief HostLoop backend on VST3 Linux::IRunLoop, and the choice of which IRunLoop an instance uses.
 *
 * A host can hand out an IRunLoop in three places: the factory host context (IPluginFactory3::setHostContext,
 * before any instance exists), the context passed to initialize(), and the IPlugFrame of an attached view.
 * Only the first two cover the closed editor. The first one found in that order is used; without any, the
 * fallback thread runs the timers until a view attaches, and again after it is removed.
 */

#include <atomic>
#include <cstdio>

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/gui/iplugview.h"

#include "IPlugLinuxHostLoop.h"

BEGIN_IPLUG_NAMESPACE

/** Registered with the host for one entry. Refcounted, because a host may still hold it, and even call it,
 * after unregistering; Detach() makes such late calls no-ops. */
class VST3RunLoopHandler final : public Steinberg::Linux::ITimerHandler
                               , public Steinberg::Linux::IEventHandler
{
public:
  explicit VST3RunLoopHandler(HostLoop& loop) : mLoop(&loop) {}

  void Detach() { mLoop = nullptr; }

  void PLUGIN_API onTimer() override { Dispatch(false, 0); }
  void PLUGIN_API onFDIsSet(Steinberg::Linux::FileDescriptor) override { Dispatch(true, HostLoop::kFdRead); }

  Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid, void** obj) override
  {
    if (!obj)
      return Steinberg::kInvalidArgument;

    if (Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::FUnknown::iid)
        || Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::Linux::ITimerHandler::iid))
      *obj = static_cast<Steinberg::Linux::ITimerHandler*>(this);
    else if (Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::Linux::IEventHandler::iid))
      *obj = static_cast<Steinberg::Linux::IEventHandler*>(this);
    else
    {
      *obj = nullptr;
      return Steinberg::kNoInterface;
    }
    addRef();
    return Steinberg::kResultOk;
  }

  Steinberg::uint32 PLUGIN_API addRef() override { return ++mRefCount; }

  Steinberg::uint32 PLUGIN_API release() override
  {
    const Steinberg::uint32 remaining = --mRefCount;
    if (!remaining)
      delete this;
    return remaining;
  }

private:
  void Dispatch(bool isFd, int flags)
  {
    if (!mLoop)
      return;
    // The callback may remove its own entry, which releases the reference the registration held.
    addRef();
    mLoop->Dispatch(isFd, (uintptr_t) this, flags);
    release();
  }

  std::atomic<Steinberg::uint32> mRefCount { 1 };
  HostLoop* mLoop;
};

class VST3HostLoopBackend final : public HostLoopBackend
{
public:
  VST3HostLoopBackend(HostLoop& loop, Steinberg::Linux::IRunLoop* pRunLoop, const char* source)
  : HostLoopBackend(loop), mRunLoop(pRunLoop)
  {
    snprintf(mName, sizeof(mName), "vst3-irunloop(%s)", source);
  }

  const char* GetName() const override { return mName; }
  bool IsHostDriven() const override { return true; }

  bool Register(HostLoopEntry& entry) override
  {
    auto* pHandler = new VST3RunLoopHandler(mLoop);
    const Steinberg::tresult result = entry.mIsFd ? mRunLoop->registerEventHandler(pHandler, entry.mFd)
                                                  : mRunLoop->registerTimer(pHandler, entry.mIntervalMs);
    if (result != Steinberg::kResultOk)
    {
      pHandler->Detach();
      pHandler->release();
      return false;
    }
    entry.mHandle = (uintptr_t) pHandler;
    return true;
  }

  void Unregister(HostLoopEntry& entry) override
  {
    auto* pHandler = reinterpret_cast<VST3RunLoopHandler*>(entry.mHandle);
    pHandler->Detach();
    if (entry.mIsFd)
      mRunLoop->unregisterEventHandler(pHandler);
    else
      mRunLoop->unregisterTimer(pHandler);
    pHandler->release();
  }

private:
  Steinberg::IPtr<Steinberg::Linux::IRunLoop> mRunLoop;
  char mName[48];
};

/** Per instance: tracks the IRunLoops the host has offered and puts the HostLoop on the preferred one. */
class VST3HostLoopSelector
{
public:
  explicit VST3HostLoopSelector(HostLoop& loop) : mLoop(loop) {}

  /** IPluginFactory3::setHostContext, via CPluginFactory::addHostContextCallback. Module-wide. */
  static void SetFactoryHostContext(Steinberg::FUnknown* pContext)
  {
    Steinberg::FUnknownPtr<Steinberg::Linux::IRunLoop> runLoop(pContext);
    ReleaseFactoryHostContext();
    sFactoryRunLoop = runLoop.getInterface();
    if (sFactoryRunLoop)
      sFactoryRunLoop->addRef();
    HostLoopTrace("VST3 factory setHostContext: context=%p IRunLoop=%p", (void*) pContext, (void*) sFactoryRunLoop);
  }

  /** At module exit: the host object must not be released from a static destructor after the host is gone. */
  static void ReleaseFactoryHostContext()
  {
    if (sFactoryRunLoop)
      sFactoryRunLoop->release();
    sFactoryRunLoop = nullptr;
  }

  void Initialize(Steinberg::FUnknown* pContext)
  {
    mFactoryRefused = mContextRefused = false;
    mFactoryRunLoop = sFactoryRunLoop;
    Steinberg::FUnknownPtr<Steinberg::Linux::IRunLoop> contextRunLoop(pContext);
    mContextRunLoop = contextRunLoop.getInterface();
#ifdef IPLUG_TEST_VST3_FRAME_RUNLOOP_ONLY // exercises the fallback and the attach/detach migration in hosts that offer more
    mFactoryRunLoop = nullptr;
    mContextRunLoop = nullptr;
#endif
    HostLoopTrace("%s: VST3 initialize: factory IRunLoop=%p context IRunLoop=%p", mLoop.GetOwnerName(),
                  (void*) mFactoryRunLoop.get(), (void*) mContextRunLoop.get());
    mInitialized = true;
    Update();
  }

  void Terminate()
  {
    mInitialized = false;
    mActive = nullptr;
    mLoop.SetBackend(nullptr);
    mFactoryRunLoop = nullptr;
    mContextRunLoop = nullptr;
    mFrameRunLoop = nullptr;
  }

  /** IPlugView attached (the frame) and removed (nullptr). */
  void SetFrame(Steinberg::IPlugFrame* pFrame)
  {
    Steinberg::FUnknownPtr<Steinberg::Linux::IRunLoop> frameRunLoop(pFrame);
    mFrameRunLoop = frameRunLoop.getInterface();
    mFrameRefused = false;
    HostLoopTrace("%s: VST3 view %s: frame IRunLoop=%p", mLoop.GetOwnerName(), pFrame ? "attached" : "removed",
                  (void*) mFrameRunLoop.get());
    Update();
  }

private:
  void Update()
  {
    if (!mInitialized)
      return;

    struct Candidate { Steinberg::Linux::IRunLoop* pRunLoop; bool* pRefused; const char* source; };
    const Candidate candidates[] = {
      { mFactoryRunLoop, &mFactoryRefused, "factory-context" },
      { mContextRunLoop, &mContextRefused, "initialize-context" },
      { mFrameRunLoop, &mFrameRefused, "frame" },
    };

    for (const Candidate& candidate : candidates)
    {
      if (!candidate.pRunLoop || *candidate.pRefused)
        continue;
      if (mLoop.GetBackend() && candidate.pRunLoop == mActive.get())
        return;
      // Seen in the SDK validator: its IRunLoop exists but registerTimer fails, so nothing would ever fire.
      if (AcceptsTimers(candidate.pRunLoop))
      {
        mActive = candidate.pRunLoop;
        if (mLoop.SetBackend(std::make_unique<VST3HostLoopBackend>(mLoop, candidate.pRunLoop, candidate.source)))
          return;
      }
      HostLoopTrace("%s: the %s IRunLoop refuses timers, trying the next source", mLoop.GetOwnerName(), candidate.source);
      *candidate.pRefused = true;
    }

    if (mLoop.GetBackend() && !mActive && !mLoop.IsHostDriven())
      return;
    mActive = nullptr;
    mLoop.SetBackend(MakeFallbackHostLoopBackend(mLoop));
  }

  /** Probes with a detached handler, so a refusing loop is caught even when no timer exists yet. */
  bool AcceptsTimers(Steinberg::Linux::IRunLoop* pRunLoop)
  {
    auto* pProbe = new VST3RunLoopHandler(mLoop);
    pProbe->Detach();
    const bool accepted = pRunLoop->registerTimer(pProbe, 1000) == Steinberg::kResultOk;
    if (accepted)
      pRunLoop->unregisterTimer(pProbe);
    pProbe->release();
    return accepted;
  }

  HostLoop& mLoop;
  Steinberg::IPtr<Steinberg::Linux::IRunLoop> mFactoryRunLoop;
  Steinberg::IPtr<Steinberg::Linux::IRunLoop> mContextRunLoop;
  Steinberg::IPtr<Steinberg::Linux::IRunLoop> mFrameRunLoop;
  Steinberg::IPtr<Steinberg::Linux::IRunLoop> mActive;
  bool mInitialized = false;
  bool mFactoryRefused = false;
  bool mContextRefused = false;
  bool mFrameRefused = false;

  static inline Steinberg::Linux::IRunLoop* sFactoryRunLoop = nullptr;
};

END_IPLUG_NAMESPACE
