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
