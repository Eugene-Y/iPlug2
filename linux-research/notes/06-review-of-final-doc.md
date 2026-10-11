# Review of LINUX_PORT_RESEARCH.md (2026-10-11)

Fact-check of the final document against notes/01–05, `evidence/`, the worktrees in `wt/` and the git
branches. notes/05 wins where it conflicts with 03. [C] = checked in code or git, [E] = checked in
evidence files, [R] = re-run by me.

## Verified OK (spot checks)

- The upstream `IGraphicsLinux.h:25` stub (`virtual ~IGraphicsLinux);`) and the CMake "to be added later" placeholders [C].
- hvoya vs master: 242 behind / 207 ahead, merge base f98027a70. `merge-tree` hvoya←master gives 1 conflict (`IPlugAPP_main.cpp`). hvoya←RR gives 4 files [C].
- The cherry-pick has 22 commits; 11 conflicted files across 4 commits [E `hvoya-trial/cherry-pick.log`].
- The start-race code exists in both RR and the trial:
  - `IGraphicsLinux.cpp:1777-1778`;
  - `IPlugTimer.cpp:158-159` [C].
- `IGraphics.cpp:1987` is `mInTextEntry->SetDirty(false)` in the trial worktree [C].
- `IPlugLogger.h:147` on hvoya is `snprintf(&cStr[i], 32, …)`. RR has `(size_t)(32 - i)` [C].
- VST3 validator: 537/537 for every build [E]. clap-validator fail counts for RR and SW match the doc [E].
- Gneiss `OnIdle` → `SetLatency` (`Gneiss.cpp:1273-1295`) [C].
- The plan table, risks and recommendation match notes/04 §8.

## Findings

1. **[wrong]** §2 clap-validator, trial column: "3 (one fewer — hvoya's state-write fix helped)".
   - The failure that went away is `layout-audio-ports-config` (it PASSES on the trial: `hvoya-trial/validators/clap-validator-IPlugControls.clap.full.txt:20,250`).
   - All three `state-reproducibility-*` tests still fail, with the same parameter diffs as RR.
   - RR already has its own looped state write (360d84011). The trial kept hvoya's equivalent, so that fix cannot explain the difference.
   - "3" is IPlugControls only. IPlugInstrument on the trial fails 4 (`param-fuzz-bounds` plus the 3 state tests).
   - (The wrong attribution comes from 03.)

2. **[wrong]** §2 table, rows "UI in REAPER": the cells contradict the bug list and notes/05 §0.3.
   - CLAP is ✅ for PR and trial, but both crash on OS text entry in CLAP (`evidence/review/hvoya-clap/gdb-crash.txt`; 03 failure 1).
   - The trial VST3 cell lists only the race. With the race fixed, the trial VST3 crashes on OS text entry too (`evidence/review/racefix-vst3/gdb-crash.txt`).
   - The PR VST3 cell has no race note, although `IGraphicsLinux.cpp` is byte-identical in both. The ⚠️ in the trial column alone reads as if hvoya caused the race.

3. **[missing → now verified]** Bug 4, "not seen on mac/win (not checked there)".
   - Checked now on macOS: `review/probes/fortify.cpp` built with Apple clang `-O2` at the default `_FORTIFY_SOURCE=2`, and also at `=3`, does **not** abort.
   - MSVC has no `_FORTIFY_SOURCE`.
   - So the abort is specific to Linux/glibc on a gcc ≥12-class toolchain at `=3` (05 §4).
   - The fix is still worth taking.

4. **[overstated]** TL;DR 2: "Stillwell — Skia/VST3 in production".
   - 01 calls it a "Skia + VST3 proof-of-concept (Event Horizon)". No shipped Linux product is evidenced.
   - "Deimos — Linux CLAP on sale": 01 only says it "ships" on itch.io. Whether it is paid was not checked.

5. **[overstated]** Bug 1: "almost always under gdb / a loaded host".
   - Only the gdb case was measured: 3/3 and 2/2 runs.
   - Without gdb, 8/8 first opens rendered.
   - "Loaded host" is the hypothesis in 05 §0.2 ("likely trigger"), not an observation.
   - Minor: the flag is `mTimerRunning` in IGraphicsLinux and `mRunning` only in IPlugTimer.

6. **[overstated]** TL;DR 5 / §5.1: "porting to hvoya is cheap".
   - The cherry-pick cost matters only if the PR is adopted wholesale, which the doc rejects.
   - The trial took 22 of 25 commits.
   - A fresh checkout does not configure without the extra IPlugEffect CMake patch (05 §1).
   - "1 conflict" for master→hvoya is textual only. Pulling in 242 commits means re-validating Gneiss on mac/win, and that is not in row 0's 0.5–2 days.

