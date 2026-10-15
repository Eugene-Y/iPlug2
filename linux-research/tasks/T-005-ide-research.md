# T-005 — IDE research and IDE/presets setup for the examples

- **Status:** ready
- **Owner:** —
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
- [ ] `notes/NN-ide-research.md` with the comparison table, sources and a recommendation (primary + alternative).
- [ ] Presets and IDE configs checked in; mac/win presets unaffected. Validate on mac: `cmake --list-presets` still works.
- [ ] On the VM: build plus both breakpoints hit in the recommended IDE. Screenshots in `evidence/t005/`.
- [ ] Docs written; self-review; Log complete.

## Log
