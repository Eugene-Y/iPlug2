# Review of T-009 (visual parity, Docker and VM parts)

Reviewer: independent subagent, fresh context, 2026-10-24.
Scope: `refs/heads/hvoya-linux..refs/heads/linux/T-009` (26b3103b2 Docker part, c3144b82c merge, 40cdfbaa5 VM part,
8888cd256 step scripts), the task file, `evidence/t009/`, the harness scripts.

## Round 1

What I checked:
- `git diff hvoya-linux linux/T-009` outside `linux-research/` is empty. No framework or product code changed, and no
  product names were added outside `linux-research/`.
- Sheets opened and read: `gneiss-01`, `-05`, `-06`, `-07`, `sand-05`, `-06`, `-07`, `-08` (2×), `vm-x1-g01`, `-g04-tall`,
  `-g07`, `vm-vs-docker-x1-g07`. I also made zoomed crops of mac vs Linux at 2× (logo SVG, text, MDI icons, knobs) and
  at 1× (mac 50% vs VM).
- Re-ran `make-vm-sheets.sh` on a copy in my scratch dir (2 min). The Docker-vs-VM column and every mac50-vs-VM value
  match `vm-x1-results.txt` exactly, except g01 and s01 (V7).
- Measured text-run bounding boxes at 1× (V2).
- Checked the fonts: `fc-match sans-serif` on the VM gives Noto Sans (VM lock taken and released, ssh only, nothing
  started). The `iplug2-linux:24.04` image gives DejaVu Sans (container `t009r-fc`, `--rm`).
- Gneiss source (`Gneiss.cpp:144`): the "?" hover help is on by default and read from the machine-local prefs, as the
  Log says.
- `IPopupMenuControl.cpp:674-685`: the submenu goes to the left when the anchor is right of the centre, as the Log
  says.
- Step-script evidence: both RESULT PASS. `gdb-crash.txt` is the harness's end-of-run SIGINT thread dump, not a crash.

Confirmed:
- At 2×, mac and Linux are the same to the pixel where the state is the same: SVG logo, MDI icons, Roboto Mono text,
  arcs, line widths, colours.
- At 1×, virgl and llvmpipe draw the same (≤ 1 pixel beyond 8% per view, apart from g07).
- The g07 menu-font difference between VM and Docker is real and has the stated cause.

### V1 — major — the about box shows the product name where mac/win show the plug-in format (missed Linux difference)
- **Evidence:** the 2× about sheets and the 2026-10-24 VM captures.
  - `sheets/gneiss-05-about.png`: mac shows "CLAP arm64 x86_64 release", Linux shows "**Gneiss** aarch64 release".
  - `sheets/sand-08-about.png`: mac shows "SAND v1.1.10 VST3", Linux shows "SAND v1.1.10 **Sand**".
  - The 2026-10-24 builds (`vm-x1-gneiss/g05-about-view.png`, 6246fb7; `vm-x1-sand/s08-about-view.png`, 95be3b0) show
    the same.
- **Cause:** `hvoya/hvoya.cmake` writes `PLUG_PRODUCT` from the `product` argument. The products call
  `hvoya_add_utils(${PROJECT_NAME}-hvoya ${PROJECT_NAME})` (e.g. `Sand-linux/CMakeLists.txt`), so `PLUG_PRODUCT` is
  the project name. The Xcode/VS pre-builds pass the format instead (`write_plug_build_info_to_file.bat … "VST3"` /
  `"AAX"`). One generated header is shared by all CMake formats (APP, VST3, CLAP), so it cannot carry a per-format
  value as it stands.
- **The Log** files both rows under "build-info text (branch/commit/date)", which hides this. The difference is
  Linux-specific (the CMake path only).
- **What should change:** record it in the Log as a Linux-specific finding, with file:line, for the coordinator.
  - A fix is outside T-009's harness-only scope. Options are a per-target define or a per-format generated header in
    `hvoya.cmake`.
  - Correct the two about-box rows of the 2× table and the VM text.
  - Optional nit: `CMAKE_SYSTEM_PROCESSOR` gives "aarch64" where Xcode says "arm64". That is cosmetic and can be
    accepted, but say it.

