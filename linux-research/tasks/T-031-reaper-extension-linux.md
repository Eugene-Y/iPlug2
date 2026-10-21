# T-031 — REAPER extensions and the REAPER-plug-in example on Linux

- **Status:** in review (found by the T-016 audit)
- **Owner:** Claude Opus 5.5 instance, 2026-10-21
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
