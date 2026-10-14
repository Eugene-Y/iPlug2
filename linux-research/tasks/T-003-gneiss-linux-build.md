# T-003 — Gneiss: Linux CMake build (CLAP, VST3, APP)

- **Status:** ready
- **Owner:** —
- **Branches/worktrees:**
  - Gneiss repo: `linux-experiment` in `/Volumes/Storage/code/hvoya/Gneiss-linux` (local only, never push).
  - iPlug2: `linux/T-003` from `hvoya-linux`, in `/Volumes/Storage/code/IPlug2-T-003`, and only if iPlug2 changes are needed.
- **Depends on:** nothing. `hvoya-linux` already has the Linux core (P0–P3b).
- **Plan phase:** P7 (PLAN.md).

## Goal

Gneiss builds on Linux as CLAP, VST3 and standalone APP from a CMake project, loads in REAPER (Docker), and shows its UI. This is the first time a real product runs on the port; expect to find port bugs.

## Context
- **Gneiss today.** It builds only via Xcode (`projects/Gneiss-macOS.xcodeproj`) and VS (`projects/*.vcxproj`), and uses NanoVG (Metal on mac, GL2 on Windows).
  - Pre-build steps: `scripts/prepare_resources-mac.py` / `-win.py` and `write_plug_build_info_to_file.bat`.
  - Resources: `resources/`, fonts (e.g. `RobotoMono-Regular.ttf`, see `config.h`), SVG/PNG images.
  - `config.h`: `PLUG_SHARED_RESOURCES 0`.
- **Shared utils** are in the iPlug2 repo under `hvoya/` (`hvoya/utils`, `hvoya/libs`: fmt, plog, ghc). Gneiss reaches them relative to IPlug2.
- **Mind the paths.** The Xcode/VS projects use relative paths to `../../../IPlug2`, the **main hvoya checkout**. For Linux, point CMake explicitly at the Linux tree (`IPLUG2_DIR=/Volumes/Storage/code/IPlug2-linux`, or your T-003 iPlug2 worktree). Never at `/Volumes/Storage/code/IPlug2`.
- **CMake API** (`Scripts/cmake/`, the `iplug_add_plugin`-style functions): see how the examples do it — `Examples/IPlugEffect/CMakeLists.txt` and `Examples/IPlugControls/CMakeLists.txt` on `hvoya-linux`.
- **Resources on Linux** go to a per-module folder `<Name>.resources/` (P1, notes/08) — not a shared `resources/`.
- **Known Gneiss specifics:**
  - `OnIdle` calls `GetUI()`/`SetLatency()` (main-thread only; P2 guarantees that in REAPER).
  - It uses platform popup menus, which on Linux become IGraphics stand-in menus (notes/13).
  - `DrawNow()` (implemented).
  - `RemoveAllControls()` from `OnIdle` on preset/morph change (notes/15 H2, fixed).
  - File dialogs are still stubs (cancelled) until T-002.

## Steps
1. Read `AGENTS.md`, PLAN §1–2, notes/08 (P0–P1 build decisions), and the example CMakeLists.
2. Write `Gneiss-linux/CMakeLists.txt`, plus `CMakePresets.json` if helpful:
   - sources from the Xcode/VS projects (keep the list in one place);
   - include paths for `hvoya/utils`, `hvoya/libs`;
   - defines matching the Windows build (`IGRAPHICS_NANOVG`, `IGRAPHICS_GL2`, `SAMPLE_TYPE_FLOAT`, release `PLOG_DISABLE_LOGGING`);
   - resources and fonts copied into the bundle/resources folder.
3. Port the pre-build steps: build-info header generation and resource preparation (a Python/CMake step that works on Linux).
4. Build in Docker on 24.04 (all three formats), and 22.04 for CLAP/VST3 (glibc floor). Check the exports (`scripts/inspect-binary.sh`).
5. Run clap-validator and the VST3 validator, then REAPER with `run-gui-test.sh` (no step file at first): the editor renders; look at the screenshot yourself. Then interact: knob drag, a menu, a preset switch (exercises `RemoveAllControls` from `OnIdle`), open/close ×5, two instances.
6. Check the host-loop trace build (`-DIPLUG_TRACE_HOST_LOOP`): `OnIdle` on the main thread for CLAP and VST3.
7. Any iPlug2-side fix goes in `linux/T-003`, following the shared-code rules in AGENTS §2. Anything that looks like a Gneiss bug: note it, and don't fix it in Gneiss beyond what the build needs; T-010 owns Gneiss-side changes.

## Acceptance
- [ ] CMake configure+build of CLAP, VST3 and APP on 24.04; CLAP+VST3 on 22.04. Exports minimal; GLIBC floor recorded.
- [ ] Validators: results recorded; failures explained or ticketed. `state-reproducibility` is known — see T-006.
- [ ] Gneiss UI renders in REAPER (CLAP and VST3) and in the APP (`--no-io`). Screenshots reviewed by eye.
- [ ] Interaction smoke test passes (above); no crash in 20× open/close; main-thread trace clean.
- [ ] The Gneiss main checkout and `/Volumes/Storage/code/IPlug2` are untouched. Nothing pushed.
- [ ] Self-review done (AGENTS §4.4); evidence in `linux-research/evidence/t003/`; Log below complete.

## Log
(append: date — what was done — commits — next)