### V2 — major — the 1× explanation ("same layout; the red is edge anti-aliasing") is not what the evidence shows
- **Evidence:** text bounding boxes at 1×, mac 50% vs VM. Docker 2× shrunk 50% equals mac 50% in every case.

  | Text | mac 50% | VM 1× |
  |---|---|---|
  | "1 : CLEAN MOOG" | 103 px wide | 96 px wide, starts 4 px later |
  | "Main hz" | 51 | 47 |
  | "FREQS" | 36 | 34, shifted +3 px |
  | "USER" | 29 | 27, shifted +3 px |
  | "20.0 kHz" | 57 | 54 |
  | Sand "10.00 kHz" | 63 | 59 |
  | Sand "comp" | 29 | 26 |

  Zoomed crop (left column, `ALL`/`FREQS`/`USER`): the 1× labels are visibly narrower and sit 1 px higher.
- **Cause:** NanoVG's fontstash snaps every glyph advance and kerning to whole device pixels:
  - `Dependencies/IGraphics/NanoVG/src/fontstash.h:1202` (`*x += (int)(adv + spacing + 0.5f)`);
  - `:1243` (`*x += (int)(glyph->xadv / 10.0f + 0.5f)`);
  - quad origins truncated at `:1216` / `:1230`.

  At 1× a device pixel is a whole point, so text runs come out 4–7 % narrower and shifted by a few pixels. At 2× they
  are not. The difference is in glyph placement, not only in edge anti-aliasing.
- **Not a Linux bug:** the same code runs on Windows at 100 % and on a non-Retina mac. So the conclusion "nothing to
  fix in Linux code" stands, but the stated reason is wrong.
- **What should change:** reword both the 2026-10-19 and 2026-10-24 scale-1 paragraphs, and the user-acceptance item.
  Suggested meaning: "At 1×, text runs are up to ~7 % narrower and their glyphs sit on whole pixels (fontstash rounds
  advances to device pixels). Same as Windows at 100 % and non-Retina macs. Strokes and shapes differ only by
  anti-aliasing."

### V3 — minor — "the VM cannot show 2×" is overstated, and the deviation is not in the user-acceptance list
- **The size argument holds for the full editor in one shot.** Gneiss at 2× is 1880 px wide and Sand 1480 px; the VM
  screen is 1368×863.
- **"Cannot" is too strong.** A 2× spot check was possible without any display-mode or scale change:
  - Sand at 2×: all but the right ~130 px fits on screen.
  - Gneiss's main view is 1880×400: two shots with the FX window moved horizontally (`xdotool windowmove` / `wmctrl`
    to a negative x) cover it, and Mutter keeps windows that are partly off-screen to the side.
- **Wording:** "At scale 2 Gneiss is 1880×1240" is true only for MORPH on; the main view is 1880×400.
- **The risk is low.** VM and Docker are pixel-identical at 1×, and at 2× the GL path differs only in buffer and atlas
  sizes. Still, the task's Steps ask for the VM at scale 1 and 2.
- **What should change:** one of the following.
  - (a) One tiled 2× VM shot per product (two editor opens, within the VM rules), compared with the Docker 2× view.
  - (b) Say "the full 2× editor does not fit on the VM's 1368×863 screen; a partial check was skipped because 1×
    showed virgl = llvmpipe", and add "no 2× run on virgl" to the "For the user to accept" list. Today it appears only
    in the acceptance item's note, not in that list.

### V4 — minor — the menu/tooltip font is described as the "desktop's" face; it is fontconfig's generic sans-serif
- **Verified on the VM:** `fc-match sans-serif` gives Noto Sans. But GNOME's interface font there is
  `'Cantarell 11'` (`gsettings get org.gnome.desktop.interface font-name`).
