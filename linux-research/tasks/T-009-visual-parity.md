# T-009 — Visual parity of Gneiss and Sand with macOS/Windows (P6)

- **Status:** in progress — Docker comparison done (no rendering differences); VM (virgl) run pending (VM hung, T-013)
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
- [ ] Side-by-side sheets for every listed view, 1× and 2×, Gneiss and Sand (`evidence/t009/`).
- [ ] Every difference fixed or explained; the remaining list goes to the user for acceptance.
- [ ] No mac/win change; step scripts PASS.
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
| Gneiss about box | 0.035 | build-info text (branch/commit/date) |
| Gneiss factory preset list | 0.094 | state behind the list; the current preset row highlighted on Linux (CLEAN MOOG is current) |
| Gneiss MIDI CC menu | 0.17 | by design: native NSMenu on mac, IGraphics stand-in on Linux (no GTK in the plug-in, P3). The stand-in opens its submenu to the left (`◀`) because the knob is right of the editor's centre (`IPopupMenuControl` places submenus towards the larger side) — upstream behaviour |
| Sand FLT, ENV, GRIND, PAN | 0.020 each | the debug "[ ]" button: mac shots are a Debug build (button exists), Linux Release (no button) |
| Sand separate dry/wet | 0.025 | hover highlights (mac: HP; Linux: sep) |
| Sand debug control bounds | 0.11 | Release has no debug button, so no bounds on Linux (expected) |
| Sand text entry | 0.092 | native NSTextField (blue selection) vs `ITextEntryControl` (grey selection); the field's size and text match |
| Sand about | 0.042 | build-info text |

Fonts, glyph shapes (MDI icon font), SVG logos, knob arcs, line widths, colours and layout are the same to the pixel where the state is the same. **No rendering difference to fix at scale 2.**

**Scale 1** (no 1x mac reference exists on a Retina Mac; compared with the 2x shots shrunk 50%): Gneiss 0.074, Sand 0.050. Same layout and colours; the red is edge anti-aliasing: Linux rasterises text and strokes at 1x directly, the reference is a 2x render averaged down, so Linux text looks slightly crisper and heavier (as P3a noted). Not a defect; for the user to accept.

**Still open:** the same views on the VM (virgl, real Mesa GL) — the VM's GPU hung during T-013 and needs the user (UTM). Then the review.
