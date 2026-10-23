# T-038 — VST3 on Linux without a host run loop: host notifications from the fallback thread

- **Status:** merged d3107afc2 (review approved in round 2, `notes/36-review-T-038.md`)
  with minors in round 2 (nits fixed after it); `notes/36-review-T-038.md`.
- **Owner:** T-038 worker (lock `.linux-port-locks/T-038`, 2026-10-23)
- **Branch/worktree:** `linux/T-038` from `hvoya-linux` 901602a14, in `/Volumes/Storage/code/IPlug2-T-038`.
- **Depends on:** T-010, T-013 (host traces), T-025 (`SetLatency` race).
- **Conflicts:** `IPlug/VST3/*` (Linux parts), `IPlug/Linux/*` host loop.

## Problem
A VST3 host on Linux gives iPlug a `Linux::IRunLoop` either through the host context or only through the plug frame.
The hosts that give it only through the plug frame (Renoise 3.5.4, Qtractor 1.5.3, Carla) leave iPlug without a main
loop while the editor is closed, so iPlug's fallback thread runs the timers. On that thread:
- `SetLatency` → `IComponentHandler::restartComponent(kLatencyChanged)` is called off the UI thread, which breaks
  VST3's threading rule. Renoise queues it to its main thread; Qtractor 1.5.3 runs a `getState`/`setState` round
  trip on our thread; REAPER (which has a host-context loop, so not affected) ignores such calls; Carla ignores all
  latency changes.
- More widely, the whole `OnIdle` runs concurrently with the host's main-thread calls (state load,
  `setParamNormalized`), and anything it sends to the host (`beginEdit`/`performEdit`/`endEdit` from a MIDI-CC
  mediator, `restartComponent`) is off-thread too. On mac/win `OnIdle` is always on the main thread.
Evidence: T-010 Log (item 1), T-013 Log (Renoise case 7, Qtractor case 5/7, `notes/33`).

## Direction (to confirm in the worker's design note before coding)
VST3 has no way to ask the host for a main-thread callback. iPlug can only act on the next call the host makes on its
own UI thread (the thread that called `initialize`/`setHostContext`): any `IEditController`/`IComponent` call, state
load, `setActive`/`setProcessing`, the editor attaching. So:
1. Off the UI thread, iPlug queues host notifications (`restartComponent` flags; `beginEdit`/`performEdit`/`endEdit`)
   instead of calling the host, and flushes them at the next UI-thread entry. Lock-free/realtime-safe where the audio
   thread is involved; no allocation in `ProcessBlock`.
2. Decide what to do about `OnIdle` itself running concurrently with host UI-thread calls: e.g. run the fallback's
   `OnIdle` under the same guard as the host's UI-thread entry points, or document it as a plug-in requirement.
   Present the options with costs before choosing.
3. Measure in hosts: Renoise, Qtractor (VM), REAPER with the fallback forced (the P2 frame-only test build), Carla.
   PDC must follow a latency change with the editor closed at least by the next transport start; no host call from a
   non-UI thread (trace builds).

## Acceptance
- [x] Design note in the Log (options, costs, choice): deferral on the fallback thread + host-call gate (option B).
- [x] No `restartComponent`/`performEdit` from a non-UI thread in the trace builds, editor closed, in Renoise,
  Qtractor and REAPER-forced-fallback; latency change honoured (PDC) where the host supports it — at the host's next
  UI-thread call (REAPER makes none while idle; it and Qtractor read the latency at transport start). Carla not run.
- [x] mac/win unchanged (Linux-only code; IPlugEffect macOS-VST3 builds).
- [x] Independent review until approval with no major finding (2 rounds); docs (`linux-development.md` §8
  Qtractor/Renoise bullets, a §9 bullet) updated.

## Log
(append: date — what was done — commits — next)

### 2026-10-23 — claimed; survey; design note

Worktree `IPlug2-T-038` (`linux/T-038` from hvoya-linux 901602a14; SDKs copied with `cp -cR`, git-ignored).
GUI container `t038`.

