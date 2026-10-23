# T-018 — Skia backend on Linux

- **Status:** merged 214baae75 (review approved after 2 rounds, `notes/34-review-T-018.md`)
- **Owner:** T-018 worker (subagent of the coordinator chat f91df715), 2026-10-22
- **Branch/worktree:** `linux/T-018` from `hvoya-linux`, in `/Volumes/Storage/code/IPlug2-T-018`.
- **Depends on:** nothing. The GL-context part touches IGraphicsLinux — coordinate (T-019/T-009/T-020 use it too).
- **Conflicts:** `IGraphics/Platforms/IGraphicsLinux*` (context/surface setup), `IGraphics/Drawing/IGraphicsSkia*` (Linux paths only), `Dependencies/IGraphics` Skia fetch/build scripts, CMake.

## Goal
`IGRAPHICS_SKIA` (with GL, and CPU if mac/win offer it) works on Linux like on mac/win, for every format, so a product can choose Skia on all platforms.

## Context
- iPlug2 downloads prebuilt Skia for mac/win/iOS/web (`Dependencies/IGraphics/download-igraphics-libs.sh`, chrome/m130). There is no Linux prebuilt.
- We already built Skia m130 for arm64 Linux during the research (`linux-research/scripts/build-skia-linux.sh`, Ganesh GL/GLX + fontconfig; notes/03). Stillwell's fork is Skia-only on Linux (notes/03, notes/04): a reference for the GL glue, not to copy blindly (its threading is wrong).
- Binary hygiene: Skia/FreeType symbols must not leak (notes/03 saw ~5.3k exports with Skia: fix with the version script/`--exclude-libs`); static, no new NEEDED beyond what's dlopen'd; fontconfig/freetype: bundle statically or dlopen — decide and record.

## Steps
1. Build Skia for Linux x86_64 and aarch64 in our Docker images (22.04 for the glibc floor), with the same GN args as mac/win where they apply (+ GL/GLX, fontconfig or custom font manager). Make it reproducible: a script that the download script can use, or hosted prebuilt archives — propose to the coordinator where they live (the user decides about hosting).
2. CMake: `IGRAPHICS_BACKEND=SKIA` on Linux links it (as mac/win).
3. IGraphicsLinux: create the Skia GL surface on our GLX context (FBO 0, stencil, sample count), resize, scale; CPU raster path if mac/win support it (`IGRAPHICS_CPU`), presenting via `XPutImage`/XShm.
4. Fonts: Skia text with our fontconfig lookup and resource fonts, same faces as NanoVG.
5. Test IPlugControls (Skia build) with the full P3b scripts, visual check vs mac Skia shots (ask the coordinator), churn 100×, main thread; exports/NEEDED.

## Acceptance
- [x] Skia builds for both arches reproducibly; IPlugControls Skia CLAP/VST3/APP pass the P3b scripts.
- [x] Exports minimal, NEEDED unchanged, glibc floor recorded.
- [x] mac/win untouched; independent review (AGENTS §4); evidence in `evidence/t018/`; Log complete.

## Log
(append: date — what was done — commits — next)

