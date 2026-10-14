# T-003 — Gneiss: Linux CMake build (CLAP, VST3, APP)

- **Status:** review-done (2026-10-14) — merges cleanly into hvoya-linux at 6b14ca1d9 (after T-002)
- **Owner:** Claude instance (T-003 session, 2026-10-14)
- **Branches/worktrees:**
  - Gneiss repo: `linux-experiment` in `/Volumes/Storage/code/hvoya/Gneiss-linux` (local only, never push).
  - iPlug2: `linux/T-003` from `hvoya-linux`, in `/Volumes/Storage/code/IPlug2-T-003`, and only if iPlug2 changes are needed.
- **Depends on:** nothing. `hvoya-linux` already has the Linux core (P0–P3b).
- **Plan phase:** P7 (PLAN.md).

## Goal

Gneiss builds on Linux as CLAP, VST3 and standalone APP from a CMake project, loads in REAPER (Docker), and shows its UI. This is the first time a real product runs on the port; expect to find port bugs.

## Context
- **Gneiss today.** It builds only via Xcode (`projects/Gneiss-macOS.xcodeproj`) and VS (`projects/*.vcxproj`), and uses NanoVG (Metal on mac, GL2 on Windows).
  - Pre-build steps: `scripts/prepare_resources-mac.py` / `-win.py` and `write_plug_build_info_to_file.bat`.
  - Resources: `resources/`, fonts (e.g. `RobotoMono-Regular.ttf`, see `config.h`), SVG/PNG images.
  - `config.h`: `PLUG_SHARED_RESOURCES 0`.
- **Shared utils** are in the iPlug2 repo under `hvoya/` (`hvoya/utils`, `hvoya/libs`: fmt, plog, ghc). Gneiss reaches them relative to IPlug2.
- **Mind the paths.** The Xcode/VS projects use relative paths to `../../../IPlug2`, the **main hvoya checkout**. For Linux, point CMake explicitly at the Linux tree (`IPLUG2_DIR=/Volumes/Storage/code/IPlug2-linux`, or your T-003 iPlug2 worktree). Never at `/Volumes/Storage/code/IPlug2`.
- **CMake API** (`Scripts/cmake/`, the `iplug_add_plugin`-style functions): see how the examples do it — `Examples/IPlugEffect/CMakeLists.txt` and `Examples/IPlugControls/CMakeLists.txt` on `hvoya-linux`.
- **Resources on Linux** go to a per-module folder `<Name>.resources/` (P1, notes/08) — not a shared `resources/`.
- **Known Gneiss specifics:**
  - `OnIdle` calls `GetUI()`/`SetLatency()` (main-thread only; P2 guarantees that in REAPER).
  - It uses platform popup menus, which on Linux become IGraphics stand-in menus (notes/13).
  - `DrawNow()` (implemented).
  - `RemoveAllControls()` from `OnIdle` on preset/morph change (notes/15 H2, fixed).
  - File dialogs are still stubs (cancelled) until T-002.

## Steps
1. Read `AGENTS.md`, PLAN §1–2, notes/08 (P0–P1 build decisions), and the example CMakeLists.
2. Write `Gneiss-linux/CMakeLists.txt`, plus `CMakePresets.json` if helpful:
   - sources from the Xcode/VS projects (keep the list in one place);
   - include paths for `hvoya/utils`, `hvoya/libs`;
   - defines matching the Windows build (`IGRAPHICS_NANOVG`, `IGRAPHICS_GL2`, `SAMPLE_TYPE_FLOAT`, release `PLOG_DISABLE_LOGGING`);
   - resources and fonts copied into the bundle/resources folder.
3. Port the pre-build steps: build-info header generation and resource preparation (a Python/CMake step that works on Linux).
4. Build in Docker on 24.04 (all three formats), and 22.04 for CLAP/VST3 (glibc floor). Check the exports (`scripts/inspect-binary.sh`).
5. Run clap-validator and the VST3 validator, then REAPER with `run-gui-test.sh` (no step file at first): the editor renders; look at the screenshot yourself. Then interact: knob drag, a menu, a preset switch (exercises `RemoveAllControls` from `OnIdle`), open/close ×5, two instances.
6. Check the host-loop trace build (`-DIPLUG_TRACE_HOST_LOOP`): `OnIdle` on the main thread for CLAP and VST3.
7. Any iPlug2-side fix goes in `linux/T-003`, following the shared-code rules in AGENTS §2. Anything that looks like a Gneiss bug: note it, and don't fix it in Gneiss beyond what the build needs; T-010 owns Gneiss-side changes.

