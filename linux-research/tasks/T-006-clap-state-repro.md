# T-006 — clap-validator `state-reproducibility` failures

- **Status:** ready
- **Owner:** —
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
