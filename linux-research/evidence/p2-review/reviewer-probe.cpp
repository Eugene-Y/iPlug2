// Reviewer probes for P2 (not part of the tree). Build in the container with ASan or TSan.
#include "IPlugTimer.h"
#include "Linux/IPlugLinuxHostLoop.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

using namespace iplug;

static int gFailures = 0;
#define EXPECT(cond) do { if (!(cond)) { fprintf(stderr, "%s:%d: EXPECT(%s) failed\n", __FILE__, __LINE__, #cond); ++gFailures; } } while (0)

class FakeBackend final : public HostLoopBackend
{
public:
  explicit FakeBackend(HostLoop& loop, int acceptTimers = 1000) : HostLoopBackend(loop), mAccept(acceptTimers) {}
  const char* GetName() const override { return "fake"; }
  bool IsHostDriven() const override { return true; }
  bool Register(HostLoopEntry& entry) override
  {
    if (!entry.mIsFd && mAccept-- <= 0)
      return false;
    entry.mHandle = ++mNext; mHandles.push_back(entry.mHandle); return true;
  }
  void Unregister(HostLoopEntry& entry) override
  {
    for (size_t i = 0; i < mHandles.size(); i++) if (mHandles[i] == entry.mHandle) { mHandles.erase(mHandles.begin() + i); return; }
  }
  void Fire(size_t i) { if (i < mHandles.size()) mLoop.Dispatch(false, mHandles[i]); }
  void FireAll() { auto h = mHandles; for (auto x : h) mLoop.Dispatch(false, x); }
  std::vector<uintptr_t> mHandles;
  uintptr_t mNext = 0;
  int mAccept;
};

// 1. a callback removes a different entry; stale handles must be ignored
static void CallbackRemovesOther()
{
  HostLoop loop("rm-other");
  auto b = std::make_unique<FakeBackend>(loop); auto* pB = b.get();
  loop.SetBackend(std::move(b));
  int idB = 0, callsA = 0, callsB = 0;
  loop.AddTimer(10, [&]() { callsA++; loop.RemoveTimer(idB); });
  idB = loop.AddTimer(10, [&]() { callsB++; });
  auto handles = pB->mHandles;
  for (auto h : handles) loop.Dispatch(false, h);
  EXPECT(callsA == 1 && callsB == 0);
  EXPECT(pB->mHandles.size() == 1);
}

// 2. nested dispatch: A's callback runs B (host nested loop); B removes A; A is freed only after it returns
static void NestedRemovesOuter()
{
  HostLoop loop("nested");
  auto b = std::make_unique<FakeBackend>(loop); auto* pB = b.get();
  loop.SetBackend(std::move(b));
  int idA = 0; int callsA = 0, callsB = 0;
  idA = loop.AddTimer(10, [&]() { callsA++; pB->Fire(1); callsA += 10; });
  loop.AddTimer(10, [&]() { callsB++; loop.RemoveTimer(idA); });
  pB->Fire(0);
  EXPECT(callsA == 11 && callsB == 1);
  EXPECT(pB->mHandles.size() == 1);
}

// 3. fallback thread: self-removal + re-add from inside the callback
static void FallbackSelfRemove()
{
  HostLoop loop("fb-self");
  loop.SetBackend(MakeFallbackHostLoopBackend(loop));
  std::atomic<int> first { 0 }, second { 0 };
  int id = 0;
  id = loop.AddTimer(1, [&]() { first++; loop.RemoveTimer(id); loop.AddTimer(1, [&]() { second++; }); });
  for (int i = 0; i < 400 && second.load() < 3; i++) std::this_thread::sleep_for(std::chrono::milliseconds(5));
  EXPECT(first.load() == 1);
  EXPECT(second.load() >= 3);
}

// 4. destroy the loop while the fallback thread is inside a callback
static void DestroyDuringFallbackCallback()
{
  for (int round = 0; round < 20; round++)
  {
    std::atomic<int> inCb { 0 };
    auto* pData = new std::vector<int>(64, 1);
    {
      HostLoop loop("fb-destroy");
      loop.AddTimer(1, [&, pData]() { inCb = 1; std::this_thread::sleep_for(std::chrono::milliseconds(5)); (*pData)[3]++; inCb = 0; });
      loop.SetBackend(MakeFallbackHostLoopBackend(loop));
      while (!inCb.load()) std::this_thread::yield();
    } // ~HostLoop must wait for the callback
    delete pData;
  }
}

// 5. a host loop that accepts the first timer and refuses a later one: is the later timer lost?
static void LaterTimerRefused()
{
  HostLoop loop("late-refuse");
  int early = 0, late = 0;
  loop.AddTimer(10, [&]() { early++; });
  auto b = std::make_unique<FakeBackend>(loop, 1); auto* pB = b.get();
  EXPECT(loop.SetBackend(std::move(b)));
  loop.AddTimer(10, [&]() { late++; });
  pB->FireAll();
  fprintf(stderr, "probe5: early=%d late=%d registered=%zu (late timer %s)\n", early, late, pB->mHandles.size(), late ? "fires" : "LOST, no fallback");
}

// 6. CLAP-like owner: the loop is stopped in a base destructor, after the derived part is gone.
struct Base
{
  HostLoop mLoop { "owner" };
  std::unique_ptr<Timer_impl> mTimer;
  Base() { mTimer = std::make_unique<Timer_impl>([this](Timer&) { OnIdle(); }, 1, &mLoop); }
  virtual void OnIdle() {}
  virtual ~Base() { mLoop.SetBackend(nullptr); }
};
struct Derived : Base
{
  std::vector<int>* mpState = new std::vector<int>(256, 0);
  void OnIdle() override { if (mpState) (*mpState)[7]++; }
  ~Derived() override
  {
    std::vector<int>* p = mpState;
    delete p; // members gone; a fallback OnIdle in this window touches freed memory
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    mpState = nullptr;
  }
};
static void DerivedDestructorRace()
{
  for (int i = 0; i < 30; i++)
  {
    auto* pD = new Derived;
    pD->mLoop.SetBackend(MakeFallbackHostLoopBackend(pD->mLoop));
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    delete pD;
  }
}

int main(int argc, char** argv)
{
  const char* only = argc > 1 ? argv[1] : "";
  auto run = [&](const char* name, void (*f)()) { if (!*only || !strcmp(only, name)) { fprintf(stderr, "== %s\n", name); f(); } };
  run("rm-other", CallbackRemovesOther);
  run("nested", NestedRemovesOuter);
  run("fb-self", FallbackSelfRemove);
  run("fb-destroy", DestroyDuringFallbackCallback);
  run("late-refuse", LaterTimerRefused);
  run("derived", DerivedDestructorRace);
  fprintf(stderr, gFailures ? "%d failure(s)\n" : "probes done\n", gFailures);
  return gFailures ? 1 : 0;
}
