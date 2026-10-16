# T-011 — Painless duplication: a new plug-in project that builds and debugs on Linux at once

- **Status:** ready once T-005 is merged (its files are the template). The T-005 worker takes it after T-005's review.
- **Owner:** —
- **Branch/worktree:** `linux/T-011` from `hvoya-linux` after the T-005 merge (or from `linux/T-005` if the merge is still pending), in `/Volumes/Storage/code/IPlug2-T-011`.
- **Depends on:** T-005 (presets, `<Name>-linux.code-workspace`, `.clangd`, `Scripts/linux/reaper-open-fx.lua`, `Documentation/linux-development.md`).
- **Plan phase:** P11b.
- **Conflicts:** `Examples/duplicate.py`, `Examples/gitignore_template` (shared with mac/win).

## Goal
`duplicate.py <Example> <NewName> <Mfr> [<path>]` gives a project that, on Linux, opens in VS Code and builds and debugs the APP, CLAP and VST3 (in REAPER) with no hand edits. That holds both inside `Examples/` and in a separate folder or repo next to iPlug2, which is how products (Gneiss, Sand) live. No hand-written CMake.

## User decision (2026-10-16)
The user wants every rough edge below fixed: "главное чтобы все эти шероховатости были исправлены". That covers the shared `duplicate.py`/`gitignore_template` changes, which also change mac/win behaviour, for the better: today an out-of-tree duplicate's CMake build is broken on every platform. Keep the Xcode/VS output of `duplicate.py` byte-identical apart from these fixes, and prove it (below).

## Known problems (found in T-005, notes/20 §"For T-011")
1. **Out-of-tree paths are not rewritten.** `duplicate.py` computes `newroot` relative to `config/` and replaces only the xcconfig's `IPLUG2_ROOT` string. Project-root files keep `../..`:
   - `CMakeLists.txt`: `IPLUG2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.."` (pre-existing upstream bug, CROSS_PLATFORM_FINDINGS candidate; T-005 Log);
   - `CMakePresets.json`: `include` path;
   - `<Name>-linux.code-workspace`: path to `Scripts/linux/reaper-open-fx.lua`.

   Fix: compute the root relative to each file's own directory, or make these files take the iPlug2 location from one place.
2. ~~`build/` is copied~~ / ~~`gitignore_template` lacks `build/`~~: resolved in T-005's review round. The presets
   now build into `build-linux/<preset>`, which `DONT_COPY` (`build-*`) and every `.gitignore` already cover.
   Still check that the duplicate's `git status` stays clean after a build (acceptance).
3. **Verify `.clangd` travels** with the project. It is needed out of tree, where iPlug2's root `.clangd` does not apply to the project's files.
4. **Check other leftovers** in a duplicate: the RPP (references the mac VST name), README, `.vscode/c_cpp_properties.json`, the hvoya utils include (`hvoya/hvoya.cmake` via `IPLUG2_DIR`). Fix what breaks Linux; report the rest.

## Also in scope
- **Gneiss:** add the same Linux setup (`CMakePresets.json` including iPlug2's Linux presets, `Gneiss-linux.code-workspace`, `.clangd`) on the Gneiss `linux-experiment` branch (worktree `/Volumes/Storage/code/hvoya/Gneiss-linux`, local only, never push). Its `CMakeLists.txt` exists from T-003. Read Gneiss's `CLAUDE.md`/`ARCHITECTURE.md` first.
- **Sand:** T-012 builds Sand. Its task file must say to reuse this template.
- **Docs:** update `Documentation/linux-development.md` §6: duplicate anywhere, then open.

## Acceptance
- [ ] In-tree and out-of-tree duplicates of IPlugEffect and IPlugControls: on the VM, open the `-linux` workspace, pick the preset, build; breakpoints in `ProcessBlock` and a control's `OnMouseDown` hit in REAPER (CLAP) and in the APP. Screenshots in `evidence/t011/`.
- [ ] The new project's `git status` after a build shows no build output.
- [ ] mac: `duplicate.py` output for IPlugEffect, diffed before/after the change, differs only in the fixed paths and the skipped `build/`. An out-of-tree duplicate builds with xcodebuild (CLAP, DerivedData in scratchpad) and with `cmake --preset macos-ninja`.
- [ ] Gneiss `linux-experiment`: the workspace opens, builds and debugs in REAPER on the VM.
- [ ] Independent review (AGENTS §4); Log complete.

## Log
