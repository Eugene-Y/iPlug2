# T-031 — REAPER extensions and the REAPER-plug-in example on Linux

- **Status:** in progress (found by the T-016 audit)
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