**Survey: what REAPER calls on its UI thread with the editor closed** (IPlugEffect VST3 Debug,
`IPLUG_TEST_VST3_FRAME_RUNLOOP_ONLY` + `IPLUG_TEST_LATENCY_FROM_IDLE`; gdb breaks on all 155 `IPlugVST3`,
`EditController(Ex1)`, `SingleComponentEffect`, `ComponentBase` methods; `evidence/t038/survey-reaper/`):
- idle, 15 s: **nothing** on the main thread. Only `process` (audio threads) and `SetLatency` (fallback thread).
- `TrackFX_GetParamNormalized` → `getParamNormalized`; `TrackFX_SetParamNormalized` → `setParamNormalized`,
  `getParamNormalized` ×4, `getState`, `getEditorState`, `getSelectedUnit` (undo point); transport start →
  `setProcessing` ×2 and **`getLatencySamples`**, all on the main thread; stop, `pdc` query, save: nothing.
- So in REAPER a deferred notification waits for the next host action, but REAPER re-reads the latency at every
  transport start anyway, and `GetLatency()` is already the new value then.

#### Design

**1. The host's UI thread.** VST3 makes `initialize()` `[UI-thread]`. iPlug records the calling thread there, per
instance. In every host traced so far (REAPER, Ardour, Carla, Renoise, Qtractor, the SDK validator) the thread that
called `initialize` is the one that later runs the frame `IRunLoop` callbacks (`first … callback … on main` in the
T-013/P2 traces), so this is the host's UI thread. Calls on any other thread get no new behaviour (below), so a
host that breaks the contract and calls, say, `getParamNormalized` from its audio thread is never made to wait.

