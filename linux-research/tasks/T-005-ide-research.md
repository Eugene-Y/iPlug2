# T-005 — IDE research and IDE/presets setup for the examples

- **Status:** in-progress
- **Owner:** Claude instance (T-005 session, 2026-10-16)
- **Branch/worktree:** `linux/T-005` from `hvoya-linux`, in `/Volumes/Storage/code/IPlug2-T-005`.
- **Depends on:** nothing.
- **Plan phase:** P11 (part a); part b, `duplicate.py`/templates, is T-011.

## Goal
Pick the recommended free IDE(s) for Linux development of iPlug2 plugins. Make IPlugEffect and IPlugControls open, build and debug in them with zero manual CMake work. The user wants this production-ready.

## Requirements from the user
- Free and good; the user sells plugins, so licence terms for **commercial** use matter.
- Configure "solutions/projects" for IPlugEffect, IPlugControls (here), and later Gneiss and Sand (T-003/T-012 will reuse your setup).
- Developers must not have to hand-write CMake lists.

## Constraint
Follow AGENTS §2 "Minimal footprint". Presets and IDE configs are **additive**: the existing Xcode/VS projects, `common-*.xcconfig`/`.props`, `config.h` and the macOS/Windows CMake presets keep working unchanged, and nothing in them may start depending on the new files.

## Steps
1. **Research.** Cover each candidate below, with sources and dates:
   - Candidates:
     - **CLion**: free for non-commercial use only since 2025 — read the exact terms for selling plugins;
     - **VS Code** + CMake Tools + clangd + CodeLLDB or cppdbg;
     - **Qt Creator** Community;
     - **KDevelop**;
     - anything notable you find.
   - For each:
     - licence for commercial use;
     - arm64 and x86_64 availability;
     - CMakePresets support;
     - clangd/indexing quality on iPlug2 (big headers);
     - debugger UX for "attach to / launch REAPER with the plugin" and for the standalone APP;
     - remote or containers workflow (building in our Docker image vs natively).