- **What the code does:** `kMenuFontFamily = "sans-serif"` with `DEFAULT_TEXT` (14 px) at
  `IGraphicsLinux.cpp:108,1905-1911`. So the stand-in menu uses fontconfig's generic alias at a fixed size. It does not
  use the desktop's UI font or size, unlike mac/win menus, which do use the system UI font.
- **"Smaller" is imprecise.** Both are 14 px. Noto Sans is narrower than DejaVu Sans (see the zoomed g07 crops), so
  the menu box is narrower.
- **What should change:** reword the g07 bullet and the user-acceptance item, e.g.: "menus and tooltips use
  fontconfig's generic sans-serif at 14 px (Noto Sans on the VM, DejaVu Sans in Docker), not GNOME's interface font".
- **Optional, for the coordinator:** following the desktop font without GTK is possible through XSETTINGS
  (`Gtk/FontName`). That is a separate decision, not T-009 work.

### V5 — minor — state differences left out of "all explained", and an unused mac reference
- **s06/s07 dry/wet state.** In `sheets/sand-06-debug-bounds.png` and `sand-07-input.png`, mac has dry/wet
  **separate** (dry + wet knobs, "sep") and Linux has **joint** (one "mix" knob, "joint"). This is the largest red area
  in both sheets, and the 2× table does not mention it.
  - Cause: `scripts/steps/t009-sand.txt` clicks 657 156 a second time after s05, which switches sep off again. The
    user's s06/s07 references were taken with it on.
  - Fix: drop that second click, so s06–s08 match the references, or list the difference.
- **Unused reference.** `mac-refs/gneiss morph off all tab.png` (custom preset, filter types USER BLEND, "2x" on) is
  never compared or mentioned.
  - No Linux sheet shows the USER BLEND state; it appears only in the mac shots.
  - Fix: compare it (load the custom state, or a preset with USER BLEND), or say why it is skipped.
- **g07 Docker vs VM.** Besides the help bubble, the Main hz knob's hover fill differs: filled in Docker 1× and on mac,
  not filled on the VM or in Docker 2×. Mention it alongside the bubble timing.

### V6 — minor — the MIDI CC submenu is never shown on Linux, and parts of the view list are not accounted for
- **g07 submenu.** The mac g07 shows the MIDI CC submenu open (Learn / Clear / Invert). No Linux g07 capture opens it;
  only the root menu with its `◀` arrow is visible. The Log says the stand-in "opens its submenu to the left"; that
  is inferred from the arrow and the code, not shown.
  - Fix: add a mouse move onto "MIDI CC" after `rclick-hold`, so the submenu is open as on mac. Or reword.
- **Task Step 1 items with no word in the Log:**
  - Sand "menus": no mac reference exists.
  - Windows references: none were supplied.
  - Bitmaps: neither product has any; both are SVG-only, per `Gneiss-linux/resources/img`, `Sand-linux/resources/img`
    and the `LoadSVG` calls.
  - Animations.

  Fix: add one line each, so the Goal's list ("fonts, icons, SVGs, bitmaps, animations … mac and win") is closed
  explicitly.

### V7 — minor — `make-vm-sheets.sh` is not fully reproducible
- **Cached half-size references.** The script reuses `mac-refs-half/*` when present. Two of the 15 files (g01 and s01)
  date from 2026-10-19 and were made with a different downscale.
  - A clean re-run gives g01 0.0750 (table: 0.0736) and s01 0.0507 (table: 0.0498).
  - All other values reproduce exactly.
  - Fix: always regenerate (drop the `[ -f … ] ||` cache) and refresh the table.
