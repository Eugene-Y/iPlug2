# T-024 — Sand: cross-platform fixes F1–F8 (not Linux work)

- **Status:** in-progress (F1, F5, F7, F8 committed; F2, F3, F4 wait for the user's decision, see Log)
- **Owner:** T-024 chat (2026-10-17). Task file, Log and evidence: iPlug2 branch `linux/T-024` (from `hvoya-linux`) in `/Volumes/Storage/code/IPlug2-T-024-log`.
- **Branches/worktrees:**
  - Sand repo: branch `fix/cross-platform-f1-f8` from `master` (cd9f063 or newer), in a new worktree `/Volumes/Storage/code/hvoya/Sand-fixes` (`git -C /Volumes/Storage/code/hvoya/Sand worktree add ../Sand-fixes -b fix/cross-platform-f1-f8 master`). Not `linux-experiment`, and never the main checkout `/Volumes/Storage/code/hvoya/Sand` (the user's working tree). Never push; the user merges.
  - F2 only (shared hvoya code): branch `fix/midi-cc-host-notify` from `hvoya` in an iPlug2 worktree `/Volumes/Storage/code/IPlug2-T-024`. Never `/Volumes/Storage/code/IPlug2` itself (Gneiss builds against it).
- **Depends on:** nothing. The coordinator later merges the fixes into Sand `linux-experiment` / `hvoya-linux`.
- **Conflicts:** Sand sources and projects; `hvoya/utils/midi_cc/*` (F2). T-023 (clap-validator sweep) overlaps on F3–F5: T-023 now covers Gneiss and Linux-only items and runs after this task.

## Why
T-012 (Sand on Linux) found bugs that are not Linux-specific: they hit mac and win users today. The user chose to fix them in a separate task (2026-10-17). This is product work on mac/win code, so AGENTS §2's "don't fix cross-platform bugs in port branches" does not apply here — but its spirit does: smallest correct change, no unrelated refactors, every behaviour change explained.

## Read first
- `AGENTS.md` (§4 workflow with the independent reviewer, §5 locks if you use the VM), Sand `CLAUDE.md`, `README.md`, `BACKLOG.md`, and the parts of Sand you touch.
- **Building is allowed** (user, 2026-10-17): Sand's CLAUDE.md "do not build" predates this; Docker Linux builds, validators, and mac builds into your scratchpad (`-derivedDataPath`, `CLAP_PATH`/install paths in the scratchpad, nothing in `~/Library`) are fine. Don't regenerate or commit Xcode/VS project churn you did not intend.
- `tasks/T-012-sand-linux-build.md` Log, "Cross-platform findings" (file:line references), and `evidence/t012/validators/`.
- `CROSS_PLATFORM_FINDINGS.md`, section "Из T-012".

## Items
1. **F1 — High.** 0-frame blocks: VST3 hosts call `process` with `numSamples = 0` (parameter flush); `SandModule::process` reads `chan.back()/front()` (`sand_module.hpp` ~376, ~421) and `hvoya::utils::derivativeInplace` (`utils.hpp` ~131-134, **shared hvoya code**) reads/writes an empty span → out-of-bounds on host buffers. Fix at least `Sand::ProcessBlock` (return early on `nFrames == 0`, keeping parameter/MIDI handling that must still happen); decide whether `derivativeInplace` should also tolerate empty spans (shared: Gneiss uses hvoya utils too — if you change it, prove Gneiss unaffected and say so). Proof: VST3 validator `-e` ×20 with no crash (was 1–4/10), ASan clean (Linux Docker ASan build; macOS ASan hangs on this Mac).
2. **F3 — Low.** NaN output at 768 kHz with default params (clap-validator `process-varying-sample-rates`). Root-cause (coefficient design at high rates, filter cutoff above Nyquist?) and fix in Sand DSP.
3. **F4 — Low.** `flt hz` display text does not parse back ('1.01 kHz' → 1.0 Hz). Fix in Sand (e.g. its own `DisplayFunc` + text-to-value for that param, or the unit/kHz handling Sand controls). **Do not** change iPlug's `IParam::StringToValue` globally (RR's 360d84011 is rejected); if a shared change looks necessary, propose it instead.
4. **F5 — Low/Medium.** Sand processes in the host's input buffer (`hvoya::AudioBuffer processed (ins, …)` in `Sand::ProcessBlock`): out-of-place 64-bit hosts get their input overwritten (`process-audio-double-*`). Process into the outputs (or a scratch copy) instead; no allocation on the audio thread; measure CPU before/after.
5. **F7 — Medium.** Windows projects likely fail to link: since 6036dec the hvoya envelope detectors call `hiir::PolyphaseIIR2Designer::compute_coefs`; Xcode compiles `IPlug/Extras/HIIR/PolyphaseIIR2Designer.cpp`, no `projects/*.vcxproj` does. Add it to the vcxproj files (and `.filters`) the way the other iPlug2 sources are listed. You cannot build on Windows: say so; the user verifies.
6. **F8 — Low.** Command-line `xcodebuild` of Sand fails (`plog/Log.h` not found; `$(HVOYA_PATH)/**` reaches clang unexpanded). Find the cause (xcconfig/recursive header search path) and fix it so `xcodebuild -scheme macOS-APP/CLAP/VST3` works from the command line, without changing what the Xcode IDE build produces.
7. **F2 — Medium, shared hvoya code → proposal first.** `hvoya/utils/midi_cc/mediator.hpp` (~140-145) calls `BeginInformHostOfParamChange`/`SendParameterValueFromDelegate`/`EndInform…` from the audio thread (`ProcessMidiMsg`). CLAP turns them into output events (fine); in VST3 they are `IComponentHandler` calls off the UI thread (spec violation) and REAPER keeps the old value. Affects Gneiss, Scape, DMT too. Write a proposal (e.g. queue the values lock-free and report them from `OnIdle` on the main thread; or per-format behaviour), with the behaviour change for each format and each plug-in, and **wait for the user's decision** before implementing. Related: C4 in `CROSS_PLATFORM_FINDINGS.md`.

## Testing
- Linux (Docker): build Sand from your branch with the Linux CMake files: create a throwaway local branch that merges `linux-experiment` into your fix branch (never commit that merge to either branch) and build it like T-012 (`DOCK_MOUNTS`, `EXTRA_CMAKE_ARGS=-DIPLUG2_DIR=/Volumes/Storage/code/IPlug2-linux`). clap-validator and VST3 validator before/after; `scripts/steps/sand.txt` in REAPER (CLAP, VST3) for no regression.
- mac: build CLAP/VST3/APP from your worktree with xcodebuild into the scratchpad (after F8, plain `xcodebuild` should work), run clap-validator (Homebrew `rust` is installed; build clap-validator 0.4.1 in your scratchpad, see T-006 Log) and, if available, the VST3 validator.
- Audio: where a fix touches DSP (F1, F3, F5), render the same input before/after (e.g. the APP or a small offline harness) and show the outputs are identical for normal block sizes and rates (or explain any difference).

## Acceptance
- [ ] F1, F3, F4, F5, F7, F8 fixed on `fix/cross-platform-f1-f8`, one commit per item, each message says what changes on mac/win.
- [ ] F2 proposal written (in this file's Log and a row update in `CROSS_PLATFORM_FINDINGS.md`); implemented only after the user agrees.
- [ ] Validators before/after on Linux and mac; ASan clean on Linux; audio identical where expected.
- [ ] Main checkouts untouched; nothing pushed.
- [ ] Independent review (AGENTS §4); evidence in `evidence/t024/`; Log complete.

## Log
(append: date — what was done — commits — next)

### 2026-10-17 — F1, F5, F7, F8 committed; F3 root cause; F2/F3/F4 proposals (waiting for the user)

Lock `.linux-port-locks/T-024` taken. Worktrees:
- `/Volumes/Storage/code/hvoya/Sand-fixes`, branch `fix/cross-platform-f1-f8` from master cd9f063.
- Throwaway, never to be merged:
  - `hvoya/Sand-t024-base` (detached master, mac "before");
  - `hvoya/Sand-t024-linux` (branch `tmp/t024-linux` = fix branch + merge of `linux-experiment`, rebuilt after each fix);
  - `IPlug2-T-024-f3test` (detached `hvoya-linux` + the uncommitted F3 proposal patch).
- `IPlug2-T-024` (reserved for F2) is not created yet.

The local mac `resources/Sand-*-Info.plist` files were copied from the user's checkout into the throwaway worktrees (ignored files, see N2). The `main.rc_mac_dlg` regenerated by the APP build was restored after a stash snapshot ("T-024: before restoring regenerated main.rc_mac_dlg").

Tools, all in this chat's scratchpad, nothing in `~/Library`:
- clap-validator 0.4.1 (cargo);
- the VST3 validator from the cached SDK v3.8.1 (`cache/sdks/VST3_SDK`; the SDK in `IPlug2/Dependencies` has an empty `cmake/`).

Scripts in `evidence/t024/`:
- `build-sand.sh`: Linux CLAP+VST3, Release or RelWithDebInfo+ASan.
- `run-checks.sh`: clap-validator, VST3 `-e` ×20, ASan VST3 ×3 + CLAP, renders.
- `mac-checks.sh`: the same on mac, without ASan.
- `render-clap.cpp`: offline CLAP host. Writes the output and reports non-finite samples, whether the input buffer was changed, and process() time.
- `cpu-bench.sh`: before/after best of N.
- `xcb-sand.sh`: xcodebuild with every install path, SYMROOT and OBJROOT in a given dir.

**Commits on `fix/cross-platform-f1-f8`** (each message says what changes on mac/win):
- `e8cb1f0` **F1** — `Sand::ProcessBlock` returns on `nFrames == 0`.
  - iPlug applies the block's parameters and MIDI before `ProcessBlock` (`IPlugVST3ProcessorBase::Process`: `ProcessParameterChanges`, `ProcessMidiIn`, then `ProcessAudio`), so nothing is lost.
  - hvoya's `derivativeInplace` already states its precondition (`assert(!data.empty())`). The defect was the caller, so shared code is unchanged and Gneiss is unaffected.
- `00cbc41` **F5** — Sand wraps `outs`, does `fillFrom(ins)`, and processes there. The final `copyTo` is gone, so the work stays at one copy per block.
  - Only the double-precision paths hand over host buffers (VST3 64-bit, CLAP data64, VST2 processDoubleReplacing).
  - AU/AAX/WAM are float and get iPlug's converted copies.
- `87beabb` **F7** — `IPlug/Extras/HIIR/PolyphaseIIR2Designer.cpp` added to `Sand-{clap,vst3,vst2,app}.vcxproj` and `.filters` (filter `IPlug`, next to `IPlugParameter.cpp`). Encoding and line endings are kept byte for byte.
  - A source diff against the Xcode CLAP target shows this was the only missing non-format source in those four.
  - The header is already on `EXTRAS_PATH`. There are no PCH or forced includes.
  - **Not built on Windows** (no Windows machine here): the user verifies.
  - `Sand-aax.vcxproj` is left alone (N3).
- `aed6804` **F8** — the real cause was not "xcodebuild does not expand `/**`".
  - The WIP commit cd9f063 changed the **CLAP** target's `"$(HVOYA_PATH)/**"` to `"$(HVOYA_PATH)"` (Debug/Release/Tracer). That target does not inherit the project-level path, so `hvoya/libs/plog/include` dropped out.
  - The CLAP build failed in the IDE as well. The user's build-mac has no CLAP.build, and the installed `~/Library/Audio/Plug-Ins/CLAP/IPlug2Dev/Sand-macOS/Sand.clap` (2026-08-29) is an empty bundle with no binary in `Contents/MacOS`, consistent with a failed CLAP build.
  - VST3 and APP inherit `/**`, which xcodebuild expands too: the same include list as the user's IDE build of 2026-10-10 (its `*-common-args.resp`).
  - Fix: the CLAP target keeps the non-recursive path and gets `"$(HVOYA_PATH)/hvoya/libs/plog/include"`.
  - Verified: `xcodebuild` macOS-CLAP Debug and Release, macOS-VST3 Debug and macOS-APP Debug all build from `Sand-fixes`; with the recursive path restored instead, CLAP builds too.
- `da0e4bc` **F3, Sand part** — `DmtGrind` (Sand's own code) sizes its delay storage for `dmt_grind_cfg::maxSR` = 3.07 MHz, guarded only by an `assert`.
  - The module runs at host rate × os. The VST3 validator's "Process Format" (up to 1.23 MHz, after earlier tests set os to 4x/8x) made `assignStorageAndLengths()` hand out spans past `_storage` and `clear()` them. That happens in `setSampleRate`, even with grind at 0.
  - Seen by ASan after F1 (heap-buffer-overflow `DelayLineWithFeedback::clear` ← `DmtGrind::setSampleRate` ← `Sand::OnReset`). In Debug it is `assert(sr <= maxSR)`.
  - Fix: line lengths, `maxBandDelaySamples()` and the LFO depth in samples use `min(rate, maxSR)`; LFO frequencies keep the true rate; the assert goes.
  - Identical up to 3.07 MHz, i.e. every rate a 48–384 kHz host reaches with os ≤ 8x.
  - Harness (Debug, grind ±0.8): finite at 3.07 / 6.14 / 9.88 MHz, where the old tree asserts.
  - The file is CRLF; endings were kept.

**Results** (before = master / `linux-experiment`; after = F1+F5 (+F7, F8)). Linux = Docker 24.04 Release; mac = Apple clang Release CLAP, Debug VST3.

| Check | before | after F1+F5 | after F1+F5 + F3 proposal |
|---|---|---|---|
| Linux VST3 `validator -e` ×20 | 5 segfaults (FlushParamTest) | 11 segfaults, all in "Process Format" (F3, see below) | T024_F3P_VST3 |
| Linux ASan VST3 ×3 | 3× heap-buffer-overflow `derivativeInplace` | T024_F15_ASAN | T024_F3P_ASAN |
| mac VST3 (Debug) ×20 | 20 aborts: `assert(!data.empty())` derivativeInplace | 20 aborts: `assert(nc <= kMaxCoefs)` HilbertEnvelopeDetector (F3) | — |
| clap-validator Linux | 34 pass / 4 fail | T024_F15_CV | T024_F3P_CV |
| clap-validator mac (main IPlug2 has no T-006 fix, so +3 state-*) | 31 / 7 | 33 / 5 (double in/out-of-place pass) | — |
| render 64-bit out-of-place: input changed | 192000 samples | 0 | 0 |
| render 768 kHz default params: non-finite | 458 | 458 (F3) | T024_F3P_768 |

- **Audio identical** (`render.sha256`, mac and Linux): 12 of 13 renders are bit-identical before/after. Covered: 32/64-bit, in-place/out-of-place, 44.1/48/96/192/384 kHz, block 64/512/1000, order 3 + os 2x, grind + AM.
  - The 13th is random block sizes with 0-frame blocks.
  - after-with-empty-blocks = after-without = **before with the host skipping the empty calls** (`skipzero=1`), all sha 6a738b9c….
  - Before, each empty block disturbed the output: 152 samples, max |diff| 0.31 against a 0.69 peak, i.e. **audible clicks on hosts that send empty blocks**, not only crashes.
- **CPU (F5)**, mac, 100 s of audio, best of 7, both builds alternating (`mac-cpu-f5.txt`):
  - 64-bit: 0.218 s before, 0.199 s after;
  - 32-bit: 0.245 s before, 0.232 s after;
  - no regression (the spread between runs is larger than the difference).

**F3 — the NaN's root cause (shared hvoya code; not committed, proposal below).**
- `hvoya/utils/hilbert_envelope_detector.hpp` `_recompute()`: the number of allpass coefficients grows as `lowestHz / (sr/2)` shrinks. Storage is `kMaxCoefs = 14`, and the guard is only an `assert`.
- With Quality::High (20 Hz, used by Sand's `CompoundEnvelopeDetector`) it needs 15 at > 384 kHz, 16 at 768 kHz, 17 at 1.536 MHz.
- In Release `_filter[i + 2].coef` writes past the array into the following `SandModule` members. `_amEnvelope` read 0.9997 right after `reset()` set it to 0, so the AM path switched on with garbage and the output diverged to 1e146 → NaN.
- The header comment already promises the fix ("For SR > 192 kHz the lower frequency bound shifts up proportionally"), but the code does not do it (its clamp floor is 1e-5).
- Reached in real use through Sand's oversampling, since the module rate is host rate × os: 88.2/96 kHz with os 8x, 176.4/192 kHz with os 4x/8x. Also the VST3 validator's "Process Format" (rates up to 1 234 567.8 Hz) and clap-validator (768 kHz).
- ASan does not see it (intra-object overflow).
- Severity: **Medium–High** (memory corruption and NaN output at plausible settings), not Low.
- Only Sand uses Hilbert/Compound detectors (grep over Gneiss, Scape, DMT, Melter, Bromo, Delta).

**Decisions for the user** (nothing implemented):
- **F3 proposal (recommended), shared hvoya, 1 line + comment.**
  - Patch: `evidence/t024/proposals/f3-hilbert-max-coefs.diff` (applies to `hvoya`); it will be committed on an iPlug2 branch `fix/hilbert-max-coefs` from `hvoya` once approved.
  - Tools: `f3-hilbert-nc-table.cpp` (coefficient count per quality × rate) and `f3-sandmodule-harness.cpp` (SandModule offline).
  - Change: clamp the design transition at `kMinTransition = 10 / (192000 / 2)`, the design point `kMaxCoefs` was measured at (Ultra at 192 kHz), so `nc ≤ 14` for every rate.
  - Every (quality, rate) pair that worked before keeps exactly the same coefficients; at 384 kHz Sand is bit-identical.
  - Above that, the lowest tracked frequency rises proportionally (High: 40 Hz at 768 kHz, 80 Hz at 1.536 MHz).
  - Harness: finite output at 386 k / 705.6 k / 768 k / 1.536 M / 3.072 MHz.
  - Linux with F1+F5: see the last column of the table.
  - Alternative, Sand only: choose the detector's Quality from the module rate in `SandModule::setSampleRate` (High ≤ 384 k, Medium ≤ 768 k, Low ≤ 1.536 M). It still overflows above 1.536 MHz (e.g. a 384 kHz host with os 8x) and leaves the trap in hvoya. Not recommended.
- **F4 proposal: no Sand-only fix keeps the current display.** Every text→value path calls the non-virtual `IParam::StringToValue` (`atof`, unit ignored):
  - CLAP `paramsTextToValue`;
  - VST3 `IPlugVST3Parameter::fromString`;
  - AUv2/AUv3;
  - VST2;
  - IGraphics text entry.

  A model of `makeSegmentedDisplay` + `atof` fails the validator's round trip for 95% of `flt hz` values. A unit-aware parser alone still fails at segment boundaries, because the segment is chosen before rounding: 999.97 → "1000.0 Hz" → 1000 → "1.00 kHz", and 9.955 ms → "10.0 ms" → "10 ms". The same applies to grind cutoff, am lp cutoff ("1000 Hz"/"1.00 kHz") and am release ("ms"/"s").
  - (a) Sand only: single-unit, single-precision displays for the four params (e.g. "12500.0 Hz", "1500.0 ms"). They round-trip through `atof`, but kHz/s go away in the UI and the host. No shared code.
  - (b) **(recommended)** In three parts:
    1. iPlug: an opt-in per-parameter text→value function (`IParam::SetTextToValueFunc`, used by `StringToValue` only when set). Unlike RR's 360d84011 (a global change), params without it behave exactly as today.
    2. hvoya: `makeSegmentedParser(segments)` (number + unit → value), and `makeSegmentedDisplay` choosing the segment after rounding. The boundary text changes for e.g. 99.95–99.999 Hz in Gneiss/Sand.
    3. Sand opts in for its four params.

    The same also fixes Gneiss C5 once Gneiss opts in.
  - (c) leave it (Low; only typed values and validators are affected).
- **F2 proposal** (hvoya `midi_cc/mediator.hpp`; Sand, Gneiss, Scape, DMT).
  - Today, per CC on the audio thread: `BeginInformHostOfParamChange` → `SetNormalized` + `OnParamChange` → `SendParameterValueFromDelegate` → `EndInform…`. Three problems:
    1. `InformHostOfParamChange` is never called, so VST3/AU/VST2 hosts never get the value. VST3 has no `performEdit`, AU no `kAudioUnitEvent_ParameterValueChange`, VST2 no automate. Hosts that keep their own value (REAPER VST3) show the old one, and nothing records automation. CLAP hosts see it only by polling `get_value`.
    2. VST3 `beginEdit/endEdit` (IComponentHandler) are called off the UI thread: a spec violation.
    3. `SendParameterValueFromDelegate` sets IGraphics controls from the audio thread: a data race in every format. It also makes the audio thread a second producer on CLAP's single-producer `mParamValuesToHost` queue, alongside UI gestures.
  - Proposal:
    - The audio thread keeps only `SetNormalized` + `OnParamChange`, so the DSP reacts in the same block as now, and stores the value in a preallocated per-param slot (`std::atomic<double>` + `std::atomic<bool> dirty`; lock-free, no queue overflow, coalesces a CC sweep).
    - A new `CCMediator::onIdle()`, called from each plug-in's `OnIdle` (main thread), sends dirty params as `Begin` → `InformHostOfParamChange` → `SendParameterValueFromDelegate` → `End`.
    - Gneiss' relative-CC (Modulate) path is untouched.
  - Behaviour change:
    - VST3/AU/VST2/AAX hosts now receive the CC-driven value, so host display and automation recording work. Calls arrive on the main thread, one gesture per idle tick per param.
    - CLAP: Begin/Value/End as output events (a Value event is new), sent with the next process or a `request_flush`.
    - UI knobs follow on the next idle tick instead of immediately (and racily).
    - APP: only the UI part moves.
    - Each plug-in needs one line in `OnIdle` (Sand, Gneiss, Scape, DMT; Gneiss already calls `takeUIDirty` there).
  - Risk to check in a DAW: a VST3 host echoes the performEdit value back as a parameter change one or more blocks later. During a fast CC sweep that can step the param back briefly (params are applied before MIDI in a block, so a block with a newer CC still ends on the newer value).
  - Alternative: per-format behaviour, keeping CLAP on the audio thread. Not recommended, because CLAP's queue would still have two producers.

**Other findings (not fixed):**
- **N1:** `SandModule::process` copy-constructs `hvoya::AudioBuffer dryCopy (processed)` every block, which allocates (`resize()`) on the audio thread, against Sand's CLAUDE.md ("no heap allocations in the audio path").
- **N2:** a fresh clone of Sand cannot build on mac (IDE or command line). The macOS `resources/Sand-*-Info.plist` were removed from git and ignored (9d0c8ea / 05c854b, 2026-05-04), and `prepare_resources-mac.py` only updates existing plists ("Build input file cannot be found" for the CLAP Info.plist). Options: track template plists and let the script create the working copies, or track them again.
- **N3:** `Sand-aax.vcxproj` lacks `SandUI.cpp` and every hvoya source, so it is stale beyond F7.
- **N4:** the BACKLOG report "Sand crashes 100% in FL Studio when adjusting the filter settings, particularly the mix knob" (Win 10). Not reproduced here (no Windows/FL). Candidates, unconfirmed: F1, if FL sends empty blocks (VST3 flushes do); F3, if the project runs at ≥ 88.2 kHz with os 8x. Worth re-testing with this branch.

Next: the user's decisions on F2, F3, F4 → implement what is approved → the independent review (AGENTS §4) → hand-over.
