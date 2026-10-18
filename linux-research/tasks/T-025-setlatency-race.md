# T-025 — X1: `SetLatency` races the audio thread (crash; all platforms)

- **Status:** ready — **important** (user, 2026-10-17: "must be fixed")
- **Owner:** —
- **Branches/worktrees:** iPlug2 `linux/T-025` from `hvoya-linux`, in `/Volumes/Storage/code/IPlug2-T-025`. Gneiss local `linux-experiment` only if the plug-in side must change. The fix is cross-platform: it goes to the user before merging and, once approved, to `hvoya` as well (as T21-2 did).
- **Depends on:** nothing.
- **Conflicts:** `IPlug/IPlugProcessor.*`, `IPlug/Extras/NChanDelay.h`; T-010 item 1 (latency without a host loop) touches the same path — coordinate, or fold T-010 item 1 into this task.

## The bug
`CROSS_PLATFORM_FINDINGS.md` X1. `IPlugProcessor::SetLatency` (`IPlugProcessor.cpp:272`) calls `NChanDelayLine::SetDelayTime` (`NChanDelay.h:26`), which reallocates and clears the delay buffer with no synchronisation, while the audio thread may be inside `NChanDelayLine::ProcessBlock` via `PassThroughBuffers` (`IPlugProcessor.cpp:497`; the bypass path in VST3/AU/AAX). `mDTSamples`, the buffer pointer and `mWriteAddress` change under the reader: null or out-of-bounds reads and writes.

Who calls `SetLatency` off the audio thread:
- Gneiss: `Gneiss::OnIdle` (`Gneiss.cpp:~1288`) on an oversampling change. On Linux, when the host gives no run loop, `OnIdle` runs on iPlug's fallback timer thread, concurrently with `process`. A host that hands out its IRunLoop only through the editor window does the same while the editor is closed. On mac/win `OnIdle` is a UI-thread timer, but the race with the audio thread is the same.
- Sand: `Sand::updateOversampling` from `OnReset`.
- Smaller, formal races on the same path (T-010 review R1-2): on the fallback thread CLAP's `SetLatency` writes `mLatency` and the plain `bool` `mLatencyUpdate`, which the main thread reads in `latencyGet` and `activate`. Make them atomic in the same fix.

## Evidence
- T-003: ASan in the VST3 validator, `evidence/t003/validators/vst3-asan-latency-race.txt`: SEGV on address 0 in `NChanDelayLine<double>::ProcessBlock` ← `PassThroughBuffers` ← `IPlugVST3ProcessorBase::Process` ← `Steinberg::Vst::ProcessTest::run`.
- T-021: the release x86_64 Gneiss VST3 crashes the Steinberg validator `-e` about 1 run in 5–8 in an idle container (the T-021 reviewer: 3/8 smoke-step runs, 5/40 validator runs), in or right after "Process function running in another thread" (`evidence/t021/smoke-r2/gneiss-x86-64-paid-ubuntu22.04-amd64/vst3-validator.txt`). There is no stack yet: gdb cannot run programs under Rosetta, and an `LD_PRELOAD` SIGSEGV handler hides the crash (0/55), so do not use one to measure.

## Steps
1. Reproduce at a known rate before changing anything: on aarch64, an ASan build of Gneiss VST3 + `validator -e` in a loop (T-003 command, T-003 Log); on x86_64, the release archive `validator -e` ×40 in `iplug2-rt:ubuntu22.04-amd64` with no preload (count the crashes).
2. Fix in iPlug2 so that every plug-in is safe, not only Gneiss. Options, best first:
   - **The audio thread applies the change.** `SetLatency` stores the new latency (atomic); the processor applies it to `mLatencyDelay` at the start of the next block, before `PassThroughBuffers`. The host notification stays where it is now. Check that the new size never allocates on the audio thread: preallocate to a maximum, or swap in a buffer prepared off-thread.
   - Double buffering with an atomic swap and deferred free.
   - At minimum, `SetDelayTime` must never leave `mBuffer`/`mDTSamples`/`mWriteAddress` inconsistent while `ProcessBlock` reads them.
   Keep the latency the host sees unchanged in every format (VST3 `restartComponent(kLatencyChanged)`, AU property, CLAP `latency.changed`, AAX).
3. Gneiss/Sand: nothing should be needed once the framework is safe. Check that their `SetLatency` calls stay where they are, and fold in T-010 item 1 (latency reporting without a host loop) if it is still open.
4. Prove it: the same loops as step 1 show 0 crashes (ASan clean, x86_64 0/40); clap-validator and the VST3 validator otherwise unchanged; REAPER: Gneiss oversampling changes while playing and while bypassed, CLAP and VST3, latency reported correctly (REAPER's PDC display). macOS: build IPlugEffect CLAP and Gneiss (VST3, AU) with xcodebuild (scratch DerivedData/DSTROOT, AGENTS §2), and run an AU/VST3 OS change in a mac host if the user can.
5. Update `CROSS_PLATFORM_FINDINGS.md` X1 and the T-021 Log's R2-1 note.

## Acceptance
- [ ] Reproduced before the fix at a stated rate; 0 crashes after it, by the same method.
- [ ] Every format reports latency changes as before.
- [ ] No allocation or lock on the audio thread.
- [ ] mac builds; shared-code change approved by the user before merging.
- [ ] Independent review (AGENTS §4).

## Log
(append: date — what was done — commits — next)
