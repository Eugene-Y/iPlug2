# T-009 — Visual parity of Gneiss and Sand with macOS/Windows (P6)

- **Status:** in review — Docker comparison (scale 2 and 1) and VM (virgl) run (scale 1) done: no rendering difference to fix
- **Owner:** coordinator (chat f91df715), lock `.linux-port-locks/T-009`
- **Branch/worktree:** `linux/T-009` from `hvoya-linux`, in `/Volumes/Storage/code/IPlug2-T-009`. Product repos only on their local `linux-experiment` branches, and only if a product itself must change (unlikely).
- **Depends on:** T-003 (Gneiss build), T-012 (Sand build).
- **Plan phase:** P6.
- **Conflicts:** `IGraphics/Platforms/IGraphicsLinux*`, `IGraphics/Drawing/IGraphicsNanoVG*` (Linux paths only), fonts.

## Goal
Gneiss and Sand on Linux look like on mac and win: same layout, colours, fonts, icons, SVGs, bitmaps, animations, at scale 1 and 2. Differences are either fixed in Linux code or explained (e.g. no font hinting anywhere in NanoVG) and accepted by the user.

## Context
- Renderers: mac NanoVG/Metal, win NanoVG/GL2, Linux NanoVG/GL2. Text is stb_truetype/fontstash on all three, so differences should come from scale, gamma, MSAA/AA, texture filtering, or our font loading (fontconfig, `LoadPlatformFont`), not from the rasterizer itself.
- Known so far (notes/11 #13, P3a): IPlugControls same layout and colours; Linux text slightly heavier and less smooth at 1× vs mac at 2× downscaled; IBKnob shows its 1× bitmap (no @2x picked at scale 1 — correct, but check scale 2 picks @2x).
- HiDPI (T-007): scale from the host, `Xft.dpi`, `GDK_SCALE`.
- **References:** ask the user (through the coordinator) for mac screenshots of the views listed below at 1× and 2× (APP debug build has a screenshot menu; or plug-in screenshots in REAPER), and Windows ones if available. Don't build in or write to the main checkouts.

## Steps
1. List the views to compare for each product (Gneiss: main, morph on/off, USER tab, each popup/menu, preset strip, about box, text entry, MIDI CC menu; Sand: main, menus, preset strip, about) and capture Linux shots in Docker (llvmpipe) and on the VM (virgl) at scale 1 and 2 (`scripts/compare-shots.sh`).
2. Compare side by side and pixel-diff where layouts match; zoom on text, icons (MDI font), SVG logos, bitmaps, gradients, shadows, rounded corners, line widths.
3. Root-cause each difference; fix in Linux-only code (sRGB/gamma, `NVG_ANTIALIAS`/stencil strokes, texture filtering, font-face selection, scale rounding).
4. Re-run the full P3b scripts and `steps/gneiss.txt` / `sand.txt`, churn, main-thread census.

## Acceptance
- [x] Side-by-side sheets for every listed view, 1× and 2×, Gneiss and Sand (`evidence/t009/sheets/`). *2×: Docker
  against the mac references, every view; the VM shows only the left 1302 px of the main views at 2× (below), which
  match Docker and mac. 1×: VM against the mac references shrunk 50% and against Docker, every view. No Windows
  references exist.*
- [x] Every difference fixed or explained; the remaining list goes to the user for acceptance. *One Linux-only fix:
  the about box named the product where mac names the format (dc1ba588a, `hvoya/hvoya.cmake`). For the user to
  accept: at 1× NanoVG rounds glyph advances to whole pixels, so text runs are a few percent narrower than the 2× mac
  render shrunk (as on Windows at 100 % or a non-Retina mac); the stand-in menus and tooltips use fontconfig's
  generic sans-serif face at fixed sizes (Noto Sans on the VM, DejaVu Sans in Docker), not the desktop's UI font; 2×
  on virgl seen only in part (the VM's screen is too small).*
- [x] No mac/win change; step scripts PASS. *Besides `linux-research/` (harness), T-009 changes `hvoya/hvoya.cmake`,
  which only CMake builds use (Xcode/VS run their own pre-build); no product code. `steps/gneiss.txt` and `steps/sand.txt` PASS in Docker with the CLAP builds at dc1ba588a
  (`evidence/t009/steps-{gneiss,sand}-clap/RESULT.txt`).*
- [ ] Independent review (AGENTS §4); Log complete.

## Log
(append: date — what was done — commits — next)

### 2026-10-19 — side-by-side with the user's mac references (Docker, llvmpipe)
**References** (the user, 2026-10-19): `evidence/t009/mac-refs/`: Retina (2x) screenshots of Gneiss CLAP (CLEAN MOOG project `clean moogRPP.RPP`, ALL, FREQS, USER, MORPH on, factory preset list, about box, MIDI CC menu) and Sand VST3 (FLT/ENV/GRIND/PAN tabs, separate dry/wet, debug control bounds, text entry, about) in REAPER on mac. Gneiss has no text entry yet (user).

**Harness:**
- `scripts/t009-capture.sh`: REAPER in the GUI container at `REAPER_UI_SCALE` 2 (or 1) on a 2600×1900 Xvfb; opens the user's RPP (same CLAP id, same state chunk) or adds a fresh instance; runs `scripts/steps/t009-{gneiss,sand}.txt` (editor points, scaled by xact); crops every shot to the editor view. No Escape in steps: REAPER takes it and closes the FX window.
- `evidence/t009/find-offset.sh`: locates the editor inside a mac screenshot (coarse subimage search at 1/4, then ±6 px at full size).
- `evidence/t009/make-sheet.sh`: mac crop | Linux | Linux with pixels differing >8% in red, plus RMSE.
- Sheets: `evidence/t009/sheets/*.png` (PNGs are git-ignored: local in this worktree, copied to `IPlug2-linux/linux-research/evidence/t009/sheets/` too).

**Results at scale 2 (= Retina):**

| View | RMSE | Differences, all explained |
|---|---|---|
| Gneiss CLEAN MOOG, ALL | 0.014 | undo arrow enabled on mac (it had undo history); "?" hover-help toggle on (a machine-local preference, on by default on a fresh Linux profile); the mac window's rounded corner |
| Gneiss FREQS | 0.068 | the hover-help bubble (help on, pointer resting on FREQS); mac had MAIN oct 1 (shot before the project was loaded) |
| Gneiss USER, morph off | 0.097 | state (mac shot is a custom preset) |
| Gneiss USER, morph on | 0.054 | state: the morph field's contours follow the points' sounds; grid, points, numbers, line style identical |
| Gneiss about box | 0.035 | build-info text (branch/commit/date); and the product name where mac names the format: a Linux build bug, fixed 2026-10-24 (dc1ba588a) |
| Gneiss factory preset list | 0.094 | state behind the list; the current preset row highlighted on Linux (CLEAN MOOG is current) |
| Gneiss MIDI CC menu | 0.17 | by design: native NSMenu on mac, IGraphics stand-in on Linux (no GTK in the plug-in, P3). The stand-in opens its submenu to the left (`◀`) because the knob is right of the editor's centre (`IPopupMenuControl` places submenus towards the larger side) — upstream behaviour |
| Sand FLT, ENV, GRIND, PAN | 0.020 each | the debug "[ ]" button: mac shots are a Debug build (button exists), Linux Release (no button) |
| Sand separate dry/wet | 0.025 | hover highlights (mac: HP; Linux: sep) |
| Sand debug control bounds | 0.11 | Release has no debug button, so no bounds on Linux (expected) |
| Sand text entry | 0.092 | native NSTextField (blue selection) vs `ITextEntryControl` (grey selection); the field's size and text match |
| Sand about | 0.042 | build-info text; product name for format, as for Gneiss (fixed, dc1ba588a) |

Fonts, glyph shapes (MDI icon font), SVG logos, knob arcs, line widths, colours and layout are the same to the pixel where the state is the same. **No rendering difference to fix at scale 2.**

**Scale 1** (no 1x mac reference exists on a Retina Mac; compared with the 2x shots shrunk 50%): Gneiss 0.074, Sand 0.050. Same layout and colours. *Corrected in review (notes/38 V2):* the red is not only edge anti-aliasing. At 1× text runs are 4–7 % narrower and their glyphs sit 2–4 px off (e.g. "1 : CLEAN MOOG" 103 → 96 px, "Main hz" 51 → 47 px): NanoVG's fontstash rounds each glyph's advance and position to whole device pixels (`fontstash.h`, `fonsTextIterNext`/`fons__getQuad`), which at 2× shrunk to 1× happens at half-pixel steps. Windows at 100 % and non-Retina macs render the same way. Not a Linux defect; for the user to accept.

**Still open:** the same views on the VM (virgl, real Mesa GL) — the VM's GPU hung during T-013 and needs the user (UTM). Then the review.

### 2026-10-24 — the VM (virgl) run
**Branch:** merged hvoya-linux b8079e9df into linux/T-009 (c3144b82c). `scripts/install-sdks.sh` run for this worktree
(it modifies `Dependencies/IPlug/VST3_SDK/README.md`; not committed).

**Builds.** Release, no traces: Gneiss CLAP (linux-experiment 6246fb7) and Sand CLAP (95be3b0) against this worktree at
c3144b82c (`build/t009-{gneiss,sand}`), staged on the VM in `~/iplug-builds/t009`; `~/.clap` and `~/.vst3` untouched
(Release builds of T-038 eda432127).

**Harness changes** (`scripts/`, committed):
- `t009-capture.sh`: takes REAPER's own FX window (Mutter's frame carries the same title) and moves it to the top
  (20,40) before the steps. At 1× near the screen's bottom, Gneiss growing to 620 points with MORPH on made the window
  manager move the window up, and the later clicks missed.
- `reaper-up.sh`: `verchk=0`. REAPER 7.82 now opens a modal update notice (7.83 is out), which blocks `rpr.sh`; the
  first Docker 1× run captured nothing usable because of it. Same change as on linux/T-013.
- `evidence/t009/make-vm-sheets.sh`: every view's sheets and the table below.

**What ran.**
- Docker (llvmpipe, Xvfb 1600×1000), REAPER at scale 1, both products, all steps: `evidence/t009/docker-x1-{gneiss,sand}/` (Sand's taken before the window move was
  added; Sand does not grow, so its crops are the same).
- VM (GNOME X11, Mesa virgl GL 2.1), REAPER at scale 1, same steps and builds:
  `scripts/vm/vm-run.sh t009-capture.sh evidence/t009/vm-x1-gneiss "CLAP: Gneiss" ~/iplug-builds/t009/.clap/Gneiss.clap
  scripts/steps/t009-gneiss.txt "evidence/t009/mac-refs/clean moogRPP.RPP"` with `VM_ENV=REAPER_UI_SCALE=1`, and the
  same for Sand without a project (`evidence/t009/vm-x1-{gneiss,sand}/`). Two editor opens; `xdpyinfo` answered
  before and after each.
- **2× on the VM, in part.** At scale 2 Gneiss is 1880×400 (1880×1240 with MORPH on) and Sand 1480×340; the VM's
  screen is 1368×863, and display-mode or scale changes are not allowed on it (they hung virgl before).
  `scripts/vm/probe/t009/x2-tiles.sh` tried two horizontal positions per product: Mutter keeps the FX window inside
  the work area (66…1368, 1302 px wide) and refuses the move to a negative x, so REAPER's window clips the editor and
  only its left 1302 px can be seen (`evidence/t009/vm-x2-{gneiss,sand}/geometry.txt`). Those 1302 px of the main
  views (g01, s01) match Docker at 2× with the same builds (`docker-x2-one-*`) to the pixel (no pixel beyond 8 %,
  RMSE ≤ 0.0004; `sheets/vm-vs-docker-x2-*.png`) and the mac references (RMSE 0.009 / 0.006, `sheets/vm-x2-*.png`).
  The rest of 2× on virgl stays unseen; listed for the user.
- `g04` (MORPH on) is cropped to the full 940×620 editor by `make-vm-sheets.sh` (`*-tall-view.png`), as at 2×.

**Results** (`evidence/t009/vm-x1-results.txt`; sheets `sheets/vm-x1-*.png` = mac 50% | VM | red where >8%, and
`sheets/vm-vs-docker-x1-*.png`):

| View | RMSE mac 50% vs VM | RMSE Docker vs VM | pixels >8% Docker vs VM |
|---|---|---|---|
| g01 CLEAN MOOG, ALL | 0.075 | 0.0017 | 1 |
| g02 FREQS | 0.086 | 0.0005 | 0 |
| g03 USER, morph off | 0.101 | 0.0005 | 0 |
| g04 USER, morph on | 0.058 | 0.0008 | 1 |
| g05 about | 0.046 | 0.0003 | 0 |
| g06 factory presets | 0.108 | 0.0006 | 0 |
| g07 MIDI CC menu | 0.174 | 0.111 | 11956 |
| s01–s05 FLT, ENV, GRIND, PAN, dry/wet | 0.045–0.055 | ≤ 0.0004 | 0 |
| s06 debug bounds | 0.105 | 0.0002 | 0 |
| s07 text entry | 0.095 | 0.0003 | 0 |
| s08 about | 0.049 | 0.0004 | 0 |

- **virgl draws like llvmpipe.** Apart from g07, VM and Docker differ by at most one pixel beyond 8% per view (RMSE
  ≤ 0.002): same fonts, MDI icons, SVG logo, arcs, line widths, colours, layout. So the GL driver adds no difference.
- **g07: the stand-in MIDI CC menu uses fontconfig's generic sans-serif face** (`kMenuFontFamily`,
  `IGraphicsLinux.cpp`; tooltips too) at fixed sizes (menu 14, tooltip 13 points), standing in for the system font of
  mac/win menus. It is not the desktop's UI font (GNOME's is Cantarell 11): fontconfig resolves `sans-serif` to Noto
  Sans on the VM and to DejaVu Sans in Docker, so the menu's text is narrower on the VM
  (`sheets/vm-vs-docker-x1-g07-midi-cc-menu.png`). The rest of g07's difference is Gneiss' hover-help bubble, which
  was up in the Docker shot and not yet in the VM one, and the hover fill of the knob under the pointer (timing of the
  held right click). Against mac the menu differs anyway (native NSMenu). The MIDI CC submenu, open in the mac shot,
  is not opened in the Linux shots.