**2. Which calls are deferred, from which thread.** Only calls made **on this instance's fallback thread**
(`HostLoop::IsFallbackThread()`, a `thread_local` set by the fallback thread). Not "any thread but the UI thread":
- hosts with a loop (REAPER's factory context, Ardour, JUCE hosts) never start a fallback thread, so nothing changes
  for them at all;
- the audio thread and plug-in-owned threads keep today's behaviour, which is also mac/win's (their calls are the
  plug-in's own responsibility there too), and the queue needs no lock-free design.

Deferred host calls (grep of `IPlug/VST3/*`, `IGraphics` for `componentHandler`, `beginEdit`, `restartComponent`):
- `restartComponent` flags: `kLatencyChanged` (`IPlugVST3::SetLatency`), `kParamTitlesChanged`
  (`InformHostOfParameterDetailsChange`). OR-ed into one value, sent once.
- `beginEdit`/`performEdit`/`endEdit` (`Begin/Inform/EndInformHostOfParamChange`, used by `SetParameterValue`, the
  MIDI-CC mediator pattern, `DirtyParametersFromUI`): the **last value per parameter**, in an array of `NParams()`
  allocated in the constructor. A flush sends `beginEdit`/`performEdit`/`endEdit` per changed parameter, in one
  `startGroupEdit`/`finishGroupEdit` when there are several. Deferred `beginEdit`/`endEdit` themselves are dropped.
  Cost: gesture boundaries and intermediate values of edits made on the fallback thread are lost (the host sees one
  short gesture per parameter per flush). Gain: memory is bounded and nothing allocates on either thread, also when a
  host makes no call for an hour while `OnIdle` keeps editing (a call log would grow without bound).
- `startGroupEdit`/`finishGroupEdit` (`DirtyParametersFromUI`): dropped, see above.
- Not deferred: `IComponentHandler3::createContextMenu` (`IGraphics.cpp`) and `IPlugFrame::resizeView`
  (`EditorResize`): editor-open only, and with the editor open every known host gives the frame `IRunLoop`.
  `setDirty`/`requestOpenEditor`: iPlug never calls them. The distributed VST3 (`VST3P_API`) is not built on Linux and
  is left as it is.
- Implemented by overriding `EditController::beginEdit/performEdit/endEdit/startGroupEdit/finishGroupEdit` in
  `IPlugVST3` (Linux only), plus one deferral check before each of the two `restartComponent` calls.

**3. Where they are sent (flush points).** On the UI thread, at the start of the outermost host call of:
`getParamNormalized`, `setParamNormalized`, `getState`, `setState`, and the view's `attached`/`removed`.
- Not in `setActive`, `setupProcessing`, `setBusArrangements`, `setProcessing`, `initialize`/`terminate`: a
  `restartComponent` inside a processing-state transition livelocks Renoise (the comment in `IPlugVST3::SetLatency`).
- Not in `getLatencySamples`: hosts call it while handling the restart, and Renoise calls it on an audio thread.
- Only the outermost call flushes, so the calls a host makes inside our `restartComponent` (Qtractor 1.5.3:
  `getState`/`setState` and a parameter refresh) do not flush again.
- `attached` matters: from then on `OnIdle` runs on the UI thread, and anything still queued goes out at once.
- What is not covered: a host that makes none of these calls while the editor is closed (REAPER idle) gets the
  notification only at its next such call. The latency itself is never stale: `getLatencySamples` returns the new
  value as soon as `SetLatency` returns, and REAPER (transport start), Qtractor (transport start) and Renoise
  (processing resume) read it there.

**4. Realtime and thread safety of the queue.** The queue is touched only while holding the per-instance
host-call gate (point 5): the fallback thread holds it while it runs callbacks, the UI thread while it is inside a
guarded host call. So the queue needs no lock or atomic of its own. The audio thread never touches it: `process`,
`setProcessing` (VST3 allows the processing thread) and `getLatencySamples` take no gate and flush nothing. No new
allocation after construction.

**5. `OnIdle` concurrency with the host's UI-thread calls.** Options:
- **A. Document it** as a plug-in requirement ("`OnIdle` may run concurrently with `setState`, `setParamNormalized`,
  `OnReset`… when the host gives no loop"). Free in iPlug; every plug-in's `OnIdle` must then be thread-safe against
  every host call, which mac/win never require, so the races stay latent and Linux-only (a large `OnIdle` such as
  a MIDI-CC mediator, preset morphing or layout work is exposed).
- **B. A gate (chosen).** A recursive mutex per instance, in `HostLoop`. The VST3 wrapper holds it during each host
  call on the UI thread (point 1). The fallback thread runs a round of callbacks only if `try_lock` succeeds;
  otherwise the due timer ticks are dropped until their next interval, and ready fds are retried 2 ms later. Costs: the UI thread may wait for one running `OnIdle` (the
  same serialisation as on mac/win, where both share one thread); a skipped tick when they collide; one uncontended
  lock per guarded host call in every host (tens of ns; no contention where a host loop exists, because then there
  is no fallback thread). No deadlock: the fallback thread never blocks on the gate, so a UI thread that joins the
  fallback thread inside a guarded call (`attached`, `terminate` → `SetBackend`) cannot hang; while a fallback thread exists the
  lock order is gate → `HostLoop::mMutex` on both threads (with a host-driven backend the UI thread may take them the
  other way round, harmless with no second thread); and with point 2 the fallback thread never calls the host while it holds
  the gate.
- C. Run `OnIdle` from the flush points on the UI thread instead of the fallback thread. Then `OnIdle` stops
  whenever the host makes no call (REAPER idle: none at all), and plug-ins that apply settings in `OnIdle` (an
  oversampling change → `SetLatency`) stall. Rejected.
- D. A blocking lock on the fallback thread. Deadlocks when the UI thread holds the gate and joins the fallback
  thread (`attached` → `SetBackend`). Rejected.

Guarded entries (UI thread only): `initialize`, `terminate`, `setBusArrangements`, `setActive`, `setupProcessing`,
`setState`, `getState`, `getParamNormalized`, `setParamNormalized`, `setChannelContextInfos`, `createView`, view
`attached`/`removed`. Not guarded: `process`, `setProcessing`, `getLatencySamples`/`getTailSamples`, pure getters of
static data (`IUnitInfo`, `getMidiControllerAssignment`, `canProcessSampleSize`), and editor-open view calls
(sizing, keys), which run with a host loop in every known host.

**Not chosen, for the user (follow-up option): the host's GLib main context.** REAPER (SWELL/GDK), Ardour (GTK),
and Qt hosts built with the GLib event dispatcher (likely Qtractor and Carla) iterate the default `GMainContext` on
their UI thread. A plug-in could `dlopen("libglib-2.0.so.0", RTLD_NOLOAD)` (no new dependency: only when the host
already has it), check at `initialize` that the UI thread owns the default context, and run its timers there instead
of on the fallback thread. `OnIdle` would then be on the UI thread with the editor closed, as on mac/win, and nothing
would need deferring in those hosts. Costs: a new backend and its own host matrix; a host that loads GLib but
iterates it elsewhere, or not at all, must be detected; Renoise's toolkit is unknown. This task's deferral and
gate are needed anyway for hosts without it.

Next: implement (HostLoop gate + `IsFallbackThread`, VST3 deferral), unit tests, Docker, VM.

### 2026-10-23 (2) — implementation; REAPER with the fallback forced

Commits 136b82a9e (fix, tests, REAPER evidence), 0b09f76ce (trace lines, smoke script).

**Code** (all Linux-only; mac/win see two empty macro lines per entry point, i.e. nothing):
- `HostLoop` (`IPlugLinuxHostLoop.*`): `GetHostCallGate()` (a `std::recursive_mutex`) and `IsFallbackThread()`
  (a `thread_local` set by the fallback thread). The fallback thread dispatches a round only after `try_lock`; when
  the gate is busy it waits 2 ms on its wake fd and tries again.
- `VST3HostThread` (new `IPlug/Linux/IPlugLinuxVST3HostThread.h`): the UI thread from `initialize`; `Call` (RAII,
  gate + outermost-call flush, a no-op off the UI thread); `DeferRestart`, `DeferEdit`, `DeferGesture`; the flush
  (`SendDeferred`) and two trace lines (`IPLUG_TRACE_HOST_LOOP`).
- `IPlugVST3`: `VST3_HOST_CALL(kSendDeferred|kKeepDeferred)` at the entry points of the design note (the guarded
  list there, minus `terminate` and `createView`: `terminate` stops the fallback thread before anything else, and
  `createView` touches nothing `OnIdle` uses; `initialize` got its `Call` in review round 1, R1-1); the view's `attached`/`removed` hold a
  `Call` with `kSendDeferred`; overrides of `beginEdit`/`performEdit`/`endEdit`/`startGroupEdit`/`finishGroupEdit`;
  `DeferRestart` before the two `restartComponent` calls.
- Test switch `IPLUG_TEST_EDIT_FROM_IDLE` in `Examples/IPlugEffect` (inside the existing `IPLUG_TEST_LATENCY_FROM_IDLE`
  block): a gesture on the mix parameter with each latency change.

**Unit tests** (`Tests/LinuxHostLoop`, 4 new): the fallback runs no callback while the gate is held; backend swaps
inside a held gate with a ready fd never hang; deferred restart/edits are sent once, in order, on the UI thread, only
by an outermost `kSendDeferred` call, also with a host that calls back inside `restartComponent`; a `Call` off the UI
thread neither sends nor waits. ASan+UBSan pass; TSan (`setarch -R`) ×3 pass with no warning.

**REAPER, fallback forced** (`scripts/t038/reaper-deferred-test.sh`, Debug IPlugEffect VST3 with
`IPLUG_TEST_VST3_FRAME_RUNLOOP_ONLY`, `IPLUG_TEST_LATENCY_FROM_IDLE`, `IPLUG_TEST_EDIT_FROM_IDLE`; gdb finds REAPER's
own `IComponentHandler` methods through the plug-in's `componentHandler` vtable and logs every call with its LWP):

| phase (editor closed unless said) | before (hvoya-linux) | after |
|---|---|---|
| idle, 4 s, REAPER makes no call | `restartComponent(8)` ×7 from the fallback thread; `pdc` 0 throughout | `SetLatency` ×7 on the fallback thread, **no host call**; `pdc` 0 throughout |
| poll: `TrackFX_GetParamNormalized` every 0.5 s | ×12 off the main thread; `pdc` stays 0 | `restartComponent(8)`, `beginEdit`/`performEdit`/`endEdit` ×12, all on REAPER's main thread; REAPER re-reads `getLatencySamples` each time; **`pdc` follows: 256 0 256 0 …** |
| transport start | `getLatencySamples` on main | same |
| editor open (frame `IRunLoop`) | `SetLatency` and `restartComponent` on main | the attach sends what waited; then everything on main |

Evidence: `evidence/t038/reaper-deferred-{fix,base}/` (`summary.txt`, `calls-by-phase.txt`, `RESULT.txt`). The
"base" run uses the unfixed hvoya-linux build (no edit switch) with `expect=offthread`: the test does catch the bug.

### 2026-10-23 (3) — VM: Renoise 3.5.4 and Qtractor 1.5.3 (VST3, editor closed)

Gneiss VST3 Release + `IPLUG_TRACE_HOST_LOOP` from this branch (0b09f76ce; Gneiss linux-experiment 6246fb7, its tree
untouched), deployed to `~/.vst3`. Symbol lists for `call-trace.sh` regenerated (`scripts/vm/probe/t038/*.syms`,
`make-vst3-syms.sh`): the `HostLoop` gate moves the `IPlugVST3` thunk offsets (1320 → 1368, 1832 → 1880, …), so
T-013's lists would miss every host call. VM checked with `timeout 8 xdpyinfo` before and after every batch; no hang.

**Renoise** (`evidence/t038/vm-renoise/`; song `~/t013-renoise/t013.xrns`, Gneiss on track 1, oversampling =
parameter 26, set from Lua):
- **Playing** (`latency-playing-editor-closed.txt`): `SetLatency` on the fallback thread → `TVst3Plug::restartComponent(8)`
  on Renoise's **main** thread within the same second, with **no deferred lambda** (T-013: the call came off-thread and
  Renoise queued it) → `setProcessing(0)`, `setActive(0/1)`, `getLatencySamples`, `setProcessing(1)` on main. The flush
  came from Renoise's own `setParamNormalized` calls, which it makes continuously while playing (2310 in 30 s, all on
  main).
- **Stopped** (`latency-stopped-editor-closed.txt`): the restart waited 6 s, until the next parameter change from Lua
  (a `setParamNormalized` on main); Renoise then restarted and re-read the latency on main. Reading a value from Lua
  makes no plug-in call.
- **Transport start with a restart pending** (`latency-pending-at-transport-start.txt`): Renoise reads
  `getLatencySamples` on its audio thread as it resumes, and the pending restart goes out on main at once (the
  playing parameter sync), followed by a re-read on main.
- Trace (`stderr-session1.txt`): every "deferred" line is on the fallback tid, every "sent" line on the main tid.

**Qtractor** (`evidence/t038/vm-qtractor/`; session `~/t013-qtractor/t013vst3.qtr`, transport playing, Gneiss's
plug-in form open at Page 2 — it opened at Page 1 this time, so the first attempt typed into parameter 1
(`attempt1-wrong-page.txt`)):
- `latency-editor-closed.txt`: oversampling 8x, then 1x in the form: `SetLatency` on the fallback thread; the form's
  next `setParamNormalized` (main) sends the restart; Qtractor's state round trip `getState`/`setState` now runs on its
  **main** thread (T-013: on iPlug's fallback thread). Transport start: `getLatencySamples` on main.
- Trace: deferred on the fallback tid, sent on the main tid, twice.

Clean-up: Renoise quit (song not saved), its OSC server off again; Qtractor exited with "Discard" (session file
unchanged); my `jackd` stopped (the desktop's `jackdbus` was already running and was left alone); no gdb left. VM lock
released between batches.

### 2026-10-23 (4) — Docker regression smoke; macOS

`scripts/t038/smoke.sh` → `evidence/t038/smoke-r0/` (`summary.txt`), Release builds from 136b82a9e–0b09f76ce:
- `host-loop-test.sh`, trace + latency (+ edit) builds: IPlugEffect CLAP, IPlugEffect VST3, IPlugControls VST3 PASS
  (REAPER's factory loop: everything on main, `pdc` follows 0↔256, no "deferred" line in the VST3 trace, churn flat);
  IPlugEffect VST3 fallback-forced with `EXPECT_CLOSED=worker` PASS (threads 16→16, fds 8→8 over 50× churn; `pdc`
  stays 0 because REAPER makes no call while idle, as designed — see the REAPER table above).
- IPlugControls P3b, Gneiss and Sand steps in REAPER: all PASS in CLAP and VST3, except Gneiss VST3
  `g04b-context-menu` (the known synthetic-click limitation, T28-c; STATUS gotchas).
- macOS: IPlugEffect `macOS-VST3` Debug via `scripts/coord/mac-xcodebuild.sh`, all paths in the scratchpad: exit 0,
  no warning from the changed files. Xcode's `Examples/IPlugEffect/plug_build_info.hpp` removed afterwards.
- Unit tests at 5e8e667db: `evidence/t038/unit/asan-tsan-x3.txt`.

### 2026-10-23 (5) — review round 1 (approve with minors; `notes/36-review-T-038.md`) and fixes

- **R1-1:** `initialize` holds a `Call` (`kKeepDeferred`) right after `mHostThread.Initialize()`, so
  `OnHostIdentified`/`OnParamReset` cannot overlap the fallback thread's first `OnIdle`.
- **R1-2 (accepted risk, documented at `VST3HostThread::Initialize`):** a host that calls `initialize` on a thread
  other than its later UI thread would never get the deferred notifications (before, they went out off-thread). Every
  traced host (REAPER, Ardour, Carla, Renoise, Qtractor, the SDK validator) initializes on the thread that later runs
  its frame `IRunLoop`, and the other way round breaks VST3's own threading rules. No runtime diagnostic: a call on
  another thread cannot be told apart from a legitimate audio-thread call.
- **R1-3:** §9 now says that parameter changes from the fallback thread are merged (last value per parameter, one
  gesture).
- **R1-4:** design note and comments: dropped timer ticks vs retried fds; the lock-order claim qualified;
  `GetHostCallGate`/`IsFallbackThread` comments.
- **R1-5:** `DeferEdit`'s doc says a non-parameter tag is dropped.
- **R1-6 (watch item):** `getParamNormalized` is a flush point, so a host's getter can trigger a synchronous
  `restartComponent`; fine in REAPER, Renoise and Qtractor. If a host misbehaves there, it is the first flush point to
  drop.
- **R1-7 (evidence scope):** deferred `performEdit` was measured only in REAPER (forced fallback) and the unit tests;
  the Renoise and Qtractor runs changed only oversampling ("0 edit(s)"). Carla was not run (its source ignores VST3
  latency restarts, T-010).
- **R1-8:** `smoke.sh` prints each run's name before its RESULT; `smoke-r0/summary.txt` carries a note for the old
  order.
- **R1-9:** the task id is gone from the `IPlugEffect.cpp` comment.
- Re-checked at 5ac2d648f: unit tests pass; `reaper-deferred-test.sh` PASS (`evidence/t038/reaper-deferred-fix-r1/`,
  same picture as before: idle no host call, poll 12× all on main, `pdc` 256/0); plain `host-loop-test.sh` VST3 PASS
  with `pdc` following and no "deferred" line (`evidence/t038/hostloop-effect-vst3-r1/`).

### 2026-10-23 (6) — review round 2 (approve with minors, nits only) and fixes

- **R2-1:** `mHasDeferred` lets a flush return at once when nothing waits, so hosts that sweep `getParamNormalized`
  no longer scan every parameter per call.
- **R2-2:** the header names which host calls hold the gate.
- **R2-3:** §8's lead-in no longer says iPlug2 does nothing: it has no host-specific workarounds, and what the bullets
  describe is §9's general behaviour.
- Re-checked: unit tests ASan ×3 and TSan ×3 pass; `reaper-deferred-test.sh` PASS (`evidence/t038/reaper-deferred-fix-r2/`).

### 2026-10-23 (7) — handover

- Commits on `linux/T-038`: f1c385946 … eda432127 (`git log 901602a14..linux/T-038`). Merges cleanly into the current
  hvoya-linux (T-018 merged meanwhile; `git merge-tree`).
- **VM left with Release builds (no trace) from eda432127** (Gneiss linux-experiment 6246fb7, Sand 95be3b0) of Gneiss,
  Sand and IPlugControls in `~/.clap` and `~/.vst3` (`build/t038-final-*`). IPlugEffect there is still the older build.
- Renoise's and Qtractor's song/session files unchanged; Renoise's OSC server off.
- Open, for the user: the GLib-main-context option in the design note (OnIdle on the host's UI thread with the editor
  closed in GLib hosts, so nothing needs deferring there) as a possible follow-up; R1-6 (`getParamNormalized` as a
  flush point) is a watch item.
- Containers removed; T-038 and VM locks released.
