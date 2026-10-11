# Review of the build & test results (notes/03), 2026-10-10

Reviewer's re-runs, probes and screenshots are in `evidence/review/`. Scripts are in
`evidence/review/probes/`. **[R]** = re-run or measured by the reviewer. **[E]** = checked against
the build agent's evidence files. **[C]** = checked in code.

## 0. Headline: three 03 claims about the REAPER GUI were wrong

1. **"hvoya trial: VST3 IPlugControls passes the full step script, reopen ×5 and 2 instances" was vacuous.** [E][R]
   - All 15 shots of the original run (`r01`, `c00`…`c13`) were byte-identical: the editor is black and never painted.
   - `draw-threads.txt` was empty.
   - `threads.txt` had no render thread (`IGraphicsLinux::TimerThreadProc`) and no `IPlugTimer` thread.
   - No X event was ever processed. "REAPER alive" only meant that nothing ran.
   - I reproduced it on a fresh build: `evidence/review/hvoya-vst3`. The first open of each new instance is blank; the reopen renders.
2. **Root cause: a thread-start race in PR #1336 itself, not in hvoya.** [C][R]
   - The race came from commit `cb8ec371c` ("Harden Linux IGraphics dlopen, timer, and clipboard").
   - `IGraphicsLinux.cpp` is byte-identical in RR and in the trial.
   - Both `IGraphicsLinux::StartTimer` (`IGraphicsLinux.cpp:1777`) and the Linux `Timer_impl` ctor (`IPlugTimer.cpp`) do this:
     ```cpp
     if (pthread_create(&mThread, nullptr, ThreadProc, this) == 0)
       mRunning = true;
     ```
     The new thread starts with `while (mRunning)`. If it runs before the creator sets the flag, it exits at once. The editor then never paints or processes input, and `OnIdle`/`ISender` never run.
   - Evidence:
     - The real `IPlugTimer.cpp` compiled alone: the timer never fired in **1 of 400** runs (`probes/timer_race.cpp`).
     - In REAPER with the harness's gdb attached before instantiation, the first open lost the race **every time** (3/3 runs; also 2/2 instances in my run).
     - Without gdb: **8/8** first opens rendered (`evidence/review/fo-hvoya`).
     - Under gdb with breakpoints on the thread entry it renders. This is a classic heisenbug.
     - Setting the flag before `pthread_create` (`probes/racefix.patch`, built in a detached worktree `wt/review-racefix`) makes the first open render under the harness.
   - So: rare on a quiet machine, near-certain under ptrace/strace or a loaded host. The CLAP editor is immune because the host drives it, but **every format's `IPlugTimer` has the same race**. A real host that's busy loading a project is the likely trigger.
3. **"CreateTextEntry crash is CLAP-only; VST3 is fine" is false.** [E][R][C]
   - With the race fixed, the trial VST3 crashes in exactly the same place (`evidence/review/racefix-vst3/gdb-crash.txt`):
     `CreateTextEntry ← OnMouseDblClick ← ProcessX11Events ← OnDisplayTimer ← TimerThreadProc`.
   - The RR-alone VST3 IPlugControls evidence had died too:
     - `c11`–`c13` were all-black screens.
     - `r02`/`r03` showed a **fresh, empty REAPER**: `rpr.sh`'s `reaper -nonewinst` became a new instance after the crash.
     - The log had `LUA ERROR … TrackFX_AddByName (MediaTrack expected)`.
     - So RR VST3 IPlugControls reopen ×5 and 2 instances were **never tested**. The older harness checked liveness with `pgrep reaper`.
   - The cause is not the IVNumberBox double-click as such but the **OS ("platform") text-entry path**:
     1. `CreatePlatformTextEntry` runs `gtk_dialog_run`, which is synchronous.
     2. It calls `SetControlValueAfterTextEdit`, then `ClearInTextEntryControl`, so `mInTextEntry` becomes `nullptr`.
     3. Back in `IGraphics::CreateTextEntry`, `mInTextEntry->SetDirty(false)` (`IGraphics.cpp:1987`) dereferences null.
   - The crash fires only after "OS Text Entry" is toggled on and the "Edit Value" dialog is committed. IGraphics text entry (c05/c06) works in both formats.
   - The fix is one line (null-check or reorder), but the synchronous nested GTK loop is the real problem. For CLAP it runs on the **host main thread**: my CLAP crash backtrace is on thread 1. While the dialog is open, the editor ignores clicks (`probes/sheet-myclap.png`: colour/font clicks do nothing).