- **Against the mac references at 1×** (VM): the same picture as Docker's 1× (Log 2026-10-19, corrected): same layout
  and colours; text a few percent narrower (glyph rounding, above); plus the state differences listed there (custom
  preset vs CLEAN MOOG in g03, help bubbles, the debug "[ ]" button, build info, the native text field and menu). Also:
  in s06 and s07 mac has dry/wet separate, Linux joint (the steps switch it back after s05).
- **Found and fixed (Linux-only): the about box named the product where mac names the format.** mac: "CLAP arm64
  x86_64 release", Sand "SAND v1.1.10 VST3"; Linux: "Gneiss aarch64 release", "SAND v1.1.10 Sand". The 2026-10-19
  table put it under "build-info text". Cause: `hvoya/hvoya.cmake` wrote one `plug_build_info.hpp` for every CMake
  format target, with `PLUG_PRODUCT` = the project's name, where the Xcode/VS pre-builds pass the format. **Fix
  dc1ba588a**: one copy per format target, found through the consuming target's name; CMake builds only. Repro and
  check: `evidence/t009/fix-about-gneiss-clap/` and `fix-about-sand-vst3/` (Docker, scale 1): "CLAP aarch64 release",
  "SAND v1.1.10 VST3". The VM's x1 about shots (g05, s08) predate the fix; the x2 tiles were taken with it.
  `PLUG_ARCHS` stays the CMake processor name ("aarch64"; mac lists its `ARCHS`).
