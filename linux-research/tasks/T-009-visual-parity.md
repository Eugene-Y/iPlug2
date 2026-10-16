# T-009 — Visual parity of Gneiss and Sand with macOS/Windows (P6)

- **Status:** ready (Sand part after T-012); planned after T-019 (same files)
- **Owner:** — (planned: coordinator)
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
- [ ] Side-by-side sheets for every listed view, 1× and 2×, Gneiss and Sand (`evidence/t009/`).
- [ ] Every difference fixed or explained; the remaining list goes to the user for acceptance.
- [ ] No mac/win change; step scripts PASS.
- [ ] Independent review (AGENTS §4); Log complete.

## Log
(append: date — what was done — commits — next)
