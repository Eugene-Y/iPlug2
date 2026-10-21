# T-011 — Painless duplication: a new plug-in project that builds and debugs on Linux at once

- **Status:** work done, awaiting independent review (AGENTS §4); Gneiss part awaits the user's commit of a patch
- **Owner:** T-011 worker (chat f91df715), lock `.linux-port-locks/T-011`
- **Branch/worktree:** `linux/T-011` from `hvoya-linux` (64312897b), in `/Volumes/Storage/code/IPlug2-T-011`.
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
- [x] The new project's `git status` after a build shows no build output.
- [x] mac: `duplicate.py` output for IPlugEffect, diffed before/after the change, differs only in the fixed paths and the skipped `build/`. An out-of-tree duplicate builds with xcodebuild (CLAP, DerivedData in scratchpad) and with `cmake --preset macos-ninja`. (A duplicate's `CMakePresets.json` holds only the Linux presets, so there is no `macos-ninja` in it; checked with plain `cmake -G Ninja` instead, see Log.)
- [ ] Gneiss `linux-experiment`: the workspace opens, builds and debugs in REAPER on the VM.
- [ ] Independent review (AGENTS §4); Log complete.

## Log

### 2026-10-21 — claimed and done (worker, chat f91df715)
- Lock `.linux-port-locks/T-011`; worktree `/Volumes/Storage/code/IPlug2-T-011`, branch `linux/T-011` from `hvoya-linux` 64312897b. SDKs cloned in with `cp -cR` (not committed).
- **Fixes** (commit 6b35f425d):
  - `Examples/duplicate.py`, out-of-tree only: files in the project folder and in `.vscode/` now get the iPlug2 root relative to the project folder (`CMakeLists.txt` `IPLUG2_DIR`, the `CMakePresets.json` include, the `-linux` workspace's `reaper-open-fx.lua` path, `.vscode/c_cpp_properties.json`). A second pass after `dirwalk()`, matching only whole runs of `..` components, so the existing `config/`/`projects/` rewrite is untouched.
  - `Examples/duplicate.py`: `.knob` added to the binary extensions. **Every** duplicate of IPlugControls (in-tree too, on mac and Linux) stopped halfway with a `UnicodeDecodeError` on `scripts/IBitmapSlider.knob`. Only IPlugControls failed of the 20 examples.
  - `Examples/gitignore_template`: `plug_build_info.hpp`. The Xcode/VS pre-build writes it into the project folder; the example's own `.gitignore` had it, but `duplicate.py` replaces that file, so a mac build left it untracked.
- **Byte-identity proof** (`evidence/t011/duplicate-before-after.diff`, `random.seed(11)` via `evidence/t011/dup-seeded.sh` so the unique ID matches): IPlugEffect in-tree differs only in `.gitignore` (+1 line); out-of-tree only in `.gitignore`, `CMakeLists.txt`, `CMakePresets.json`, `MyFx-linux.code-workspace`, `.vscode/c_cpp_properties.json`. No Xcode/VS file, xcconfig, props or source changes. All 20 examples duplicated before/after: same picture (IPlugControls had no complete "before").
- **Known problems 1–4:** 1 fixed (above). 2: `build-linux/` is not copied and is ignored; verified. 3: `.clangd` travels (root file; seen in VS Code). 4: see "Reported, not fixed" below; the `hvoya/hvoya.cmake` include resolves through `IPLUG2_DIR`, so it works out of tree.
- **mac** (scratchpad only, nothing in `~/Library`):
  - out-of-tree MyFx: `xcodebuild` CLAP (`scripts/coord/mac-xcodebuild.sh`, DerivedData and install paths in the scratchpad): BUILD SUCCEEDED, `MyFx.clap` (`evidence/t011/mac-xcodebuild-clap-tail.log`);
  - `cmake -G Ninja -DIPLUG_DEPLOY_PLUGINS=OFF` + `--target MyFx-clap`: PASS: `Found iPlug2` at the rewritten path (`evidence/t011/mac-cmake-configure.log`, added in review round 1), and the build compiles iPlug2's sources from there (`evidence/t011/mac-cmake-build.log`). `cmake --preset macos-ninja` cannot work in a duplicate (its presets file is Linux-only, T-005 m2).
- **Docker** (`iplug2-linux:24.04`, `evidence/t011/docker/preset-builds.log`, script `evidence/t011/linux-preset-build.sh`): in-tree `Examples/MyFx`, `Examples/MyCtl` and out-of-tree MyFx, MyCtl (scratchpad, 10 levels from `/`): `cmake --preset linux-debug` + `cmake --build --preset linux-debug` PASS, APP + CLAP + VST3 each; the duplicate's own `git status` is clean afterwards (only `!! build-linux/`).
  - REAPER under gdb with the **workspace's own** launch args and environment (parsed from `<Name>-linux.code-workspace`, `${workspaceFolder}` expanded; `evidence/t011/reaper-gdb-workspace.sh`), all 4 duplicates: the Lua script loads the plug-in and opens the editor, `<Name>::ProcessBlock` hits on REAPER's FX thread, a click in the editor hits `IGraphics::OnMouseDown` on the main thread (`evidence/t011/docker/*/gdb-hits.txt`; `oot-MyCtl/reaper-MyCtl.png` looked at: full editor).
- **VM** (lock taken and released; `xdpyinfo` OK before and after every batch; VS Code 1.142 started with a plain PATH): iPlug2 synced to `~/dev/t011/iPlug2`, `duplicate.py IPlugEffect MyFx Mfr ~/dev/t011` there, i.e. the product layout (`../iPlug2`).
  - `MyFx-linux.code-workspace` opens; the preset pick lists the included Linux presets (`vm/01`); configure (`vm/02`); clangd idle with inlay hints.
  - "Debug CLAP in REAPER": `ProcessBlock` hit on `reaper/mediafx` (`vm/04`), editor open (`vm/05`), knob click → `IVKnobControl::OnMouseDown` (`IControls.cpp:687`) on the main thread (`vm/06`).
  - "Debug standalone APP": `ProcessBlock` hit from `IPlugAPPHost::AudioCallback` (`vm/07`), knob click → `IVKnobControl::OnMouseDown` (`vm/10`).
  - From a shell: `cmake --build --preset linux-debug` of MyFx (all three) and out-of-tree MyCtl PASS; both `git status` clean (`vm/native-builds.log`).
  - Removed the `MyFx` CLAP/VST3 I deployed to `~/.clap`/`~/.vst3`. `~/dev/t011/` stays (iPlug2 copy, MyFx, MyCtl, Gneiss copy) for the user to try.
- **Not covered as the acceptance words it:** the VS Code GUI run was done for the out-of-tree IPlugEffect duplicate only. In-tree paths are unchanged by this task (T-005 checked them in VS Code), and IPlugControls was checked with the workspace's launch args under gdb in Docker. The first acceptance box stays open; the user can decide whether that is enough.
- **Gneiss** (`/Volumes/Storage/code/hvoya/Gneiss-linux`, `linux-experiment` 773867e): no `CMakePresets.json`, `Gneiss-linux.code-workspace` or `.clangd`. Not committed there; proposed as `evidence/t011/gneiss-linux-ide-setup.patch` (`git am`-able): Sand's three files with Sand→Gneiss, include `../../IPlug2/...` like Gneiss's `CMakeLists.txt`.
  - Verified on the VM with the patch applied to a copy (`~/dev/t011/hvoya/Gneiss-linux`, `~/dev/t011/IPlug2` → this branch's iPlug2): workspace opens with both presets (`vm/20`), "Debug CLAP in REAPER" builds, deploys and hits `Gneiss::ProcessBlock` (`vm/21`); copy's `git status` clean after the build. clangd 18 shows one error (`constexpr_non_literal_param` in `lookup_table.hpp`): the C++23 P2448 case the docs already send to clangd 19 (`vm/22`).
  - Side effect: the VM's `~/.clap/Gneiss.clap` is now this Debug build (I did not back up the previous one; redeploy before VM tests, as usual).
  - Gneiss and Sand trees: `git status --porcelain --ignored` identical before and after (`!! build-linux/` only).
- **Sand** (`Sand-linux`, `linux-experiment` 052a0bc) already has `CMakePresets.json`, `Sand-linux.code-workspace`, `.clangd` (60c5b96, cec9c1d); nothing to propose.
- **Docs:** `Documentation/linux-development.md` §6 rewritten: duplicate anywhere (4th argument), then open; which paths are rewritten and that they are relative.
- `check-product-names.sh`: clean.

### Cross-platform notes (for CROSS_PLATFORM_FINDINGS.md; the coordinator writes it)
- **Changes mac/win behaviour (fixed here, user-approved 2026-10-16):**
  - out-of-tree duplicates now get correct root-relative paths in `CMakeLists.txt` (their CMake build was broken on every platform; T-005's finding), `CMakePresets.json`, the `-linux` workspace and `.vscode/c_cpp_properties.json` (mac/win cpptools IntelliSense paths were broken too). In-tree output unchanged. **Windows:** as first committed, the rewrite did nothing there (`relpath` gives `..\..`, the files say `../..`; review R1-1). Fixed in round 1: both roots are `/`-separated. Covered by an `ntpath` simulation (`evidence/t011/duplicate-windows-ntpath-sim.txt`), not on a PC.
  - a target reached through a symlink (macOS `/tmp`, a symlinked `~/dev`) now gets working relative paths everywhere, the existing xcconfig/props/mk `IPLUG2_ROOT` included (review R1-2, `realpath`). Output for targets without symlinks is byte-identical.
  - `duplicate.py IPlugControls ...` works again on every platform (pre-existing upstream bug: `UnicodeDecodeError` on `scripts/IBitmapSlider.knob`).
  - every duplicate's `.gitignore` ignores `plug_build_info.hpp` (fork-only file).
- **Reported, not fixed:**
  - (low, upstream) `replacestrs()`/`replacestrsChop()` pass the path through `glob.glob`: a target path with `[`, `]`, `*` or `?` makes every name/manufacturer/root replacement a silent no-op (exit 0, `IPlugEffect` still in `config.h`). Repro: `duplicate.py IPlugEffect BrFx Mfr '<dir>/br[1]'`. The new project-folder pass no longer uses glob (review R1-4).
  - (low, upstream) Windows, target on another drive: `os.path.relpath` raises `ValueError` ("path is on mount 'C:', start on mount 'D:'") and `duplicate.py` dies with a traceback after copying. A fix would catch it and fall back to the absolute path (`/`-separated for the CMake/JSON pass). Review R1-5.
  - (low, upstream) Windows: the existing `config/`/`projects/` rewrite writes `relpath`'s backslashes into every file, the mac `xcconfig`, `.mk` and shell scripts included (`IPLUG2_ROOT = ..\..\iPlug2`). Found while fixing R1-1, not changed.
  - (low, upstream) the first Xcode build of any duplicate modifies 6 tracked `resources/*-Info.plist`: `prepare_resources-mac.py` writes the new `PLUG_UNIQUE_ID` into `CFBundleSignature`; `duplicate.py`'s `set_uniqueid` only edits `config.h`. Repro: duplicate, `git init && git commit`, xcodebuild CLAP, `git status`.
  - (low) the duplicate's `.RPP` keeps the example's plug-in reference: `MyFx.RPP` loads `"VST: MyFx (HvoyaAudio)" MyFx.vst` with the old unique ID (manufacturer not replaced because the RPP says HvoyaAudio, not AcmeInc). Opening it shows a missing plug-in. The Linux debug flow does not use the RPP.
  - (low, fork) `Sand-linux`'s `CMakePresets.json` (and the proposed Gneiss one) include `../../IPlug2/Scripts/cmake/Linux/CMakePresets-linux.json`. On this Mac's layout that is the main checkout on `hvoya`, which has no Linux presets yet: `cmake --list-presets` in `Sand-linux` fails ("Could not read presets"), and so would VS Code's CMake Tools there, until `hvoya-linux` reaches `hvoya`. Xcode/VS are unaffected.

### Review round 1 (2026-10-21)
- `notes/29-review-T-011.md`: changes requested (R1-1 major: no rewrite on Windows; R1-2…R1-7 minor/nits). Fixes and their commits: notes/29 §"Round 1 fixes". Evidence re-run: `evidence/t011/duplicate-before-after.diff` (round-1 code; same picture as round 0), `duplicate-windows-ntpath-sim.txt`.

### Open
- Review round 2.
- User: commit the Gneiss patch on `linux-experiment` (or not); decide whether the out-of-tree IPlugEffect VS Code run + Docker gdb runs satisfy the first acceptance box.
