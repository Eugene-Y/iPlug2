# P5: HiDPI / scaling on Linux (T-007, 2026-10-14)

Branch `linux/T-007` (`e0ff5b42b..`). Evidence: `evidence/t007/`. **[D]** = Docker (REAPER 7.82 aarch64, Xvfb,
openbox, llvmpipe; containers `t007`, `t007b`). **[V]** = the VM (GNOME 46 / Mutter on Xorg, virgl). **[M]** = macOS.

## Model

X11 has no per-window scale: CLAP and VST3 sizes are physical pixels, as on Windows. iPlug's model already fits:
`GetPlatformWindowScale() == GetScreenScale()` in `IGraphicsLinux`, window = logical × draw scale × screen scale,
mouse ÷ `GetTotalScale()`, NanoVG's `nvgBeginFrame(..., GetScreenScale())` rasterises text at device resolution, and
bitmaps load at `GetRoundedScreenScale()` (`@2x` from 1.25 on). What was missing is where the scale comes from, and
telling the host the physical size **before** the editor opens.

| Topic | Choice |
|---|---|
| Scale sources, in order | 1. the host's: CLAP `gui.set_scale`, VST3 `setContentScaleFactor` (now also on Linux). Once a host gave one, the desktop's no longer counts: no fight. 2. `Xft.dpi` / 96, looked up as Xft does (`XGetDefault(dpy, "Xft", "dpi")`: the `RESOURCE_MANAGER` property, else `~/.Xdefaults`). 3. `GDK_SCALE`. 4. `QT_SCALE_FACTOR`. 5. 1 |
| Desktop value | parsed without the C library (a host may run with a decimal comma), in eighths (quarters and whole numbers stay exact; 97 dpi is 1), clamped to [1, 4] (`IGraphicsLinuxScale.h`, unit test `Tests/LinuxScale`) |
| Where the scale lives | `IGEditorDelegate` (Linux only): the host's scale, and `mEditorScreenScale`, the scale `GetEditorWidth/Height` are physical pixels at. The IGraphics object only exists while the editor is open |
| Size before the open | `UpdateEditorSizeForScreenScale()`, a Linux-only virtual on `IEditorDelegate`, called where the host is about to ask for the size: CLAP `gui.create` (the spec orders create → set_scale → get_size → set_parent), the VST3 view's constructor (createView precedes getSize), the APP before it sizes its window. It takes the host's scale or queries the desktop (one short X connection of its own), and rescales the closed editor's size with IGraphics' own arithmetic: from the last size (`mLastWidth × mLastScale`) when there is one, so a restored size is reported too, else from the configured size, kept from before the first rescale (review round 1: recovering it by dividing a truncated physical size drifted by a pixel, e.g. 449×305 after Xft.dpi 132 then a host scale 1). `set_scale` on a closed editor rescales it the same way |
| Open | `IGraphicsLinux`'s constructor applies the delegate's scale (`SetScreenScale` under `mInOpenWindow`, so the host is not notified: it already has that size), before `IGEditorDelegate::OpenWindow` applies a restored size. So that `Resize` computes physical sizes at the right scale and asks the host for nothing |
| Size changed by the open | `OpenWindow` cannot tell the host (hvoya's `mInOpenWindow` rule: a host that reacts synchronously could close the editor under it). The end of the open compares the window with `GetEditorWidth/Height`; if they differ, the first frame sends `EditorResizeFromUI` with the opened size, unless the window was resized since (then the host has its size) (CLAP `request_resize`, VST3 `resizeView`). Case: a plug-in's own draw scale in its factory (IPlugEffect's saved UI scale, Gneiss' `kPrefUIScale`) |
| Scale while open | host `set_scale`/`setContentScaleFactor` → `IGraphics::SetScreenScale` → `EditorResizeFromUI` (physical) + `PlatformResize` + bitmaps reloaded |
| VST3 host context menu | `IContextMenu::popup` coordinates in physical pixels on Linux, as on Windows (`IGraphics.cpp`, `OS_WIN \|\| OS_LINUX`) |
| Cursors | unchanged: libXcursor sizes theme cursors from `Xcursor.size`, else `Xft.dpi` (32 px at 144 dpi, 48 px at 192 dpi in Docker) |

Shared files touched, all inside `#ifdef OS_LINUX` (or `OS_WIN || OS_LINUX`): `IPlugEditorDelegate.h`,
`IGraphicsEditorDelegate.{h,cpp}`, `IPlugCLAP.cpp` (`guiCreate`), `IPlugVST3_View.h` (constructor,
`setContentScaleFactor`), `IPlugAPP_dialog.cpp`, `IGraphics.cpp` (context menu).

## Findings

1. **REAPER passes its own UI scale to every plug-in, CLAP and VST3**, after create and before get_size: SWELL's
   `ui_scale` (reaper.ini `[.swell]`), which by default (`ui_scale_auto=1`) is GDK's integer monitor scale factor
   (GNOME's scaling-factor, `GDK_SCALE`), else 1. So in REAPER the editor always follows REAPER's own UI, and
   `Xft.dpi` never counts there [D, V] (`probe-*`, `vm-gnome-*/probe-*`).
2. GNOME "Large text" (text-scaling-factor 1.5) writes `Xft.dpi: 144` with UI scale 1: REAPER passes 1, the editor
   stays at 1 like REAPER's UI; the standalone APP (no host scale) goes to 1.5 [V]. GTK apps scale only text there;
   telling the two apart needs the XSettings (`Gdk/WindowScalingFactor`, `Gdk/UnscaledDPI`), not read (issue 2).
3. GNOME 200 % (scaling-factor 2): `Xft.dpi: 192`, `Xcursor.size: 48`, REAPER passes 2 [V].
4. GNOME fractional 150 % on X11 (Mutter's experimental `x11-randr-fractional-scaling`, Ubuntu's Settings option): X
   clients run at the next integer scale and the X server scales the framebuffer down (screen 2560×1151, randr
   transform 1.333 on a 1920×863 mode): `Xft.dpi: 192`, REAPER passes 2, editor 900×610 in a 900×668 FX window [V]
   (`vm-gnome-frac150`). Right for this model. The virgl desktop hung after that modeset (issue 6), so no screenshot.
5. REAPER answers `request_resize`/`resizeView` by resizing its container and does **not** call `set_size`/`onSize`
   back, at any scale [D].

## Acceptance

| # | Check | Result |
|---|---|---|
| 1 | Host scale when offered, else `Xft.dpi` | REAPER CLAP + VST3 at ui_scale 1.5 / 2: `set_scale`/`setContentScaleFactor` 1.5 / 2, container = editor = 675×457 / 900×610 at once, no resize request [D] (`probe-{clap,vst3}-reaper{1.5,2}`); host 1 wins over `Xft.dpi` 144 [D, V]. Mini CLAP host without `set_scale` (`probe/mini-clap-host-scale.cpp`): get_size = parent = editor at no Xft.dpi / 120 / 144 / 192 (450×305, 562×381, 675×457, 900×610), `GDK_SCALE=2`, `QT_SCALE_FACTOR=1.5`; host `set_scale 2` over Xft.dpi 144; set_scale while open 1.5 → 2 → one `request_resize 900x610`, and back; reopen keeps the size [D] (`mini-host/scale.txt`) |
| 2 | Crisp text at 1.5 and 2 (by eye) | 1:1 crops: `probe-clap-reaper1.5/zoom-text.png`, `open-crop.png` (1.5, 2), `mini-host/xft-*.png`, `app-controls-xft144/view-*.png`, `vm-gnome-200/reaper-effect-clap/editor-1to1*.png` (virgl): glyph edges one pixel wide, no upscaling blur; `@2x` bitmaps above 1 |
| 3 | Mouse hit-testing at 1.5 / 2 | IPlugEffect knob drag (`iplugeffect-p3a` steps with `scale S`): PASS CLAP + VST3 at 1, 1.5, 2 [D] and at 2 on GNOME [V]. IPlugControls, the **full P3b step file** at 1.5 and 2, CLAP + VST3: PASS ×4 [D]; the number boxes (42/55/66/7/33/44) and captions (three/two/four) read the same values as at 1×; the IVKnob drag's value varies between runs (12.30, 30.95, 0.00, 22.77 %; the step accepts 0–99), as motion coalescing on the larger framebuffer cuts the drag differently, the locked-cursor drag returns the pointer to 644,474 (= 322,237 × 2), menus, text entry, clipboard. APP IPlugControls, P3b APP steps at Xft.dpi 144 and 192: every check passes [D]; IPlugEffect APP at 2 (GNOME) and 1.5 (large text) [V]. REAPER's VST3 context menu opens at the click point at 1 and 2 (`ctx-menu-vst3-x*`) |
| 4 | Resize at scale ≠ 1, no loop | Corner resize (IPlugEffect, Scale mode) at 1.5 (CLAP, VST3) and 2 (CLAP): one `EditorResizeFromUI` per drag step, none after the drag, no host `set_size`/`onSize`, REAPER's container = the editor (847×573 at 1.5, 1130×764 at 2) (`resize-effect-*`). External resize of the editor at 1.5 (host resize off, F3): back to 675×457 (`external-resize-effect-clap-x1.5`). An editor that opens larger than get_size (IPlugEffect's saved draw scale, copied to a 2nd track at 1.5): one `EditorResizeFromUI 847x573` on the first frame, REAPER's container follows (`restore-size-effect-clap-x1.5`) [D] |
| 5 | Main thread, validators, binary | `reaper-plugin-test.sh`'s gdb census: every `Draw`/`ProcessXEvents` on tid 1 in all 10 Docker and 4 VM REAPER runs. clap-validator Effect 35/3, Controls 33/3; VST3 Effect 536/537, Controls 537/537: as before (`validators.log`). NEEDED unchanged (`libX11.so.6`, libm, libc, ld-linux), GLIBC 2.38 on 24.04 (`inspect.txt`) |
| 6 | macOS | IPlugEffect CLAP + VST3 Debug BUILD SUCCEEDED, nothing installed into `~/Library` [M] (`macos/mac-builds.txt`) |
| 7 | Unit test | `Tests/LinuxScale` (ASan + UBSan): PASS; its decimal-comma case skips in the 24.04 image (no locale data); the parser does not use the locale |

## Harness

- `xact.sh`: `scale S` (or `XACT_SCALE=S`): view coordinates and rect sizes of the following steps are logical, × S.
  The 1× step files run unchanged at any scale.
- `reaper-up.sh`: `REAPER_UI_SCALE=1.5` writes SWELL's `ui_scale`, which REAPER passes on to plug-ins.
- `gui-up.sh`: `XVFB_SCREEN=2600x1900x24` (IPlugControls at 2 is 2048×1536).
- `xft-dpi.sh <dpi|->`: sets/removes `Xft.dpi` with `xprop` (no `xrdb` in the image).
- `scale-probe.sh`: the host's scale calls and the resulting sizes (gdb; only the IGraphics/IGraphicsEditorDelegate
  symbols resolve as pending breakpoints, the IPlugCLAP/VST3 view ones did not, so it watches the delegate).