## Acceptance
- [x] CMake configure+build of CLAP, VST3 and APP on 24.04; CLAP+VST3 on 22.04. Exports minimal; GLIBC floor recorded.
- [x] Validators: results recorded; failures explained or ticketed. `state-reproducibility` is known — see T-006.
- [x] Gneiss UI renders in REAPER (CLAP and VST3) and in the APP (`--no-io`). Screenshots reviewed by eye.
- [x] Interaction smoke test passes (above); no crash in 20× open/close; main-thread trace clean.
- [x] The Gneiss main checkout and `/Volumes/Storage/code/IPlug2` are untouched. Nothing pushed.
- [x] Self-review done (AGENTS §4.4); evidence in `linux-research/evidence/t003/`; Log below complete.

## Log
(append: date — what was done — commits — next)

**2026-10-14 — build, run, findings.** Evidence: `evidence/t003/` (`contact-t003.png` = the screenshots reviewed by eye).

Commits, iPlug2 `linux/T-003`:
- 85082f303 claim; `build-*.sh` tracked with the same diff as T-002's 1a249af60 (merges cleanly); `dock.sh DOCK_MOUNTS` for trees outside iPlug2 (Gneiss-linux + Gneiss `.git:ro`).
- fbe294d20 `hvoya/hvoya.cmake`: `hvoya_add_utils()` = all `hvoya/utils/*.cpp` (globbed like common-win.props), the include roots, generated `plug_build_info.hpp` (configure re-runs when the plug-in repo's HEAD moves). IPlugEffect uses it; Sand (T-012) should too.
- 4d08f1ef5 **port bug**: `IGraphicsLinux` tells the host on the first frame a size the editor took while `OpenWindow` kept the host out (`mInOpenWindow`). X11 hosts size the embed from get_size/getSize; Gneiss builds at 620 (morph on) while the host had `PLUG_HEIGHT` 200 → only the top 200 px showed. Mac hosts follow the NSView frame; Windows' `SetScreenScale` is not suppressed.
- bbbe05c86 **port bug**: APP `EditorResize` on Linux resizes the SWELL window (IGraphicsLinux sizes only its own X window): morph on/off never grew the standalone.
- 545d8a03a **port bug**: stand-in menus use the system sans-serif face (fontconfig) instead of `DEFAULT_TEXT`'s Roboto-Regular, which a plug-in need not load. Gneiss' context menus (MIDI CC) drew nothing in release and asserted in NanoVG in debug. Native menus on mac/win use the system font.
- b52ac6fc9 step files `scripts/steps/gneiss.txt` (REAPER) and `gneiss-app.txt`.

Commits, Gneiss `linux-experiment` (local): 52cb5d6 `CMakeLists.txt` (FORMATS APP VST3 CLAP; resources = what main.rc and the mac bundle ship: RobotoMono, MDI font, gneiss.svg; `-DHVOYA_IS_PAID=0/1`) + `GneissUI.cpp` boundsPad/headerH to file scope (GCC rejects a local in a lambda's default argument); 7e3a1c9 named `initializer_list`s for the filter-model/res-comp enum names (dangling temporary array: UB on every platform, clang/MSVC hide it; GCC crashed every instantiation).

Build: `DOCK_MOUNTS="/Volumes/Storage/code/hvoya/Gneiss-linux /Volumes/Storage/code/hvoya/Gneiss/.git:ro" DOCK_ENV="EXTRA_CMAKE_ARGS=-DIPLUG2_DIR=/Volumes/Storage/code/IPlug2-T-003" scripts/dock.sh -- scripts/build-cmake.sh /Volumes/Storage/code/hvoya/Gneiss-linux build/t003-gneiss Gneiss-clap Gneiss-vst3 Gneiss-app`. The task's `SAMPLE_TYPE_FLOAT` is iOS-only in Gneiss; mac/win build double, so CMake does too.

Results [R] = Docker, REAPER 7.82, llvmpipe:
- 24.04 CLAP/VST3/APP and 22.04 CLAP/VST3 build. Exports `clap_entry` / `GetPluginFactory ModuleEntry ModuleExit`; NEEDED libX11 + libc/libm; no GLIBCXX. **GLIBC floor 2.35 on 22.04** (examples: 2.34): `hypot`/`hypotf` got new symbol versions in glibc 2.35 and Gneiss calls them. 22.04 itself ships 2.35, so the floor platform is unaffected; pin with `.symver` only if pre-22.04 distros matter.
- UI in REAPER CLAP + VST3 and in the APP (`--no-io`): fonts, MDI icons, SVG logo, morph pad at 620; reopen ×5, 2 instances. Steps: knob drag, os cycle button, context menu (stand-in), preset `>` (host sees the new values in CLAP), morph off/on (RemoveAllControls + resize). PASS in CLAP, VST3, APP.
- Churn 20× CLAP/VST3 and 100× CLAP: PASS, fds/threads/sockets back, RSS flat after the first opens (same profile as IPlugControls in P3b).
- Trace build (`-DIPLUG_TRACE_HOST_LOOP`): `OnTimer`/`Gneiss::OnIdle` on REAPER's main thread in every phase, CLAP and VST3 (`hostloop-*`).
- Regression on this tree: IPlugControls full P3b script CLAP + VST3 PASS, APP 0 fails; IPlugEffect builds. macOS: IPlugEffect CLAP Debug builds (xcodebuild, scratch DerivedData/install paths); the mac **APP** target fails identically on plain `hvoya-linux` (IPlugAPP.cpp compiled as C++ includes Cocoa) — pre-existing, not from this task.
- Validators: VST3 537/537 when no race hits (see below). clap-validator 31 pass / 7 fail / 6 skip:
  - `state-reproducibility-{basic,binary,buffered}`: T-006.
  - `process-audio-double-{in,out}-of-place` "overwritten an input buffer": Gneiss processes in the host's input buffer at OS 1x (`work = input`, Gneiss.cpp `processChunk`). With float I/O iPlug passes its own scratch, so only 64-bit hosts see it — every platform. → T-010 / author.
  - `process-sleep-process-status`, `param-conversions` ('preFlt A hz' text round trip): plug-in/iPlug logic, no platform code; mac comparison pending (no clap-validator on the Mac yet; T-006 has the mac validator setup).

Findings that are NOT Linux (verified, nothing changed; for the author):
1. **`hvoya::AudioBuffer::reserve` leaves `_channels` on the freed allocation** (shared hvoya utils). Proven on macOS clang/libc++ with a 10-line test; ASan in the VST3 validator: heap-use-after-free in `Oversampler::upsample` writing `_osBuffer` (`validators/vst3-asan-audiobuffer-reserve-uaf.txt`). Gneiss: `_osBuffer` is default 2×512, `prepareAudioBuffers` reserves → stale; stereo with chunk×OS = 512 (host block 256 @2x, 128 @4x, 64 @8x) never calls `resize()`, so every block writes freed heap. Fix: rebind the channel pointers in `reserve` (as `resize` does). Affects every product using AudioBuffer::reserve.
2. **Morph toggle after a preset load needs 2 extra clicks** (CLAP and VST3; `morph-after-preset-trace.txt`): preset recall sets `par_morph_active`=1 (`kPresetRecall`), the controller stays inactive (height 200, MORPH lit, nav enabled); the next UI click sends 1 again and `isSettingNewVal` drops it. No platform code on that path. Mac repro to confirm: MORPH off → `>` → MORPH.
3. Gneiss morph-on disables knobs until a point is selected: no CC context menu then (by design, `ControlDecorator::CreateContextMenu`).
4. VST3: after `>` REAPER still reports the old parameter values (CLAP reports the new ones). iPlug VST3 preset-from-UI notification; not checked on mac.

Linux-only consequences, for T-010: `SetLatency` from `OnIdle` races the audio thread when the host gives no loop (validator: fallback thread) — ASan null read in `PassThroughBuffers`/`NChanDelayLine` (`validators/vst3-asan-latency-race.txt`); in REAPER `OnIdle` is host-driven, so it does not occur there. Deferred `SetLatency` is already T-010's.

Open: the debug-only "load preset" box at the editor's bottom left (IGraphics debug draw, release clean). T-002 also edits `IGraphicsLinux.cpp` (different functions; expect a trivial merge).

**Self-review (fresh pass over `git diff hvoya-linux...HEAD` and the Gneiss diff, against AGENTS §2 and the list above):** accepted. Linux code only in Linux files, the `IPlugAPP.cpp` hunk under `OS_LINUX` (mac IPlugEffect CLAP builds; the mac APP failure is pre-existing on plain hvoya-linux); the opened size reaches the host outside its open call and `mOpen` is re-checked after it; the menu font flag follows the GL context. Main checkouts untouched (Gneiss master holds the author's own edits from 2026-10-09 and earlier). Nothing pushed.

Next (coordinator / author): merge `linux/T-003`; decide on the cross-platform findings above (AudioBuffer::reserve first: it corrupts the heap on mac/win too); T-009 can start; T-010 takes deferred SetLatency, the input-buffer write, and the morph-after-preset desync once confirmed on mac. The Gneiss commits stay on local `linux-experiment` (52cb5d6, 7e3a1c9).
