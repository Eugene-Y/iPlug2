# Review of P0–P1 (`61e272b3a..51479ec66`, fixes to `7e4875daa`), 2026-10-11

Reviewer's re-runs: `build/review-p1-24.04`, `build/review-p1-22.04` (git-ignored). **[R]** = re-run by the
reviewer. **[E]** = checked against the implementer's evidence. **[C]** = checked in code.

## Verdict

**P0–P1 accepted** after one fix round: 2d022fa61 (1), 028a14d21 (2), 67eeacaf1 (3), a8c0b32ec (4) by the
implementer, 7e4875daa (5) by the reviewer. Re-checked: diffs read, IPlugControls CLAP+VST3 rebuilt green
on 24.04, the `PromptForDirectory` callback matches win's `(empty fileName, dir)`. The plan is respected: no GTK in the
plug-in, no render thread, no `GfxMutex`, no pthread `Timer_impl`, no change to shared resize code.
Linux code lives in `IPlug/Linux/` and the stubbed `IGraphicsLinux.*`, behind `#elif defined OS_LINUX`
hooks. Every shared-file change is either platform-neutral with identical mac/win semantics, or a
strict bug fix (the two CLAP commits). Nothing here changes what Gneiss compiles to on mac/win beyond
those two CLAP fixes.

## Findings

| # | Sev | Where | Finding | Status |
|---|---|---|---|---|
| 1 | Medium | `Scripts/cmake/FindiPlug2.cmake` (from RR 27b4a5517 via 75fa19049) | Auto-deploy removed from `iplug_configure_target` and moved into `IPlugPlugin.cmake`. Every direct caller (IPlugReaperPlugin, IPlugSwiftUI, IPlugCocoaUI…) silently lost `IPLUG_DEPLOY_PLUGINS` on **all** platforms. CMake only, so Gneiss (xcodeproj/vcxproj) is unaffected | fixed |
| 2 | Low | `IGraphicsLinux.cpp` stubs | `PromptForFile`/`PromptForDirectory` never called the completion handler; mac calls it on cancel with empty strings | fixed |
| 3 | Low | `IPlug/Linux/IPlugLinuxOpen.h` | Doc comment said it returns "once a shell has been started"; it returns after that shell **exits** | fixed |
| 4 | Low | `docker/README.md` | The "hvoya-based tree" patch no longer applies on `hvoya-linux` (IPlugEffect builds since 29c6bea6f); mount description named only `/Volumes/Storage/code/IPlug2` | fixed |
| 5 | Low | `Dependencies/IPlug/SDK_VERSIONS.linux-research.txt` | Written by `install-sdks.sh`, untracked, not ignored | fixed (root `.gitignore`) |
| 6 | Info | `Deploy.cmake` | The `<Name>.resources/` copy also runs (as a no-op) for flat Windows CLAP/VST2/APP; the SYMLINK deploy method does not link `<Name>.resources/` | P7 |
| 7 | Info | APP executables | Export ~4.5k dynamic symbols (`inspect-24.04.txt`). Harmless for an executable | P8 |
| 8 | Info | `IPlugLinuxPaths.cpp` | `std::filesystem`/`std::string` in core, against the "WDL over STL" rule. Taken as-is from Stillwell (tested); acceptable in a Linux-only TU. `LocateResource`'s last fallback (bare name resolved against CWD) mirrors mac's `fileExistsAtPath(name)` | accepted |
| 9 | Info | RR-imported APP code | Restating comments (`// Parse command line arguments`), `Sleep(10)` loop | P8 rewrites it |

## Plan compliance [C]

- `git diff 61e272b3a..HEAD` (code only): GTK/GDK appears only in the APP (`IPlugAPP_host.cpp`, `APP.cmake`) — our own process, allowed. No `pthread_create`, `std::thread`, `GfxMutex`, `XInitThreads`, `SetHostDriven`, `IPLUG2_SCREEN_SCALE`, `fork`/`execlp`/`system` added.
- Stubs are marked: `IGraphicsLinux.h` ("STUB: no X11 window yet"), `IPlugLinuxTimer.h` ("STUB: never fires"), WebView stub; both runtime stubs warn once on stderr.
- `posix_spawn` helper: `sh -c 'xdg-open "$1" … &' sh <path>` — the path is a positional argument, never parsed by the shell (no injection). The shell is `waitpid`ed (EINTR loop; `ECHILD` under `SIGCHLD=SIG_IGN` also ends the loop), xdg-open is orphaned to init → no zombie, nothing outlives unload. Blocks the caller for one fork+exit of `sh`. Inherits the host's non-CLOEXEC fds (as JUCE's equivalent does).

## mac/win safety of shared files [C], diffed against `hvoya`

