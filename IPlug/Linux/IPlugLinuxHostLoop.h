/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#pragma once

/** @file
 * @brief Per-instance timers and fd watches that run on the host's main thread (Linux).
 *
 * A HostLoop belongs to one plug-in instance. Callers add timers and fd watches at any time, also
 * before the host's loop is known (in the plug-in constructor). Those wait, unregistered, until the
 * format code installs a backend (CLAP timer-support, VST3 IRunLoop, the APP's GLib loop), and are
 * moved over whenever the format switches backends (VST3 view attach/detach).
 *
 * Every public method and every SetBackend() call belongs to the host main thread. Callbacks run on the
 * host main thread, except with the fallback backend, which runs them on a thread of its own.
 * A callback may add and remove entries, its own included, on either thread.
 * An fd callback must RemoveFd() once it sees kFdError: a hung-up fd stays ready and fires again forever.
 */

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>

#include "IPlugPlatform.h"
#include "mutex.h"
#include "ptrlist.h"

BEGIN_IPLUG_NAMESPACE

class HostLoop;

/** One registered timer or fd watch. Backends see and fill only mHandle. */
struct HostLoopEntry
{
  int mId = 0;
  bool mIsFd = false;
  uint32_t mIntervalMs = 0;
  int mFd = -1;
  int mFdFlags = 0;
  std::function<void()> mTimerFunc;
  std::function<void(int fd, int flags)> mFdFunc;

  bool mRegistered = false;
  uintptr_t mHandle = 0;
  bool mInCallback = false;
  bool mRemoved = false;
  unsigned mCalls = 0;
};

/** Delivers callbacks for a HostLoop's entries by calling HostLoop::Dispatch() with the handle it set. */
class HostLoopBackend
{
public:
  explicit HostLoopBackend(HostLoop& loop) : mLoop(loop) {}
  virtual ~HostLoopBackend() = default;
  HostLoopBackend(const HostLoopBackend&) = delete;
  HostLoopBackend& operator=(const HostLoopBackend&) = delete;

  virtual const char* GetName() const = 0;
  virtual bool IsHostDriven() const = 0;
  /** Sets entry.mHandle, unique among this backend's entries of the same kind. False if it cannot watch it. */
  virtual bool Register(HostLoopEntry& entry) = 0;
  virtual void Unregister(HostLoopEntry& entry) = 0;

protected:
  HostLoop& mLoop;
};

class HostLoop
{
public:
  enum EFdFlags { kFdRead = 1, kFdWrite = 2, kFdError = 4 };

  using TimerFunc = std::function<void()>;
  using FdFunc = std::function<void(int fd, int flags)>;

  explicit HostLoop(const char* ownerName = "");
  /** Names the loop in traces, followed by its address */
  void SetOwnerName(const char* ownerName);
  ~HostLoop();
  HostLoop(const HostLoop&) = delete;
  HostLoop& operator=(const HostLoop&) = delete;

  /** @return an id > 0. The timer starts once a backend is installed. */
  int AddTimer(uint32_t intervalMs, TimerFunc func);
  void RemoveTimer(int id);
  /** @param flags EFdFlags. Some backends cannot watch fds (CLAP hosts without posix-fd-support): poll from a timer as well. */
  int AddFd(int fd, int flags, FdFunc func);
  void RemoveFd(int id) { RemoveTimer(id); }

  /** True when callbacks come from the host's own main loop. Lock-free: never waits for a running callback. */
  bool IsHostDriven() const { return mHostDriven.load(std::memory_order_acquire); }
  bool IsFdWatched(int id) const;

  /** Format code only. Moves every entry to the new backend; nullptr leaves them waiting.
   * Never call it from inside one of this loop's callbacks.
   * @return false if the backend refused one of the timers (a host loop that exists but does not run them). */
  bool SetBackend(std::unique_ptr<HostLoopBackend> backend);
  HostLoopBackend* GetBackend() const { return mBackend.get(); }

  /** Backends only. Runs the callback of the entry registered under handle; ignores stale handles. */
  void Dispatch(bool isFd, uintptr_t handle, int fdFlags = 0);

  const char* GetOwnerName() const { return mOwnerName; }

private:
  int Add(HostLoopEntry* pEntry);
  HostLoopEntry* Find(int id) const;

  // Held across each callback, so RemoveTimer, SetBackend and ~HostLoop return only after a callback running
  // on the fallback thread has finished: after that the caller may destroy what the callback uses.
  // Recursive, so callbacks may add and remove entries.
  mutable WDL_Mutex mMutex;
  std::atomic<bool> mHostDriven { false };
  WDL_PtrList<HostLoopEntry> mEntries;
  std::unique_ptr<HostLoopBackend> mBackend;
  int mNextId = 1;
  long mInitThread = 0; // where the format installed the backend: the host main thread
  char mOwnerName[64];
};

/** A worker thread that runs the callbacks itself. Only for when the host offers no loop: callbacks then
 * run off the main thread, which breaks anything in OnIdle that needs the main thread (UI, SetLatency). */
std::unique_ptr<HostLoopBackend> MakeFallbackHostLoopBackend(HostLoop& loop);

/** Linux thread id of the caller, for the trace and thread checks. */
long HostLoopThreadId();

/** printf-style line prefixed with "IPLUG_HOSTLOOP pid tid", only in builds with IPLUG_TRACE_HOST_LOOP. */
#ifdef IPLUG_TRACE_HOST_LOOP
void HostLoopTrace(const char* format, ...);
#else
inline void HostLoopTrace(const char*, ...) {}
#endif

END_IPLUG_NAMESPACE
