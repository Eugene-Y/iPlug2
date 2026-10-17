# T-012 — Sand: Linux CMake build (CLAP, VST3, APP)

- **Status:** in-progress
- **Owner:** T-012 instance (2026-10-17)
- **Branches/worktrees:**
  - Sand repo (`/Volumes/Storage/code/hvoya/Sand`, own git repo, `master`): create a local-only branch `linux-experiment` in a new worktree `/Volumes/Storage/code/hvoya/Sand-linux` (`git -C /Volumes/Storage/code/hvoya/Sand worktree add ../Sand-linux -b linux-experiment master`). Never push it. Never touch the main checkout (its untracked `Media/` and any WIP stay as they are).
  - iPlug2: `linux/T-012` from `hvoya-linux`, in `/Volumes/Storage/code/IPlug2-T-012`, only if iPlug2 changes are needed.
- **Depends on:** nothing. T-003 (Gneiss) is the worked example; T-005 gives the IDE template.
- **Plan phase:** P11c (PLAN P7 pattern).
- **Conflicts:** none (separate repo). Don't edit IGraphicsLinux* here; port bugs found there go to the coordinator (T-019/T-009 own those files now).

## Goal
Sand builds on Linux as CLAP, VST3 and standalone APP from a CMake project, loads in REAPER (Docker), shows its UI and works. It opens in VS Code like the examples (T-005). Minimal footprint: Sand's mac/win/iOS/web projects, configs and behaviour stay unchanged.

## Read first
- `AGENTS.md` (§2 minimal footprint, §4 workflow with the independent reviewer), PLAN §1–2.
- **Sand's own docs:** `Sand/CLAUDE.md`, `README.md`, `BACKLOG.md`. Sand's CLAUDE.md says "do not build" — that rule is for the user's main checkout and Xcode; building the Linux worktree in Docker is what this task is (the user asked for it).
- `tasks/T-003-gneiss-linux-build.md`, especially its Log: everything Gneiss needed (resource list, `hvoya_add_utils()` from `hvoya/hvoya.cmake`, GCC-only build errors, the GLIBC floor, step files).
- Gneiss' Linux `CMakeLists.txt` on its `linux-experiment` (`/Volumes/Storage/code/hvoya/Gneiss-linux`), and T-005's files: `Scripts/cmake/Linux/CMakePresets-linux.json`, `Examples/IPlugEffect/{CMakePresets.json,IPlugEffect-linux.code-workspace,.clangd}`, `Documentation/linux-development.md`. If T-011 has been merged, use what it produces for out-of-tree projects instead of copying by hand.

## Context
- **Sand today:** Xcode (`projects/Sand-macOS.xcodeproj`, iOS) and VS (`projects/*.vcxproj`), WAM makefiles; NanoVG (Metal on mac, GL2 on win). Pre-build: `scripts/prepare_resources-*.py`, `write_plug_build_info_to_file.sh`/`.bat`.
- **Resources:** `resources/fonts/` (Roboto, RobotoMono faces), `resources/img/*.svg`; `config.h` names `ROBOTO_FN`, `ROBOTO_MONO_FN`, `HVOYA_LOGO_HANDLE_FN`, `SAND_LOGO_HANDLE_FN`, `SAND_LOGO_FILLED_HANDLE_FN`. Ship what `main.rc` and the mac bundle ship. `PLUG_SHARED_RESOURCES 0`.
- **MIDI in:** `PLUG_DOES_MIDI_IN 1`.
- **hvoya utils:** Sand uses PresetManager etc. from `IPlug2/hvoya/`; use `hvoya_add_utils()`.
- **Known Linux need:** `SandUI.cpp` `getOsString()` has branches for Win/iOS/Mac/Web only; on Linux it falls off the end of a non-void function (UB). Add an `OS_LINUX` branch (`"Linux"`) — the one Sand source change expected. Everything else in Sand logic stays as is.
- **Tooltips** (`EnableTooltips`, `SetTooltip`) are a no-op on Linux until T-019; not this task's problem.

