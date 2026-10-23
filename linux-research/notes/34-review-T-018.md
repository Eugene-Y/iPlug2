# Review of T-018 (Skia backend on Linux)

Reviewer: independent subagent, fresh context. Branch `linux/T-018`, range `hvoya-linux..linux/T-018`
(b7137e3b0, 6241b64c0, ccf7a38f8, bbcd68424, 8fc1fad92, 8f385dcdb). Evidence of this review:
`evidence/t018-review/` (PNGs on disk, git-ignored).

## Round 1

### What I checked
- **Diff read in full** (product code): `IGraphics/Drawing/IGraphicsSkia.{cpp,h}`, `IGraphics/Platforms/IGraphicsLinux.{cpp,h}`,
  `Scripts/cmake/IGraphics.cmake`, `Dependencies/IGraphics/build-skia-linux.sh`, `Documentation/linux-development.md`;
  the two new step files.
- **Threads.** No new thread, no `XInitThreads`; drawing, timer, X events stay on the host loop. Tooltip is drawn with a
  raster SkSurface + `XPutImage`, never on the editor's GL context, so Skia's GL state cache is not disturbed. In the
  Skia GL3 build the only raw GL in IGraphicsLinux runs before `GrDirectContext` exists (the first clear/swap);
  `BeginFrame` only reads state. OK.
- **Dependencies.** Skia's GL interface is assembled from `GLXFunctions::GetProc` (no `GrGLMakeNativeInterface`, no
  libGL link); font manager is `SkFontMgr_New_Custom_Empty` (no fontconfig link); the `egl*` names are answered with
  nullptr, which is exactly what `GrGLGetEGLQueryAndDisplay` needs to skip its EGL probe. OK.
- **CPU path.** 24-bit TrueColor visual checked for the 0xff0000/0xff00/0xff masks, own colormap; the `XVisualInfo` is a
  stack copy and correctly not `XFree`d; `XCreateImage` over Skia's pixels with `byte_order = LSBFirst`, data pointer
  cleared before `XDestroyImage`. Dirty area: `IGraphics::Draw` pixel-aligns the rects at `GetBackingPixelScale()`
  (screen × draw scale), and `DrawDirtyControls` maps `rects.Bounds()` with the same scale, floor/ceil, clipped to the
  pixmap — consistent with the raster surface size (`WindowWidth() * GetScreenScale()`, where `WindowWidth()` already
  includes the draw scale). Expose/Map/resize go through `SetAllControlsDirty`. `mGLX` stays null in a CPU build and
  every use of it is in GL-only code or behind `mGLContext`. OK.
- **GC lifetime.** Created after the view is initialised, freed in `DestroyWindowResources` before the windows (GCs do not
  depend on the window's life), reset by `w = Impl()`. The tooltip window has the editor's visual and depth, so the
  shared GC is valid for it. OK.
- **Fonts.** `makeFromData(data, faceIdx)` Linux only; per-instance `TypefaceFontProvider` registered as the asset font
  manager, family = font ID up to the first '-', matching what `DrawMultiLineText` requests. The IGraphics object is
  destroyed on every editor close (`IGEditorDelegate::CloseWindow`), so repeated `registerTypeface` of the same ID
  (it appends) cannot accumulate across opens. Tooltip typeface uses `MakeWithCopy`. OK (see R1-5 for a nit).
- **Open failure.** `GetDrawContext()` null after `DrawResize` → clean `fail()`; the GL depth is balanced before it. OK.
- **CMake.** Linux branches only (`UNIX AND NOT EMSCRIPTEN` after `WIN32`/`APPLE`), `--start-group` around the static
  libs, glad_GL3 added to Skia::GL3 on Linux only, configure error for Skia + GL2/METAL on Linux. One branch misses
  `NOT EMSCRIPTEN` (R1-1).
- **Build script.** `set -eo pipefail`, arch check, GN args = mac's set adapted (no X11/fontconfig/EGL/Vulkan/Graphite,
  bundled FreeType, `-fPIC`), clang/ninja from the system, gn fetched per run (arch-specific binary in a shared tree).
  Reuses any existing checkout without checking its branch (R1-4).
- **mac/win.** `evidence/t018/mac-proof/`: IGraphicsSkia.cpp preprocessed on macOS for Metal, CPU and GL3, old (+`<mutex>`)
  vs new: the only differences are the forward declaration, the braces around `if (cached) return true;` and
  `__FILE__`/`__LINE__`-bearing assert lines; `-fsyntax-only` passes. I read the script and the diff: **convincing**, and
  more to the point than the AGENTS §2 IPlugEffect xcodebuild, which uses NanoVG on mac and would not compile
  IGraphicsSkia at all. Windows (reading only): every change in shared files is inside `OS_LINUX`, except `<mutex>`
  and the `TypefaceFontProvider` forward declaration, both inert on MSVC. CMake mac/win paths unchanged by reading.
