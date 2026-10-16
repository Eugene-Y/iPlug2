# 20 — IDEs for Linux development of iPlug2 plug-ins (T-005, 2026-10-16)

## Recommendation

- **Primary: VS Code** + CMake Tools + clangd + C/C++ (cpptools, for the `cppdbg`/gdb debugger).
  Its whole setup is text in the repo (presets, one `<Name>-linux.code-workspace` with tasks and launch
  configurations), so `duplicate.py` (T-011) can produce it for every new plug-in. Verified end to end on the VM.
- **Alternative: Qt Creator** (Community, GPLv3). It is a full IDE, reads the same `CMakePresets.json`
  (include files too) and has gdb/lldb built in. Its run configurations cannot be checked in portably, so
  "launch REAPER with the plug-in" is a one-time manual Custom Executable in Run settings.
- **CLion** only with a paid licence. The free licence forbids developing anything sold (below). It reads the
  presets natively, so nothing extra is needed in the repo.

The user provisionally agreed to VS Code as the primary IDE on 2026-10-16, with IPlugEffect and IPlugControls as
the templates to develop in and duplicate from.

## Comparison

| | VS Code | Qt Creator | CLion | KDevelop | Zed |
|---|---|---|---|---|---|
| Licence for selling plug-ins | MS licence: "use any number of copies … to develop and test your applications"; extensions have their own licences (cpptools: Microsoft products only, enforced since v1.24.5, 2025-04) [1][2] | GPLv3 (open-source edition); the IDE's licence does not reach the code it edits, as long as you don't redistribute a modified Qt Creator; don't mix with commercial Qt in one project [3] | **Free licence forbids it**: "Commercial Use" = "use … to develop commercial products (products distributed or made available for a fee …)", Toolbox Non-Commercial Agreement v3.0, eff. 2025-12-16, §2.3, §3.2 [4]; paid licence otherwise | GPL, free | GPL/AGPL editor, free |
| arm64 / x86_64 | both (.deb, verified arm64 1.142.0) | both; official docs: Ubuntu 22.04+ x86_64, 24.04+ arm64 [5]; Ubuntu 24.04 apt has 13.0.0 arm64 (verified) | both (separate `-aarch64.tar.gz`) [6] | both (Ubuntu 24.04: 23.08.5) | both |
| CMakePresets | CMake Tools reads them, `include` works (verified) | presets up to v10 [7]; `include` works with 13.0.0 (verified: "Linux Debug (CMake preset)" kits) | native | no support found (2026-10) | none (only `compile_commands.json` for clangd) [8] |
| Code model on iPlug2 | clangd 18: 1.4–8.7 s per TU, ~200–250 MB (`clangd --check`, VM, 4 CPU); clean once `.clangd` is in place (below) | clangd-based; **disabled by default on an 8 GB machine** ("Enable Anyway") | own engine (CLion Nova) | libclang-based | clangd |
| Debug in REAPER / APP | `cppdbg` launch of REAPER with a startup script; pending breakpoints in the `dlopen`ed plug-in work (verified CLAP, VST3, APP) | gdb, "Custom Executable" run configuration (manual, once) | gdb/lldb, custom run configuration | gdb | gdb/CodeLLDB adapters since 2025-06 [9]; no CMake integration |
| Containers / remote | Dev Containers, Remote-SSH (official) | remote Linux devices via SSH | remote dev (Gateway), Docker toolchains | — | remote via SSH |

Not pursued: Eclipse CDT (heavy, weaker CMake UX), GNOME Builder (Flatpak-first, sandboxed toolchain), VSCodium
(no cpptools since 2025-04; CodeLLDB + clangd would work there, but it is a second configuration to maintain).

## What the checked-in setup does

- `Scripts/cmake/Linux/CMakePresets-linux.json`: `linux-debug`, `linux-release` (Ninja, NanoVG/GL2,
  `CMAKE_EXPORT_COMPILE_COMMANDS`, `IPLUG_DEPLOY_PLUGINS=ON`, `binaryDir` `build/<preset>`), Linux-only via
  `condition`. Included by the root `CMakePresets.json` and by each example's `CMakePresets.json`. One definition.
- Root `CMakePresets.json`: upstream `base-linux` forced `IGRAPHICS_RENDERER=GL3`; now GL2 (PLAN, notes/11).
- `Examples/<Name>/<Name>-linux.code-workspace`: tasks "Build APP", "Build and deploy CLAP/VST3" (CMake Tools tasks
  with the active build preset), launch "Debug standalone APP", "Debug CLAP/VST3 in REAPER". The examples'
  `.gitignore` ignores `.vscode/*`, and a separate workspace leaves mac/win VS Code users' folder settings alone.
- `Scripts/linux/reaper-open-fx.lua`: REAPER runs it at startup (`reaper -new <script>`); it adds the plug-in named
  in `IPLUG2_REAPER_FX` to a new track and opens its editor.
- `.clangd` (root and each example): GCC defines `__FLOAT_WORD_ORDER__`, clang does not, so with GCC compile commands
  `WDL/wdlendian.h` hits `#error Unsupported float endian` in every TU under clangd. The fix adds
  `-D__FLOAT_WORD_ORDER__=__BYTE_ORDER__` for clangd only. The same `#error` would stop a real clang build on Linux
  (Linux-only; macOS takes the `__APPLE__` branch).

