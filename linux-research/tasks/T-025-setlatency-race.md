# T-025 — X1: `SetLatency` races the audio thread (crash; all platforms)

- **Status:** merged 2026-10-19 (hvoya-linux 1e0e4be70; ported to `hvoya` 8ca9f3324), approved by the user — **important** (user, 2026-10-17: "must be fixed")
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

### Review round 1 (independent reviewer)

2026-10-19. Reviewed 5876054e9 (fix) and eaafcfdeb (evidence), fresh context. **Verdict: accept.** No blocker or major findings. The minors below are for the author/coordinator to take or leave before the change goes to `hvoya`.

**Re-run by me (Docker, `build/t025r-*`, overlay recreated from `hvoya-linux`):**

| 5 runs × 3 s | before | after |
|---|---|---|
| ASan | 5/5 fail, all `SEGV NChanDelay.h:51` ← `PassThroughBuffers` | 0/5 |
| TSan (`setarch -R`) | 5/5 fail, 9 warnings each, 6–13 bad blocks | 0/5, no warnings, 0 bad blocks over 100k–245k changes per run |

- My own scratch test (`build/t025r-scratch/silence-count.cpp`) counts silent, input and mixed blocks for several setter patterns, then checks that the delay is exact again after the race. TSan was clean in all 4 patterns. Every pattern gave `bad=0` and `exact-after=ok`.
- macOS: IPlugEffect `macOS-VST3` Debug via `mac-xcodebuild.sh` (paths in the scratchpad) built with exit 0 and no warnings from `IPlugProcessor.cpp`. The tree stayed clean, and I removed `plug_build_info.hpp` afterwards.
- Evidence matches the Log. Base/fix stress summaries are as stated. The REAPER `latency.txt` files show `pdc` 0↔256 for CLAP and VST3, with RESULT PASS. Gneiss VST3 validator ×10 gives `537 passed, 0 failed` each. clap-validator gives 38/0. Gneiss REAPER CLAP PASS; Gneiss REAPER VST3 failed only `g04b-context-menu`; no crash in the gdb files. I opened `g02-os-cycle.png`.

**Design check (no defect found):**
- The flag is a correct acquire/release spinlock. The setter does `exchange(acquire)` … `store(release)`. The audio thread does one `compare_exchange_strong(acquire)` and on failure reads nothing and writes only its outputs. Both paths release the flag on every exit; nothing between take and release throws or calls out.
- **SetLatency on the audio thread:** no deadlock. The audio thread holds the flag only inside `PassThroughBuffers`, never across calls. It can at most spin briefly behind a non-audio setter, and it allocates, as before.
- **Re-entrancy:** none possible. Nothing inside the critical section calls the host. The VST3/AU/AAX/VST2/CLAP notifications happen outside `IPlugProcessor::SetLatency`'s flag.
- **Two non-audio setters at once:** the flag serialises them (see n1 for the value they leave behind).
- **`InitLatencyDelay`:** called only from constructors (`IPlugVST3ProcessorBase` ctor, `IPlugAU` ctor, `IPlugAAX` ctor), before any process call. The unique_ptr is never re-seated, so the unsynchronised `if (!mLatencyDelay)` reads are fine. CLAP, VST2, APP and WAM never create it.
- **Silence branch:** it zeroes `MaxNChannels(kOutput)` = `mChannelData[kOutput].GetSize()` pointers. That is the same pointer set and frame count that the plain-copy `IPlugProcessor::ProcessBlock` already zeroes or writes, so it adds no new out-of-bounds risk. In the `PLUG_SAMPLE_SRC` path (e.g. AU float) the zeros are cast-copied out as usual.
- **`mDTSamples == 0`:** it can no longer reach `NChanDelayLine::ProcessBlock`, which would compute `% 0`. The fix guards on `GetDelayTime()`.
- **Bit-identical when nothing races:** the delay length equals `mLatency` in steady state, because `SetLatency` sets both and `InitLatencyDelay` uses `GetLatency()`. So latency 0 → plain copy and latency >0 → the same delay, in VST3, AU and AAX bypass. With no delay line (no inputs) → copy, as before.
- **Host-facing latency:** the VST3, AU, AAX, VST2 and CLAP `SetLatency` overrides are untouched and still call `IPlugProcessor::SetLatency`. The `getLatencySamples`, AU `kAudioUnitProperty_Latency`, AUv3 `latency` and CLAP `latencyGet` getters read `GetLatency()` (a relaxed load of an int is enough). The VST3/CLAP "changed" comparisons still see the previous value.
- **`std::atomic<int> mLatency`:**
  - `mLatency(config.latency)` direct-initialises; that is fine on clang, GCC and MSVC.
  - `IPlugProcessor` was already non-copyable (unique_ptr member) and non-movable (user-declared dtor), so the atomics change nothing there.
  - Nothing takes `&mLatency`; there are no Windows-specific uses.
  - The CLAP `exchange(false)` is equivalent to the old test-and-clear.
- **AGENTS §2:**
  - The shared change is minimal.
  - The two new comments are WHY-comments.
  - No product names in code or comments.
  - C++17; style consistent.
  - `<atomic>` is already used in core (`IPlugQueue.h`, `IPlugParameter.h`, `IPlugWasmDSP.cpp`). `<thread>` (`std::this_thread::yield`) is new to `IPlugProcessor.cpp` but standard on every target.

**Findings:**

