/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#include "IPlugLinuxHostLoop.h"

#include <atomic>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>

#include <poll.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "heapbuf.h"

using namespace iplug;

long iplug::HostLoopThreadId()
{
  return (long) syscall(SYS_gettid);
}

#ifdef IPLUG_TRACE_HOST_LOOP
void iplug::HostLoopTrace(const char* format, ...)
{
  char line[512];
  va_list args;
  va_start(args, format);
  vsnprintf(line, sizeof(line), format, args);
  va_end(args);
  fprintf(stderr, "IPLUG_HOSTLOOP pid=%d tid=%ld %s\n", (int) getpid(), HostLoopThreadId(), line);
  fflush(stderr);
}
#endif

#pragma mark - HostLoop

// Across all loops in the module; the trace prints it so a test can spot registrations that leak.
static std::atomic<int> sLiveRegistrations { 0 };

HostLoop::HostLoop(const char* ownerName)
{
  SetOwnerName(ownerName);
}

void HostLoop::SetOwnerName(const char* ownerName)
{
  snprintf(mOwnerName, sizeof(mOwnerName), "%s@%p", ownerName ? ownerName : "", (void*) this);
}

HostLoop::~HostLoop()
{
  SetBackend(nullptr);
  WDL_MutexLock lock(&mMutex);
  HostLoopTrace("%s: destroyed with %d entries, live registrations in module=%d", mOwnerName, mEntries.GetSize(), sLiveRegistrations.load());
  mEntries.Empty(true);
}

HostLoopEntry* HostLoop::Find(int id) const
{
  for (int i = 0; i < mEntries.GetSize(); i++)
  {
    if (mEntries.Get(i)->mId == id)
      return mEntries.Get(i);
  }
  return nullptr;
}

int HostLoop::Add(HostLoopEntry* pEntry)
{
  WDL_MutexLock lock(&mMutex);
  pEntry->mId = mNextId++;
  mEntries.Add(pEntry);
  if (mBackend)
  {
    pEntry->mRegistered = mBackend->Register(*pEntry);
    if (pEntry->mRegistered)
      ++sLiveRegistrations;
    else
      HostLoopTrace("%s: %s cannot register %s id=%d", mOwnerName, mBackend->GetName(), pEntry->mIsFd ? "fd" : "timer", pEntry->mId);
  }
  return pEntry->mId;
}

int HostLoop::AddTimer(uint32_t intervalMs, TimerFunc func)
{
  HostLoopEntry* pEntry = new HostLoopEntry;
  pEntry->mIntervalMs = intervalMs > 0 ? intervalMs : 1;
  pEntry->mTimerFunc = std::move(func);
  return Add(pEntry);
}

int HostLoop::AddFd(int fd, int flags, FdFunc func)
{
  HostLoopEntry* pEntry = new HostLoopEntry;
  pEntry->mIsFd = true;
  pEntry->mFd = fd;
  pEntry->mFdFlags = flags;
  pEntry->mFdFunc = std::move(func);
  return Add(pEntry);
}

void HostLoop::RemoveTimer(int id)
{
  WDL_MutexLock lock(&mMutex);
  HostLoopEntry* pEntry = Find(id);
  if (!pEntry)
    return;

  if (pEntry->mRegistered)
  {
    mBackend->Unregister(*pEntry);
    --sLiveRegistrations;
  }

  mEntries.DeletePtr(pEntry);

  if (pEntry->mInCallback)
    pEntry->mRemoved = true; // Dispatch() deletes it once the callback that removed it returns
  else
    delete pEntry;
}

bool HostLoop::IsHostDriven() const
{
  WDL_MutexLock lock(&mMutex);
  return mBackend && mBackend->IsHostDriven();
}

bool HostLoop::IsFdWatched(int id) const
{
  WDL_MutexLock lock(&mMutex);
  HostLoopEntry* pEntry = Find(id);
  return pEntry && pEntry->mRegistered;
}

