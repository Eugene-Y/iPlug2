#define INIT_CLASS_IID // defines the SDK interface ids in this TU
#include "pluginterfaces/gui/iplugview.h"
#undef INIT_CLASS_IID

#include "IPlugTimer.h"
#include "Linux/IPlugLinuxHostLoop.h"
#include "Linux/IPlugLinuxHostLoopVST3.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <dirent.h>
#include <thread>
#include <type_traits>
#include <unistd.h>
#include <vector>

using namespace iplug;
using namespace Steinberg;

namespace {

int gFailures = 0;

#define EXPECT(cond) do { if (!(cond)) { fprintf(stderr, "%s:%d: EXPECT(%s) failed\n", __FILE__, __LINE__, #cond); ++gFailures; } } while (0)

int CountThreads()
{
  int n = 0;
  if (DIR* pDir = opendir("/proc/self/task"))
  {
    while (dirent* pEntry = readdir(pDir))
      n += pEntry->d_name[0] != '.';
    closedir(pDir);
  }
  return n;
}

int CountFds()
{
  int n = 0;
  if (DIR* pDir = opendir("/proc/self/fd"))
  {
    while (dirent* pEntry = readdir(pDir))
      n += pEntry->d_name[0] != '.';
    closedir(pDir);
  }
  return n - 1; // the DIR's own fd
}

template <class Pred>
bool WaitFor(Pred pred, int ms = 2000)
{
  for (int i = 0; i < ms / 5; i++)
  {
    if (pred())
      return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return pred();
}

/** A host loop the test fires by hand, like a host main loop would. */
class FakeBackend final : public HostLoopBackend
{
public:
  explicit FakeBackend(HostLoop& loop, int* pAlive = nullptr) : HostLoopBackend(loop), mAlive(pAlive) { if (mAlive) ++*mAlive; }
  ~FakeBackend() override { if (mAlive) --*mAlive; }
  const char* GetName() const override { return "fake"; }
  bool IsHostDriven() const override { return true; }
  bool Register(HostLoopEntry& entry) override { entry.mHandle = ++mNextHandle; mHandles.push_back(entry.mHandle); return true; }
  void Unregister(HostLoopEntry& entry) override
  {
    for (size_t i = 0; i < mHandles.size(); i++)
      if (mHandles[i] == entry.mHandle) { mHandles.erase(mHandles.begin() + i); return; }
    EXPECT(!"unregistered an unknown handle");
  }
  void FireAll(bool isFd = false)
  {
    const std::vector<uintptr_t> handles = mHandles;
    for (uintptr_t handle : handles)
      mLoop.Dispatch(isFd, handle, HostLoop::kFdRead);
  }
  std::vector<uintptr_t> mHandles;
  uintptr_t mNextHandle = 0;
  int* mAlive;
};

void TestDeferredUntilBackend()
{
  HostLoop loop("deferred");
  int calls = 0;
  const int id = loop.AddTimer(10, [&]() { calls++; });
  EXPECT(id > 0);
  EXPECT(!loop.IsHostDriven());

  auto backend = std::make_unique<FakeBackend>(loop);
  FakeBackend* pFake = backend.get();
  loop.SetBackend(std::move(backend));
  EXPECT(loop.IsHostDriven());
  EXPECT(pFake->mHandles.size() == 1);
  pFake->FireAll();
  EXPECT(calls == 1);

  loop.RemoveTimer(id);
  EXPECT(pFake->mHandles.empty());
  pFake->FireAll();
  EXPECT(calls == 1);
}

void TestRemovedBeforeBackendNeverRegisters()
{
  HostLoop loop("removed-early");
  const int id = loop.AddTimer(10, []() {});
  loop.RemoveTimer(id);
  auto backend = std::make_unique<FakeBackend>(loop);
  FakeBackend* pFake = backend.get();
  loop.SetBackend(std::move(backend));
  EXPECT(pFake->mHandles.empty());
}

void TestMigrationKeepsEntriesAndIgnoresOldHandles()
{
  HostLoop loop("migrate");
  int timerCalls = 0, fdCalls = 0, alive = 0;
  loop.AddTimer(10, [&]() { timerCalls++; });
  loop.AddFd(0, HostLoop::kFdRead, [&](int, int flags) { fdCalls += (flags & HostLoop::kFdRead) != 0; });

  auto first = std::make_unique<FakeBackend>(loop, &alive);
  FakeBackend* pFirst = first.get();
  loop.SetBackend(std::move(first));
  const std::vector<uintptr_t> oldHandles = pFirst->mHandles;
  EXPECT(oldHandles.size() == 2);

  auto second = std::make_unique<FakeBackend>(loop, &alive);
  second->mNextHandle = 100;
  FakeBackend* pSecond = second.get();
  loop.SetBackend(std::move(second));
  EXPECT(alive == 1); // the first backend is gone
  EXPECT(pSecond->mHandles.size() == 2);

  for (uintptr_t handle : oldHandles)
    loop.Dispatch(false, handle);
  EXPECT(timerCalls == 0);

  pSecond->FireAll(false); // only the timer's handle matches a timer
  pSecond->FireAll(true);
  EXPECT(timerCalls == 1);
  EXPECT(fdCalls == 1);

  loop.SetBackend(nullptr);
  EXPECT(alive == 0);
}

void TestRemoveInsideOwnCallback()
{
  HostLoop loop("self-remove");
  auto backend = std::make_unique<FakeBackend>(loop);
  FakeBackend* pFake = backend.get();
  loop.SetBackend(std::move(backend));

  int id = 0, calls = 0, otherCalls = 0;
  id = loop.AddTimer(10, [&]() { calls++; loop.RemoveTimer(id); loop.AddTimer(10, [&]() { otherCalls++; }); });
  pFake->FireAll();
  EXPECT(calls == 1);
  EXPECT(pFake->mHandles.size() == 1);
  pFake->FireAll();
  EXPECT(calls == 1);
  EXPECT(otherCalls == 1);
}

void TestCallbackRemovesAnotherEntry()
{
  HostLoop loop("remove-other");
  auto backend = std::make_unique<FakeBackend>(loop);
  FakeBackend* pFake = backend.get();
  loop.SetBackend(std::move(backend));

  int otherId = 0, calls = 0, otherCalls = 0;
  loop.AddTimer(10, [&]() { calls++; loop.RemoveTimer(otherId); });
  otherId = loop.AddTimer(10, [&]() { otherCalls++; });
  pFake->FireAll(); // the second handle is stale by the time it fires
  EXPECT(calls == 1);
  EXPECT(otherCalls == 0);
  EXPECT(pFake->mHandles.size() == 1);
}

void TestNestedCallbackRemovesTheOuterEntry()
{
  // A host running a nested loop inside A's callback fires B, and B removes A: A must outlive its own callback.
  HostLoop loop("nested-remove");
  auto backend = std::make_unique<FakeBackend>(loop);
  FakeBackend* pFake = backend.get();
  loop.SetBackend(std::move(backend));

  int outerId = 0, outerSteps = 0, innerCalls = 0;
  outerId = loop.AddTimer(10, [&]() { outerSteps++; loop.Dispatch(false, pFake->mHandles.back()); outerSteps++; });
  loop.AddTimer(10, [&]() { innerCalls++; loop.RemoveTimer(outerId); });
  loop.Dispatch(false, pFake->mHandles.front());
  EXPECT(outerSteps == 2);
  EXPECT(innerCalls == 1);
  EXPECT(pFake->mHandles.size() == 1);
}

void TestFallbackSelfRemoval()
{
  HostLoop loop("fallback-self-remove");
  std::atomic<int> calls { 0 }, laterCalls { 0 };
  int id = 0;
  // Added before the backend, so the id is known before the thread can fire it.
  id = loop.AddTimer(1, [&]() { calls++; loop.RemoveTimer(id); loop.AddTimer(1, [&]() { laterCalls++; }); });
  loop.SetBackend(MakeFallbackHostLoopBackend(loop));
  EXPECT(WaitFor([&]() { return laterCalls.load() >= 3; }));
  EXPECT(calls.load() == 1);
}

void TestDestroyWhileFallbackCallbackRuns()
{
  for (int round = 0; round < 20; round++)
  {
    std::atomic<bool> inCallback { false };
    auto pState = std::make_unique<std::vector<int>>(64, 0);
    std::vector<int>* pRaw = pState.get();
    {
      HostLoop loop("destroy-in-flight");
      loop.AddTimer(1, [&inCallback, pRaw]() {
        inCallback = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        (*pRaw)[3]++;
      });
      loop.SetBackend(MakeFallbackHostLoopBackend(loop));
      while (!inCallback.load())
        std::this_thread::yield();
    } // ~HostLoop waits for the running callback before anything it uses goes away
    pState.reset();
  }
}

void TestNoReentryIntoTheSameEntry()
{
  HostLoop loop("reentry");
  auto backend = std::make_unique<FakeBackend>(loop);
  FakeBackend* pFake = backend.get();
  loop.SetBackend(std::move(backend));
  int depth = 0, maxDepth = 0;
  loop.AddTimer(10, [&]() { depth++; maxDepth = std::max(maxDepth, depth); pFake->FireAll(); depth--; });
  pFake->FireAll();
  EXPECT(maxDepth == 1);
}

void TestFallbackThread()
{
  const int threadsBefore = CountThreads();
  const int fdsBefore = CountFds();

  for (int round = 0; round < 50; round++)
  {
    HostLoop loop("fallback");
    loop.SetBackend(MakeFallbackHostLoopBackend(loop));
    EXPECT(!loop.IsHostDriven());
    std::atomic<int> calls { 0 };
    std::atomic<long> tid { 0 };
    const int id = loop.AddTimer(2, [&]() { calls++; tid = HostLoopThreadId(); });
    EXPECT(WaitFor([&]() { return calls.load() >= 3; }));
    EXPECT(tid != HostLoopThreadId());
    loop.RemoveTimer(id);
    const int afterRemove = calls.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT(calls.load() == afterRemove); // RemoveTimer waits out a running callback, then none follow
  }

  EXPECT(CountThreads() == threadsBefore);
  EXPECT(CountFds() == fdsBefore);
}

void TestFallbackFd()
{
  int fds[2];
  EXPECT(pipe(fds) == 0);
  HostLoop loop("fallback-fd");
  std::atomic<int> reads { 0 };
  loop.AddFd(fds[0], HostLoop::kFdRead, [&](int fd, int flags) {
    char c;
    if ((flags & HostLoop::kFdRead) && read(fd, &c, 1) == 1)
      reads++;
  });
  loop.SetBackend(MakeFallbackHostLoopBackend(loop));
  EXPECT(write(fds[1], "xy", 2) == 2);
  EXPECT(WaitFor([&]() { return reads.load() == 2; }));
  loop.SetBackend(nullptr);
  close(fds[0]);
  close(fds[1]);
}

void TestFallbackToHostMigrationUnderLoad()
{
  // The fallback thread may be inside Dispatch, waiting for the loop's lock, while SetBackend swaps.
  HostLoop loop("swap-under-load");
  std::atomic<int> calls { 0 };
  for (int i = 0; i < 4; i++)
    loop.AddTimer(1, [&]() { calls++; std::this_thread::sleep_for(std::chrono::microseconds(200)); });
  for (int round = 0; round < 100; round++)
  {
    loop.SetBackend(MakeFallbackHostLoopBackend(loop));
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    auto backend = std::make_unique<FakeBackend>(loop);
    FakeBackend* pFake = backend.get();
    loop.SetBackend(std::move(backend));
    const int before = calls.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    EXPECT(calls.load() == before); // nothing from the old thread after the swap returned
    pFake->FireAll();
    EXPECT(calls.load() == before + 4);
  }
  EXPECT(calls.load() > 0);
}

void TestTimerImpl()
{
  HostLoop loop("timer");
  int calls = 0;
  {
    Timer_impl timer([&](Timer&) { calls++; }, 10, &loop);
    auto backend = std::make_unique<FakeBackend>(loop);
    FakeBackend* pFake = backend.get();
    loop.SetBackend(std::move(backend));
    pFake->FireAll();
    EXPECT(calls == 1);
    timer.Stop();
    pFake->FireAll();
    EXPECT(calls == 1);
    EXPECT(pFake->mHandles.empty());
  }

  std::atomic<int> standaloneCalls { 0 };
  std::unique_ptr<Timer> standalone(Timer::Create([&](Timer&) { standaloneCalls++; }, 2));
  EXPECT(WaitFor([&]() { return standaloneCalls.load() >= 2; }));
  standalone.reset();
}

#pragma mark - VST3

/** IRunLoop + IPlugFrame. Keeps a reference to every handler, as a host may, and can call one late. */
class FakeRunLoop final : public Linux::IRunLoop, public IPlugFrame
{
public:
  tresult PLUGIN_API registerEventHandler(Linux::IEventHandler* pHandler, Linux::FileDescriptor) override { pHandler->addRef(); mEvents.push_back(pHandler); return kResultOk; }
  tresult PLUGIN_API unregisterEventHandler(Linux::IEventHandler* pHandler) override { return Drop(mEvents, pHandler); }
  tresult PLUGIN_API registerTimer(Linux::ITimerHandler* pHandler, Linux::TimerInterval) override
  {
    if (mRefuseTimers)
      return kResultFalse;
    pHandler->addRef();
    mTimers.push_back(pHandler);
    return kResultOk;
  }
  tresult PLUGIN_API unregisterTimer(Linux::ITimerHandler* pHandler) override { return Drop(mTimers, pHandler); }
  tresult PLUGIN_API resizeView(IPlugView*, ViewRect*) override { return kResultOk; }

  tresult PLUGIN_API queryInterface(const TUID iid, void** obj) override
  {
    if (FUnknownPrivate::iidEqual(iid, Linux::IRunLoop::iid) || FUnknownPrivate::iidEqual(iid, FUnknown::iid))
      *obj = static_cast<Linux::IRunLoop*>(this);
    else if (FUnknownPrivate::iidEqual(iid, IPlugFrame::iid))
      *obj = static_cast<IPlugFrame*>(this);
    else { *obj = nullptr; return kNoInterface; }
    addRef();
    return kResultOk;
  }
  uint32 PLUGIN_API addRef() override { return ++mRefs; }
  uint32 PLUGIN_API release() override { return --mRefs; } // lives on the test's stack

  void FireTimers() { const auto timers = mTimers; for (auto* p : timers) p->onTimer(); }

  /** Calls a handler the plug-in already unregistered, then lets go of it. */
  void FireAndReleaseLate()
  {
    for (auto* p : mLate) { p->onTimer(); p->release(); }
    mLate.clear();
  }

  std::vector<Linux::ITimerHandler*> mTimers;
  std::vector<Linux::IEventHandler*> mEvents;
  std::vector<Linux::ITimerHandler*> mLate;
  std::atomic<uint32> mRefs { 1 };
  bool mRefuseTimers = false;

private:
  template <class T>
  tresult Drop(std::vector<T*>& list, T* pHandler)
  {
    for (size_t i = 0; i < list.size(); i++)
    {
      if (list[i] == pHandler)
      {
        list.erase(list.begin() + i);
        if constexpr (std::is_same_v<T, Linux::ITimerHandler>) mLate.push_back(pHandler); else pHandler->release();
        return kResultOk;
      }
    }
    EXPECT(!"unregister of an unknown handler");
    return kResultFalse;
  }
};

/** A host context without IRunLoop. */
class FakeHostApp final : public FUnknown
{
public:
  tresult PLUGIN_API queryInterface(const TUID, void** obj) override { *obj = nullptr; return kNoInterface; }
  uint32 PLUGIN_API addRef() override { return 2; }
  uint32 PLUGIN_API release() override { return 1; }
};

void TestVST3FrameOnlyHost()
{
  FakeRunLoop frame;
  FakeHostApp hostApp;
  HostLoop loop("vst3-frame");
  VST3HostLoopSelector selector(loop);
  std::atomic<int> calls { 0 };
  std::atomic<long> lastTid { 0 };
  loop.AddTimer(2, [&]() { calls++; lastTid = HostLoopThreadId(); });

  selector.Initialize(&hostApp);
  EXPECT(!loop.IsHostDriven()); // closed editor: fallback thread
  EXPECT(WaitFor([&]() { return calls.load() >= 2; }));

  selector.SetFrame(&frame);
  EXPECT(loop.IsHostDriven());
  EXPECT(frame.mTimers.size() == 1);
  const int before = calls.load();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  EXPECT(calls.load() == before); // the thread is gone
  frame.FireTimers();
  EXPECT(calls.load() == before + 1);
  EXPECT(lastTid == HostLoopThreadId());

  selector.SetFrame(nullptr);
  EXPECT(frame.mTimers.empty());
  EXPECT(!loop.IsHostDriven());
  const int beforeLate = calls.load();
  frame.FireAndReleaseLate(); // a host calling the old handler after unregistering it
  EXPECT(WaitFor([&]() { return calls.load() > beforeLate; })); // from the fallback again
  EXPECT(lastTid != HostLoopThreadId());

  selector.Terminate();
  EXPECT(!loop.GetBackend());
  frame.FireAndReleaseLate();
}

void TestVST3FactoryContextWins()
{
  FakeRunLoop factory, frame;
  HostLoop loop("vst3-factory");
  int calls = 0;
  loop.AddTimer(10, [&]() { calls++; });

  VST3HostLoopSelector::SetFactoryHostContext(static_cast<Linux::IRunLoop*>(&factory));
  VST3HostLoopSelector selector(loop);
  selector.Initialize(nullptr);
  EXPECT(loop.IsHostDriven());
  EXPECT(factory.mTimers.size() == 1);

  selector.SetFrame(&frame); // attach does not move the timers off the factory loop
  EXPECT(frame.mTimers.empty());
  EXPECT(factory.mTimers.size() == 1);
  selector.SetFrame(nullptr);
  factory.FireTimers();
  EXPECT(calls == 1);

  selector.Terminate();
  EXPECT(factory.mTimers.empty());
  factory.FireAndReleaseLate();
  EXPECT(calls == 1);
  VST3HostLoopSelector::ReleaseFactoryHostContext();
  EXPECT(factory.mRefs == 1);
  EXPECT(frame.mRefs == 1);
}

void TestVST3RunLoopThatRefusesTimers()
{
  FakeRunLoop factory, frame;
  factory.mRefuseTimers = true;
  HostLoop loop("vst3-refusing");
  std::atomic<int> calls { 0 };
  loop.AddTimer(2, [&]() { calls++; });

  VST3HostLoopSelector::SetFactoryHostContext(static_cast<Linux::IRunLoop*>(&factory));
  VST3HostLoopSelector selector(loop);
  selector.Initialize(nullptr);
  EXPECT(!loop.IsHostDriven());
  EXPECT(WaitFor([&]() { return calls.load() >= 2; }));

  selector.SetFrame(&frame);
  EXPECT(loop.IsHostDriven());
  EXPECT(frame.mTimers.size() == 1);
  selector.SetFrame(nullptr);
  EXPECT(!loop.IsHostDriven());

  selector.Terminate();
  frame.FireAndReleaseLate();
  VST3HostLoopSelector::ReleaseFactoryHostContext();
  EXPECT(factory.mRefs == 1);
}

void TestIsHostDrivenDoesNotWaitForACallback()
{
  HostLoop loop("lock-free-query");
  std::atomic<bool> inCallback { false };
  loop.AddTimer(1, [&]() { inCallback = true; std::this_thread::sleep_for(std::chrono::milliseconds(300)); inCallback = false; });
  loop.SetBackend(MakeFallbackHostLoopBackend(loop));
  EXPECT(WaitFor([&]() { return inCallback.load(); }));
  const auto start = std::chrono::steady_clock::now();
  EXPECT(!loop.IsHostDriven());
  EXPECT(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(100));
  loop.SetBackend(nullptr);
}

} // namespace

int main()
{
  TestDeferredUntilBackend();
  TestRemovedBeforeBackendNeverRegisters();
  TestMigrationKeepsEntriesAndIgnoresOldHandles();
  TestRemoveInsideOwnCallback();
  TestCallbackRemovesAnotherEntry();
  TestNestedCallbackRemovesTheOuterEntry();
  TestNoReentryIntoTheSameEntry();
  TestFallbackSelfRemoval();
  TestDestroyWhileFallbackCallbackRuns();
  TestFallbackThread();
  TestFallbackFd();
  TestFallbackToHostMigrationUnderLoad();
  TestTimerImpl();
  TestVST3FrameOnlyHost();
  TestVST3FactoryContextWins();
  TestVST3RunLoopThatRefusesTimers();
  TestIsHostDrivenDoesNotWaitForACallback();

  if (gFailures)
  {
    fprintf(stderr, "%d failure(s)\n", gFailures);
    return 1;
  }
  printf("all host-loop tests passed\n");
  return 0;
}
