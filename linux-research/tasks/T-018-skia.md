# T-018 — Skia backend on Linux

- **Status:** in progress
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
- [ ] Skia builds for both arches reproducibly; IPlugControls Skia CLAP/VST3/APP pass the P3b scripts.
- [ ] Exports minimal, NEEDED unchanged, glibc floor recorded.
- [ ] mac/win untouched; independent review (AGENTS §4); evidence in `evidence/t018/`; Log complete.

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

