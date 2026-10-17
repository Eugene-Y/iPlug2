# T-006 — clap-validator `state-reproducibility` failures

- **Status:** in-progress
- **Owner:** 2nd instance (T-006 session, 2026-10-17)
- **Branch/worktree:** `linux/T-006` from `hvoya-linux`, in `/Volumes/Storage/code/IPlug2-T-006`.
- **Depends on:** nothing.
- **Conflicts:** `IPlug/CLAP/IPlugCLAP.cpp`, `IPlug/IPlugAPIBase*`/state code — **shared with mac/win**.

## Goal
Find out why clap-validator's `state-reproducibility-*` tests fail (3 tests, all examples) on hvoya-based builds. Determine whether it is Linux-specific or affects the mac/win CLAP of Gneiss too. If it is a real bug, fix it in shared code without changing mac/win behaviour beyond the fix.

## Context
- Seen since the first hvoya trial (notes/03, notes/05) and in every phase since (notes/08, 10, 12, 15): 3 failures, identical diffs. Evidence example: `evidence/hvoya-trial/validators/*IPlugControls.clap.full.txt`.
- hvoya has its own CLAP state fixes: looped state write and deferred latency (commits 5ceb702de, eee9468ee on hvoya). RR's 360d84011 was **rejected by the user**; don't apply it.
- Likely suspects:
  - parameters not round-tripping exactly — float vs double, normalisation, or stepped/enum params;
  - non-deterministic state content: timestamps, `ISender` data, unset params;
  - params changed on load by `OnParamChange` side effects;
  - the hvoya chunk format.