## 1. Reproducibility (following docker/README.md only)

| step | result |
|---|---|
| `docker build -t iplug2-linux:24.04 linux-research/docker` | OK (cached, 1.6 s). Network inputs are pinned: REAPER 7.82, clap-validator 0.4.1, vst3sdk v3.8.1. |
| `fetch-sdks.sh`, `install-sdks.sh <wt>` | OK. CLAP and clap-helpers are on `main`, recorded by SHA in `VERSIONS.txt`, but `fetch-sdks.sh` cannot be pinned *to* a SHA (`git clone --branch`). Over time this is not reproducible. |
| `build-cmake.sh wt/hvoya-trial-rangerrick build/review-hvoya IPlugControls-{vst3,clap,app}` | **PASS ×3 in 73 s** on the existing worktree [R] |
| same on a **fresh checkout** of `linux-trial/rangerrick` | **CONFIGURE FAILED**: `IPlugEffect.h` is missing. It needs the uncommitted `Examples/IPlugEffect/CMakeLists.txt` change that sits only in the build agent's worktree. That step was undocumented. The patch file in `evidence/hvoya-trial/patches/01` also carried the install-sdks `VST3_SDK/README.md` diff, so it did not `git apply` after `install-sdks.sh`. **Fixed by the build agent after review**: README section "Building a hvoya-based tree". |
| `run-gui-test.sh <vst3> <out>` | Runs end to end (3 min), but it reported success for a blank editor (§0). |
| `run-gui-test.sh … review-gui` (own container) | OK. **Concurrency trap**: every script uses fixed `/tmp/reaper.pid` and `/tmp/rhome` inside a *shared* container (`gui` by default). When the build agent and I both used `gui`, one of my thread probes measured the other agent's REAPER (a VST3 window in a CLAP test). Use a separate container per person. |

Undocumented or awkward items:
- `dock.sh` mounts only the repo; the scratchpad is not visible. The README now says so.
- `run-gui-test.sh` and `rerun-reaper-matrix.sh` resolve paths relative to `linux-research/`.
- `steps/iplugcontrols.txt` refers to a non-existent `iplugcontrols-host.txt`.
- Every REAPER crash leaves a ~580 MB `core` in `linux-research/`. It is now git-ignored, but not size-limited (`ulimit -c 0` in `gui-up.sh` would avoid it).

## 2. Evidence check (screenshots vs claims)

| claim in 03 | evidence | verdict |
|---|---|---|
| RR APP renders, knob drag, IGraphics popups, GTK menu/file dialog | `rangerrick/gui/app-IPlugControls/*` | consistent [E] |
| RR VST3 IPlugControls: popups, text entry, about, reopen ×5, 2 instances | crashed after OS text entry. Reopen and 2 instances ran against an empty fresh REAPER (§0.3) | **refuted** |
| RR CLAP: reopen ×5 and 2 instances OK | only from the **no-steps** run. The steps run crashed (text entry) | partly supported: open/reopen yes, interaction plus reopen no |
| hvoya trial VST3 "passes everything" | never painted (§0.1) | **refuted** |
| hvoya trial CLAP renders, same crash | identical result in my run (`evidence/review/hvoya-clap`) | confirmed [R] |
| stillwell VST3/CLAP: renders, IGraphics popup, text entry commits "42", 2 instances | the contact sheets show exactly that. The OS-menu/OS-text-entry steps do nothing, because SW forces IGraphics popups | confirmed [E] |
| "redraw on every tick" (Draw = OnDisplayTimer hits) | my counts: VST3 134/134, CLAP 150/150, APP 109/109 in 6 s | confirmed [R] |

