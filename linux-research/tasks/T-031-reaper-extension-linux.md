# T-031 — REAPER extensions and the REAPER-plug-in example on Linux

- **Status:** merged cbb4d7105 (found by the T-016 audit) — independent review approved in round 2
- **Owner:** Claude Opus 5.5 instance, 2026-10-21 (lock released)
- **Branch/worktree:** `linux/T-031` from `hvoya-linux`.
- **Depends on:** nothing. The WebUI extension's web view is T-017.
- **Conflicts:** `Scripts/cmake/ReaperExt.cmake`, `IPlug/ReaperExt/*`, `Examples/IPlugReaperExtension*`, `Examples/IPlugReaperPlugin/CMakeLists.txt`.

## Problem (evidence: `evidence/t016/`)
- `IPlugReaperExtension` and `IPlugReaperExtensionWebUI` do not link on Linux: SWELL functions (`GetMenuItemCount`, `DestroyWindow`, `SWELL_CreateDialog`, `SWELL_curmodule_dialogresource_head`, …) are undefined. A REAPER extension on Linux takes SWELL from REAPER itself through `WDL/swell/swell-modstub-generic.cpp` (`SWELL_dllMain` receives REAPER's function getter); `Scripts/cmake/ReaperExt.cmake`'s Linux branch does not add it.
- `IPlugReaperPlugin` (a CLAP/VST3 that calls the REAPER API) works on Linux — its editor turns red for "stopped" — but shows no text: its CMake ships the font only into mac bundles (`MACOSX_PACKAGE_LOCATION`), so there is no `IPlugReaperPlugin.resources/` on Linux ("resource folder … not found" on stderr).

Once it links, the extension's editor still would not open on Linux (T-016 review M1):
- (a) `IGraphicsLinux::OpenWindow` (`IGraphics/Platforms/IGraphicsLinux.cpp:491`) refuses a delegate that is not an `IPlugAPIBase` (the HostLoop lives there); `ReaperExtBase` is only an `IGEditorDelegate`.
- (b) The SWELL window handle is passed where an X window id is expected (`IPlug/ReaperExt/ReaperExtBase.cpp:434` → `IGraphicsLinux.cpp:525`); it needs `SWELL_GetOSWindow(hwnd, "GdkWindow")` and the GDK window's XID.
- (c) `GetScaleForHWND` returns 1 off Windows: no HiDPI.
- (d) Resizing from the UI is implemented for mac only.
- Exports: the Linux branch should export only `ReaperPluginEntry` (and `SWELL_dllMain`).

## Steps
1. `ReaperExt.cmake` Linux: compile `swell-modstub-generic.cpp` with `SWELL_PROVIDED_BY_APP`, export `SWELL_dllMain`, link nothing else from SWELL; check how the mac/win branches and WDL's own REAPER extension samples do it.
2. `Examples/IPlugReaperPlugin/CMakeLists.txt`: the Linux resource folder, as `iplug_add_plugin(... RESOURCES ...)` gives the other examples.
3. (a)–(d) above: a HostLoop for the extension (REAPER's main thread timers via its API, or the frame's), the XID from SWELL's GDK window, the scale, UI resize.
4. Test in REAPER (Docker): both extension examples load (`reaper_*.so` in `~/.config/REAPER/UserPlugins`), their action/menu entry and dialog work, nothing on stderr; IPlugReaperPlugin shows "STOPPED"/"PLAYING", the track name and channel count. The WebUI extension's web view waits for T-017 (state that).
5. Independent review.

Estimate: 2–3 days.

## Log

### 2026-10-21 — implementation (Claude Opus 5.5 instance; worktree `/Volumes/Storage/code/IPlug2-T-031`)

Commits on `linux/T-031`:
- `8d8ffa465` — link + example resources. `ReaperExt.cmake`'s Linux branch compiles `swell-modstub-generic.cpp`
  with `SWELL_PROVIDED_BY_APP` (as the macOS branch does with `swell-modstub.mm`) and restricts exports with the new
  `Scripts/cmake/Linux/reaperext-exports.map` (`ReaperPluginEntry`, `SWELL_dllMain`; checked with `nm -D`: exactly
  those two). `Examples/IPlugReaperPlugin/CMakeLists.txt`: on Linux only, the font goes through `_iplug_add_resources`
  into `<Name>.resources/` and the deploy runs after that copy (`_IPLUG_DEFER_DEPLOY` + `iplug_deploy_target` in the
  helper, which now takes the format); the mac/win branch of the helper is unchanged.
- `c0b99f50b` — shared Linux code. `IEditorDelegate::GetEditorHostLoop()` (inside `#ifdef OS_LINUX`, default
  `nullptr`); `IPlugAPIBase` returns its own loop, so plug-ins behave as before. `IGraphicsLinux::OpenWindow` takes
  the loop from the delegate and uses `IPlugAPIBase` only where there is one (window title, the APP's focus-on-click).
  **Plus a Mesa fix that affects every Linux editor**: `OpenWindow` clears and swaps once after the GL setup. Mesa's
  software rasterizer (drisw/llvmpipe, Mesa 25.2.8 in the image) keeps the drawable size from the first
  make-current until a swap, so a resize before the first frame drew that frame at the old size, cut off (content
  shifted up, a black band below), and nothing redrew it. Found when the extension's window reopened at another
  size after undocking (it opens at the docked size, 1024×350, then gets 520×420 before its first frame). XSync after
  the resize did not help; the swap did. A host that sizes a plug-in editor right after the open would hit the same.
- `86f8e2224` — the extension editor, all behind `OS_LINUX` in `IPlug/ReaperExt/*`, plus the new
  `IPlug/Linux/IPlugLinuxHostLoopSWELL.h`:
  - (a) `ReaperExtBase` owns a `HostLoop` whose backend is SWELL timers (`SetTimer(nullptr, …, TIMERPROC)`), which
    SWELL runs from REAPER's message loop on the main thread. Its idle timer is a `Timer_impl` on that loop:
    before, `Timer::Create()` ran `OnIdle` (which calls the REAPER API and touches the UI) on the fallback thread.
    Chosen over REAPER's `plugin_register("timer")` because that is ~30 Hz and passes no context; SWELL timers take
    any interval. SWELL cannot watch fds: the backend refuses them, and `IGraphicsLinux` drains X events from its
    display timer anyway.
  - (b) The editor's X parent comes from `SWELL_CreateXBridgeWindow(dialog, &xid, &clientRect)` rather than the
    dialog's GDK window XID (step 3's suggestion): it is SWELL's own mechanism for embedding a foreign X window,
    the one REAPER uses for plug-in editors, and it keeps the X window placed when the dialog moves, docks into
    another top-level and resizes. `WM_SIZE` resizes the bridge; `WM_DESTROY` closes the editor first (REAPER can
    destroy a docked dialog without `DestroyMainWindow`, and `~ReaperExtBase` destroys it at exit).
  - (c) `GetScaleForHWND` returns `SWELL_GetScaling256() / 256` (REAPER's UI scale; SWELL coordinates are
    physical pixels); `WM_INITDIALOG` passes it to `SetScreenScale` before `OpenWindow`.
  - (d) `EditorResizeFromUI` resizes the floating dialog so its client area is the editor's size (not when docked,
    as on mac).
  - `scripts/reaper-up.sh` installs `reaper_*.so` into the test profile's `UserPlugins/`.