void HostLoop::SetBackend(std::unique_ptr<HostLoopBackend> backend)
{
  std::unique_ptr<HostLoopBackend> old;
  {
    WDL_MutexLock lock(&mMutex);
    for (int i = 0; i < mEntries.GetSize(); i++)
    {
      HostLoopEntry* pEntry = mEntries.Get(i);
      if (pEntry->mRegistered)
      {
        mBackend->Unregister(*pEntry);
        --sLiveRegistrations;
      }
      pEntry->mRegistered = false;
      pEntry->mHandle = 0;
    }
    old = std::move(mBackend);
  }

  // Outside the lock: a fallback backend joins its thread here, and that thread may be waiting for mMutex.
  if (old)
    HostLoopTrace("%s: backend %s -> %s", mOwnerName, old->GetName(), backend ? backend->GetName() : "none");
  old.reset();

  WDL_MutexLock lock(&mMutex);
  mBackend = std::move(backend);
  if (!mBackend)
    return;

  mInitThread = HostLoopThreadId();
  HostLoopTrace("%s: backend=%s host-driven=%d entries=%d init-tid=%ld", mOwnerName, mBackend->GetName(), mBackend->IsHostDriven(), mEntries.GetSize(), mInitThread);

  for (int i = 0; i < mEntries.GetSize(); i++)
  {
    HostLoopEntry* pEntry = mEntries.Get(i);
    pEntry->mRegistered = mBackend->Register(*pEntry);
    if (pEntry->mRegistered)
      ++sLiveRegistrations;
    else
      HostLoopTrace("%s: %s cannot register %s id=%d", mOwnerName, mBackend->GetName(), pEntry->mIsFd ? "fd" : "timer", pEntry->mId);
  }
}

void HostLoop::Dispatch(bool isFd, uintptr_t handle, int fdFlags)
{
  WDL_MutexLock lock(&mMutex);
  HostLoopEntry* pEntry = nullptr;
  for (int i = 0; i < mEntries.GetSize(); i++)
  {
    HostLoopEntry* pCandidate = mEntries.Get(i);
    if (pCandidate->mRegistered && pCandidate->mIsFd == isFd && pCandidate->mHandle == handle)
    {
      pEntry = pCandidate;
      break;
    }
  }

  // A host that runs a nested loop inside our callback would otherwise re-enter the same entry.
  if (!pEntry || pEntry->mInCallback)
    return;

  pEntry->mInCallback = true;
#ifdef IPLUG_TRACE_HOST_LOOP
  const long tid = HostLoopThreadId();
  const char* where = tid == mInitThread ? "main" : "OFF-MAIN";
  if (pEntry->mCalls == 0)
    HostLoopTrace("%s: first %s callback id=%d backend=%s on %s (init-tid=%ld)", mOwnerName, isFd ? "fd" : "timer", pEntry->mId, mBackend ? mBackend->GetName() : "none", where, mInitThread);
  else if (tid != mInitThread && pEntry->mCalls % 250 == 0)
    HostLoopTrace("%s: %s callback id=%d call=%u on OFF-MAIN (init-tid=%ld)", mOwnerName, isFd ? "fd" : "timer", pEntry->mId, pEntry->mCalls, mInitThread);
#endif
  pEntry->mCalls++;

  if (isFd)
    pEntry->mFdFunc(pEntry->mFd, fdFlags);
  else
    pEntry->mTimerFunc();

  pEntry->mInCallback = false;
  if (pEntry->mRemoved)
    delete pEntry;
}

#pragma mark - Fallback backend

namespace
{

uint64_t NowMs()
{
  timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t) ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

class FallbackHostLoopBackend final : public HostLoopBackend
{
  struct Slot
  {
    uintptr_t mHandle;
    bool mIsFd;
    uint32_t mIntervalMs;
    uint64_t mDueMs;
    int mFd;
    int mFdFlags;
  };

  struct Ready
  {
    uintptr_t mHandle;
    bool mIsFd;
    int mFdFlags;
  };

public:
  explicit FallbackHostLoopBackend(HostLoop& loop)
  : HostLoopBackend(loop)
  {
    static std::atomic<bool> sWarned { false };
    if (!sWarned.exchange(true))
      fprintf(stderr, "iPlug2: the host provides no event loop; timers (OnIdle) run on a plug-in thread\n");

    mWakeFd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    // Set before the thread exists, so it cannot see a stale "stopped" and exit at once.
    mRunning = true;
    if (mWakeFd < 0 || pthread_create(&mThread, nullptr, ThreadProc, this) != 0)
    {
      mRunning = false;
      fprintf(stderr, "iPlug2: could not start the host-loop fallback thread: %s\n", strerror(errno));
    }
    else
      mThreadStarted = true;
  }

  ~FallbackHostLoopBackend() override
  {
    {
      WDL_MutexLock lock(&mMutex);
      mRunning = false;
    }
    Wake();
    if (mThreadStarted)
      pthread_join(mThread, nullptr);
    if (mWakeFd >= 0)
      close(mWakeFd);
  }

  const char* GetName() const override { return "fallback-thread"; }
  bool IsHostDriven() const override { return false; }

  bool Register(HostLoopEntry& entry) override
  {
    if (!mThreadStarted)
      return false;

    entry.mHandle = (uintptr_t) entry.mId;
    Slot slot { entry.mHandle, entry.mIsFd, entry.mIntervalMs, NowMs() + entry.mIntervalMs, entry.mFd, entry.mFdFlags };
    {
      WDL_MutexLock lock(&mMutex);
      mSlots.Add(slot);
    }
    Wake();
    return true;
  }

