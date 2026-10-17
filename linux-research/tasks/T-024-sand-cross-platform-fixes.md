# T-024 — Sand: cross-platform fixes F1–F8 (not Linux work)

- **Status:** ready
- **Owner:** —
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
