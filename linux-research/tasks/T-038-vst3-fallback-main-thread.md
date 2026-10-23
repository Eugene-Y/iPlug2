# T-038 — VST3 on Linux without a host run loop: host notifications from the fallback thread

- **Status:** in progress (user 2026-10-23: fix it; T-010 item 1, re-opened by T-013)
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
- [ ] Design note in the Log (options, costs, choice).
- [ ] No `restartComponent`/`performEdit` from a non-UI thread in the trace builds, editor closed, in Renoise,
  Qtractor and REAPER-forced-fallback; latency change honoured (PDC) where the host supports it.
- [ ] mac/win unchanged (Linux-only code).
- [ ] Independent review until approval with no major finding; docs (`linux-development.md` §8 Qtractor/Renoise
  bullets) updated.

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
  otherwise it skips the round and retries 2 ms later. Costs: the UI thread may wait for one running `OnIdle` (the
  same serialisation as on mac/win, where both share one thread); a skipped tick when they collide; one uncontended
  lock per guarded host call in every host (tens of ns; no contention where a host loop exists, because then there
  is no fallback thread). No deadlock: the fallback thread never blocks on the gate, so a UI thread that joins the
  fallback thread inside a guarded call (`attached`, `terminate` → `SetBackend`) cannot hang; the lock order is
  gate → `HostLoop::mMutex` on both threads; and with point 2 the fallback thread never calls the host while it holds
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