- `resize-scale-probe.sh`, `restore-size-probe.sh`, `probe/ctx-menu-scale.sh`, `probe/mini-clap-host-scale.cpp` +
  `probe/mini-scale.sh`, `vm/probe/scale-desktop.sh` (GNOME scaling-factor / text-scaling-factor),
  `vm/probe/scale-fractional.sh` + `mutter-scale.py` (fractional through Mutter's DisplayConfig; read its warning).

Runs, e.g.: `scripts/gx.sh t007 env REAPER_UI_SCALE=2 XACT_SCALE=2 scripts/reaper-plugin-test.sh <out> "CLAP: IPlugControls" <.clap> scripts/steps/iplugcontrols-p3b.txt`;
VM: `scripts/vm/vm-run.sh vm/probe/scale-desktop.sh <out> - 2 1.0 2 2`.

## Known issues

| # | Sev. | Issue |
|---|---|---|
| 1 | Info | The host's scale wins even when it is 1 and `Xft.dpi` says more (REAPER with GNOME Large Text): by design, the editor matches the host's UI. Users scale REAPER with its `ui_scale` |
| 2 | Low | Without a host scale (the APP, hosts that never call set_scale), `Xft.dpi` also carries GNOME's text scaling, so Large Text scales the whole editor. Reading the XSettings (`Gdk/WindowScalingFactor` × `Gdk/UnscaledDPI` vs `Xft/DPI`) would separate UI scale from text size |
| 3 | Low | The desktop scale is read when the host creates the GUI (and when the APP starts). A desktop scale change while the editor is open is followed only if the host passes it (set_scale while open works, mini host). X11 has no per-monitor scale |
| 4 | Low | Host-initiated resize at a fractional scale truncates the logical size (`OnParentWindowResize`, shared, as on Windows): the editor can end 1 px short of the host's container (e.g. 562 → 449 → 561 at 1.25). Not exercised: neither the examples nor Gneiss enable host resize |
| 5 | Info | Scales are rounded to eighths and clamped to [1, 4]; a DPI below 96 gives 1 |
| 6 | Info (VM) | Fractional scaling on the virgl VM: gnome-shell hung in Mesa's DRI3 buffer-age query right after the Mutter modeset (D-Bus to the shell timed out, root window black); an `xrandr --output Virtual-1 --auto` woke it. The VM was left at scale 1 but at **1920×863** (UTM's preferred size after the modesets; it was 1440×863). Check fractional scaling once on real hardware in P10 |
| 7 | Info | IPlugEffect (hvoya example) saves its UI scale from the open editor only; a closed editor saves its pending scale (1 after a normal close), so copying a closed instance loses its size. Example code, not P5 |
| 8 | Info | Host-resize size limits mix units at scale ≠ 1: CLAP `guiAdjustSize` and VST3 `checkSizeConstraint` compare the host's physical sizes with the plug-in's unscaled min/max. Shared code, the same on Windows, older than T-007 |

