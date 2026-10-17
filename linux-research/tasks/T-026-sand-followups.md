# T-026 — Sand: follow-ups after T-024 (N1–N4, F2 host checks, docs)

- **Status:** ready
- **Owner:** —
- **Branches/worktrees:**
  - Sand repo: branch `fix/t026-followups` from `master` (54be993 or newer), in a new worktree `/Volumes/Storage/code/hvoya/Sand-t026` (`git -C /Volumes/Storage/code/hvoya/Sand worktree add ../Sand-t026 -b fix/t026-followups master`). Never the main checkout `/Volumes/Storage/code/hvoya/Sand` (the user's working tree; its untracked `Media/` stays). Never push; the coordinator merges at the user's command.
  - iPlug2/hvoya only if an item needs shared code (item 6): branch from `hvoya` in `/Volumes/Storage/code/IPlug2-T-026`. Never `/Volumes/Storage/code/IPlug2` itself (Gneiss builds against it).
- **Depends on:** T-024 (merged 2026-10-17: Sand master 54be993, hvoya 083bc87c5/228f2efc2/a865ee9f7), T-021 (merged: `Scripts/linux/package.sh`, runtime smoke images).
- **Conflicts:** Sand sources/projects; nothing in the Linux port files.
- **Not Linux work:** these are product fixes for every platform. AGENTS §4 (independent reviewer) and §5 (locks) apply; AGENTS §2's "don't fix cross-platform bugs in port branches" does not, but its spirit does: smallest correct change, no unrelated refactors, each behaviour change explained in its commit message.

## Read first
- `AGENTS.md`, Sand `CLAUDE.md` (its "do not build" line predates 2026-10-17: **building is allowed**, user), `README.md`, `BACKLOG.md`.
- `tasks/T-024-sand-cross-platform-fixes.md` Log: N1–N4 (~line 177), F2 residual risk (~line 222), reviews R6/R7; `tasks/T-021-release-packaging.md` Log: T21-1, T21-5; `CROSS_PLATFORM_FINDINGS.md` sections "Из T-012" and the T-024 summary.
- T-024's helpers in `linux-research/evidence/t024/`: `run-checks.sh` (clap-validator, VST3 `-e` ×20, ASan VST3 ×3 + CLAP, renders), `mac-checks.sh`, `render-clap.cpp` (offline CLAP host: output, non-finite samples, input-buffer change, process() time), `cpu-bench.sh`, `xcb-sand.sh` (command-line xcodebuild with every install path in a scratch dir). Its `build-sand.sh` (Linux CLAP+VST3, Release or RelWithDebInfo+ASan) was git-ignored (`build-*`) and is lost: rebuild it from the T-012 Log's build command (`DOCK_MOUNTS`, `EXTRA_CMAKE_ARGS=-DIPLUG2_DIR=…`, `DOCK_ENV="BUILD_TYPE=…"`) and commit it under a name the ignore rules don't match (e.g. `sand-build.sh`).

## Items

### 1. N1 — allocation on the audio thread (fix; highest value)
`SandModule::process` copy-constructs `hvoya::AudioBuffer dryCopy (processed)` on every block (`sand_module.hpp:304`), which allocates in `resize()`. Sand's CLAUDE.md forbids heap allocation in the audio path; at small buffers this risks dropouts on every platform.
- Make `dryCopy` a member (or a pre-sized scratch buffer) sized for the maximum block × channels in the reset path (`OnReset`/`prepare`-like code that already sizes Sand's other buffers), and fill it per block without allocation (`fillFrom`/copy into the existing storage; keep `assert(!isWrapper())` semantics where they matter).
- **Audit the whole audio path** (`Sand::ProcessBlock` → `SandModule::process` → grind/diffusor/filters/oversampler/envelope paths and hvoya helpers they call) for any other allocation: copies of `AudioBuffer`, `std::vector` growth, `std::function` captures, string formatting, logging. List each in the Log with file:line and fix the Sand-side ones; shared hvoya ones → item 6.
- **Proof:** an allocation counter in a Linux test build (e.g. `LD_PRELOAD` malloc/realloc/calloc counter or a `malloc` hook armed only inside `ProcessBlock`) over the offline renderer with several block sizes, rates and OS factors, including a block-size change: 0 allocations after warm-up, before vs after. Audio identical before/after (`render-clap.cpp`, bit-exact or explained). CPU before/after (`cpu-bench.sh`).

### 2. N2 — fresh clone builds on mac (verify, likely closed)
The macOS `resources/Sand-*-Info.plist` were untracked (9d0c8ea / 05c854b) and `prepare_resources-mac.py` only updates existing plists. The user's 961be6c "puts back plists" tracks them again and drops the `.gitignore` line.
- Verify: `git clone` Sand master into the scratchpad (sibling `IPlug2` must resolve: clone into a scratch folder that also holds a symlink or worktree of iPlug2 `hvoya` at `../../IPlug2` relative to `projects/`, or pass `IPLUG2_ROOT`), then `xcb-sand.sh` CLAP, VST3, AU, APP. Record the result; if anything is still missing, fix it the way the user's commit chose (tracked plists), nothing else.

### 3. N3 — `Sand-aax.vcxproj` is stale (fix the file list)
It lacks `SandUI.cpp`, every hvoya source and (after F7) `PolyphaseIIR2Designer.cpp`. Bring its source list (and `.filters`) in line with `Sand-vst3.vcxproj`, plus whatever AAX-specific sources iPlug2's AAX template lists (compare with `IPlug2/Examples/IPlugEffect/projects/IPlugEffect-aax.vcxproj`). You cannot build on Windows/AAX here: say so in the commit; the user verifies. Don't touch other project settings.

### 4. N4 — FL Studio crash (prepare the user's check)
BACKLOG: "Sand crashes 100% in FL Studio when adjusting the filter settings, particularly the mix knob" (Win 10). Candidates fixed in T-024: F1 (FL sends 0-frame VST3 flushes), F3 (≥ 88.2 kHz with 8x). Without FL here:
- try to reproduce the class of the crash on Linux/mac: VST3 validator ×20 and the offline renderer with automation sweeps of the filter params and mix at 44.1/48/88.2/96/192 kHz × OS 1/2/4/8, 0-frame blocks interleaved, ASan build on Linux;
- write a short checklist for the user's FL test (version, rate, OS factor, which knob, what to send back) into the Log and BACKLOG wording proposal (BACKLOG is the user's living file: propose the edit in the Log, don't edit it).

### 5. F2 follow-up — CC-driven values in hosts other than REAPER (verify)
After T-024 the mediator reports CC-driven values from `OnIdle`. Verify on mac: REAPER (CLAP, VST3, AU; scriptable with ReaScript like the Linux harness), `auval -v` for the AU, and — if you can drive them — Bitwig (CLAP, VST3) and Ableton Live Lite (AU, VST3); otherwise write a 5-minute manual checklist for the user. Check: the host's parameter display and automation recording follow CC; Sand's knob follows within one idle tick; no echo step-back at the end of a fast CC sweep in VST3 (residual risk: a value stepped back for ≤ 20 ms). If the step-back reproduces, propose a fix (e.g. ignore the host's echo of a value Sand itself reported in the same tick) — shared code → item 6.

### 6. Shared hvoya/iPlug items → proposal first
Anything in the above that lives in shared code (hvoya utils, iPlug), plus, if you get to it:
- **T21-5** — `hvoya/hvoya/utils/host_info_model.hpp:187`: the displayed beat is right only at 120 BPM (4/4, one quarter in: beat 4 at 40 BPM, 3 at 60, 2 at 120, 1 from ~134). Display only, every hvoya plug-in. Root cause + fix proposal with a small table test (Apple clang and GCC).
Write each as a proposal (commit on the iPlug2 branch, behaviour change per plug-in and format, a row in `CROSS_PLATFORM_FINDINGS.md`) and wait for the user's decision before the coordinator merges.

### 7. Docs: Sand `CLAUDE.md` Grind section (propose)
R6 from the T-024 review: the Grind section says "Two parallel bands of 4 delay lines" and "SR scaling `sr * 0.6 / 1000`"; `dmt_grind.hpp` has 5 bands (`lfoFreq*` size 5), `srScale = sr * 0.6 / 10000`, and `dmt_grind_cfg::maxSR` 3.07 MHz (with T-024's guard) is not mentioned. Also the "do not build" line (item "Read first"). `CLAUDE.md` is the user's instruction file: put the exact proposed text in the Log; apply it on your branch only if the user says so.

## Testing (every code item)
- Linux (Docker): build from a throwaway local branch that merges Sand `linux-experiment` into your branch (never commit that merge to either), like T-024; or package with `Scripts/linux/package.sh <that tree>` and smoke with `scripts/package-smoke.sh` (images `iplug2-rt:*`). clap-validator (known Sand list: `scripts/known-clap-failures/Sand.txt`), VST3 validator ×20, ASan build, REAPER `scripts/steps/sand.txt` CLAP + VST3.
- mac: `xcb-sand.sh` CLAP/VST3/AU/APP into the scratchpad (nothing in `~/Library`), clap-validator 0.4.1 (build it in your scratchpad with Homebrew `rust`, T-006 Log), `auval`.
- Restore anything a build regenerates in a tracked file (e.g. `main.rc_mac_dlg`) after a snapshot (`git add -A && git stash store -m … "$(git stash create)"`), never commit it by accident.

## Acceptance
- [ ] N1 fixed with the allocation proof (0 after warm-up), audio identical, CPU recorded; audit table of the audio path in the Log.
- [ ] N2 verified (fresh clone builds) or fixed minimally.
- [ ] N3 file list fixed; Windows build left to the user, stated.
- [ ] N4: Linux/mac stress results + the user's FL checklist.
- [ ] F2 host checks done (or the manual checklist), step-back answered.
- [ ] Shared items written as proposals; CLAUDE.md text proposed in the Log.
- [ ] One commit per item, messages say what changes on mac/win; main checkouts untouched; nothing pushed.
- [ ] Independent review (AGENTS §4); evidence in `evidence/t026/`; Log complete.

## Log
(append: date — what was done — commits — next)