## Steps
1. Reproduce on Linux (Docker): run clap-validator on IPlugEffect, IPlugControls and IPlugInstrument, with `--only-failed` and verbose output. Read the validator source for exactly what the test does (https://github.com/free-audio/clap-validator, `src/tests/plugin/state.rs` or similar).
2. Reproduce on **macOS** with a mac CLAP build of IPlugEffect/IPlugControls from `hvoya-linux` (scratchpad DerivedData, never `~/Library`). clap-validator on mac needs Rust: if `cargo` is unavailable, install via rustup into the user's home. Also try the Gneiss mac CLAP if one is already installed on the user's machine — read-only use.
3. Root-cause it with a minimal diff of the saved state before and after.
4. Fix, if it's a bug, in the narrowest place. **If the fix touches shared code (anything that also builds on mac/win), it changes mac/win behaviour, so it needs the user's decision (AGENTS §2):** commit it on `linux/T-006`, prove the mac build, describe the mac/win behaviour change in the task file and add a row to `CROSS_PLATFORM_FINDINGS.md`; set the status to `review-done — awaiting user decision`. The coordinator merges only after the user agrees. A Linux-only fix needs no decision.
5. Re-run the validators on Linux and mac.

## Acceptance
- [ ] Root cause documented with evidence (state dumps, validator output).
- [ ] Linux vs mac answer: does the Gneiss mac CLAP fail too?
- [ ] Fix committed with validator passes (shared-code fix: awaiting the user's decision, see step 4), or a documented reason why it's a validator/test artefact.
- [ ] mac build proof if shared code changed; self-review; Log complete.

## Log

### 2026-10-17 — root cause, fix, Linux + mac proof
- Claimed (lock `T-006`), worktree `IPlug2-T-006`, `install-sdks.sh`.
- **What the test does** (clap-validator 0.4.1, `src/tests/plugin_instance/state.rs`): randomise params via `params.flush`, save; new instance; read values, `state.load`, poll host callbacks, read values again. If the values changed and the plug-in did **not** call `clap_host_params.rescan(CLAP_PARAM_RESCAN_VALUES)` it fails with "values changed without a rescan request". Only after that come the value comparison with the saved instance and (`-binary`) byte equality of a re-save.
- **Root cause:** `IPlugCLAP::stateLoad` restores every param and never calls `params->rescan`. The diffs in every earlier run are "default vs loaded" (e.g. IPlugControls Gain 100 % vs 5.56 %). No iPlug2 fork has the call (upstream, RR, stillwell, tomtrnk: `grep rescan` empty). Not about rounding, chunk format, non-determinism or `OnParamChange` side effects: once the rescan is there, value equality and byte equality pass on all four plug-ins, Gneiss included.
- **Not Linux-specific:** the user's installed **Gneiss mac CLAP** (Oct 16, read-only) fails the same three tests with the same message (71 params). A mac IPlugEffect CLAP from `hvoya-linux` fails them too; with the fix it passes 44/44.
- **Fix** 46d10516c, `IPlug/CLAP/IPlugCLAP.cpp` `stateLoad`: after `OnRestoreState()`, `if (canUseParams()) paramsRescan(CLAP_PARAM_RESCAN_VALUES)`. Same thing clap-juce-extensions does when a JUCE processor's program/state changes. `stateLoad` is the only load entry point (no `state-context`, no `preset-load`). Thread: `stateLoad` is `[main-thread]`; Debug builds already terminate in clap-helpers' `clapStateLoad` thread check before reaching the call, Release does no checks.
- **Results** (`evidence/t006/README.md`):
  - Linux, before: IPlugEffect/IPlugControls/IPlugInstrument 3 state failures each. After: 0 / 0 / 1 failure (`param-fuzz-bounds` subnormal output, known since notes/03, not state). Gneiss (`linux-experiment` 4219350 against this branch): all 5 state tests pass; its other 4 failures are known (C3, C5, T-003 Log).
  - mac, IPlugEffect CLAP Debug via xcodebuild (DerivedData and `CLAP_PATH` in the scratchpad): before 3 failed, after 0 failed. No `.rc_mac` regenerated (`git status` clean).
  - REAPER 7.82 (Docker) with the new `scripts/steps/state-roundtrip.txt` (set param 0 → `GetTrackStateChunk` → move param 0 → `SetTrackStateChunk` → read back): Gneiss CLAP and IPlugControls CLAP PASS, REAPER alive, all draws on tid 1. IPlugControls' editor shows the restored 25 % after the load. REAPER passes param changes to a CLAP via process/flush, i.e. later, so the step file waits between changes.
- **mac/win behaviour change (needs the user's decision, AGENTS §2):** after every host-driven state load (project open, host preset/undo, `SetTrackStateChunk`) a CLAP host now re-reads all param values through `params.value`. Spec says the host does not record automation from it and the values take effect immediately. Before, hosts that cache values (generic param UI, automation lanes, Bitwig/Studio One modulators) could show the pre-load values until the next change of each param. No change to VST3/AU/AAX, the state format, or the audio path. Row **T6-1** in `CROSS_PLATFORM_FINDINGS.md`.
- **Related, not fixed:** CLAP has no `InformHostOfPresetChange` override, so a preset loaded inside the plug-in (preset strip, Gneiss PresetManager) also leaves CLAP hosts with stale values — that is C4's CLAP half. `rescan(VALUES)` from `InformHostOfPresetChange` (main thread) would be the analogous fix; out of scope here.
- **Side data point for C5 / T-003** (`process-sleep-process-status`, T-003 left the mac comparison to T-006): the installed mac Gneiss passes it 3/3; Linux Gneiss fails it 3/3 in **both Debug and Release** ("The plugin is sleeping but output port 0, channel 0 contains non-constant data (-22.45 dBFS)"). So it is not a build-config artefact, and C5's explanation ("Gneiss declares no tail") does not account for the mac pass. iPlug never sets `constant_mask`, so under `CONTINUE_IF_NOT_QUIET` the validator never counts the plug-in as sleeping; it only sleeps on `CLAP_PROCESS_SLEEP`, i.e. the finite-tail branch of `IPlugCLAP::process`. Possibly Linux-specific: for the coordinator to file (T-010 or a new task), not investigated further here.

### Review round 1 (independent reviewer)
Fresh-context review of 46d10516c, 5512f7be5, 0020953f9 (`git diff hvoya-linux...linux/T-006`).

**Re-ran myself:**
- Validator source (0.4.1, `src/tests/plugin_instance/state.rs:100-300`): the worker's reading is right. Second instance → `state.load` → `poll_callback` collects `ParamsRescan` events; if no `Values`/`All` rescan and any value differs from the instance's defaults, it bails with "changed without a rescan request" (l.240) before the value comparison (l.276) and the binary check. So without a rescan every plug-in whose randomised values differ from its defaults fails all three tests, whatever its serialisation quality.
- Linux (Docker), examples from this branch, **Release** (`build/t006r`) and **Debug** (`build/t006r-debug`, Maximal clap-helpers checks): IPlugEffect 0 failed, IPlugControls 0 failed, IPlugInstrument 1 failed (`param-fuzz-bounds`, subnormal) in both configs; all 5 state tests PASS everywhere. `state-invalid-random` WARNING on IPlugControls/IPlugInstrument is the same as in `before-*.txt` (random bytes accepted; pre-existing).
- Linux Gneiss Debug (`linux-experiment` 4219350 against this tree, `build/t006r-gneiss`): 5/5 state PASS; 4 known failures (`process-audio-double-*` ×2, `process-sleep-process-status`, `param-conversions`), matching `after-Gneiss.full.txt`.
- mac, scratchpad validator, `-t state`: `inst-base` IPlugEffect 3 FAILED / `inst-fix` 5/5 PASS. Both binaries are Debug (thread-check strings present); only the fix binary has a `paramsRescan` symbol. Installed mac Gneiss (read-only): 3 FAILED with "without a rescan request" — the bug is in the shipped mac CLAP, as claimed.
- REAPER 7.82: `state-roundtrip.txt` on the **Debug** IPlugControls CLAP (in-call rescan under Maximal checks, Terminate handler): PASS, REAPER alive, draws on tid 1; my `after-load.png` shows 25 %, as does the committed one. Looked at all four committed `before/after-load.png` (IPlugControls 90 → 25 %, Gneiss editor intact).
- Did not rebuild a pre-fix Linux binary (`build/t006-base` is **not** pre-fix: it passes the state tests). The Linux "before" rests on `before-*.txt` + every earlier phase's identical 3 failures + my mac before/after; that is sufficient.

**Findings**
1. *minor* — `evidence/t006/README.md:4` says "Debug builds". On Linux, `BUILD_TYPE=Debug scripts/dock.sh …` does **not** reach the container (only `DOCK_ENV` is passed through; `build-cmake.sh` defaults to Release): `build/t006-base/CMakeCache.txt` is Release, and the same command in my brief produced Release. The Gneiss REAPER gdb trace has no Debug template frames; the IPlugControls one does (`CheckingLevel)2`). Results are unaffected (I ran both configs), but the README should say which config each Linux file is, and the trap belongs in AGENTS §3 / `docker/README.md` (`DOCK_ENV="BUILD_TYPE=Debug"`).
2. *minor* — Log "Related, not fixed" (and T6-1's trailing note in `CROSS_PLATFORM_FINDINGS.md`): a CLAP `InformHostOfPresetChange` override would cover iPlug's own preset paths (`RestorePreset`, FXP/FXB, `IPlugPluginBase.cpp:890/908/976/1048/1108`) but **not** Gneiss's PresetManager, which never calls `InformHostOfPresetChange` (grep in `Gneiss-linux`: none); nor the morph undo path (`morph/morph_controller.hpp:864` calls `UnserializeState` directly). So for Gneiss the C4 fix still has to be in Gneiss (or C4's `DirtyParametersFromUI`), and the note should say so instead of naming the PresetManager as covered by the analogous fix.
3. *minor* — Log "Same thing clap-juce-extensions does": not quite. The JUCE wrapper's `stateLoad` (`cje/src/wrapper/clap-juce-wrapper.cpp:2432`) does not rescan; it rescans `VALUES` in `audioProcessorChanged(programChanged)` via `runOnMainThread` (l.656-667), i.e. only if the processor reports a change, and through the helper that defers when not on the main thread. Reword to "same call clap-juce-extensions makes on a program change".
4. *nit (design option for the user's decision)* — the rescan is synchronous inside the host's `stateLoad`. Spec-legal (`stateLoad` and `rescan` are both `[main-thread]`; `VALUES` is allowed while active, only `ALL` needs deactivation), re-entrancy is safe (`paramsValue`, `IPlugCLAP.cpp:506`, takes no lock; the params mutex in `UnserializeState` is released by then), and REAPER handles it in Release and Debug. The residual risk is a host that violates the spec and calls `state.load` off the main thread: Release has no checks, so the rescan would hit the host's handler on the wrong thread, where previously nothing was called. `runOnMainThread([&]{ if (!isBeingDestroyed() && GetClapHost().canUseParams()) GetClapHost().paramsRescan(…); })`, the pattern already used at `IPlugCLAP.cpp:71/135`, runs inline on the main thread (so the validator still passes) and defers otherwise. Not required; worth a line in the user's decision. Also unverified on mac/win DAWs: whether any host marks the project dirty on a `rescan(VALUES)` right after project open (spec says no automation, says nothing about dirty state) — a cheap check in Bitwig/REAPER mac when the user decides.
5. *nit* — Log "Debug builds already terminate in clap-helpers' `clapStateLoad` thread check" holds only when the host provides `thread-check` (`plugin.hxx:2335`); otherwise Debug does no check either.
6. *process* — Status is still `in-progress` and the acceptance boxes are unticked; per step 4 it should become `review-done — awaiting user decision` at hand-over.

**Checked, no finding:** diff is the minimal one in the only state-load entry point (no `state-context`/`preset-load` implemented; `UnserializeState` is called only in `stateLoad`); guarded by `canUseParams()` (which also satisfies clap-helpers' `assert(canUseParams())`); comment is a single WHY line, no restatement; removed trailing-whitespace line only; no VST3/AU/AAX/state-format/audio-path change; `Dependencies/IPlug/VST3_SDK/README.md` is in no commit (still an unstaged local change); commit messages carry the trailer; T6-1 row describes the mac/win change correctly and flags it as needing the user's decision; the step file is generic (FX 0, param 0) and its waits are explained. The REAPER step is a no-regression/integration check rather than a before/after discriminator (REAPER reads values via `params.value` on demand); the Log does not claim otherwise.

**Verdict: pass with minors.** The root cause and fix are correct and proven on Linux and mac; findings 1–3 are documentation accuracy, 4–5 are input for the user's decision, none blocks. Merge still waits for the user's decision on T6-1 (shared code).