- **Hand-made tall crops.** `g04-user-morph-on-tall-view.png` is cropped "by hand" in both `docker-x1-gneiss/` and
  `vm-x1-gneiss/`. The command is recorded nowhere, but the script depends on these files.
  - Fix: script the crop (the tall geometry is the view's x,y with 940×620 at 1×), or record the exact command in the
    Log.

### V8 — nit — harness script quality
- **Stale comment.** `evidence/t009/find-offset.sh:3` says "+-4 px window", but the code searches ±6 (`seq -6 6`;
  the Log also says ±6). This breaks the comment rule; fix the comment.
- **`t009-capture.sh` does not stop when the FX window is not found.** `view_geom` then prints `0 0 0 0` and the loop
  crops with `-crop 0x0+0+0`, which ImageMagick treats as the whole image. Every "view" silently becomes a full
  screenshot. Exit with an error when `W`/`H` is 0.
- **Temp dirs.** `find-offset.sh` and `make-sheet.sh` leave their `mktemp -d` directories behind on failure. A
  `trap 'rm -rf "$T"' EXIT` would fix it.
- **Comments otherwise follow the rules.** The window-manager frame title, the 40 px top bar, "No Escape" and
  `verchk=0` all state a non-obvious why.

### Verdict
**Changes needed.**
- **V1:** a missed Linux-only difference in the about box, filed under "build info".
- **V2:** a wrong technical explanation of the 1× difference, which goes to the user for acceptance.

Neither needs framework code changed within T-009: V1 is a finding to record and route, and V2 is a rewording. V3–V7
are wording, list and reproducibility fixes, and V8 is a nit.

## Author's response to round 1 (2026-10-24)
- **V1:** fixed in Linux-only CMake: dc1ba588a (`hvoya/hvoya.cmake`, one `plug_build_info.hpp` per format target via
  `$<TARGET_PROPERTY:NAME>`). Checked in Docker: "CLAP aarch64 release", "SAND v1.1.10 VST3"
  (`evidence/t009/fix-about-*`). Logged as a finding, the 10-19 table corrected.
- **V2:** the 1× explanation rewritten (glyph advance/position rounding in fontstash, a few percent narrower; same on
  Windows 100 % and non-Retina macs), in the 10-19 section, the VM section and the acceptance list.
- **V3:** a 2× try on the VM (`scripts/vm/probe/t009/x2-tiles.sh`): Mutter keeps the FX window inside the work area
  and refuses negative x, so only the left 1302 px are visible; those match Docker 2× (same builds) to the pixel and
  the mac references. The rest is listed for the user. 1880×400 vs 1880×1240 corrected.
- **V4:** "fontconfig's generic sans-serif at fixed sizes, not the desktop UI font"; "narrower".
- **V5:** s06/s07 joint vs separate, the unused "morph off all" reference and the g07 knob hover fill are listed.
- **V6:** no MIDI CC submenu shot, no Windows or Sand menu references, SVG/fonts only, no animation comparison: listed.
- **V7:** `make-vm-sheets.sh` writes the half-size references afresh and makes the tall g04 crops from the logged
  origin; the table now has g01 0.075 and s01 0.051 (as your clean re-run).
- **V8:** `find-offset.sh` comment (±6), temp dirs removed on exit in both helpers, `t009-capture.sh` stops when the FX
  window is not found.
- Step scripts re-run with the fixed builds: PASS.

## Round 2

Scope: dc1ba588a (`hvoya/hvoya.cmake`) and 48dbe8193 (Log, harness, evidence).

### Checked and resolved
- **V1: fixed, and the code is sound.**
  - The change only touches the CMake path. The Xcode pre-build passes `${TARGET_NAME}` and the VS `.bat` passes the
    format, and neither reads `hvoya.cmake`. So Xcode/VS builds are unaffected.
  - Each format target compiles its own sources (`IPlugPlugin.cmake:142-212`: `<name>-app/-vst3/-clap/...`). So
    `$<TARGET_PROPERTY:NAME>`, evaluated in the consuming target's context, picks the right directory. It comes
    before the product-named fallback, and quote includes then find it.
  - Confirmed in the run's build dirs:
    - `linux-research/build/t009-{gneiss,sand}/generated/<product>-<fmt>/plug_build_info.hpp` hold "APP"/"VST3"/"CLAP"/….
    - `build.ninja` includes `generated/<product>-app`, `-clap` and `-vst3` for the matching targets only.
  - The rebuilt about boxes read "CLAP aarch64 release" and "SAND v1.1.10 VST3"
    (`fix-about-gneiss-clap/g05-about-view.png`, `fix-about-sand-vst3/s08-about-view.png`, opened).
  - The comment states a WHY, and the commit message has the repro.
- **V2 and V4:** the rewording is accurate (`DEFAULT_TEXT_SIZE` 14, `kTooltipFontSize` 13).
- **V3: the partial 2× run supports the new claim.**
  - `vm-x2-*/geometry.txt`: Mutter left the FX window at x 66 and 1302 px wide for both requested positions. The view
    stays 1880 (1480) px wide, so REAPER's own window clips it.
  - `sheets/vm-vs-docker-x2-g01-clean-moog-all.png`: RMSE 0.00035, no red. `sheets/vm-x2-s01-flt.png` (mac) shows
    the expected sep/joint state only.
  - The deviation is now in the user-acceptance list.
- **V5–V8:** all addressed as described.
  - `make-vm-sheets.sh` regenerates the half references and the tall crops, and the table matches my clean re-run.
  - The temp-dir traps and the empty-geometry stop are in place.

### R1 — minor — the 2× VM sheets have no recorded command
- **What is missing:** how `sheets/vm-x2-*.png`, `sheets/vm-vs-docker-x2-*.png` and the `docker-x2-one-*` captures
  were made. That covers the Docker run's arguments, the crop of the Docker 2× view to the visible 1302 px, and the
  mac offset. None of it is in a script or the Log, which is the same gap V7 closed for the 1× sheets.
- **Fix:** add these few lines to `make-vm-sheets.sh` (or a sibling script), or record the exact commands in the Log.

### R2 — nit — `scripts/vm/probe/t009/x2-tiles.sh` header promises what the VM did not deliver
- The header says the two positions are taken "so that the two tiles cover the whole view". On this VM the window
  manager clamps the window, both tiles are the same left 1302 px, and `tiles.txt` shows `1 0 1302` / `2 0 1302`.
- **Fix:** reword to "tries two horizontal positions; tiles.txt records which part of the view each tile shows".

### R3 — nit — `hvoya.cmake` relies on an unstated naming coupling
- The per-format lookup works only when `product` is the name passed to `iplug_add_plugin`, which creates
  `<product>-clap` etc. Any other name silently falls back to the product-named header, which shows the old bug.
- Add a few words to the function comment or the usage header, e.g. "`product` must be `iplug_add_plugin`'s name".
- AUv3, iOS and WAM targets (`<name>AU-framework`, `-ios-app`, `-wam`) also fall back to the product name. They are
  not built on Linux; acceptable as is.

### R4 — nit — a comment in `make-vm-sheets.sh` gets the order of events wrong
- It says "MORPH on grows Gneiss … after the run cropped its shots". The capture script reads the view geometry
  before the steps and crops every shot to it later, so the growth happens between the geometry read and the crops.
- Suggested: "`t009-capture.sh` crops every shot to the 200-point view it measured before the steps; MORPH on grows
  Gneiss to 620".

### R5 — nit — the task's Status line is stale
- It still reads "VM (virgl) run (scale 1) done: no rendering difference to fix".
- Mention the about-box build fix (dc1ba588a) and the partial 2× on virgl.

### Verdict (round 2)
**Approved with minor findings.** No major finding is left. R1 is a reproducibility gap in the evidence, and R2–R5
are wording nits that the author can fix without another review round.

## Author's response to round 2 (2026-10-24)
- R1: `make-vm-sheets.sh` now also makes the 2× sheets (mac offset from `find-offset.sh`, crops to the VM tile); the
  Docker 2× and VM tile commands are in the Log's Evidence list. Re-run: same RMSE (0.0087, 0.00035, 0.0055, 0.0002).
- R2: `x2-tiles.sh` header says it tries two positions and `tiles.txt` records what each tile shows.
- R3: `hvoya.cmake` comment: `<product>` must be `iplug_add_plugin`'s name.
- R4: the tall-crop comment states the order (geometry measured before the steps).
- R5: Status updated (review-done, the about-box fix, partial 2×).

Final verdict: approved with minor findings (round 2); all fixed.