### 2026-10-22 — implementation and Docker verification (worker; VM hung, Docker only)
**Skia libraries (step 1).** `Dependencies/IGraphics/build-skia-linux.sh` (new, next to `build-skia-mac.sh`; force-added
because the root `.gitignore` ignores `build-*`, as the mac/win scripts are): clones `chrome/m130` into
`Dependencies/Build/src/skia` if missing, `git-sync-deps`, `fetch-gn`, builds the mac library set for the machine's
arch into `Dependencies/Build/linux/<arch>/lib`. GN args = the mac ones, plus: `skia_use_x11=false` (no GLX in Skia:
`GrGLMakeNativeInterface_none`), `skia_use_fontconfig=false` + `skia_enable_fontmgr_custom_empty`, bundled FreeType,
no Vulkan/EGL/Graphite, `-fPIC`. Built in `iplug2-linux:22.04` (clang 14, glibc 2.35): aarch64 3.5 min, x86_64 21 min
under Rosetta (`iplug2-linux:22.04-amd64`). No Linux prebuilt exists in iPlug2's releases (`v1.0.0-beta` has
MAC/IOS/WIN only). **Hosting is the user's decision**: the libraries are ~52 MB per arch uncompressed; for now every
developer runs the script once per arch (source ~7 GB with all DEPS; only the GN args' deps are compiled).

**Dependency decisions (recorded).**
- libGL: stays dlopen'ed. Skia's GL interface is `GrGLMakeAssembledInterface` over `GLXFunctions::GetProc`, passed as
  `pContext` to `IGraphicsSkia::OnViewInitialized` (on mac it is the CAMetalLayer). No `glX*`/`gl*` undefined symbols.
- fontconfig: stays dlopen'ed (`IGraphicsLinuxFontconfig.h`). Skia gets every font as data from `LoadPlatformFont`
  (resources or the fontconfig lookup), through `SkFontMgr_New_Custom_Empty()` + its built-in FreeType; same faces as
  NanoVG. `makeFromData` gets the face index (ttc), Linux only. `DrawMultiLineText` (skparagraph) finds families only
  among loaded fonts: a per-instance `TypefaceFontProvider` registered under the font ID up to its first '-' (the name
  `DrawMultiLineText` asks for). No system fallback fonts for missing glyphs (NanoVG has none either).
- FreeType, ICU (with its data), HarfBuzz, expat, libpng/zlib: static inside the Skia libs, hidden by the existing
  version scripts + `--exclude-libs,ALL`.

**Code (steps 2–4).** b7137e3b0:
- CMake (`Scripts/cmake/IGraphics.cmake`, Linux branches only): Skia libs linked in a `--start-group`, Skia::GL3 gets
  glad_GL3, Linux default renderer for SKIA is GL3, SKIA with GL2/METAL on Linux is a configure error.
- `IGraphicsLinux.cpp`: Skia GL3 on the existing core 3.2 context (FBO 0, stencil 8, samples read by IGraphicsSkia's
  BeginFrame, as on mac/win); Skia CPU without GLX at all: 24-bit TrueColor visual, frames copied with `XPutImage`
  (union of the drawn rects only), so a CPU build opens without any GL driver. Tooltip for Skia drawn in software
  (raster SkSurface + XPutImage, word-wrapped like nvgTextBox), so it never touches the editor's GL context or Skia's GL
  state cache. The open now fails cleanly when the drawing backend cannot start (`GetDrawContext()` null) instead of
  crashing later; NanoVG gets that too.
- `IGraphicsSkia.cpp/.h`: only `OS_LINUX` blocks, plus `#include <mutex>` (the file uses `std::call_once`; GCC's headers
  don't bring it in transitively).
- XShm: not used. `XPutImage` of the dirty union was enough in every run here; XShm would need libXext (already
  dlopen'ed for the tooltip shape) and a shared-memory path that fails on remote X — left as a possible optimisation.
- GL2 + Skia: not offered, like mac (`#error` there). On a GL-2.1-only driver (the VM's virgl) Skia GL3 does not open
  (`no OpenGL 3.2 core context`), Skia CPU does.

**mac/win untouched (proof).** `evidence/t018/mac-proof/`: IGraphicsSkia.cpp preprocessed on macOS (Metal, CPU, GL3)
before (+ `<mutex>` after `<map>`) and after: the only differences are a forward declaration
`class TypefaceFontProvider;`, braces around `if (cached) return true;`, and `__FILE__`/`__LINE__` in asserts; the new
file passes `-fsyntax-only` in all three. CMake: every change is in a `UNIX AND NOT APPLE`/`UNIX AND NOT EMSCRIPTEN`
branch after the WIN32/APPLE ones. Windows: only `OS_LINUX` blocks in shared files; not built here.

**Verification (step 5), all Docker (REAPER 7.82, Xvfb, openbox, llvmpipe), evidence `evidence/t018/`.** Builds
`build/t018-{skia-gl3,skia-cpu,nvg}` (24.04 aarch64, Release), IPlugControls + IGraphicsTest CLAP/VST3/APP.
| Check | Skia GL3 | Skia CPU | NanoVG GL2 (regression) |
|---|---|---|---|
| Full P3b, CLAP / VST3 (`*-controls-{clap,vst3}`) | PASS / PASS | PASS / PASS | PASS / PASS |
| P3b APP steps (`*-controls-app`, no failed check) | PASS | PASS | — |
| Full P3b CLAP at REAPER UI scale 2 (`*-controls-clap-scale2`, 2600×1900 screen) | PASS | PASS | — |
| Tooltip steps (`iplugcontrols-tooltip.txt`, `sheet-tooltip-gl3-cpu-nvg.png`) | PASS | PASS | PASS |
| Editor churn ×100, `EDIT=1`, CLAP / VST3 (`*-churn-*`) | PASS / PASS | PASS / PASS | — |
| IGraphicsTest open/reopen/2 instances CLAP / VST3 | PASS / PASS | PASS / PASS | PASS (CLAP) |
| `Draw`, `OnDisplayTimer`, `ProcessXEvents` threads (gdb, every run) | tid 1 only | tid 1 only | tid 1 only |
- Visuals: contact sheets (`*/sheet.png`), IGraphicsTest page tour NanoVG/GL3/CPU side by side
  (`sheet-igraphicstest-pages-{1,2}.png`). Skia GL3 and CPU match each other; vs NanoVG only the renderer's own
  differences (multi-stop gradients, dashed MultiPath stroke, mask, ~1 px text baseline). **No mac Skia shots were
  compared**: none exist in the repo, and the main checkout's mac Skia prebuilt is an older release (one
  `libskunicode.a`); a mac Skia build of IPlugControls would be the next step if the coordinator wants that check.
- Skia adds no threads (thread census `*/threads.txt`); the CPU build has no llvmpipe threads at all.
- Churn RSS (closed, before → after 100 cycles × 2 instances): Skia GL3 103 → 343 MB (CLAP and VST3), NanoVG GL2
  103 → 300 MB (T-019's run, same script), Skia CPU 157 → 185 / 103 → 129 MB. The GL growth is mostly Mesa's per
  context (known, notes/03 V3); Skia GL adds ~0.2 MB per open on top. fds, sockets, threads equal before/after.
- **Binary hygiene** (`evidence/t018/package/*.inspect.txt`, `Scripts/linux/package.sh --variant
  skia:-DIGRAPHICS_BACKEND=SKIA`, 22.04-gcc13 images): CLAP exports `clap_entry` only, VST3 the 3 entry points,
  NEEDED `libX11 libm libc ld` = the NanoVG archive's, **glibc floor 2.35** on both arches; CLAP 25.5 MB vs ~4 MB.
- **x86_64** (Rosetta): `scripts/package-smoke.sh` on `iplug2-rt:ubuntu22.04-amd64`, Skia GL3 and Skia CPU archives:
  validators pass (clap-validator 0 failed, VST3 537/537), REAPER CLAP + VST3 PASS, standalone opens
  (`x86_64-package-smoke-{skia,skiacpu}/`, `sheet-x86_64-reaper-clap-gl3-cpu.png`). Its first run found a real bug:
  Skia asks the GL loader for `eglQueryString` and calls the stub glXGetProcAddress returns → crash at open on x86_64
  llvmpipe. Fixed 8fc1fad92 (the loader callback answers `egl*` with nullptr); P3b CLAP+VST3 re-run on the fixed GL3
  build: PASS. The smoke's remaining FAIL, "CLAP without resources crashed REAPER", is not Skia: NanoVG IPlugControls
  crashes too (below).
- gdb breakpoints don't fire under Rosetta, so the x86_64 thread check rests on aarch64 (same code).

### Cross-platform findings (report, not fixed)
- **Low — IGraphics::LoadBitmap dereferences a null bitmap when the image is missing** (`IGraphics/IGraphics.cpp:1814`,
  `pAPIBitmap->GetScale()` after an `assert` that release builds drop). Repro: IPlugControls (any backend) with its
  resources folder removed; the Skia build dies there, the NanoVG build a step later in `ISVGSliderControl::OnResize`
  (invalid SVG). mac/win: the same code; a broken install crashes instead of drawing without images.
- **Low — `IGraphicsSkia::LoadAPIFont` ignores the font's face index** (`makeFromData(data)` without `ttcIndex`): a
  `.ttc` system font found by name gets its first face on mac/win too. Fixed for Linux only (`#if defined OS_LINUX`);
  proposal: pass `data->GetFaceIdx()` everywhere (mac CoreText font data can carry a face index).

### Proposals for the user (not committed)
- Hosting of Linux Skia prebuilts (~52 MB per arch uncompressed): today `build-skia-linux.sh` per developer and arch.
- No shared-code change altered mac/win behaviour, so nothing else is held back.

Next: independent review (`notes/34-review-T-018.md`).

### 2026-10-23 — review round 1 (approved, 3 Minor + 2 Info) and fixes
- Round 1 (043c7187c): APPROVED; the reviewer re-ran builds, P3b GL3 CLAP + CPU VST3, tooltip, churn ×30, inspect.
- Fixed: R1-1 `NOT EMSCRIPTEN` on the Linux Skia default renderer; R1-2 CPU frames flushed after `XPutImage`, the
  device-pixel rectangle is a named helper (`DevicePixelsCovering`); R1-3 `build-skia-linux.sh` stops when the source
  checkout is not `chrome/m130`; R1-5 the Linux font provider and collection are dropped before the font cache is
  released (`~IGraphicsSkia`, `OS_LINUX` block), comment says so.
- Not changed: R1-4 (Info) a GrContext released with no GL context current when the host destroyed the window first:
  NanoVG does the same, harmless on Mesa/libglvnd.
- Re-verified after the fixes: `build-skia-linux.sh` re-run (no work, libs copied), P3b CLAP + VST3 for Skia GL3 and
  CPU: PASS ×4, `Draw`/timer/X events on tid 1 only, mac proof re-run against `hvoya-linux` (same three differences, `-fsyntax-only` passes).

### 2026-10-23 — review round 2: APPROVED, no findings (cc86be810)
**Final verdict: APPROVED after 2 rounds** (`notes/34-review-T-018.md`; round 1 043c7187c: 3 Minor + 2 Info, fixed in
36ca28de7 except R1-4 Info; round 2 cc86be810: no findings). Reviewer evidence: `evidence/t018-review/`.

**Hand-over.** Commits (`git log refs/heads/hvoya-linux..refs/heads/linux/T-018`): b7137e3b0 implementation,
8fc1fad92 x86_64 EGL-stub fix, 36ca28de7 review fixes; the rest evidence/Log/review. Evidence `evidence/t018/`.
Open for the coordinator / user:
- Hosting Linux Skia prebuilts, or keep the per-developer `build-skia-linux.sh` (needs ~7 GB of Skia source).
- Cross-platform findings above (LoadBitmap null deref on missing images; Skia face index on mac/win) — not fixed.
- Not checked here: mac Skia screenshots for a visual comparison; the VM (GL 2.1 virgl: Skia GL3 cannot open there by
  design, Skia CPU should); real GPU drivers. Worktree keeps `Dependencies/Build/src/skia` (7 GB, ignored) and the
  libraries in `Dependencies/Build/linux/{aarch64,x86_64}/lib`; copy them with `cp -cR` for later Skia work.

