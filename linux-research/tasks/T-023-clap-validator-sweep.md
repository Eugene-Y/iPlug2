# T-023 — CLAP: every clap-validator failure of Gneiss and Sand, Linux and mac

- **Status:** in progress — review next; two shared-code proposals wait for the user
- **Owner:** coordinator (chat f91df715), lock `.linux-port-locks/T-023`
- **Branch/worktree:** `linux/T-023` from `hvoya-linux`, in `/Volumes/Storage/code/IPlug2-T-023`. Gneiss/Sand side: local experiment branches in their own worktrees (AGENTS §4), never the main checkouts.
- **Depends on:** T-006 merged (its state fix removes 3 failures from every list below).
- **Conflicts:** `IPlug/CLAP/IPlugCLAP.*` (shared with mac/win); Gneiss and Sand sources.
- **Scope narrowed (user, 2026-10-17):** Sand's cross-platform items (F1, F3, F4, F5 = `process-varying-sample-rates`, `param-conversions`, `process-audio-double-*` for Sand) are fixed in **T-024**. This task covers Gneiss, the Linux-only items (Gneiss `process-sleep-process-status`), the iPlug CLAP items below, and a final re-run of the validators on Sand after T-024 is merged. Run it after T-024, and not in parallel with T-010 (both may touch `IPlug/CLAP` and Gneiss `linux-experiment`).

## Why
The user (2026-10-17): CLAP has needed fixes "here and there" over time. Instead of meeting them one by one in hosts, sweep everything clap-validator finds in the two products at once, on both platforms, and fix or decide each item.

## Known failures (clap-validator 0.4.1, after T-006)
| Test | Who | Platform | What is known | Where it belongs |
|---|---|---|---|---|
| `process-audio-double-{in,out}-of-place` | Gneiss, Sand | Linux + mac | "overwritten an input buffer": the plug-in writes into the host's input buffer; in 64-bit mode iPlug passes host pointers straight through | C3 in `CROSS_PLATFORM_FINDINGS.md`; plug-in side, or an iPlug copy for 64-bit |
| `param-conversions` | Gneiss, Sand | Linux + mac | text→value round trip, `StringToValue` = `atof` ("1.06 kHz" → 1.06) | C5; RR's 360d84011 is **rejected**, don't apply it |
| `process-sleep-process-status` | Gneiss | **Linux only** (mac installed build passes 3/3; Linux fails 3/3 in Debug and Release) | "sleeping but output non-constant"; iPlug never sets `constant_mask`, so the plug-in can only "sleep" via `CLAP_PROCESS_SLEEP` (finite-tail branch of `IPlugCLAP::process`) | T-006 Log; find out first whether it is a port bug |
| `process-varying-sample-rates` | Sand | Linux (mac not run yet) | not analysed | `evidence/t012/validators/clap-validator-Sand.clap.txt` |
| `param-fuzz-bounds` | IPlugInstrument (example only) | Linux | subnormal output | notes/03; low priority |
| `state-invalid-random` (warning) | IPlugControls, IPlugInstrument | Linux | 1 MB of random bytes loads "successfully" | check Gneiss/Sand too |

Also, not from the validator:
- CLAP has no `InformHostOfPresetChange` override: presets loaded by iPlug itself leave CLAP hosts with stale values. Gneiss's PresetManager and morph undo bypass it anyway (C4, T-006 Log).
- `IPlugCLAP::FlushParamsIfNeeded` calls `paramsRequestFlush()` without `canUseParams()`. That asserts in Debug on a host without `clap.params`.

## Steps
1. Build Gneiss and Sand CLAP on Linux (Docker, Debug **and** Release; `DOCK_ENV="BUILD_TYPE=Debug"`, see AGENTS §3) and on mac. For the mac, build xcodebuild from the experiment worktree with `IPLUG2_ROOT=<your iPlug2 worktree>`, `-derivedDataPath` and `CLAP_PATH` in your scratchpad; otherwise its `../../IPlug2` resolves to the main checkout. The user's installed builds under `~/Library/Audio/Plug-Ins/CLAP/IPlug2Dev/` serve as a read-only "before".
2. Run the full `clap-validator validate` on each, Linux and mac, and keep the full logs in `evidence/t023/`. Validator on mac: Homebrew `rust` is installed; build 0.4.1 from `https://github.com/free-audio/clap-validator` (tag `0.4.1`) with `cargo build --release --locked` in your scratchpad (T-006 Log).
3. For each failure: root cause, Linux-only or cross-platform, and which repo it belongs to (iPlug2 shared code, the Linux port, Gneiss, Sand).
4. Fix:
   - Linux-only port bugs: fix in `linux/T-023`.
   - Shared iPlug2 code and the products' business logic: AGENTS §2. Commit a proposal, describe the mac/win behaviour change, add a row to `CROSS_PLATFORM_FINDINGS.md`, and wait for the user's decision before merging.
