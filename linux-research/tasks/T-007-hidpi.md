# T-007 — P5: HiDPI / scaling on Linux

- **Status:** in-progress
- **Owner:** worker instance (coordinator-launched), 2026-10-14
- **Branch/worktree:** `linux/T-007` from `hvoya-linux`, in `/Volumes/Storage/code/IPlug2-T-007`.
- **Depends on:** T-002 (merged).
- **Conflicts:** `IGraphics/Platforms/IGraphicsLinux*`, which T-008 also touches — don't run them in parallel. Also CLAP/VST3 GUI scale code (shared files; Linux-only branches).
- **Plan phase:** P5.

## Goal
Correct UI scale on HiDPI Linux desktops:
- crisp rendering at 1.25/1.5/2.0;
- the right physical window size;
- mouse coordinates mapped correctly;
- no fight between the host's scale and ours.

## Context
- On X11, CLAP and VST3 sizes are in **physical pixels** (notes/02 §5).
- **Scale sources, priority order:**
  1. host-provided — CLAP `gui.set_scale`, VST3 `IPlugViewContentScaleSupport::setContentScaleFactor`;
  2. `Xft.dpi`/96 from `XResourceManagerString`;
  3. `GDK_SCALE` / `QT_SCALE_FACTOR`;
  4. 1.0.
- iPlug's own model is `SetScreenScale`, drawing scale vs window scale (see IGraphicsMac/Win). hvoya has resize fixes: `mInOpenWindow`, `DrawNow`; host resize only if enabled (notes/12 F3).
- RR had an `IPLUG2_SCREEN_SCALE` env hack, which was dropped in P0. Don't reintroduce env hacks.
- Fonts and NanoVG: text must be rasterised at the device scale (no blurry upscaling).

## Steps
1. Read how mac/win implement `SetScreenScale`/`GetScreenScale` and the CLAP/VST3 scale callbacks in this tree, plus `IGraphicsLinux` (notes/11).
2. Implement the scale query plus the host callbacks on Linux, and apply the scale to the window size, GL viewport and NanoVG pixel ratio, and mouse coordinates.
3. Test in Docker:
   - Xvfb at the same resolution with `Xft.dpi` set via `xrdb` to 144 and 192, and `GDK_SCALE=2`;
   - REAPER passes its own scale? Check what REAPER/SWELL does with `Xft.dpi`.
   - Screenshots at 1.0, 1.5 and 2.0: text crisp, layout proportional, knob drag hit-testing correct (the xdotool step coordinates must be scaled — adapt the step runner or write scale-aware steps).
4. On the VM (take the VM lock): GNOME "Scale 200%" and fractional scaling under X11 (`gsettings`), screenshots.
5. Make sure plugin-initiated resize (IPlugEffect corner resizer) still works at scale ≠ 1.

## Acceptance
- [ ] Scale picked up from the host when offered, else `Xft.dpi`; crisp text at 1.5 and 2.0 (screenshots reviewed by eye).
- [ ] Mouse hit-testing correct at 1.5/2.0 (knob drag changes the right control).
- [ ] Resize works at scale ≠ 1; no resize loop with REAPER.
- [ ] Main-thread check and validators unchanged; mac build proof if shared files are touched.
- [ ] Self-review; evidence in `evidence/t007/`; Log complete.

## Log

- 2026-10-14, worker instance. Worktree `/Volumes/Storage/code/IPlug2-T-007`, container `t007`, build dirs
  `build/t007-*`, evidence `evidence/t007/`. Lock `T-007` held by the coordinator.