- Not compared: the mac reference "gneiss morph off all tab.png" (a custom preset with USER BLEND, not reproducible
  here; the ALL tab with MORPH off is g01); Sand has no menu reference; no Windows references exist. Both products
  draw SVGs and fonts only (no bitmaps). Animations were not compared (still shots only).
- **For the user to accept:** the 1× glyph rounding, menus/tooltips in fontconfig's sans-serif face, and that 2× on
  virgl was seen only in part.

**Evidence:** `evidence/t009/{docker,vm}-x1-{gneiss,sand}/` (xact logs, window trees, VM `run.log`; PNGs local),
`vm-x2-{gneiss,sand}/`, `docker-x2-one-{gneiss,sand}/`, `fix-about-*/`, `vm-x1-results.txt`, `sheets/` (local),
`mac-refs-half/` (local; `make-vm-sheets.sh` writes them afresh each run).

**Builds after the fix:** Gneiss CLAP/VST3/APP and Sand CLAP/VST3 at dc1ba588a, staged on the VM in
`~/iplug-builds/t009` (the x2 tiles used them).

**VM state left:** no REAPER or jackd running; X11 `:0` answers; the builds stay staged in `~/iplug-builds/t009`;
`~/iplug-harness/linux-research/evidence/t009/mac-refs/clean moogRPP.RPP` copied there for the run. VM lock released.

**Cross-platform findings:** none.