- **m1 — minor: a bypassed block can now go silent where it used to be a plain copy.** If `SetLatency` holds the flag when the audio thread arrives, the audio thread outputs silence, even when the latency is and stays 0. A plug-in that calls `SetLatency(sameValue)` repeatedly while bypassed (e.g. from `OnIdle` or `OnReset`) used to get an untouched copy at latency 0. Now it gets occasional dropouts.
  - Measured with an audio thread on a 2 ms period and `SetLatency(0)` every 200 µs: 3 silent blocks in 3,556 blocks / 24,133 calls (before: 0).
  - With the audio thread looping flat out: 473 silent blocks per 1,882 calls (this overstates real hosts).
  - Latency >0 already glitched on every `SetLatency` before the fix, because the resize clears the line, so that case does not change.
  - Cheap remedies, author's choice:
    - (a) in `ResizeLatencyDelay`, skip `SetDelayTime` when `GetDelayTime() == samples` (under the flag). This shrinks the window and also stops same-value calls clearing the line.
    - (b) and/or `if (mLatency.exchange(samples) == samples) return;` at the top of `IPlugProcessor::SetLatency`.
    - (c) and/or on contention, plain-copy instead of silence when `GetLatency() == 0`. `mLatency` is stored before the flag is taken, so it is the target value.
  - Not a blocker: it is far better than the crash, and it only affects bypass.
- **m2 — minor (evidence quality): the committed stress test's `bad-blocks=0` is a weak criterion for the fix build.**
  - With the hot-loop setter, 99% of fix-build blocks are silent (my count: 9.07M of 9.13M), and a silent block always passes.
  - The real proof is the ASan/TSan result plus the single-threaded exact-delay check, and that proof holds.
  - With a 1 ms paced setter, 99% of blocks carry the delayed input and stay correct (`random 3 1000`: 2.75M input blocks, 1.9k mixed, 0 bad).
  - Suggest adding the silent/input counts and a paced-setter mode to `latency-race-stress.cpp`, or saying this in the Log.
- **m3 — minor (bookkeeping): step 5's T-021 R2-1 note is not updated, and the R2-1 link to X1 is still a hypothesis.**
  - The x86_64 release validator crash did not reproduce at base (0/40), so the fix cannot be shown to cure it.
  - `CROSS_PLATFORM_FINDINGS.md` X1 still quotes "~1 из 5". It should say that the x86 rate was not reproduced here and that X1 is proven by the ASan stack and the stress test.
- **m4 — minor (evidence gap): no in-host latency change while the plug-in is bypassed.** Step 4 asks for REAPER oversampling changes while bypassed. The IPlugEffect PDC run and the Gneiss runs change latency while not bypassed. The stress test covers the race itself.
  - Also not compiled: AAX (no SDK), Windows, iOS and web. That is low risk (standard atomics and `this_thread` only), but the user should know before the `hvoya` merge.
- **n1 — nit: two concurrent setters can leave `mLatency` and the delay length different.** A stores 100, B stores 200, B resizes, A resizes → reported 200, delayed 100, until the next call. There is no crash, and it is unlikely in practice. Storing `mLatency` inside the flag's critical section (when a delay line exists) would make them consistent.
- **n2 — nit: stale comment `IPlugProcessor.cpp:539` "(if mLatency)".** The delay line's length decides now. Suggest "run the bypass delay on the PLUG_SAMPLE_DST buffers".
- **n3 — nit: the `SetLatency` doc could add that it may wait up to one audio block.** It does not deadlock, but callers on a UI thread may care.
- **n4 — nit: stray "Terminated" line.** `evidence/t025/stress-summary.txt` starts with a "Terminated" line left from killing the hung base run.

Containers: none left (`dock.sh` runs `--rm`). Scratch: `linux-research/build/t025r-{base,base-src,fix,scratch}` (git-ignored), mac build in the session scratchpad.

### 2026-10-19 (2) — review round 1 fixes
- **m1 + n1:** `SetLatency` with an unchanged latency no longer touches the delay line (`mLatency.exchange`), so a plug-in that keeps re-sending the same value while bypassed gets no silent blocks. The resize reads the latency under the flag, so concurrent setters leave the line at the value set last. Side effect: an unchanged `SetLatency` no longer clears the bypass delay line (it used to, incidentally).
- **m2:** the stress test counts silent blocks and takes a setter pause (`latency-race-stress [seconds] [maxLatency] [setterPauseUs]`; `run-stress.sh … [setterPauseUs]`). Re-run (`evidence/t025/stress2-*`, 5 runs × 3 s each):

| | ASan | TSan | blocks / silent / latency changes (one run) |
|---|---|---|---|
| fix, flat-out setter | 0/5 | 0/5 | 275 878 / 262 607 / 246 084 |
| fix, 500 µs pause | 0/5 | 0/5 | 279 024 / 37 390 / 3 768 |
| before, 500 µs pause | **5/5** | **5/5** | 3 551 / 402 / 83, 13 bad blocks |

  With the pause, silent blocks per change are the same before (~5) and after (~9): they are mostly the delay line refilling after a resize, which clears it (random latency up to 4096 = up to 16 empty blocks). The silence from contention alone was measured by the reviewer: 3 blocks in ~24k calls, and now 0 for unchanged values.
- **m3:** X1 row and proposal in `CROSS_PLATFORM_FINDINGS.md` updated; T-021 Log has a note on R2-1 (the x86_64 validator crash did not reproduce at base, so its link to X1 is a hypothesis).
- **m4:** left open: no host bypasses through the plug-in while its latency changes in our harness; AAX/Windows/iOS/web not compiled here. That goes to the user with the approval request; the `hvoya` port gets the mac product builds.
- **n2, n3, n4:** stale "(if mLatency)" comment, `SetLatency` doc ("waits for the audio thread to finish a bypassed block"), stray line in `stress-summary.txt`.