## Review round 1 (notes/18)

| # | Fix |
|---|---|
| 1 (Medium) | `RescaleClosedEditorSize` keeps the configured size instead of dividing a truncated physical size. Re-run (`mini-host/review-r1-drift.txt`): Xft.dpi 132 then host scale 1 / 1.25 / 1.5 / 2: get_size = editor = 450×305 / 562×381 / 675×457 / 900×610, no `request_resize`; desktop only 618×419, reopen the same |
| 2 (Medium) | The 1:1 crops are whitelisted in `.gitignore` and committed |
| 3 (Low) | The opened size is decided at the end of `OpenWindow`; the first frame sends it only if the window still has it. Re-run: `restore-size-probe` one `EditorResizeFromUI 847x573`, container follows; REAPER IPlugEffect CLAP at 1.5 PASS, Draw/X on tid 1 |
| 4 (Low) | Acceptance row 3 wording (knob values) corrected |
| 5 (Low) | `plug_build_info.hpp` from the Xcode build deleted (mac proof re-run, `macos/mac-builds.txt`) |
| 6 (Low) | `xft-dpi.sh` refuses a display whose X resources hold anything but Xft.dpi; `restore-size-probe.sh` says it is IPlugEffect-only |
| 7 (Low) | Known issue 8 |
| 8 (Nits) | `@param` split; test CMake comment; whitespace-only hunk in `IGraphicsEditorDelegate.h` removed |

