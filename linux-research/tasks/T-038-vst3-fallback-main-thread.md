# T-038 — VST3 on Linux without a host run loop: host notifications from the fallback thread

- **Status:** ready (user 2026-10-23: fix it; T-010 item 1, re-opened by T-013)
- **Owner:** —
- **Branch/worktree:** `linux/T-038` from `hvoya-linux`.
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