## Steps
1. Worktree + branch as above; `scripts/install-sdks.sh` in any new iPlug2 worktree.
2. `Sand-linux/CMakeLists.txt` (+ presets, `Sand-linux.code-workspace`, `.clangd`), following Gneiss' and T-005's layout. Sources listed once; defines as the Windows build; point `IPLUG2_DIR` at `/Volumes/Storage/code/IPlug2-linux` (or your T-012 worktree), never at `/Volumes/Storage/code/IPlug2`.
3. Build-info header and resources via the same CMake mechanism as Gneiss.
4. Build in Docker: 24.04 all three formats, 22.04 CLAP+VST3. `scripts/inspect-binary.sh`: exports, NEEDED, GLIBC floor.
5. clap-validator and the VST3 validator; explain or ticket failures (`state-reproducibility` → T-006).
6. REAPER in Docker (`run-gui-test.sh`): UI renders (look at the screenshots yourself); write `scripts/steps/sand.txt` and `sand-app.txt` (knob drag, a menu, preset switch, open/close ×5, two instances, MIDI note in if Sand reacts to it). 20× churn. Host-loop trace build: everything on the main thread.
7. Sand bugs that are not Linux-specific: note them in `CROSS_PLATFORM_FINDINGS.md` style in your Log, don't fix.

## Acceptance
- [ ] CLAP/VST3/APP build on 24.04; CLAP/VST3 on 22.04; exports minimal; GLIBC floor recorded.
- [ ] Validators recorded; failures explained or ticketed.
- [ ] UI renders in REAPER (CLAP, VST3) and APP (`--no-io`); step scripts PASS; churn 20× PASS; trace clean.
- [ ] Opens and builds in VS Code via the presets (Docker or the VM, take the VM lock).
- [ ] Sand main checkout and `/Volumes/Storage/code/IPlug2` untouched; nothing pushed; Sand diff = Linux build files + the `getOsString` branch.
- [ ] Independent review (AGENTS §4); evidence in `evidence/t012/`; Log complete.

## Log
(append: date — what was done — commits — next)

**2026-10-17 — build, tests, VS Code; findings.** Evidence: `evidence/t012/` (contact sheets `reaper-{clap,vst3}/sheet.png`, `app/sheet.png`, `vm/contact-vscode.png`; reviewed by eye).