## 3. Which thread draws and runs `IPlugTimer`/`OnIdle` (hvoya trial build, measured)

Method (`probes/whichthread.sh`):
1. Instantiate with no debugger attached.
2. Close and reopen the editor.
3. Count `IGraphics::Draw`, `IGraphicsLinux::OnDisplayTimer` and `IPlugAPIBase::OnTimer` hits per gdb thread for 6 s (gdb thread 1 = process main thread).

| format | Draw / OnDisplayTimer | `IPlugAPIBase::OnTimer` → `OnIdle` | 03 said | 04 said |
|---|---|---|---|---|
| VST3 | private pthread (`TimerThreadProc`), not main | **another** pthread (`Timer_impl::ThreadProc`) | private pthread ✔ (silent on OnIdle) | ✔ |
| CLAP | **host main thread** (via `timer-support`) | pthread `Timer_impl::ThreadProc`, **not** main | main ✔ (silent on OnIdle) | ✔ "broken for OnIdle" |
| APP | private pthread (tid 2) | pthread (tid 17); main = tid 1 | private ✔ | ✔ |

03 and 04 agree on drawing. 04 is right, and 03 silent, that **`OnIdle` never runs on the main thread in any RR format**. For Gneiss that is the decisive fact (04 §1).

## 4. Other cross-checks between 03 and 04

- **RR close-editor deadlock** (`IGEditorDelegate::CloseWindow` holds GfxMutex and joins the render thread, while the render thread waits on the same mutex in `OnDisplayTimer`):
  - Code path [C]: confirmed. `TimerThreadProc` checks `mTimerRunning`, then `OnDisplayTimer` takes the lock. `CloseWindow` locks first, then `StopTimer` joins.
  - Not reproduced [R] in **1000** open/close cycles with live meters/scope: 10 toggles per ReaScript call, random 0–50 ms holds, VST3, race-fixed build (`probes/closestress.sh`).
  - Verdict: real but rare. Treat it as a design defect (lock-then-join), not an observed failure.
- **CLAP `onPosixFd` without the lock** [C]: confirmed. `IPlugCLAP.cpp` calls `ProcessX11Events()` directly, while `OnDisplayTimer` takes `GfxMutex`.
  - Also [C]: the CLAP `GUIWindowAttach` calls `OpenWindow` (which **starts the private render thread**) and only then `SetHostDriven(true)` (which stops it). So for a moment every CLAP editor has a private render thread as well.
- **`DrawNow()` / `mInOpenWindow`** [C]: `IGraphicsLinux` implements neither, so the base no-op applies. This confirms 04.
- **Stillwell CLAP worker thread**: not re-measured. 03 and 04 agree (`LinuxTimerWorker`).
- **`_FORTIFY_SOURCE` abort in hvoya's `IPlugLogger.h`** [C][R]: confirmed, with a refinement.
  - `CurrentTime()` calls `snprintf(&cStr[i], 32, …)` with `i≈15`, which overstates the space by 17 bytes.
  - It is evaluated in every build: it is an argument of `Trace(…)` in the `IPlugAPIBase` ctor, and `Trace` is a no-op only in its body.
  - Extracted verbatim and compiled (`probes/fortify.cpp`):
    - **aborts** with `-O2` on Ubuntu 24.04 / gcc 13 (default `_FORTIFY_SOURCE=3`);
    - **does not abort** with `=2` or on 22.04 / gcc 11.
  - So shipping hvoya code built on a 24.04-class toolchain crashes at plugin construction. RR's fix `(size_t)(32 - i)` is correct.

## 5. Harness quality

- **Good:**
  - Paths are generic (any worktree under the repo).
  - Builds are idempotent.
  - The step DSL is small and readable.
  - Crash capture via gdb works.
  - The PID-based liveness fix is right.