7. **[overstated / nit]** Estimate "≈29–44".
   - The doc's own table sums to 29–45: row 0 is 0.5–2, while 04's total assumed the merge option.
   - The estimate comes from code reading with no prototype, and the doc doesn't say so.

8. **[missing]** Row 3, "base: Stillwell".
   - SW's `IGraphicsLinux.cpp` is Skia-only: `:28` includes `ganesh/GrDirectContext.h` (03 failure 7).
   - Its NanoVG path was never exercised (04 §4).
   - v1 is NanoVG-only (§5.4), so de-Skia work hides inside the 6–9 days and isn't named.

9. **[missing]** Gneiss uses **platform** popup menus. It never calls `AttachPopupMenuControl` or `AttachTextEntryControl` (grep of Gneiss and `hvoya/utils`). Call sites:
   - `user_tab_panel.cpp:586` `CreatePopupMenu`;
   - `morph_pad_control.hpp`;
   - `midi_cc/control_decorator.hpp`.

   On RR, those menus would land in GTK `gtk_main` on the render thread (04 defect 3). "Menus via IGraphics" is therefore a Gneiss-side change and needs a visual check: IGraphics menus are clipped to the editor bounds (04 §3 [I]).

   Conversely, Gneiss knobs are not value-editable (IVKnobControl default `false`; `wrap_knob_control.hpp:34` `false`). So bug 2 (the OS text-entry crash) probably does not hit Gneiss, and the reader should be told that.

10. **[missing]** Gneiss-side Linux work is not in the estimate:
    - a `SetLatency` fallback (risk 1);
    - `hvoya/utils/preset_manager.hpp:461-464` `openFolder()`, which has only OS_MAC/OS_WIN branches and is a no-op on Linux;
    - the prebuild steps (`scripts/write_plug_build_info_to_file.sh` and `prepare_resources-*.py`), which must be replicated in CMake. IPlugEffect on hvoya fails for exactly this reason (03).

11. **[missing]** Resources layout (03 failure 2), which 04 row 7 had but the doc's row 7 dropped.
    - RR's CLAP looks for a **shared** `resources/` next to the `.clap`.
    - Missing resources give a blank editor or a REAPER segfault (`ISVGSliderControl::OnResize`).
    - A shared `~/.clap/resources` would collide across several hvoya products.

12. **[missing]** The `state-reproducibility-*` ×3 failures persist on hvoya.
    - The CLAP state code is shared across platforms, so hvoya's CLAP likely fails these on macOS/Windows too [I, unverified].
    - SW passes them.
    - Worth running clap-validator on the Gneiss mac CLAP.

13. **[missing]** Test-scope caveat in §2:
    - Only REAPER was tested: arm64, Xvfb/openbox, llvmpipe.
    - Bitwig, Carla, Ardour and Renoise were not tested.
    - Both validators are headless, so 537/537 says nothing about UI or threading.

14. **[missing]** Row 9 drops two items from 04: "latency change with the editor closed" and pluginval. That latency test is the one that exercises Gneiss's decisive requirement.

15. **[missing / nit]** "Good editor lifecycle" in SW:
    - Its `OpenWindow` calls `CloseWindow` first, so `OnUIClose` runs before a UI exists. That crashed IPlugControls (03 failure 9).
    - Gneiss doesn't override `OnUIClose`, so the impact is low.
    - It must still be reconciled with `mOpening` and `mInOpenWindow` (04 §7).

16. **[nit]** The fix snippet `sizeof(cStr) - i` is not literally "already in PR #1336": the PR has `(size_t)(32 - i)`, which is equivalent. Also, `i` is 16 at the `snprintf`, not ≈15: 15 chars from `strftime`, then `i++`.

17. **[nit]** §3 states vsync blocking as fact. It is [I] in 02 and 04.

18. **[nit]** §2 says confidence levels are given, but the table has no markers.

## Clarity for the author

- The TL;DR and "Decisions for the author" sections work.
- The bug list mixes two kinds of bug:
  - bugs in code that won't be taken (they explain why it isn't taken);
  - the one bug that affects hvoya today (the logger).

  Split them into two lists.
- The doc should recommend a short spike before committing to 29–44 days. Risk 1 already hints at it. The spike: host-driven `Timer_impl` in REAPER and Bitwig, using CLAP `timer-support` plus VST3 `IRunLoop` via `setHostContext`, with the editor closed.
- Add a short "what this means for Gneiss specifically" list, covering findings 9–11.