2. **Native build on the VM** (take the VM lock). Can a developer build natively on Ubuntu 24.04 without Docker? Write the exact apt package list (the Dockerfile shows what's needed), and verify by building IPlugEffect natively on the VM.
3. **Presets and IDE configs, checked in.**
   - `CMakePresets.json` at the repo root or per example, with Linux configure/build presets (Debug/Release, formats as targets). Keep mac/win presets working; upstream has a `CMakePresets.json` at the root — extend it, don't break it.
   - For the chosen IDE(s):
     - VS Code: `.vscode/settings.json`, `launch.json` with "Debug standalone" and "Debug in REAPER" — launch `/opt/REAPER/reaper` with the plugin deployed, using a pre-build deploy task;
     - Qt Creator/CLion: whatever is needed beyond presets.
4. **Prove it on the VM.** Open IPlugEffect in the chosen IDE; build; set a breakpoint in `ProcessBlock` and one in the UI (e.g. a knob's `OnMouseDown`); hit both while running in REAPER and in the APP. Take screenshots via `scripts/vm/vm-shot.sh`.
5. **Document.** Write `Documentation/linux-development.md` (or similar): install, open, build, debug, deploy.

## Acceptance
- [x] `notes/NN-ide-research.md` with the comparison table, sources and a recommendation (primary + alternative). → `notes/20-ide-research.md`
- [x] Presets and IDE configs checked in; mac/win presets unaffected. Validate on mac: `cmake --list-presets` still works.
- [x] On the VM: build plus both breakpoints hit in the recommended IDE. Screenshots in `evidence/t005/`.
- [x] Docs written; self-review; Log complete. → `Documentation/linux-development.md`

## Log

### 2026-10-16 — claimed
- Lock `/Volumes/Storage/code/.linux-port-locks/T-005`, worktree `/Volumes/Storage/code/IPlug2-T-005`, branch `linux/T-005`.
- Findings so far:
  - Root `CMakePresets.json` already has upstream `linux-ninja`/`linux-make`, but `base-linux` forces `IGRAPHICS_RENDERER=GL3`, against PLAN's GL2 decision (CMake's own Linux default is GL2).
  - `Examples/*/.gitignore` ignores `.vscode/*`; only `c_cpp_properties.json` (mac/win cpptools config) is tracked. So the Linux VS Code setup goes into a separate `<Name>-linux.code-workspace` (settings + tasks + launch in one file), which mac/win VS Code users never open.
  - VM reachable; it has gdb/g++ but no cmake/ninja: a clean target for the native-build check.

### 2026-10-16 — work done (commits cdb79d56c, 1c5f3f480, + docs commit)
- **Decision:** VS Code primary, Qt Creator alternative, CLion only with a paid licence (its free licence v3.0 §2.3 forbids developing sold products). The user provisionally agreed to VS Code and asked for IPlugEffect + IPlugControls as templates to develop in and duplicate from. Details and sources: `notes/20-ide-research.md`.
- **Files:**
  - `Scripts/cmake/Linux/CMakePresets-linux.json` (linux-debug / linux-release), included by the root and both examples' `CMakePresets.json`;
  - root `base-linux` GL3 → GL2;
  - `Examples/<Name>/<Name>-linux.code-workspace` (tasks + launch: APP, CLAP/VST3 in REAPER);
  - `Scripts/linux/reaper-open-fx.lua`;
  - `.clangd` (root + examples);
  - `Documentation/linux-development.md`.
- **mac:** `cmake --list-presets` (root, and `Examples/IPlugEffect` → no presets, rc 0) and `cmake --preset macos-ninja` configure PASS (renderer METAL). No Xcode/VS project, xcconfig, props or C++ source touched.
- **Docker:** example `linux-debug` configure + full build; root `linux-debug` and `linux-ninja` configure (GL2). REAPER under gdb with the Lua script: ProcessBlock + `IVKnobControl::OnMouseDown` breakpoints hit (`evidence/t005/docker/gdb-reaper.log`).
- **VM** (lock taken and released twice; `xdpyinfo` OK before and after):
  - fresh clone from GitHub, apt list from the docs, SDK download scripts;
  - native build of both examples PASS;
  - VS Code 1.142: preset pick, clangd clean;
  - breakpoints hit from the IDE: CLAP in REAPER ×2, APP ×2, VST3 ProcessBlock;
  - IPlugControls: CLAP debug run opens the full editor;
  - Qt Creator 13: presets as kits, configure.

  Screenshots `evidence/t005/vm/*.png` (git-ignored, local).
- **Bugs fixed on the way:**
  - clangd got no flags for framework sources outside the project → `--compile-commands-dir`;
  - clangd `#error` from `WDL/wdlendian.h` → `.clangd`;
  - ~~`scripts/vm/README.md` wrongly said `linux_audio_mode=2` is dummy audio (it is JACK)~~. **Wrong, corrected in the review round:** 2 *is* Dummy Audio; the JACK I saw was the root-run REAPER's default profile. Kept: the gotcha that the sudo gdb wrapper on PATH makes IDE debuggees run as root.
- **VM state left behind** (useful for later tasks):
  - installed: VS Code + extensions, clangd, Qt Creator, native build deps;
  - `~/dev/iPlug2`, a hvoya-linux clone with T-005 files;
  - `~/.clap`, `~/.vst3`: **Debug** builds of IPlugEffect/IPlugControls (deploy before VM tests, as usual);
  - `~/.config/REAPER`: a full profile with PulseAudio (`linux_audio_mode=3`), not the seeded minimal one;
  - `~/.config/Code/User/settings.json`: trust prompt off;
  - apport crash reports from a root REAPER I killed.
- **For T-011:** task file `tasks/T-011-duplicate-templates.md` (written 2026-10-16 at the user's request: every rough edge must be tracked and fixed). Out of tree, `duplicate.py` does not rewrite `../..` in project-root files (notes/20 §"For T-011"). The `build/` copy is resolved by the review fix M1 below.
- **For T-004:** APP under the debugger logs `RtApiAlsa::probeDeviceOpen: unable to synchronize input and output devices` at start (ALSA `default` = PipeWire). It still runs, with underruns while stopped in the debugger.

### Review round 1 (2026-10-16, fresh-context reviewer)
- Full text: `notes/21-review-T-005.md`. **Verdict: changes requested.**
- Re-ran and PASS:
  - mac preset list (identical to hvoya-linux) and `macos-ninja` configure;
  - Docker IPlugControls `linux-debug` build;
  - REAPER under gdb: `IPlugControls::ProcessBlock` hit, editor open;
  - pristine ubuntu:24.04 build with only the docs' apt list.
- Major:
  - M1 — `build/<preset>` + `duplicate.py` (in-tree too) copies a stale CMakeCache, so the duplicate's `cmake --preset` fails. Suggested fix: binaryDir `build-linux/<preset>`.
  - M2 — `scripts/vm/README.md` edit is wrong: `linux_audio_mode=2` is Dummy Audio, 0 is JACK (verified in Docker). Log line 82 above is wrong too.
- Minor:
  - m1 — add `-newinst` (a running REAPER swallows the debug launch);
  - m2 — the examples' `CMakePresets.json` puts CMake Tools on mac/win into an empty presets mode;
  - m3 — `iplug2.reaperPath` override needs a tracked-file edit;
  - m4 — 31-qtcreator-configured.png doesn't show a configure.
- Plus nits.

### Cross-platform findings
- (low, pre-existing upstream) `Examples/duplicate.py` with an output path outside the iPlug2 tree leaves `IPLUG2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.."` in the copied `CMakeLists.txt`: `newroot` is relative to `config/`, and only the xcconfig's `IPLUG2_ROOT` value is replaced. CMake builds of such a duplicate fail on mac/win too. Repro: `python3 duplicate.py IPlugEffect MyFx Mfr /tmp/x`, then `grep IPLUG2_DIR /tmp/x/MyFx/CMakeLists.txt`.


### Review round 1 — fixes (2026-10-16)
- **M1:** presets build into `build-linux/<preset>` (covered by `build-*` in `DONT_COPY` and every `.gitignore`); workspaces point `copyCompileCommands` and clangd there. Verified in Docker: built IPlugEffect, duplicated it in-tree (`T005Dup`), and the duplicate configures and builds (`T005Dup.clap`). Test duplicate removed.
- **M2:** `scripts/vm/README.md` now gives the mapping 0 JACK / 2 Dummy / 3 PulseAudio (1 presumably ALSA). Fixed the stale comments in `reaper-up.sh` and `vm-provision.sh`, and my Log line above.
- **m1:** `-newinst` added to both REAPER launches and to the Qt Creator args in the docs. Verified in Docker with a REAPER already running: 2 instances, ProcessBlock breakpoint hits in the debugged one (`evidence/t005/review-fix/gdb-newinst.log`).
- **m3:** the docs explain the `/opt/REAPER` symlink and that workspace settings win over user settings.
- **m4:** notes/20 no longer claims a captured configure in Qt Creator.
- **Nits:**
  - docs say `reaper/mediafx` instead of "audio thread";
  - the hidden preset is now `linux-common` (it was `linux-base`, next to root's `base-linux`).
- **Open, user's call:**
  - **m2**: the examples' `CMakePresets.json` puts CMake Tools on mac/win into presets mode with an empty list.
- Also: the T-004 task file got a pointer to the APP's RtAudio warning, and `tasks/T-011-duplicate-templates.md` was written (the user asked for every rough edge to be tracked).