- **Problems found:**
  - No render check.
  - Empty `draw-threads.txt` was not flagged.
  - `LUA ERROR` was ignored.
  - xact `alive` used `pgrep` (matches the fresh instance).
  - The early gdb attach perturbs thread timing.
  - A shared container with fixed `/tmp` paths.
  - Steps hard-coded to IPlugControls' 1024×768 layout (fine, but the README should say so).
- **Fixed by the build agent during review** (round 1): see §6.

How rare the race is in practice [R]: 16 instances per REAPER, no debugger attached, 4 runs (VST3 and CLAP).
**64/64** `Timer_impl` threads were alive (`probes/timercount.sh`). So the race is rare in the field
and near-certain under ptrace. It is still a real bug.

## 6. Review rounds with the build agent

**Round 1** (requests in §0–§1).

What the agent delivered:
- 03 revised: failure 1 is now "VST3 and CLAP, OS text entry"; new failure 1a for the race; the RR VST3 and trial rows are corrected; the threading row now says "`OnIdle` never on main"; the judgement was extended.
- `reaper-plugin-test.sh` exits 1 with `RESULT.txt` on:
  - a blank editor area (first open, reopen, 2nd instance);
  - no repaint;
  - REAPER PID death (later phases are then skipped);
  - `LUA ERROR`;
  - zero draw hits.
- gdb attaches after the first open (`GDB_CATCH_EARLY=1` to override).
- Per-person `GUI=<container>`; `--ulimit core=0`.
- `rerun-reaper-matrix.sh` re-ran every REAPER row (`evidence/_env/reaper-matrix-run.txt`).
- The README documents the hvoya CMake patch, which was regenerated clean.

**Round 2 spot-check** [R], own container, trial VST3:
- With steps: **FAIL**, exit 1, "REAPER pid … dead at 'steps'", crash in `CreateTextEntry` (thread 2 = render thread).
- No steps: **FAIL**, exit 1, "render[after-reopen] … blank (stddev 0)". Only 1 `Timer_impl` thread for 2 instances.
- The harness now fails loudly on both known defects.
- Skimmed the corrected 03 and the new matrix screenshots (`probes/sheet-round1.png`). The claims now match the evidence.

Leftovers sent in round 2 (minor; no re-run needed):
- (a) The harness still PASSes a run where an `IPlugTimer` thread died. RR VST3 no-steps and trial CLAP no-steps have 1 timer thread for 2 instances, so one instance has no `OnIdle`.
- (b) 03 failure 8 should say the abort needs `_FORTIFY_SOURCE=3`, the 24.04 / gcc ≥12 default.
- (c) 03 failure 1a should say the race is rare without a debugger.

**All three are now done**, checked in the files:
- `reaper-plugin-test.sh` fails when an RR-style build (`nm` finds `Timer_impl::ThreadProc`) has fewer than 2 timer threads.
- 03 failure 8 now states the `=3` condition.
- 03 failure 1a now gives the no-debugger rate.

Nothing was re-run after these edits.

## 7. Final confidence per key claim

| claim | confidence |
|---|---|
| RR, stillwell and the trial build as stated; validator numbers | high (rebuilt the trial myself; validators not re-run) |
| The trial is reproducible from README (with the documented CMake patch) | high |
| RR timer-thread start race (render and `IPlugTimer`) | high (code + standalone + REAPER + fix verified) |
| OS-text-entry null-deref crash, VST3 + CLAP | high (code + reproduced in both formats) |
| RR threads: VST3/APP draw on a private pthread, CLAP on host main; `OnIdle` on a pthread in every format | high (measured, trial build) |
| Stillwell renders, popups and text entry in REAPER VST3/CLAP | high from screenshots; threading not re-measured |
| RR close-editor deadlock | medium: real in code, not reproduced in 1000 cycles |
| CLAP `onPosixFd` without the lock; no Linux `DrawNow`/`mInOpenWindow` | high (code) |
| hvoya `_FORTIFY_SOURCE` abort | high, but toolchain-dependent (=3 only) |
| "redraw on every tick" | high |
| Judgement: RR is the best scaffolding, not shippable as is | agree; reinforced by the race, the crash and `OnIdle`-off-main (consistent with 04 §8) |