| File | Change | mac/win effect |
|---|---|---|
| `format_compat.hpp` | `… \|\| !__has_include(<format>)` | Only adds a {fmt} case where `<format>` is absent (GCC < 13). Xcode ≥ 14.3 and MSVC 2019 16.10+ ship `<format>` → unchanged |
| `host_time_info.hpp` | `#include <cstdint>` | none |
| `preset_manager.hpp` | `OS_LINUX` branch in `openFolder()`, include under `#if defined(OS_LINUX)` | none |
| `IGraphics.cpp`, `IGraphicsStructs.h` | `std::expf`→`std::exp`, `std::fmodf`→`std::fmod` | arguments are `float`, so the `float` overloads are chosen: identical results |
| `IPlugCLAP.cpp` ef002e167 | non-double params: `value = round(Constrain(value))` before `GetDisplay` | Faithful to RR 3580ba00d. Enum/bool/int display for a host value that is not exactly on a step now finds the display text instead of printing a number. Int display already rounded → same. Strict improvement |
| `IPlugCLAP.cpp` da7cd2493 | `size <= GetLength()` → `false` | Faithful to RR 1e2dab793. Was a 1-byte overflow when `size == strlen`; now returns false in that one case |
| `IPlugCLAP.cpp`, `IPlugVST3_View.h`, `IPlugTimer.h`, `IPlugPlatform.h`, `IPlug_include_in_plug_hdr.h`, `IGraphics_*.h`, `IGraphicsPrivate.h`, `IGraphicsNanoVG.cpp`, `IPlugAPP_*.cpp` | new `#elif defined OS_LINUX` branches; `#else #error` → `#elif` | none (mac/win branches untouched) |
| `VoiceAllocator.h` | `<climits>`, `<memory>` | none |
| `Examples/IPlugControls` | `OnUIClose` null guard | same result whenever the UI exists |
| `Examples/IPlugEffect` | constants to namespace scope, `hvoya::format`, `refreshUI()` (the old name no longer exists in hvoya utils), CMake gets hvoya utils | CMake build of the example fixed on every platform; Xcode/VS projects untouched |
| `VST3.cmake` | adds `base/source/timer.cpp` for all platforms | matches what the example Xcode/VS projects already compile |
| `Deploy.cmake`, `IPlugPlugin.cmake`, `FindiPlug2.cmake` | flat-module resource deploy; `iplug_add_plugin` deploys after resources | finding 1 (fixed: direct callers deploy as on `hvoya`; `iplug_add_plugin` targets now also deploy their web resources on mac, an improvement); finding 6 |
| `CLAP/APP/IGraphics/IPlug/WebView/ReaperExt.cmake`, `CMakePresets.json` | Linux-only branches/presets; `iplug_linux_restrict_exports` defined everywhere, used on Linux only | none |

The mac IPlugEffect CLAP Debug build from this worktree is green (`evidence/p0-p1/macos/`). The APP failure
there is in `IPlugAPP.cpp`, which this range does not touch (nor the xcodeproj): pre-existing. Windows was
not built; it is covered by reading only.

## Binary hygiene [R]

Clean rebuilds (`dock.sh -- scripts/build-cmake.sh .. build/review-p1-<ver> …`): 24.04 IPlugControls CLAP+VST3,
22.04 IPlugControls CLAP + IPlugEffect VST3 — all PASS, no warnings in the new Linux files.
`inspect-binary.sh`, matching `inspect-*.txt`:

- `.clap`: one dynamic export, `clap_entry`. VST3 `.so`: `GetPluginFactory`, `ModuleEntry`, `ModuleExit`.
- NEEDED: `libm`, `libc`, `ld-linux-aarch64` only. No `GLIBCXX_*` refs (static libstdc++).
- GLIBC floor 2.38 (24.04) / **2.34 (22.04)**.
- Mechanism: `CXX_VISIBILITY_PRESET hidden` + inlines hidden + version script `local: *` + `--exclude-libs,ALL` + `-static-libstdc++ -static-libgcc` + `--no-undefined`.
- Resources: `IPlugControls.resources/` beside the `.clap`, `IPlugControls.vst3/Contents/Resources/`.

## Validators [R]

On the fresh 24.04 IPlugControls build: VST3 validator 537/537; clap-validator 33 passed / 3 failed
(`state-reproducibility-{basic,binary,buffered}`) / 1 warning (`state-invalid-random`) — identical to
`evidence/p0-p1/validators/`.

## Leftovers

- `git stash` snapshots from the implementer (leave as is):
  `stash@{0}` before restoring install-sdks README for rebase · `stash@{1}` before restoring xcodebuild-regenerated `main.rc_mac_dlg` ·
  `stash@{2}` before dropping IGraphicsLinux hunk of 166c02d1b · `stash@{3}` before trimming RR dda961827 CLAP/VST3 hunks ·
  `stash@{4}` before dropping RR 14418467f hunks. (`stash@{5..10}` predate the port.)
- `SDK_VERSIONS.linux-research.txt`: now git-ignored (finding 5).

## Open decision: RR 360d84011 (not applied)

The commit has two parts.

1. **`stateSave` loop** — hvoya already has the same loop (`IPlugCLAP.cpp:410-434`). Nothing to take.
2. **`IParam::StringToValue`** — for enum/bool params whose text matches no display text, fall through
   to `atof` + `Constrain` instead of returning 0.

What part 2 fixes: RR's motivation is clap-validator `param-conversions` (value→text→value roundtrip).
With ef002e167 in place, value→text already yields the exact display text, which maps back, and
`param-conversions` **passes** on every p0-p1 build and on the review re-run. So the validator does not
need it.

What it would change, in every format (`StringToValue` is called by VST3 `getParamValueByString`,
AUv2/AUv3 value-from-string, VST2, CLAP `text_to_value`, and IGraphics text entry):

- Host or user types a number into an enum/bool param (e.g. REAPER's generic param field): today
  `"2"` → index **0** (silent reset to the first item); after → index 2. Better.
- Non-numeric unknown text: `atof` gives 0, `Constrain` clamps to `mMin` = 0 → same as today.
- Numeric-looking display texts that don't match exactly: oversampling `{1x,2x,4x,8x}` with input `"4"`
  → today `1x`, after `atof=4` → clamped index 3 = `8x`. Also `"2 voices"` → 2. Both wrong today;
  the new wrong is less predictable.
- `kFlagNegateDisplay` on an enum would negate the index (edge case).
- Gneiss does not call `StringToValue` directly; its exposure is hosts' text entry on enum/bool params.

Recommendation: optional. It is not needed for the port or the validators. If taken, take only the
`IPlugParameter.cpp` hunk, and ideally only for strings that parse fully as an integer (`strtol` with
end check), so that `"4"` for `"4x"` doesn't jump to another item.