  void Unregister(HostLoopEntry& entry) override
  {
    WDL_MutexLock lock(&mMutex);
    for (int i = 0; i < mSlots.GetSize(); i++)
    {
      if (mSlots.Get()[i].mHandle == entry.mHandle && mSlots.Get()[i].mIsFd == entry.mIsFd)
      {
        mSlots.Delete(i);
        break;
      }
    }
  }

private:
  static void* ThreadProc(void* pParam)
  {
    static_cast<FallbackHostLoopBackend*>(pParam)->Run();
    return nullptr;
  }

  void Wake()
  {
    if (mWakeFd < 0)
      return;
    const uint64_t one = 1;
    ssize_t written = write(mWakeFd, &one, sizeof(one));
    (void) written;
  }

  void Run()
  {
    HostLoopTrace("%s: fallback thread started", mLoop.GetOwnerName());
    WDL_TypedBuf<pollfd> pollFds;
    WDL_TypedBuf<uintptr_t> pollHandles;
    WDL_TypedBuf<Ready> ready;

    while (true)
    {
      int timeoutMs = -1;
      {
        WDL_MutexLock lock(&mMutex);
        if (!mRunning)
          break;

        pollFds.Resize(0, false);
        pollHandles.Resize(0, false);
        pollFds.Add(pollfd { mWakeFd, POLLIN, 0 });
        pollHandles.Add(0);

        const uint64_t now = NowMs();
        for (int i = 0; i < mSlots.GetSize(); i++)
        {
          const Slot& slot = mSlots.Get()[i];
          if (slot.mIsFd)
          {
            const short events = ((slot.mFdFlags & HostLoop::kFdRead) ? POLLIN : 0) | ((slot.mFdFlags & HostLoop::kFdWrite) ? POLLOUT : 0);
            pollFds.Add(pollfd { slot.mFd, events, 0 });
            pollHandles.Add(slot.mHandle);
          }
          else
          {
            const int untilDue = slot.mDueMs > now ? (int) (slot.mDueMs - now) : 0;
            if (timeoutMs < 0 || untilDue < timeoutMs)
              timeoutMs = untilDue;
          }
        }
      }

      if (poll(pollFds.Get(), pollFds.GetSize(), timeoutMs) < 0 && errno != EINTR)
        break;

      if (pollFds.Get()[0].revents & POLLIN)
      {
        uint64_t count;
        ssize_t got = read(mWakeFd, &count, sizeof(count));
        (void) got;
      }

      ready.Resize(0, false);
      {
        WDL_MutexLock lock(&mMutex);
        if (!mRunning)
          break;

        const uint64_t now = NowMs();
        for (int i = 0; i < mSlots.GetSize(); i++)
        {
          Slot& slot = mSlots.Get()[i];
          if (slot.mIsFd || slot.mDueMs > now)
            continue;
          // A thread that fell far behind skips the missed ticks instead of firing them in a burst.
          slot.mDueMs = (now - slot.mDueMs > slot.mIntervalMs) ? now + slot.mIntervalMs : slot.mDueMs + slot.mIntervalMs;
          ready.Add(Ready { slot.mHandle, false, 0 });
        }

        for (int i = 1; i < pollFds.GetSize(); i++)
        {
          const short revents = pollFds.Get()[i].revents;
          if (!revents)
            continue;
          const int flags = ((revents & POLLIN) ? HostLoop::kFdRead : 0) | ((revents & POLLOUT) ? HostLoop::kFdWrite : 0)
                          | ((revents & (POLLERR | POLLHUP | POLLNVAL)) ? HostLoop::kFdError : 0);
          ready.Add(Ready { pollHandles.Get()[i], true, flags });
        }
      }

      // Without mMutex: Dispatch takes the loop's lock, and the main thread takes the two in the other order.
      for (int i = 0; i < ready.GetSize(); i++)
        mLoop.Dispatch(ready.Get()[i].mIsFd, ready.Get()[i].mHandle, ready.Get()[i].mFdFlags);
    }
    HostLoopTrace("%s: fallback thread stopped", mLoop.GetOwnerName());
  }

  WDL_Mutex mMutex;
  WDL_TypedBuf<Slot> mSlots;
  pthread_t mThread {};
  int mWakeFd = -1;
  bool mRunning = false;
  bool mThreadStarted = false;
};

} // namespace

std::unique_ptr<HostLoopBackend> iplug::MakeFallbackHostLoopBackend(HostLoop& loop)
{
  return std::make_unique<FallbackHostLoopBackend>(loop);
}