- **Product names:** `scripts/coord/check-product-names.sh` → `clean (HEAD)`. Doc text added ≤120 chars (some new code
  lines are 133–137 chars; the file already has such lines, see R1-2).
- **Evidence opened myself:** `evidence/t018/skia-gl3-controls-clap/sheet.png` (all 15 P3b steps drawn, popups, text
  entry, about box), `sheet-igraphicstest-pages-1.png` (GL3 and CPU match; NanoVG differences are renderer features;
  p06-text differs only by animation frame), `sheet-tooltip-gl3-cpu-nvg.png` (same tooltip in the three builds). They
  show what the Log claims.

### Re-run by me [R] (Docker, 24.04 aarch64, Release, container `t018rev`, removed at the end)
| Run | Result |
|---|---|
| [R] Build `build/t018rev-gl3` (`-DIGRAPHICS_BACKEND=SKIA` → `SKIA/GL3 -> iPlug2::IGraphics::Skia::GL3`), IPlugControls CLAP/VST3/APP | PASS (`build-gl3.txt`) |
| [R] Build `build/t018rev-cpu` (`… -DIGRAPHICS_RENDERER=CPU` → `Skia::CPU`), IPlugControls CLAP/VST3/APP | PASS (`build-cpu.txt`) |
| [R] `-fsyntax-only` of the IGraphicsLinux.cpp TU (which includes IGraphicsSkia.cpp) with the build's flags, CPU and GL3 | 0 warnings |
| [R] Full P3b, Skia GL3 **CLAP** (`gl3-controls-clap/`) | RESULT: PASS; Draw/OnDisplayTimer/ProcessXEvents tid=1 only |
| [R] Full P3b, Skia CPU **VST3** (`cpu-controls-vst3/`) | RESULT: PASS; tid=1 only; thread census: REAPER's threads only |
| [R] Tooltip steps, Skia GL3 VST3 (`gl3-tooltip-vst3/`) | RESULT: PASS; `t03-tooltip-again.png` shows "ISVGSlider H" |
| [R] Editor churn ×30, `EDIT=1`, Skia CPU CLAP (`cpu-churn-clap.log`) | PASS; fds 6→6, sockets 1→1, threads 14→14, RSS 103→119 MB |
| [R] `inspect-binary.sh` GL3 CLAP (`inspect-gl3-clap.txt`) | exports `clap_entry` only; NEEDED libX11 libm libc ld; no undefined `gl*`/`glX*`/`egl*`/`Fc*`/`FT_*`/`hb_*` (`undef-gl-fc.txt`; VST3: 3 exports). glibc 2.38 here because built on 24.04; the 22.04 package inspects record 2.35 |

Screenshots looked at: `gl3-controls-clap/r03-two-instances.png`, `cpu-controls-vst3/c09-popup-os.png`,
`gl3-tooltip-vst3/t03-tooltip-again.png` — full, correct UIs (not black), OS popup and tooltip drawn.

