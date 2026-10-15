# T-007 — P5: HiDPI / scaling on Linux

- **Status:** review-done
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
- [x] Scale picked up from the host when offered, else `Xft.dpi`; crisp text at 1.5 and 2.0 (screenshots reviewed by eye).
- [x] Mouse hit-testing correct at 1.5/2.0 (knob drag changes the right control).
- [x] Resize works at scale ≠ 1; no resize loop with REAPER.
- [x] Main-thread check and validators unchanged; mac build proof if shared files are touched.
- [x] Self-review; evidence in `evidence/t007/`; Log complete.

## Log

- 2026-10-14, worker instance. Worktree `/Volumes/Storage/code/IPlug2-T-007`, container `t007`, build dirs
  `build/t007-*`, evidence `evidence/t007/`. Lock `T-007` held by the coordinator.
- Notes: `notes/17-p5-hidpi.md` (model, findings, acceptance table, harness, known issues).
- e0ff5b42b: scale model. `IGraphicsLinuxScale.h` (Xft.dpi via `XGetDefault`, GDK_SCALE, QT_SCALE_FACTOR; eighths,
  [1, 4]); `IGEditorDelegate` keeps the host's scale and rescales the closed editor's size (Linux-only hook
  `UpdateEditorSizeForScreenScale()` from CLAP `gui.create`, the VST3 view constructor, the APP); VST3
  `setContentScaleFactor` on Linux; `IGraphicsLinux` takes the scale in its constructor and tells the host a size the
  open changed on the first frame; VST3 context menu in physical pixels.
- 8b3284876: harness (`xact.sh scale`, `XVFB_SCREEN`, `REAPER_UI_SCALE`, probes, VM scale scripts), `Tests/LinuxScale`.
- bcc44227d, 38d903eaf, e09de685e: evidence. Docker: REAPER IPlugEffect CLAP+VST3 at 1/1.5/2 PASS x6, IPlugControls
  full P3b steps at 1.5/2 PASS x4, APP P3b at Xft.dpi 144/192, mini host matrix, resize/restore/context-menu probes.
  VM: GNOME 200 % and Large Text PASS; fractional 150 % sizes verified, no screenshot (virgl hang, notes/17 issue 6).
  Validators and NEEDED unchanged; mac IPlugEffect CLAP+VST3 build.
- Finding: REAPER passes SWELL's `ui_scale` to every plug-in (CLAP and VST3), 1 by default, GDK's integer scale with
  `ui_scale_auto`; so in REAPER the editor always matches REAPER's UI.
- VM lock taken 21:42-22:15, released. **The VM screen was left at 1920x863 (was 1440x863)** after the Mutter
  modesets: UTM's preferred size changed; scale 1 and the experimental features are restored.
- Evidence screenshots were replaced by contact sheets (`sheet.jpg`) plus 1:1 crops; history rewritten on this branch
  only (soft reset before any merge) to keep 35 MB of full-screen PNGs out of it.
- Independent review (fresh subagent, notes/18-review-T-007.md). Round 1: accept after fixing findings 1-2 (Medium:
  closed-editor size drifted by 1 px over two rescales; 1:1 crops not committed) plus Lows/nits; all fixed in
  3ae56b76e, with re-runs. Round 2 (fresh build of 3ae56b7): **accept**, no regressions; every acceptance item Pass.
  Non-blocking nits left: no unit test of the rescale arithmetic itself (covered by the mini-host drift matrix).
- VM restored 2026-10-15 02:29: GNOME scale 100 %, text 1.0, no experimental features, screen back at 1440x863,
  desktop responsive (screenshot checked); VM lock released. Containers t007, t007b removed.
- Hand-over: commits 11af162f9..HEAD on linux/T-007. Evidence `evidence/t007/`, notes/17 (design, results, known
  issues 1-8), notes/18 (review). Open issues: notes/17 Known issues.