5. Re-run the validators. Also do a short REAPER check on Linux (`scripts/steps/state-roundtrip.txt`, the products' steps files).

## Acceptance
- [ ] Full validator logs for Gneiss and Sand, Linux and mac, before and after.
- [ ] Every failure and warning has a root cause, a platform answer and an owner (fixed / user decision / won't fix with reason).
- [ ] Linux-only fixes committed; cross-platform ones written up as proposals for the user.
- [ ] Independent review (AGENTS §4); Log complete.

## Log

### 2026-10-19 — sweep done: Gneiss and Sand clean everywhere; two iPlug CLAP proposals

Worktree `IPlug2-T-023` (`linux/T-023` from hvoya-linux 78253dd4e). clap-validator 0.4.1 on both platforms (mac: the binary T-006 built, copied into this session's scratchpad).

**Before** (`evidence/t023/*-before-*`): Gneiss linux-experiment f799cb3 and Sand linux-experiment 87ccdbf against hvoya-linux; mac against `hvoya` 8ca9f3324 from temporary detached worktrees of their `master` (c6f4442, 0b71747), `xcodebuild` Debug with every path in the scratchpad.

| | Linux Debug | Linux Release | mac Debug |
|---|---|---|---|
| Gneiss | 38 / 0 failed / 0 warnings | 38 / 0 / 0 | 38 / 0 / 0 |
| Sand | 38 / 0 / 0 | 38 / 0 / 0 | 38 / 0 / 0 |

(6 skipped everywhere: tests for extensions the plug-ins don't implement.)

Every failure in this task's table is gone:
- `process-audio-double-*`, `param-conversions`: C3/C5, fixed in T-024 (Sand) and T-028 (Gneiss).
- `process-varying-sample-rates` (Sand F3): fixed in T-024.
- Gneiss `process-sleep-process-status` "Linux only": passes 3/3 configurations after T-028 declared a tail (the cause was "no tail" plus timing, T-028 Log).
- Sand once showed a `process-audio-denormals` **warning** (2.21× slower on denormal input, one Release run from the IPlug2-linux tree earlier today). It did not recur in the 3 later Sand runs. It is a timing measurement, not a failure; noted only.
- `state-invalid-random` (warning) and `param-fuzz-bounds`: examples only (IPlugControls, IPlugInstrument), not in Gneiss or Sand. Left as they are (examples, low priority).

**Changes on `linux/T-023`:**
- `ea94dc457` `scripts/known-clap-failures/Sand.txt` emptied (it still listed F3–F5); Gneiss's was emptied by T-028.
- `abfe168fc` **proposal, shared code (mac/win CLAP), waits for the user:**
  - `IPlugCLAP::InformHostOfPresetChange`: new override asking the host to rescan values (`params.rescan(CLAP_PARAM_RESCAN_VALUES)` on the main thread), like `stateLoad` does since f71ddd876. Before it, a preset restored by iPlug itself (`RestorePreset`) changed every parameter while CLAP hosts kept showing and automating the old values. Gneiss and Sand restore through their own preset managers, not this path (C4), so this matters for plug-ins that use iPlug's presets.
  - `FlushParamsIfNeeded`: `paramsRequestFlush` only when `canUseParams()`; clap-helpers asserts on that call in Debug with a host without `clap.params`.
  - Not exercised at runtime: no plug-in here restores iPlug presets from its UI. Compiled on Linux and mac.

**After** (`evidence/t023/*-after-*`, this branch): Gneiss and Sand Linux Debug + Release 38/0/0; mac IPlugEffect CLAP built from this tree, 38/0/0.

Next: independent review; then the user's decision on `abfe168fc`.

### Review round 1 (independent reviewer)

Reviewed `hvoya-linux...linux/T-023` (abfe168fc, ea94dc457, 60188aeba), the evidence logs, clap-helpers `plugin.hxx`/`host-proxy.hxx`, the CLAP `params.h`, and the T-024/T-028 Logs. No Docker containers left behind. My test files are in `linux-research/build/t023r*` (git-ignored).

**Re-run and runtime checks:**
- IPlugEffect CLAP Debug from this tree (`build/t023r`): clap-validator 0.4.1 gives 44 run, 38 passed, 0 failed, 0 warnings, 6 skipped.
- `InformHostOfPresetChange` and the `FlushParamsIfNeeded` guard, run for real. I built a copy of IPlugEffect with hooks chosen by an env var, plus a small headless CLAP host with `thread-check`. The host logs each `params.*` call, its thread, and `get_value(0)` at rescan time. Files: `build/t023r-presettest/` (`host.cpp`; `run.txt` = this branch; `run-base*.txt` = the pre-change tree, exported with `git archive abfe168fc^`).

| Scenario | Before (abfe168fc^) | This branch |
|---|---|---|
| `LoadStateFromFXP` from `OnActivate` (main thread) | no rescan | `rescan(0x1)` on main, synchronous; `get_value` already returns the loaded value |
| `InformHostOfPresetChange` from `ProcessBlock` (audio thread) | — | `request_callback` from the audio thread, then `on_main_thread`, then `rescan` on main |
| `LoadStateFromFXP` from `OnIdle` on iPlug's plug-in timer thread | — | queued, then `rescan` on main with the loaded value |
| Host without `clap.params`, `Begin/Inform/EndInformHostOfParamChange` while not processing | **Debug: assertion `canUseParams()` (exit 134); Release: SIGSEGV (exit 139)** | no call, no crash; no rescan |

**Code:** correct against the spec.
- `rescan` is `[main-thread]`. Only `CLAP_PARAM_RESCAN_ALL` requires the plug-in to be deactivated, so `VALUES` is legal while active, processing or activating.
- `runOnMainThread` calls the callback directly when the host's `thread-check` says main. Otherwise it locks a mutex, queues the callback and calls `request_callback`. `clapDestroy` drains the queue with `_isBeingDestroyed` already set, so the lambda's guard holds.
- No double rescan: `stateLoad` never reaches `InformHostOfPresetChange`, and each FXP/FXB loader calls it once.
- No product names in the code.

**Findings**

- **M1 (major): the impact written up for the user's decision is wrong in both directions.**
  - `RestorePreset` never calls `InformHostOfPresetChange` (`IPlugPluginBase.cpp:423-449`). Only `LoadPresetFromFXP` (:890, :908), `LoadStateFromFXP` (:976) and `LoadBankFromFXB` (:1048, :1108) do. So the case named in the Log and the code comment, "a preset restored by iPlug itself (`RestorePreset`)", is **not** fixed. That includes `IVBakedPresetManagerControl` and host program changes.
  - The claim "Gneiss and Sand restore through their own preset managers, not this path" is also wrong for Gneiss. hvoya `PresetManager` loads user presets with `LoadStateFromFXP`: `loadFromFile`, `preset_manager.hpp:428`, and `applyIdx`, :901. With this proposal, Gneiss on mac/win CLAP asks the host to rescan values after every user-preset load and every strip step onto a user preset. Factory presets (`RestorePreset`, :894) still don't. Sand has no preset manager and is unaffected.
  - The same error is in `CROSS_PLATFORM_FINDINGS.md:43` ("его PresetManager не зовёт InformHostOfPresetChange").
  - Fix: correct the Log bullet and the commit rationale, and state the Gneiss behaviour change for the user. Whether `RestorePreset` should also inform the host is a separate question for the user. It is reached from AU/VST2/VST3 program-change paths too.
- **m1: the `FlushParamsIfNeeded` bug is a Release crash, not only a Debug assertion.** `HostProxy::paramsRequestFlush` (`host-proxy.hxx:374-377`) dereferences a null `_hostParams` in Release (reproduced: SIGSEGV). It happens on a host without `clap.params` whenever the UI changes a parameter while the plug-in isn't processing. The fix is right; the Log and the commit message understate it.
- **m2: in Gneiss the rescan comes before the workspace restore** (follows from M1). With a `thread-check` host (REAPER, Bitwig) the rescan runs synchronously inside `LoadStateFromFXP`. `applyIdx` restores the CC map and the workspace only afterwards. The workspace restore sets `par_oversampling` and the morph params through `setParamAppliedByCaller` (`Gneiss.cpp:226`, `Gneiss.hpp:585`), which does not inform the host. So the host reads the file's oversampling/morph values while the plug-in runs the live ones. This is display-level staleness until the next change, but it is a Gneiss mac/win difference the user should see next to M1. It is fixed by the product-side C4 fix (notify after the manager has finished), or by iPlug always deferring this rescan.
- **m3 (acceptance 1): no mac "after" logs for Gneiss and Sand**, only IPlugEffect. That is defensible: no validator test reaches `InformHostOfPresetChange`, and the validator host has `clap.params`, so the new guard is a no-op there. But say so in the Log, or run them.
- **m4 (step 4): no `CROSS_PLATFORM_FINDINGS.md` row for the proposal** `abfe168fc`, and line 43 still describes the issue as unfixed (and is wrong, see M1).
- **m5 (acceptance 2): the Sand `process-audio-denormals` warning has no log, root cause or owner.** I could not verify the "2.21×" run: no log was kept. The cause is already known (T-024 Log: Sand sets no flush-to-zero, README "TODO subnormals"). Write cause = no FTZ/DAZ in Sand; it is load/timing-dependent and cross-platform; owner = Sand; won't fix now.
- **m6 (acceptance 2): `state-invalid-random` is put down to "examples only", but the cause is iPlug's default `UnserializeParams`** (`IPlugPluginBase.cpp:128-149`). It reads raw doubles with no magic or size check, so every plug-in on the default state path accepts random bytes. Gneiss and Sand pass because they override `UnserializeState`. Owner: iPlug2 (upstream), won't fix, low priority.
- **m7: step 5's short REAPER check on Linux is neither done nor waived in the Log.**
- **n1: the 6 skipped tests are iPlug-wide, not the plug-ins' choice.** They are:
  - 3 `preset-discovery-*` (no preset-discovery factory);
  - `layout-audio-ports-activation` and `layout-configurable-audio-ports` (extensions not implemented);
  - `process-note-wildcard` (iPlug's note ports declare only `CLAP_NOTE_DIALECT_MIDI`, `IPlugCLAP.cpp:877/884`).
- **n2: the new comment in `InformHostOfPresetChange` names the wrong callers** ("a preset restored by iPlug"; the callers are the FXP/FXB loaders) and repeats `stateLoad`'s reason. Shorten it to an accurate WHY, or drop it.
- **n3: thread note.** iPlug's callers are file loaders on the UI thread. A plug-in that calls this from `ProcessBlock` works (tested), but `runOnMainThread` locks a mutex and may allocate there, the same as the existing `SetLatency`/`FlushParamsIfNeeded` patterns. Acceptable; a doc line at most.
- **n4: Gneiss `process-sleep-process-status` cause and platform answer.** Per T-028, the cause is "the validator host implements `tail`, iPlug reported 0, and the ring length depends on the random starter preset", not "timing". Platform answer: cross-platform, not a port bug. Say so.
- **n5: leftover detached worktrees** `hvoya/Gneiss-t023mac` and `hvoya/Sand-t023mac` (called temporary in the Log). Remove them at hand-over.

**Evidence checked:** all 11 logs read 44 run / 38 passed / 0 failed / 0 warnings / 6 skipped. `process-sleep-process-status`, `process-audio-double-*`, `param-conversions`, `process-varying-sample-rates`, `process-audio-denormals`, `state-invalid-random` and `param-fuzz-bounds` show PASSED in every log. Gneiss "before" f799cb3 already contains T-028's tail (5b31aa0), so "before" here means after T-024/T-028. The Log says this.

**Verdict:** the code in `abfe168fc` is correct and safe, and I recommend it to the user as a proposal. Changes requested on the write-up before hand-over: M1 (the user would decide on a wrong impact statement), then m1, m2, m4–m6. m3 and m7 need either a run or a sentence in the Log.