### Findings
| ID | Severity | Where | Description | Suggested fix |
|---|---|---|---|---|
| R1-1 | Minor | `Scripts/cmake/IGraphics.cmake:523` | The new default-renderer branch `elseif(UNIX AND NOT APPLE AND IGRAPHICS_BACKEND STREQUAL "SKIA")` lacks `NOT EMSCRIPTEN`. Emscripten's CMake platform sets `UNIX`, so a web configure with `IGRAPHICS_BACKEND=SKIA` now defaults to GL3 (`Skia::GL3`) instead of GL2 (which fell to the non-existent `Skia::Metal`). Web Skia via CMake does not work either way, but it is a change outside Linux, and every other Linux branch, including the new FATAL_ERROR check six lines below, excludes Emscripten. | Add `AND NOT EMSCRIPTEN`. |
| R1-2 | Minor | `IGraphics/Platforms/IGraphicsLinux.cpp:978-988` | (a) The CPU frame is put with `XPutImage` but not flushed. Xlib copies a request that fits into its output buffer (small dirty areas: a knob, a meter) and sends it only at the next flush, i.e. the next `XPending` in the following timer tick: up to one frame of extra latency on small updates (big ones go out at once). The tooltip path does `XFlush`; the GL path's swap flushes. (b) The two 135-char `SkIRect::MakeLTRB(floor…, ceil…)` lines narrate a "rect to covering pixel rect" conversion. | (a) `XFlush(w.mDisplay)` after `PutSkiaPixels`. (b) A small named helper (e.g. `CoveringPixels(const IRECT&, float scale)` returning `SkIRect`), or `IRECT::GetScaled` + `GetPixelAligned`, so the block reads itself and fits 120 columns. |
| R1-3 | Minor | `Dependencies/IGraphics/build-skia-linux.sh:43` | An existing `Dependencies/Build/src/skia` checkout is used as is, whatever its branch. The main checkout's tree, per the Log, comes from an older iPlug2 Skia release; a developer whose tree is not `chrome/m130` silently builds a different Skia (headers and libs stay consistent, but the result is not the documented, reproducible m130). | After the clone/if-exists, check `git -C "$SKIA_SRC_DIR" rev-parse --abbrev-ref HEAD` (or the commit) equals `chrome/m130` and stop with a clear message otherwise. |
| R1-4 | Info | `IGraphics/Platforms/IGraphicsLinux.cpp:792`, `IGraphics/Drawing/IGraphicsSkia.cpp:469` | When the host destroyed the parent before closing the editor, `ActivateGLContext` leaves no context current and `OnViewDestroyed` drops the `GrDirectContext`, whose destructor issues GL deletes without a current context. With libglvnd/Mesa these hit the no-op dispatch and the context is destroyed right after, so nothing leaks or crashes; NanoVG's `nvgDelete` is in the same position. Skia's documented way is `abandonContext()` when the context is gone. | Optional: in a Linux-only spot, abandon the Skia context when the make-current failed. Not required for approval. |
| R1-5 | Info | `IGraphics/Drawing/IGraphicsSkia.cpp:730`, `:388` | The comment says the per-instance provider exists because the typefaces read the cache's font data, "which lives as long as some instance does". In the last instance's destructor, `storage.Release()` (body) frees that data before the members `mLoadedFontFamilies`/`mFontCollection` are destroyed, so the typefaces briefly outlive their bytes. Harmless (destroying a FreeType memory face reads no font data; the global strike cache keeps such typefaces on mac/win too), but the comment's guarantee is slightly stronger than the code's. | Optional: reword the comment, or reset the two members before `Release()` in an `OS_LINUX` block. |

No Major or Medium finding. All acceptance items that the reviewer can check hold: Skia builds (both arches per the
Log; aarch64 libraries re-linked here), P3b CLAP/VST3 pass (re-run), APP builds, exports/NEEDED/glibc as recorded,
mac/win untouched (convincing proof for mac, reading for Windows), evidence in `evidence/t018/`, Log complete. The
Minor findings are worth fixing in the same branch but do not block.

Verdict round 1: APPROVED

## Round 2

Fix commit 36ca28de7 (`git diff 043c7187c 36ca28de7`), read in full.

| ID | Status | Check |
|---|---|---|
| R1-1 | Fixed | `IGraphics.cmake:523` now `UNIX AND NOT APPLE AND NOT EMSCRIPTEN AND … SKIA`. |
| R1-2 | Fixed | `XFlush` right after the CPU frame's `PutSkiaPixels`; `DevicePixelsCovering(const IRECT&, float)` names the floor/ceil conversion, and the call site reads itself within 120 columns. |
| R1-3 | Fixed | `build-skia-linux.sh` stops with a clear message when `rev-parse --abbrev-ref HEAD` of the checkout is not `chrome/m130` (a detached or other branch fails too). Checked by reading and `bash -n`; the local tree reports `chrome/m130`, so the success path is the implementer's re-run. My attempt to exercise the failure path in a throwaway container was not permitted; the logic is three lines under `set -e`. |
| R1-4 | Accepted as is | Info; the task Log records it (NanoVG is in the same position). |
| R1-5 | Fixed | `~IGraphicsSkia` drops `mFontCollection` and `mLoadedFontFamilies` before `storage.Release()`, inside `OS_LINUX && !IGRAPHICS_NO_SKIA_SKPARAGRAPH` (mac/win destructor unchanged); the comment now matches the code. |

Re-run [R] (Docker 24.04 aarch64, incremental rebuild of `build/t018rev-{gl3,cpu}` at 36ca28de7, container `t018rev`,
removed afterwards):
- [R] Full P3b, Skia CPU **CLAP** (`evidence/t018-review/r2-cpu-controls-clap/`): RESULT: PASS; Draw/timer/X events
  tid=1 only. Looked at `c06-textentry-igraphics.png`: complete UI, text entry open.
- [R] Editor churn ×25, `EDIT=1`, Skia GL3 **VST3** (`r2-gl3-churn-vst3.log`), which runs the changed destructor 50×:
  PASS; fds 6→6, sockets 1→1, threads 14→14 closed; RSS growth in line with round 1 / Mesa per-context.
- Implementer's re-runs present and green: `evidence/t018/skia-{gl3,cpu}-controls-{clap,vst3}/RESULT.txt` all PASS,
  `draw-threads.txt` all tid=1.

No new findings.

Verdict round 2: APPROVED
