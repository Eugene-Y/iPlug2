# T-025 — X1: `SetLatency` races the audio thread (crash; all platforms)

- **Status:** in progress — **important** (user, 2026-10-17: "must be fixed")
- **Owner:** coordinator (chat f91df715), lock `.linux-port-locks/T-025`
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

### 2026-10-19 — fix, before/after evidence

Worktree `IPlug2-T-025` (`linux/T-025` from hvoya-linux d5198e178; SDK folders symlinked from `IPlug2-linux`, git-ignored).

**Reproduction (step 1).**
- The validator methods don't reproduce it at a usable rate any more:
  - ASan Gneiss VST3 (aarch64, Debug, libasan preloaded): one `validator -e` run takes 14.5 min and passes 537/537 (`evidence/t025/vst3-validator-asan-base-one-run.txt`).
  - Release x86_64 archive (T-028 merge smoke build) `validator -e` ×40 under emulation: 0 crashes (`vst3-validator-x86-release-base/`; T-021's reviewer had 5/40).
  - The race window is a bypassed block that meets a `SetLatency`, and the validator rarely lines them up.
  - Mistake of mine, corrected: my first ASan validator ×20 counts were void. ASan in a non-ASan host needs libasan preloaded, and every run had stopped at load. `scripts/t025-validator-loop.sh` now preloads it and counts a run without the validator's "Result:" line as failed.
- **Deterministic stress test** instead (`evidence/t025/latency-race-stress.cpp`, `build-stress.sh`, `run-stress.sh`). It uses the real `IPlugProcessor`: one thread calls `SetLatency(random 0..4096)` in a loop, the other runs the bypass path (`AttachBuffers` + `PassThroughBuffers`) on 256-frame blocks of a constant input.
  - A block passes if every sample is the input or 0 (silence).
  - It also checks, single-threaded first, that the bypass output is the input delayed by exactly the latency (0, 100, 700, 1, across changes).
  - Built with ASan and with TSan (TSan run under `setarch -R`: TSan on aarch64 Docker refuses ASLR mappings). The "before" build is `git show hvoya-linux:` of the three changed files.

| 10 runs × 3 s | before (hvoya-linux) | after |
|---|---|---|
| ASan | **10/10 fail**: SEGV `NChanDelay.h:51` ← `PassThroughBuffers` (T-003's stack) | 0/10 |
| TSan | **10/10 fail**: 9 data races per run (`SetLatency`/`SetDelayTime`/`Resize` vs `ProcessBlock`), heap-use-after-free, 8–11 broken blocks; run 10 hung (killed after 23 min) | 0/10, no warnings, 0 broken blocks over 94k–188k latency changes per run |
| exact delay, single thread | ok | ok |

`evidence/t025/stress-summary.txt`, `stress-{base,fix}/`.

**Fix (5876054e9), iPlug2 shared code:**
- `IPlugProcessor`: `mLatencyDelayBusy` (atomic flag) guards `mLatencyDelay`.
  - `SetLatency` → `ResizeLatencyDelay` waits (yield) until the audio thread is out of the delay line, which takes one block at most, then resizes it.
  - `PassThroughBuffers` takes the flag with one compare-exchange and never waits. If the flag is taken, the block is silence (the resize clears the line anyway).
  - The delay line's own length decides between delay and copy, so a latency read mid-change can no longer index an empty buffer.
- `mLatency` and CLAP `mLatencyUpdate` are `std::atomic` (T-010 R1-2: hosts read the latency on their own threads).
- `NChanDelayLine::GetDelayTime()`.
- No allocation, lock or wait on the audio thread. The cost per bypassed block is one uncontended compare-exchange.
- Host-facing latency reporting is unchanged in every format (VST3 `restartComponent`, AU property, CLAP `latency.changed`, AAX): the wrappers' `SetLatency` overrides still call `IPlugProcessor::SetLatency`.
- `SetLatency`'s doc says it may allocate, so not from the audio thread, as before.

**Checks on the fix:**
- REAPER, IPlugEffect `IPLUG_TEST_LATENCY_FROM_IDLE`, editor closed: `pdc` follows 0↔256 in CLAP and VST3, every hit on the main thread, churn stable. `host-loop-test.sh` PASS ×2 (`evidence/t025/reaper-latency-{clap,vst3}/`).
- Gneiss Release (linux-experiment f799cb3 + this tree):
  - VST3 validator ×10: 537/0 each, no crash (`vst3-validator-gneiss-fix/`);
  - clap-validator 38/0 (`clap-validator-gneiss-fix.txt`);
  - `gneiss.txt` CLAP PASS, VST3 only `g04b-context-menu` (T28-c) (`reaper-gneiss-{clap,vst3}/`).
- Gneiss Debug+ASan VST3 built with the fix (`build/t025-gneiss-asan-fix`). The ASan validator is not repeated: 14.5 min per run and no usable rate, see above.
- macOS: IPlugEffect CLAP, VST3 and AUv2 Debug from this tree, `xcodebuild` with every path in the scratchpad: all built. Xcode's `plug_build_info.hpp` was removed afterwards (ignored file; it shadows the CMake one).

Not done here: AAX (no SDK); Gneiss/Sand mac builds against `hvoya` (that happens when the fix is ported to `hvoya` after the user's approval); a mac host OS change while bypassed (needs the user).

Next: independent review, then the user's approval of the shared-code change.