## Verified (evidence `evidence/t005/`; PNGs are git-ignored, local in the T-005 worktree)

Docker (`docker/`): `cmake --preset linux-debug` + `cmake --build --preset linux-debug` in `Examples/IPlugEffect`
(154 steps, CLAP/VST3 copied to `~/.clap` / `~/.vst3` with `IPlugEffect.resources`). REAPER under gdb with
`reaper-open-fx.lua`: source-line breakpoints `IPlugEffect.cpp:53` (ProcessBlock, audio thread, transport
stopped) and `IControls.cpp:687` (`IVKnobControl::OnMouseDown`, main thread via `IGraphicsLinux::HandleButton`)
both hit (`gdb-reaper.log`).

VM (Ubuntu 24.04.5 arm64, GNOME/X11), as a developer would:
1. `git clone --depth 1 --branch hvoya-linux https://github.com/Eugene-Y/iPlug2.git`, T-005 files on top.
2. apt list from `Documentation/linux-development.md`; `Dependencies/IPlug/download-vst3-sdk.sh` and
   `download-clap-sdks.sh` work unchanged on Linux.
3. Native build of IPlugEffect and IPlugControls with the presets: PASS (CMake 3.28.3, GCC 13).
4. VS Code 1.142.0 (`--disable-gpu`, to spare virgl): open `IPlugEffect-linux.code-workspace`, pick "Linux Debug"
   (`04-select-preset.png`), configure; clangd "No problems" (`07-clangd-fixed.png`).
5. Breakpoints hit from the IDE:
   - "Debug CLAP in REAPER": `ProcessBlock` (`20-vscode-clap-bp-processblock.png`), knob `OnMouseDown`
     (`22-vscode-clap-bp-onmousedown.png`);
   - "Debug standalone APP": `ProcessBlock` (`23-…`), `OnMouseDown` (`25-…`);
   - "Debug VST3 in REAPER": `ProcessBlock` (`26-…`).
6. IPlugControls workspace: fresh configure, build, "Debug CLAP in REAPER" opens the full editor (`27-…`).
7. Qt Creator 13.0.0 (apt): opens `Examples/IPlugEffect/CMakeLists.txt`, offers both presets as kits, configures
   (`30-…`, `31-…`). Not driven further.

## Lessons (also in the docs)

- One manual step remains in VS Code: selecting the configure preset once per checkout. CMake Tools has no
  "default preset" setting.
- REAPER needs a working audio device, or `ProcessBlock` never runs: on PipeWire desktops choose PulseAudio.
- `cmake.copyCompileCommands` alone is not enough: clangd searches for the database above each source file, so
  framework sources outside the project (`../../IGraphics/...`) got no flags. `--compile-commands-dir` fixes it.
- Harness trap: `vm-display-env` puts the sudo gdb wrapper on PATH; an IDE started from it debugs REAPER as root
  (other HOME and profile, no audio). Recorded in `scripts/vm/README.md`.

## For T-011 (`duplicate.py`)

- In-tree duplication (`duplicate.py IPlugEffect MyFx Mfr`) already renames `MyFx-linux.code-workspace` and
  rewrites target names (`MyFx-clap`) and `IPLUG2_REAPER_FX` ("CLAP: MyFx"). Tested 2026-10-16.
- Out-of-tree (4th argument): `build/` is copied (172 MB here). `DONT_COPY` has `build-*` but not `build`, and the
  presets put builds in `build/<preset>`. `../..` in `CMakePresets.json`, the workspace and `CMakeLists.txt` is
  not rewritten: `newroot` is computed for `config/`, and only the xcconfig's `IPLUG2_ROOT` string is replaced.
  The `CMakeLists.txt` case predates T-005.

## Sources (read 2026-10-16)

1. VS Code licence, §1.1 and §5: https://code.visualstudio.com/license
2. cpptools blocked in forks from v1.24.5: https://www.theregister.com/2025/04/24/microsoft_vs_code_subtracts_cc_extension/
3. Qt Creator licence discussion with Qt Company staff (2020-03): https://lists.qt-project.org/pipermail/interest/2020-March/034747.html
4. JetBrains Toolbox Subscription Agreement for Non-Commercial Use v3.0: https://www.jetbrains.com/legal/docs/toolbox/license_non-commercial/ ; announcement 2025-05: https://blog.jetbrains.com/clion/2025/05/clion-is-now-free-for-non-commercial-use/
5. Qt Creator 20.0.2 supported platforms: https://doc.qt.io/qtcreator/creator-desktop-platforms.html
6. CLion aarch64 tarball (community packaging pointing at JetBrains' URL): https://git.calculate-linux.org/calculate/gentoo-overlay/src/branch/master64/dev-util/clion/clion-2025.2.4.ebuild
7. Qt Creator CMake presets: https://doc.qt.io/qtcreator/creator-build-settings-cmake-presets.html
8. Zed C/C++: https://zed.dev/docs/languages/c.md
9. Zed debugger: https://zed.dev/docs/development/debuggers