Tests (Docker `iplug2-linux:24.04`, REAPER 7.82 aarch64, Xvfb + openbox; evidence `evidence/t031/`):
- `scripts/reaper-ext-test.sh <out> <reaper_X.so>` (new, runs in the GUI container) — **IPlugReaperExtension:
  PASS** (`ext-igraphics.log`, `ext-igraphics/`): 4 actions registered; Show/Hide opens the window and the editor
  fills the 300×300 client and paints; the Add Track button adds a track (0 → 1) and the NumTracks text changes
  (OnIdle → ITextControl); floating resize to 520×420, dock (1024×60), a taller docker (1024×350), undock (520×420,
  no black band — this check caught the Mesa bug above), close/reopen ×5; the MsgBox action shows and closes its
  box; File: Quit with the window open: exit status 0; stderr clean (only REAPER's own `GdkWindow … unexpectedly
  destroyed` from closing the About nag; `reaper-up.sh` with no extension prints the same line). Thread count (gdb, 8 s, slider dragged): `OnTimer`
  230, `OnDisplayTimer`/`ProcessXEvents` 333, `IGraphics::Draw` 22 — **all tid=1** (REAPER's main thread).
  `e09-extensions-menu.png`: Extensions → IPlugReaperExtension submenu with the Show/Hide tick.
- **IPlugReaperExtensionWebUI: PASS for what exists** (`ext-webui.log`): actions, window open/resize/dock/undock/
  reopen, the X bridge follows the client size, quit exit 0. The web view itself is T-017 (Linux WebView is still a
  stub): the window shows SWELL's empty bridge (black where it was first mapped, white elsewhere).
- HiDPI (`ext-scale2/`, `REAPER_UI_SCALE=2`, Xvfb 2600×1900): window 600×600, editor drawn at 2×, the Add Track
  button hit at its scaled position adds a track.
- Resize from the UI (`ext-ui-resize/`): with a temporary `AttachCornerResizer` in the example (built in a throwaway
  build dir, source restored, not committed), dragging the corner resized the editor to 444×395 and the floating
  dialog followed.
- **IPlugReaperPlugin** CLAP + VST3 (`reaperplugin-{clap,vst3}/`): `p01-stopped.png` red "STOPPED", `p02-playing.png`
  green "PLAYING", both with the track name ("Bass Track") and "Channel count 6"; no "resource folder … not found" on
  stderr; reopen ×5 and a second instance alive. `run-gui-test.sh`'s `render[...] blank` FAILs are false positives:
  this editor is one solid fill with a little text, below its stddev threshold of 0.02.
- Regression for the shared `IGraphicsLinux` change: IPlugControls full step script (`run-gui-test.sh`) CLAP **PASS**,
  VST3 **PASS**, APP 0 fails (`regress-iplugcontrols-{clap,vst3,app}/`).
- macOS (shared files touched): xcodebuild IPlugEffect `macOS-CLAP` Debug and IPlugReaperExtension Debug (signing
  off), DerivedData and install paths in the scratchpad: both **BUILD SUCCEEDED**; no `.rc_mac` regenerated.

Not verified: a real desktop (GNOME VM), XWayland, x86_64, GPU (DRI3) drivers — the Mesa fix is harmless there (one
extra clear+swap at open) but its necessity was only shown on llvmpipe. Release-mode deploy of the WebUI extension's
`web/` folder on Linux belongs with T-017.

Cross-platform findings (report only, not fixed): none that matter. (Low: `ReaperExtBase::EditorResizeFromUI`
compares against `GetEditorWidth/Height()`, which nothing updates on mac/win, so the comparison always passes; harmless.)

Next: independent review.

### Review round 1 (independent reviewer, 2026-10-21)

Scope: `git diff hvoya-linux..linux/T-031` (5 commits), AGENTS §2, the evidence PNGs (opened, not only the logs), and
own re-runs in Docker (container `t031r`, build dir `build/t031r`).

Re-run by the reviewer:
- Build `IPlugReaperExtension IPlugReaperExtensionWebUI IPlugReaperPlugin-clap IPlugControls-clap` (Release): all 4
  PASS, no new warnings from T-031 code.
- `reaper-ext-test.sh` IPlugReaperExtension: **PASS** (same numbers as the Log: 300×300 → 520×420 → docked 1024×60 →
  1024×350 → undocked 520×420, no black band, tracks 0 → 1; gdb: OnTimer 236, OnDisplayTimer 330, ProcessXEvents 329,
  Draw 22, all tid=1; quit exit 0; stderr only REAPER's own `GdkWindow … unexpectedly destroyed`). Screenshots
  `build/t031r/ext/e0*.png` looked at: editor fills the client, paints, NumTracks updates.
- `reaper-ext-test.sh` IPlugReaperExtensionWebUI: **PASS** (window, bridge size follows open/resize/dock/undock/reopen,
  quit exit 0).
- `nm -D --defined-only reaper_*.so`: exactly `ReaperPluginEntry`, `SWELL_dllMain` in both. `readelf -d`: NEEDED only
  libX11, libm, libc, ld-linux (no GTK/GDK).
- IPlugControls CLAP full step script (`run-gui-test.sh`, shared IGraphicsLinux change): **PASS** (steps, reopen ×5,
  second instance; Draw/OnDisplayTimer/ProcessXEvents all tid=1); `build/t031r/regress/c01-knob-drag.png` looked at.
- macOS: CMake configure of `Examples/IPlugReaperPlugin` (Ninja, scratchpad build dir, nothing built or deployed):
  OK. All C++ changes are inside `OS_LINUX`, the CMake ones inside `UNIX AND NOT APPLE` (the mac/win branch of
  `iplug_add_plugin_resources` is unchanged apart from the unused `format` argument); I did not repeat the xcodebuild.

Evidence checked (PNG opened): `ext-igraphics/e01–e09` (editor painted, Add Track → NumTracks 1, resized, docked short
and tall, undocked without a band, MsgBox, Extensions submenu with the Show/Hide tick), `ext-scale2/s01, s02` (600×600
window, editor drawn at 2×), `ext-ui-resize/u02` (444×395 editor, dialog followed), `ext-webui/e06` (empty bridge, as
stated), `reaperplugin-clap/p01` red STOPPED and `reaperplugin-vst3/p02` green PLAYING, both with "Bass Track" and
"Channel count 6". The evidence shows what the Log claims.

Design checks that passed: the SWELL-timer backend honours the HostLoop contract (main thread only, ids unique
among live SWELL timers, a timer killed inside its own callback is not recycled while SWELL still holds it, Dispatch
ignores foreign ids); `mHostLoop` is declared before `mTimer`; SWELL's `DestroyWindow` sends `WM_DESTROY` to the
dialog before it destroys the bridge child (whose `~bridgeState` `XDestroyWindow`s the editor's parent), so closing
the editor in `WM_DESTROY` is in the right place; the second `CloseWindow` on the IGraphics paths is made harmless by
the `GetUI()` guard (which also avoids a second `OnUIClose`); the remaining `dynamic_cast<IPlugAPIBase*>` uses in
`IGraphicsLinux` are null-safe (`IsHostResizeEnabled()` short-circuits line 1171); `SWELL_GetScaling256` is the right
source for SWELL's physical-pixel layout; `EditorResizeFromUI` gets physical sizes (`GetPlatformWindowScale`), so the
Linux frame arithmetic is in the right units. The Mesa clear+swap is sound: one black frame on a window whose
background pixel is already black, after `DisableSwapWait`, so it neither blocks nor flashes; the comment matches
drisw's behaviour (the drawable is revalidated on swap). The X-bridge choice over the raw GDK XID is justified.

Findings:
- **R1-1 (major)** `IPlug/Linux/IPlugLinuxHostLoopSWELL.h:70-74`, used at `:37` — heap use-after-free at process exit.
  `Backends()`'s function-local static is constructed at the first `ReaperExtBase` (inside `ReaperPluginEntry`), after
  the global `gPlug` registered its destructor at load, so `exit()` destroys the list first; `~IPlugReaperExtension →
  ~HostLoop → SetBackend(nullptr) → ~SWELLHostLoopBackend` then calls `DeletePtr(this)` on the freed buffer
  (`~WDL_HeapBuf` frees `m_buf` but leaves the pointer and size). Proven with gdb on REAPER's File: Quit
  (`build/t031r/gdbexit/gdb.txt`): `free(sBackends buffer)` from `exit()` hits first, then `~SWELLHostLoopBackend`
  from `~IPlugReaperExtension` (also from `exit()`), with the list's first slot already overwritten by the allocator.
  It only does no damage today because `Find()` misses in the freed chunk. Fix: a list that is never destroyed, e.g.
  `static auto* sBackends = new WDL_PtrList<SWELLHostLoopBackend>; return *sBackends;`, with a one-line WHY (it must
  outlive the extension's static instance).
- **R1-2 (minor)** `Scripts/cmake/Deploy.cmake:86-103` (Linux branch of `iplug_get_default_deploy_path`) — no
  `REAPEREXT` case. With the default `IPLUG_DEPLOY_PLUGINS=ON`, configuring `Examples/IPlugReaperExtension` on Linux
  prints `CMake Warning … [iPlug2] Unknown plugin format: REAPEREXT` and deploys nothing (reproduced:
  `build/t031r/cfg-deploy`); only the test script installs the `.so`. Add `$ENV{HOME}/.config/REAPER/UserPlugins`
  (Linux branch only; mac/win untouched).
- **R1-3 (minor, hand-off)** `IPlug/ReaperExt/ReaperExtBase.cpp:452-462, 492-498` — for the WebView delegate
  (`NO_IGRAPHICS`), `WM_DESTROY` calls `CloseWindow()` unguarded, i.e. a second `CloseWebView()` after
  `DestroyMainWindow`/`ToggleDocking`/`WM_CLOSE` already closed it, and `OpenWindow` hands it the bridge's X window id,
  not an HWND. Both are harmless with today's Linux WebView stub. Write them into T-017's task file as constraints
  (Linux `CloseWebView` must be idempotent; its parent is an XID inside a SWELL X bridge), or guard the second close
  here.
- **R1-4 (nit)** `Examples/IPlugReaperPlugin/CMakeLists.txt:33, 42` — "Font resources for bundles" / "Helper function
  to add resources to plugin bundles" are now inaccurate (Linux has no bundle; the helper also deploys there, which is
  what the new `format` argument is for). Reword the helper's comment to say so.
- **R1-5 (nit)** `IPlug/ReaperExt/ReaperExtBase.h:114-116` — the `GetEditorHostLoop()` override sits between the
  documented public setters `SetDockId` and `SetMenuName`; move it next to the other delegate overrides
  (`EditorResizeFromUI`, line 52).

No cross-platform findings beyond the implementer's low one. No product names; comment discipline otherwise good.

Verdict: approve with fixes (R1-1 must be fixed; R1-2 should be; R1-3 can be the T-017 note; R1-4/R1-5 optional).

### Round 1 fixes (implementer, 2026-10-21)
- R1-1 fixed: `Backends()` is a never-destroyed heap list, with the WHY. Re-checked with the reviewer's gdb probe,
  adapted (`evidence/t031/review-fixes/r1-1-exit-probe.{sh,txt}`): on File: Quit no `free` of the list or its buffer,
  `~SWELLHostLoopBackend` finds itself as the list's first entry, exit status 0.
- R1-2 fixed: `Deploy.cmake` Linux branch: `REAPEREXT` → `~/.config/REAPER/UserPlugins`. Deploying then failed on
  `reaper_X.dll` (`iplug_deploy_target` named non-Apple extensions `.dll`): now `.so` on UNIX, mac/win unchanged.
  Checked with a deploy-on configure + build in Docker (`review-fixes/r1-2-deploy-check.txt`): both extensions into
  `UserPlugins/`, the WebUI's `web/` folder into `UserPlugins/IPlugReaperExtensionWebUI/`, and IPlugReaperPlugin's
  `.clap` with its `.resources/` (the deploy runs after the resource copy).
- R1-3: handed to T-017 as a note in its task file (idempotent `CloseWebView`, XID parent, scale, web deploy path).
- R1-4, R1-5 fixed (comments in the example's CMake; the override moved next to `EditorResizeFromUI`).
- Re-run after the fixes: `reaper-ext-test.sh` IPlugReaperExtension **PASS** (OnTimer 231, OnDisplayTimer 336, Draw
  22, all tid=1; exit 0), IPlugReaperExtensionWebUI **PASS**.
- Evidence: per `linux-research/.gitignore` only contact sheets are committed: `ext-igraphics/sheet.png` (e01–e09),
  `ext-webui/sheet.png`, `ext-scale2/sheet.png`, `ext-ui-resize/sheet.png`, `sheet-reaperplugin.png` (CLAP + VST3
  STOPPED/PLAYING), `regress-iplugcontrols-clap/sheet.png`; the single PNGs stay in the worktree.

### Review round 2 (independent reviewer, 2026-10-21)

Checked `940cfc6a1` (code) and `99a85cc4d` (log, sheets, T-017 note); rebuilt `build/t031r` (3 targets PASS).
- **R1-1 resolved.** `Backends()` now returns a never-destroyed heap list; the WHY comment is accurate. Re-ran the
  implementer's probe against my build (`build/t031r/gdbexit2/`): on File: Quit no `free` of the list or its buffer,
  `~SWELLHostLoopBackend this=0x3943f5e0` finds itself as the list's first entry, exit status 0.
- **R1-2 resolved.** `Deploy.cmake`: the `REAPEREXT` case is inside the Linux branch only, and the file-name change is a
  new `elseif(UNIX)` between the unchanged `if(APPLE)` (`.dylib`) and `else()` (`.dll`), so macOS and Windows behave as
  before (confirmed from the diff). The `.dll` → `.so` fix is right: the source path is `out/<name>`, which never
  existed as `.dll` on Linux. My own deploy-on configure + build of `IPlugReaperExtensionWebUI` with a throwaway
  `HOME` inside `build/t031r/fakehome` (default method): no warning, `reaper_IPlugReaperExtensionWebUI.so` and
  `IPlugReaperExtensionWebUI/{index.html,script.js}` land in `~/.config/REAPER/UserPlugins/`.
- **R1-3 resolved** as a hand-off: the note at the end of `tasks/T-017-webview.md` states the XID parent, the
  second `CloseWindow` (idempotent `CloseWebView` required), the scale division and the web deploy path, all accurate.
- **R1-4, R1-5 resolved** (helper comment reworded; the override sits with `EditorResizeFromUI`).
- Re-ran `reaper-ext-test.sh`: IPlugReaperExtension **PASS** (OnTimer 232, OnDisplayTimer 334, Draw 22, all tid=1;
  exit 0), IPlugReaperExtensionWebUI **PASS**.
- The "Round 1 fixes" section matches what I found. Contact sheets opened: `ext-igraphics/sheet.png` (e01–e09, e09
  with the Extensions submenu and Show/Hide tick) and `sheet-reaperplugin.png` (CLAP and VST3 STOPPED/PLAYING with
  track name and channel count) show what the Log says.

New: **R2-1 (nit, optional)** `Examples/IPlugReaperPlugin/CMakeLists.txt:33` — `# Font resources` now only restates
`set(FONT_FILES …)`; delete it.

Verdict: approve.

### Final verdict and hand-over (implementer, 2026-10-21)
- **Independent review: approved in round 2** (round 1 "approve with fixes": R1-1 major, R1-2 minor fixed; R1-3 handed
  to T-017; R1-4/R1-5/R2-1 nits fixed — R2-1, the redundant "# Font resources" comment, removed after round 2).
- Commits on `linux/T-031` (merge `--no-ff` into `hvoya-linux`): code `8d8ffa465`, `c0b99f50b`, `86f8e2224`,
  `940cfc6a1`, R2-1 nit `14588023b`; tests/evidence/log `2072a2406`, `fd2570404`, `99a85cc4d`; reviews `14ce018ea`,
  `db7ca3f52`.
- Shared files touched (all changes behind `OS_LINUX` / `UNIX AND NOT APPLE`): `IPlug/IPlugEditorDelegate.h`,
  `IPlug/IPlugAPIBase.h`, `IPlug/ReaperExt/*`, `Scripts/cmake/{ReaperExt,Deploy}.cmake`,
  `Examples/IPlugReaperPlugin/CMakeLists.txt`, `Examples/IPlugReaperExtension*/CMakeLists.txt` (comment only).
  Linux-shared: `IGraphics/Platforms/IGraphicsLinux.cpp` (delegate host loop; **clear+swap at open affects every Linux
  editor**). New: `IPlug/Linux/IPlugLinuxHostLoopSWELL.h`, `Scripts/cmake/Linux/reaperext-exports.map`,
  `linux-research/scripts/reaper-ext-test.sh`; `reaper-up.sh` installs extensions.
- Smoke for the coordinator after merging: build `IPlugReaperExtension IPlugReaperExtensionWebUI IPlugControls-clap`,
  `scripts/gx.sh <c> scripts/reaper-ext-test.sh <out> ../build/<dir>/out/reaper_IPlugReaperExtension.so` (PASS
  expected), IPlugControls `run-gui-test.sh` CLAP.
- Open: the web view of the WebUI extension (T-017, note in its task file); not tried on the GNOME VM, XWayland,
  x86_64 or DRI3 GPU drivers; for PLAN/notes/22: the REAPER extension row can move to ✅ (WebUI: window yes, web view T-017).
- Docker containers removed; T-031 lock released; worktree left in place. `Dependencies/IPlug/VST3_SDK/README.md` is
  modified by `install-sdks.sh` and deliberately not committed.