Commits, Sand `linux-experiment` (local, worktree `/Volumes/Storage/code/hvoya/Sand-linux`, from master cd9f063):
- 4dcd5cd `CMakeLists.txt` (FORMATS APP VST3 CLAP; `hvoya_add_utils()`; resources = the five files `config.h` names, i.e. what `main.rc` ships — the mac bundle copies all of `resources/{fonts,img}`, the other files are unused); `IPlug/Extras/HIIR/PolyphaseIIR2Designer.cpp` compiled as in the Xcode project. `SandUI.cpp`: `getOsString()` `OS_LINUX` branch (without it the deduced return type is `void` → compile error), `boundsPad`/`headerH` to file scope (GCC rejects a local in a lambda's default argument; same fix as Gneiss).
- 2456460 `svf_zdf.hpp`: `std::isfinite` in an `assert` (libstdc++ has no global `isfinite`; Debug builds failed).
- 60c5b96 `CMakePresets.json` (includes `../../IPlug2/Scripts/cmake/Linux/CMakePresets-linux.json`: the same relative iPlug2 location as the Xcode/VS projects and `CMakeLists.txt`) + `Sand-linux.code-workspace` (T-005 template; REAPER lua path `../../IPlug2/Scripts/linux/…`).
- e259e96 C++23, as the Xcode (`c++23`) and VS (`stdcpp23`) projects; C++20 was only `hvoya_add_utils`' minimum.
- Sand diff vs master: the four new files + `SandUI.cpp` (getOsString, file-scope constants) + `svf_zdf.hpp` (`std::`). Sand's own `.clangd` (mac) left untouched (see D1).

Commits, iPlug2 `linux/T-012`: d3497e960 claim + `docker/gcc13/Dockerfile` (`iplug2-linux:22.04-gcc13`); 5391d32a9 `steps/sand.txt`, xact `rclick-hold`, `[CLAP]`/`[VST3]` step prefix (`reaper-plugin-test.sh` passes `XACT_FMT`), expect-change/expect-same FAIL on a missing shot (compare's error used to count as "changed" — it gave me a false PASS on a stale shot), expect-param doc says INDEX; next commit: `steps/sand-app.txt`, docker README, this Log.

Build: `DOCK_MOUNTS="/Volumes/Storage/code/hvoya/Sand-linux /Volumes/Storage/code/hvoya/Sand/.git:ro" DOCK_ENV="EXTRA_CMAKE_ARGS=-DIPLUG2_DIR=/Volumes/Storage/code/IPlug2-T-012" scripts/dock.sh -- scripts/build-cmake.sh /Volumes/Storage/code/hvoya/Sand-linux build/t012-sand Sand-clap Sand-vst3 Sand-app`; 22.04: add `IMAGE=iplug2-linux:22.04-gcc13` and `CC=gcc-13 CXX=g++-13` to `DOCK_ENV`.

Results [D] = Docker, REAPER 7.82, llvmpipe (final run after the C++23 commit, `build-*.log`):
- 24.04 CLAP/VST3/APP and 22.04 CLAP/VST3 build. **22.04 needs GCC 13**: Sand uses `std::format`, GCC 11 has no `<format>`; GCC 13 from the toolchain PPA in a derived image. iPlug2 links libstdc++ statically → no GLIBCXX. Exports `clap_entry` / `GetPluginFactory ModuleEntry ModuleExit`; NEEDED libX11 + libc/libm. **GLIBC floor 2.35** (22.04), 2.38 on 24.04 (`inspect-*.txt`).
- UI in REAPER CLAP + VST3 and APP `--no-io`: fonts, SVG logo, tabs, sliders; reopen ×5, two instances. `steps/sand.txt` PASS in CLAP and VST3: comp knob drag (host value 0.300 → 1.000), LP→HP (host sees HP), ENV tab, host sets softness → knob follows, context menu → MIDI CC → Learn, then CC 1 from REAPER's virtual keyboard binds and drives comp to 0, about panel reads "Linux aarch64 release / linux-experiment e259e96". VST3: the menu is REAPER's GTK host menu with the plug-in's items (held right click). `steps/sand-app.txt` PASS (drag, tab, stand-in menu + Escape, about). Draw and X events on tid 1.
- MIDI notes: Sand ignores them (`ProcessMidiMsg` kNoteOn is empty); CC is what it uses, covered above. Presets: `kNumPresets = 1`, no preset UI, so "preset switch" is replaced by host-sets-param.
- Churn 20× CLAP/VST3 PASS (fds/sockets/threads back); 60× CLAP: RSS flat (closed 297.6 → 297.4 → 298.1 MB after 25/50/60 cycles; the first open loads the GL stack).
- Trace build (`-DIPLUG_TRACE_HOST_LOOP`): `OnTimer`/`Sand::OnIdle` only on REAPER's main thread in every phase, CLAP and VST3; churn 50× add/delete: threads/fds back (`hostloop-*`).
- **VS Code [VM]** (VM lock taken and released; `xdpyinfo` OK after): layout `~/dev/IPlug2` (rsync of this tree) + `~/dev/hvoya/Sand-linux` (clone of `linux-experiment` from a bundle). `cmake --list-presets` shows linux-debug/release; VS Code 1.142 opens `Sand-linux.code-workspace`, offers the presets, Linux Debug → F7: 158/158, exit 0, CLAP/VST3 copied to `~/.clap`/`~/.vst3`; `git status` clean (`build-linux/` ignored). The launch configs' lua path exists. Debugging in REAPER not re-checked (same template as T-005).
- **clangd on Linux** (see D1, D2): clean only with clangd ≥ 19 and a Linux `.clangd`.
- mac: the changed TUs (`Sand.cpp`, `SandUI.cpp`) compile with the exact Xcode 26.5 commands (Apple clang, C++23, OS_MAC, Metal; +`-I` plog, see F8). Scratch DerivedData/CLAP_PATH; nothing installed in `~/Library`; no tracked file changed by the build; temporary artefacts removed.

Validators [D]:
- clap-validator 31 pass / 7 fail / 6 skip: `state-reproducibility-{basic,binary,buffered}` → T-006; `process-audio-double-{in,out}-of-place` → F5; `process-varying-sample-rates` → F3; `param-conversions` → F4.
- VST3 validator: 537/537 when it finishes, but **segfaults in 4 of 10 runs** → F1 (ASan: `validators/vst3-asan*.txt`).

Cross-platform findings (NOT Linux; nothing changed; for the author / CROSS_PLATFORM_FINDINGS):
- **F1 — High. Sand processes 0-frame blocks out of bounds.** VST3 hosts call `process` with `numSamples = 0` to flush parameters (validator `FlushParamTest`; iPlug passes it on, `IPlugVST3_ProcessorBase.cpp:432` → `Sand::ProcessBlock`). `SandModule::process` then reads `chan.back()`/`chan.front()` (`sand_module.hpp:376`, `:421` `_prevLastSamples = chan.back()`) and `hvoya::utils::derivativeInplace` reads `data.back()` and writes `data.front()` of an empty span (`utils.hpp:132-134`): reads before and writes into host buffers that are not the block's — in the validator already freed (ASan heap-buffer-overflow and heap-use-after-free), and garbage enters the DSP state. Release: VST3 validator segfault 4/10; Debug: `assert(!data.empty())` aborts. Fix suggestion: return early from `Sand::ProcessBlock` when `nFrames == 0` (and/or make `derivativeInplace` tolerate empty spans).
- **F2 — Medium. MIDI-CC-driven values never reach a VST3 host.** `hvoya/utils/midi_cc/mediator.hpp:140-145` runs on the audio thread (`ProcessMidiMsg`) and calls `BeginInformHostOfParamChange`/`SendParameterValueFromDelegate`/`EndInform…`. CLAP queues them as output events (REAPER sees 0.000); in VST3 these are UI-thread `IComponentHandler` calls — REAPER keeps reporting 1.000 while the knob shows 0.000 (automation/recall would miss the CC value), and calling the handler off the UI thread breaks the VST3 threading rules. Same class as T-003 finding 4 (Gneiss, VST3 after `>`). Shared hvoya code: Gneiss/Scape/DMT too.
- F3 — Low. NaN output at 768 kHz with default parameters (clap-validator `process-varying-sample-rates`, sample 54, port 0 ch 0); all lower rates pass; ASan clean. DSP only.
- F4 — Low. `flt hz` display text does not parse back: '1.01 kHz' → 0.0154 → '1.0 Hz' (`param-conversions`); the string-to-value path ignores the unit prefix. Same family as Gneiss 'preFlt A hz'.
- F5 — Low/Medium. Sand processes in the host's input buffer (`hvoya::AudioBuffer processed (ins, …)` in `Sand::ProcessBlock`), so out-of-place 64-bit hosts get their input overwritten (`process-audio-double-*`). With float I/O iPlug passes its own scratch. Same as Gneiss.
- **F7 — Medium. Windows projects likely fail to link.** Since 6036dec (2026-05-13) the hvoya envelope detectors call `hiir::PolyphaseIIR2Designer::compute_coefs`; the Xcode project compiles `PolyphaseIIR2Designer.cpp`, none of `projects/*.vcxproj` (last updated 2026-03-30) does. Not built on Windows here.
- F8 — Low. Command-line `xcodebuild` of Sand (master too, verified in a temporary worktree, removed) fails: `plog/Log.h` not found; `$(HVOYA_PATH)/**` reaches clang unexpanded (`-I../../../IPlug2/hvoya` twice). Xcode IDE builds presumably work for the author; matters for CI/scripts.
- Info: GCC 13 `-Wstringop-overflow` in `IEditorDelegate::SendCurrentParamValuesFromDelegate` (inlined `GetParam(i)->GetNormalized()`): a false positive on the null branch, iPlug core.

Decisions for the user:
- **D1 — Sand's `.clangd` is mac-only** (`-DOS_MAC`, `-DIGRAPHICS_METAL`, `/Volumes/Storage/code/...` include paths, homebrew `clang++`). clangd reads it on every OS, so on Linux it adds OS_MAC on top of the CMake compile commands and every TU errors. clangd has no per-OS condition. Options: (a) move those flags to `clangd.fallbackFlags` in the mac `Sand.code-workspace` (they only matter there because the mac side has no compile_commands) and make `.clangd` the neutral Linux/T-005 one (`-D__FLOAT_WORD_ORDER__=__BYTE_ORDER__`); (b) give the mac side compile commands (CMake `macos-ninja`) and drop the hard-coded flags; (c) leave it, and Linux clangd stays broken for Sand. Tested on the VM: with a Linux `.clangd` and clangd 19, `Sand.cpp`/`SandUI.cpp` check clean. Not changed (author's mac IDE config).
- **D2 — clangd ≥ 19 for Sand.** hvoya's `constexpr` LUT constructor with a `std::function` parameter is valid only with C++23's P2448, which clang implements from 19; Ubuntu 24.04's default clangd is 18 (`apt install clangd-19`, or let the VS Code extension download one). The workspace does not pin `clangd.path`; say if it should (`clangd-19`).

VM state left: `~/dev/IPlug2` (T-012 tree, no .git), `~/dev/hvoya/Sand-linux` (clone, `build-linux/linux-debug`), `clangd-19` installed, Debug `Sand.clap`/`Sand.vst3` in `~/.clap`/`~/.vst3`. Nothing running.

Next: independent review; then hand over. T-021 can use `iplug2-linux:22.04-gcc13` for Sand.
