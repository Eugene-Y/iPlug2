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
