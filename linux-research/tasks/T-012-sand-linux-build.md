# T-012 — Sand: Linux CMake build (CLAP, VST3, APP)

- **Status:** ready
- **Owner:** —
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
