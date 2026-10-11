# P2: host-loop timer layer (2026-10-11)

Commits `534be52df..` on `hvoya-linux`. Evidence: `evidence/p2/`. **[R]** = run in Docker (REAPER 7.82 aarch64,
headless, editor window still the P0 stub). **[U]** = unit test (`Tests/LinuxHostLoop`, ASan + UBSan).

## What was built

`IPlug/Linux/IPlugLinuxHostLoop.{h,cpp}`: `HostLoop`, one per plug-in instance (member of `IPlugAPIBase` on Linux,
`GetHostLoop()`), with `AddTimer / RemoveTimer / AddFd / RemoveFd / IsHostDriven / IsFdWatched`. Entries wait
unregistered until the format installs a backend, and move when it swaps backends (`SetBackend`). Backends:

| Backend | File | Installed by | Callback thread |
|---|---|---|---|
| CLAP `timer-support` + `posix-fd-support` | `IPlugLinuxHostLoopCLAP.h` | `IPlugCLAP::init()`; removed in `~IPlugCLAP` (host still valid) | host main |
| VST3 `Linux::IRunLoop` | `IPlugLinuxHostLoopVST3.h` | `VST3HostLoopSelector`: `initialize`, view `attached`/`removed`, `terminate` | host main |
| GLib default context | `IPlugLinuxHostLoopGLib.h` | `IPlugAPP` constructor | APP main |
| fallback thread | `IPlugLinuxHostLoop.cpp` | CLAP without timer-support; VST3 with no usable IRunLoop | its own thread, warns once on stderr |

`Timer_impl` (`IPlugLinuxTimer.*`) is a timer on a `HostLoop`. `IPlugAPIBase::CreateTimer()` passes the instance's
loop, so `OnTimer → OnIdle`, `ISender` draining and the queue pumps run where the backend says. `Timer::Create()`
has no instance: such timers (OSC, ReaperExt) get a private loop on the fallback thread.

VST3 IRunLoop sources, in order of preference: the factory host context (`CPluginFactory::addHostContextCallback`,
hooked into `BEGIN_FACTORY_DEF`; released in `DeinitModule`), the `initialize()` context, the attached view's
`IPlugFrame`. A source whose `registerTimer` fails is skipped. Handlers are refcounted, FUnknown-correct, and
detached on unregister, so a late host call is a no-op.

Trace: build with `-DIPLUG_TRACE_HOST_LOOP` (e.g. `CXXFLAGS=-DIPLUG_TRACE_HOST_LOOP` at the first CMake configure).
Lines start `IPLUG_HOSTLOOP pid= tid=`: backend per instance, the VST3 IRunLoop of each source, the first callback of
each entry with `main`/`OFF-MAIN` against the thread that installed the backend, every 250th off-main call, and the
module's live host registrations when an instance dies.

## Findings

1. **REAPER 7.82 gives VST3 `IRunLoop` through the factory host context** (`IPluginFactory3::setHostContext`) — the
   editor never has to open. The `initialize()` context does **not** expose it. The view's `IPlugFrame` exposes a
   different `IRunLoop` object. So in REAPER closed-editor idle is host-driven without any fallback. [R]
   (`evidence/p2/reaper-vst3-*/trace.txt`)
2. **CLAP REAPER**: `timer-support=1 posix-fd-support=1`. [R]
3. **The SDK `validator` offers an IRunLoop in the factory context whose `registerTimer` fails.** Without the
   "refuses timers → next source" rule, OnIdle would never run there. clap-validator has no timer-support at all.
   Both now run on the fallback thread. [R] (`evidence/p2/validators/trace-validators.txt`)
4. **REAPER ignores a VST3 latency change reported off its main thread.** With the frame-only test build
   (closed editor → fallback thread), `SetLatency` from `OnIdle` calls `restartComponent(kLatencyChanged)` from the
   worker and REAPER's `pdc` never changes; from the main thread it follows within 0.5 s. This is the concrete cost of
   the fallback for Gneiss: P9 must defer `SetLatency` to a host-thread call when `!GetHostLoop().IsHostDriven()`. [R]
5. Observed rates under gdb (50 Hz requested): REAPER CLAP ≈ 26 Hz, REAPER VST3 ≈ 35 Hz, APP (GLib) ≈ 45 Hz.
   gdb breakpoints slow the process, so these are lower bounds.

## Acceptance

| # | Check | Result |
|---|---|---|
| 1 | IPlugEffect / IPlugControls × CLAP / VST3: `OnTimer` and `OnIdle` hits all on REAPER's main LWP, before any open, while open, after close, with 2 instances, after churn (`scripts/host-loop-test.sh`, gdb per hit with LWP + `this`) | PASS ×4 [R] |
| 2 | VST3 IRunLoop with the editor never opened | yes, factory context (finding 1) [R] |
| 3 | 2 instances → 2 timer objects; 50× add/delete: threads, fds equal before/after; live host registrations = 2 | PASS ×4 [R] |
| 4 | Latency from `OnIdle` (IPlugEffect `IPLUG_TEST_LATENCY_FROM_IDLE`), editor closed: `SetLatency` on the main tid, REAPER `pdc` toggles 0/256 | PASS CLAP + VST3 [R] |
| 5 | APP `--no-io`: `OnTimer`/`OnIdle` on the main LWP (`scripts/app-host-loop-test.sh`) | PASS ×2 [R] |
| 6 | clap-validator, VST3 validator on plain builds | identical to P0 (state-reproducibility ×3, Instrument param-fuzz-bounds; Effect 536/537) [R] |
| 7 | macOS: IPlugEffect CLAP + VST3 Debug from this worktree (DerivedData/DSTROOT in the scratchpad) | BUILD SUCCEEDED |

Migration in a real host: `IPLUG_TEST_VST3_FRAME_RUNLOOP_ONLY` ignores the factory/initialize sources. REAPER then
shows fallback thread (closed) → frame IRunLoop on the main LWP (open) → a new fallback thread (closed), and the
instance keeps ticking through every switch, no thread or fd growth over 50× churn (`reaper-vst3-effect-frame-only`). [R]

Unit test [U]: deferral until a backend exists, removal before a backend, migration ignoring old handles,
self-removal and re-add inside a callback, no re-entry, fallback timers and fds, 50× fallback create/destroy without
thread or fd leaks, 100 fallback→host swaps under load, `Timer_impl`, VST3 frame-only, factory precedence, a
refusing IRunLoop, late host calls on unregistered handlers. Two mutations checked: dropping the deferred delete →
ASan use-after-free; deleting the old backend under the lock → deadlock (timeout).

## Not done / for later

- VST3 distributed (`VST3P_API`) got the same hooks but is not built on Linux (no CMake target).
- APP: the main loop now blocks in `g_main_context_iteration` with a 10 ms wake for SWELL's polled timers and posted
  messages. SWELL's own modal loops still `Sleep(10)`. P8 owns the rest (audio-device handling).
- fd watches are unused until P3. The unit test covers fds on the fallback and a fake backend only; CLAP `posix-fd`, VST3
  `registerEventHandler` and GLib `g_unix_fd_add` registration are untested until the X fd exists.
