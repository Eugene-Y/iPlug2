# Linux port of hvoya iPlug2 — START HERE

Working plan for continuing the port from a cold session. Written 2026-10-11.

## Progress

**P3a done (2026-10-11)**: host-driven X11 editor window, NanoVG on GL2, ec828eacc..HEAD. Details: `notes/11-p3a-window.md`. Evidence: `evidence/p3a/`. Review (accepted, 2 rounds): `notes/12-review-p3a.md`, `evidence/p3a-review/`.

| Commit | What |
|---|---|
| ec828eacc | `IGraphics/Platforms/IGraphicsLinux.{h,cpp}` (pimpl, own Display, child of the host window, `_XEMBED_INFO`, GLX context per editor, make-current/restore, swap interval 0, X fd + frame timer on `GetHostLoop()`, drain in both, draw only when dirty, `DrawNow`, `mInOpenWindow`, resize, expose/map, mouse, file/memory fonts), `IGraphicsLinuxGLX.h` (libGL `dlopen`, scoped X error trap). CMake: GL2 default on Linux, no libGL link |
| 69ffcd184 | APP: the editor's window goes below SWELL's menu bar |
| 362ef9f13 | External resize the editor does not take: the window is put back to the editor's size |
| d6ce31016, 75e265d91, 362ef9f13 | Harness: `expect-change` step, P3a step files, main-thread check of editor work, editor churn test, X fd probe, external-resize probe, contact sheets, Linux/mac comparison |

**Results** [R] = Docker, REAPER 7.82, llvmpipe

- IPlugControls and IPlugEffect × CLAP/VST3: render on first open, after reopen ×5 and for a 2nd instance; knob drag, wheel, XY pad, keyboard, IGraphics popups change the screen; plug-in resize (IPlugEffect corner resizer) grows REAPER's window; every `Draw` / X-event hit on REAPER's main thread. APP IPlugControls renders and reacts, main thread.
- 100× open/close of 2 instances (every 5th close before the new window's first timer): fds, sockets, threads equal before/after, RSS flat, host registrations back to 0.
- Validators identical to P2. NEEDED: `libX11.so.6` + libc/libm only. 22.04 builds green (GLIBC 2.34). macOS IPlugEffect CLAP Debug builds.
- Visual: same layout and colours as macOS; text a little heavier at 1× (stb_truetype).
- REAPER polls plug-in fds once per tick with its own `ppoll`: CLAP (POLLIN) rarely finds input left after the frame timer's drain, VST3 (no event mask, so POLLIN|POLLOUT) fires every tick on the always-writable X socket. Our registration is correct (reviewer's mini host); input latency is bounded by the timer period in both formats. The VST3 factory IRunLoop delivers on the main thread (closes notes/10 #8 for REAPER).

**Deviations**

- GL2, not RR's GL3: FBOs work on GL2 in iPlug (`IGraphicsNanoVG.h` defines `NANOVG_FBO_VALID`); RR's llvmpipe patches to `nanovg_gl_utils.h` / `IGraphicsNanoVG.cpp` were not needed.
- libGL is `dlopen`ed, not linked.
- Host resize goes through the shared `OnParentWindowResize` (as CLAP `set_size` does), not RR's Linux-only Scale-mode branch; our own resize echoes are filtered by X request serial instead of RR's flags.
- The P3a step file is mouse-only. The original `steps/iplugcontrols.txt` runs (`fullsteps-controls-clap`), but its keyboard, OS-menu, OS-text-entry and dialog steps do nothing yet: P3b/P4.
- APP menu-bar offset done in `IPlugAPP_host.cpp` (Linux branch), not with RR's env var.

**Next:** P3b: keyboard + focus/XEmbed, in-place text entry, cursors (hide/lock during drags), clipboard, platform popup menus decision (Gneiss), fonts by name. Then P4.

---

**P2 done (2026-10-11)**: host-loop timer layer, 534be52df..HEAD. Details and findings: `notes/09-p2-host-loop.md`. Evidence: `evidence/p2/`.

| Commit | What |
|---|---|
| 534be52df | `IPlug/Linux/IPlugLinuxHostLoop.{h,cpp}` (per-instance timers + fd watches, deferred until a backend exists, migrated on swap; fallback thread with the running flag set before `pthread_create`), backends CLAP (`IPlugLinuxHostLoopCLAP.h`, installed in `init()`), VST3 (`IPlugLinuxHostLoopVST3.h`), GLib (`IPlugLinuxHostLoopGLib.h`, APP). `Timer_impl` on the instance loop. `OS_LINUX` hooks: `IPlugAPIBase` (`mHostLoop`, `GetHostLoop()`, `CreateTimer`), `IPlugCLAP` (`init`, `onTimer`/`onPosixFd`; destroy hook added in review), VST3 component/controller/processor (`initialize`/`terminate`), view (`attached`/`removed`), factory (`addHostContextCallback`, `DeinitModule`). IPlugEffect test hook `IPLUG_TEST_LATENCY_FROM_IDLE` |
| 5c406b13a | `Tests/LinuxHostLoop` (ASan/UBSan); `scripts/host-loop-test.sh`, `gdb-hits.sh`; trace counters |
| 393191673 | APP main loop blocks in `g_main_context_iteration` (10 ms wake for SWELL timers) instead of `Sleep(10)`; `scripts/app-host-loop-test.sh`; `IPLUG_TEST_VST3_FRAME_RUNLOOP_ONLY` |
| 16a1ea43b | VST3: an IRunLoop whose `registerTimer` fails is skipped (the SDK validator's does) |
| b987f53c2, later | evidence, notes/09, PLAN |
| 38cd38313, 7e9a2c653, 51189a81d, 5e436a788, d02b36cd0, 971acf2bd | Review round 1 (notes/10): CLAP loop stopped in a wrapped `clap_plugin.destroy` (fallback `OnIdle` on a half-destroyed plug-in); lock-free `IsHostDriven`; refused-timer warning + VST3 IRunLoop probe; `Timer::Create` start order; nits; notes/09 |

**Results** (all [R] in Docker, REAPER 7.82 headless; see notes/09 for the table)

- `OnTimer`/`OnIdle` on REAPER's main thread for IPlugEffect and IPlugControls × CLAP/VST3: never opened, open, closed again, 2 instances (2 timer objects), after 50× add/delete (threads, fds and live host registrations unchanged).
- Latency from `OnIdle` with the editor closed reaches REAPER (`pdc` toggles 0/256) from the main thread, CLAP and VST3.
- APP `--no-io`: `OnIdle` on the main thread (GLib backend).
- Validators identical to P0. Exports/NEEDED unchanged. 22.04 builds green. macOS IPlugEffect CLAP + VST3 Debug build.
- **Key risk answered for REAPER: VST3 `IRunLoop` comes through the factory host context (`setHostContext`), not `initialize()`'s context; closed-editor idle is host-driven.** Other hosts: probe with `-DIPLUG_TRACE_HOST_LOOP` (VM, P10).
- REAPER **ignores** a VST3 latency change reported from the fallback thread (forced with the frame-only test build). Gneiss must defer `SetLatency` when `!GetHostLoop().IsHostDriven()` (P9).

**Deviations**

- Backends are header-only and included only by their format's TU, so no per-format CMake source lists.
- VST3 prefers the factory, then the `initialize()` IRunLoop over the frame's, even while attached: one stable loop, no migration on every open/close. The frame's loop is used only when neither exists.
- The fallback warns once per process with `fprintf(stderr)`, not `DBGMSG` (which is compiled out in release builds).
- `Timer::Create()` without an instance (OSC, ReaperExt) runs on a fallback thread of its own.
- The APP's polling loop was replaced here, not in P8 (small; SWELL's modal loops still poll).
- VST3 distributed (`VST3P_API`) has the hooks but no Linux build target; untested.

**Next:** P3 (IGraphicsLinux, host-driven): register the X fd with `GetHostLoop().AddFd(...)` and the frame timer with `AddTimer` from `OpenWindow`; drain `XPending` in both. The VST3 view already picks the loop before `OpenWindow`.

**Open decisions for the user**

- RR 360d84011 `StringToValue` change: not applied; optional (`notes/08`, "Open decision").

---

**P0 and P1 done (2026-10-11)** on `hvoya-linux`, 61e272b3a..131518f90. Evidence: `evidence/p0-p1/`.

| Commit | What |
|---|---|
| 75fa19049 | RR 27b4a5517 CMake. Conflict `ReaperExt.cmake`: kept upstream's macOS export list, added RR's Linux branch (output name `reaper_<name>` like the other platforms) |
| 025764cd5 | RR 14418467f, partial: `OS_LINUX` guard, EXPORT/BUNDLE_ID, VoiceAllocator includes, WebView stub. Dropped the pthread `Timer_impl` (start race, `OnIdle` off-main), RR's IPlugPaths, the logger fix (hvoya has it), the `EditorResizeFromUI` change (shared resize code) |
| 161dc60f4 | RR dda961827, partial: VST3 X11 platform type, CLAP X11 api/parent, APP SWELL/GDK main loop. Dropped CLAP timer/posix-fd/`SetHostDriven`, `guiGetResizeHints` and the Scale aspect clamp (all platforms), the `IPLUG2_SCREEN_SCALE` setenv. macOS settings-folder code left as it was. `fork()`+`execlp` replaced by `IPlug/Linux/IPlugLinuxOpen.h` |
| 38fad33b9, 379852a71, e816a7ba5, 5f5ea9bcc, 10c75357a | RR deploy, pkg-config audio, `-fsigned-char` (clean) |
| b67ea64ae | RR 166c02d1b CMake only (static libstdc++/libgcc); its IGraphicsLinux fontconfig dlopen not taken |
| ef002e167, da7cd2493 | RR CLAP 3580ba00d (snap stepped values before display text) and 1e2dab793 (buffer off-by-one): hvoya lacked both. 360d84011 skipped: hvoya already loops the state write, and its `StringToValue` change alters text parsing in every format; left for a decision |
| 5e23641b8 | P1 paths: Stillwell's `IPlugLinuxPathUtils.h` + test (`Tests/LinuxPaths`, passes) in `IPlug/Linux/`. XDG data/config dirs. Resources: VST3 `Contents/Resources/`, CLAP and APP `<Name>.resources/` beside the module; CMake copy and deploy follow it |
| 6c7b09472 | GCC portability: `std::expf`/`fmodf`, `<cstdint>`, `SWELL_DLG_WS_DEFAULT_SCALING`, gio-2.0 for the APP |
| c57c4f908 | Stub `IGraphicsLinux` (no window, warns once) and stub `Timer_impl` (never fires, warns once); thin `OS_LINUX` hooks; `--no-undefined` |
| 29c6bea6f | IPlugEffect builds with CMake (hvoya utils, generated `plug_build_info.hpp`, C++20) |
| 3430dd8e8 | P1: CLAP `guiSetParent` validation; `PresetManager::openFolder()` via xdg-open |
| 702ed635d | Exports: hidden visibility + version script + `--exclude-libs,ALL` |
| 2f3944b05 | `format_compat.hpp`: `{fmt}` when `<format>` is missing (GCC 11 on 22.04) |
| 131518f90 | Examples: IPlugControls `OnUIClose` null guard (crashed REAPER on editor close), IPlugEffect `hvoya::format` |
| 2d022fa61, 028a14d21, 67eeacaf1, a8c0b32ec, 7e4875daa, b4ef31cdc | Review round 1 (`notes/08-review-p0-p1.md`): restored auto-deploy in `iplug_configure_target` (RR's move had dropped it for direct callers on all platforms); stub prompts call `completionHandler` as cancelled; `IPlugLinuxOpen.h` comment; docker README |

**Results**

- Build: IPlugEffect, IPlugControls, IPlugInstrument × CLAP/VST3/APP green on 24.04 (`cmake-24.04.log`). CLAP/VST3 also green on 22.04 (`cmake-22.04.log`).
- Exports: `.clap` → `clap_entry` only; `.so` → `GetPluginFactory`, `ModuleEntry`, `ModuleExit`. NEEDED: libc, libm, ld-linux only. No GLIBCXX refs.
- GLIBC floor: 2.38 when built on 24.04, **2.34 on 22.04** (`inspect-*.txt`).
- VST3 validator: Controls and Instrument 537/537. Effect 536/537: hvoya's IPlugEffect never names `par_gain_L/R` and the reserved params ("has no title"). That is an example bug on every platform, not Linux.
- clap-validator: the same failures as the hvoya trial. `state-reproducibility-{basic,binary,buffered}` ×3 on all three, plus Instrument `param-fuzz-bounds` (subnormal). RR's `layout-audio-ports-config` failure does not occur. The 22.04 builds give the same numbers.
- REAPER 7.82 headless (`reaper/`, `reaper-summary.txt`, steps `/dev/null`): all 6 plug-ins load (`TrackFX_AddByName` → 0). REAPER stays alive through open, close/reopen ×5 and a second instance. No LUA errors. Each run is still `RESULT: FAIL`, but only on the expected checks: blank editor ×3 and no Draw hits (stub window). Before 131518f90, IPlugControls crashed REAPER in `OnUIClose` on the first close.
- macOS: IPlugEffect CLAP Debug builds from this worktree (`macos/`). IPlugEffect APP fails in `IPlugAPP.cpp` (Cocoa imported into a C++ TU). That is pre-existing since hvoya cf2b2a1f4: Gneiss's project marks the file `objcpp`, the example's does not. The APP files touched here (`IPlugAPP_dialog/host/main.cpp`) compiled.

**Deviations**

- The resources layout was not left for P7. RR's shared `resources/` beside the `.clap` contradicts §1, and P1 took SW's lookup. So the CMake copy/deploy was switched to `<Name>.resources/` now.
- Linux files live in `IPlug/Linux/` (timer, paths, xdg-open helper), added via `IPlug.cmake`, not in `#elif` branches of the shared `.cpp`s.
- Platform-neutral fixes touched shared code: `std::exp`/`std::fmod`, `format_compat.hpp` (extra `!__has_include(<format>)` condition), two examples. macOS behaviour is unchanged.
- hvoya utils' `host_time_info.hpp` and `preset_manager.hpp` were edited in this repo (`hvoya/hvoya/utils`).

**Open issues**

- APP: the `--no-io` segfault without ALSA is still there (P8). The `Sleep(10)` main loop was replaced in P2.
- Each format build compiles every iPlug source again (9 targets ≈ 5 min on colima). Fine for now.
- `IPlugEffect` APP on macOS needs `IPlugAPP.cpp` set to Objective-C++ in its project (pre-existing).

Research summary for humans: [`../LINUX_PORT_RESEARCH.md`](../LINUX_PORT_RESEARCH.md) (Russian).
VM setup for visual tests: [`../LINUX_VM_SETUP.md`](../LINUX_VM_SETUP.md).

## 0. Cold start checklist

**Repo layout and remotes**

- Branch **`hvoya-linux`** (pushed to `gene`) is the Linux port branch.
  - It is cut from `hvoya` after upstream master (d54f69050) was merged into `hvoya` (merge 5d854712d, 2026-10-11).
  - Local worktree: `/Volumes/Storage/code/IPlug2-linux`. The main checkout `/Volumes/Storage/code/IPlug2` stays on `hvoya`, because Gneiss builds against it via relative paths.
- Reference branch **`linux-trial/rangerrick`** (local only, not pushed) is PR #1336 cherry-picked onto **pre-merge** hvoya. Use it to read RR code adapted to hvoya. Do NOT build the port on it. If it is gone, recreate it by cherry-picking RR's non-chore commits onto hvoya; the conflict resolutions are recorded in `evidence/hvoya-trial/cherry-pick.log`.
- Remotes to (re)add if missing:
  ```bash
  git remote add rangerrick https://github.com/RangerRick/iPlug2.git
  git remote add stillwell https://github.com/stillwellaudio/iPlug2.git
  git remote add tomtrnk https://github.com/TomTrnk/iPlug2bn.git
  git fetch rangerrick feature/linux-support && git fetch stillwell master
  ```
  Pinned heads at research time: RR `03d3c1af1`, Stillwell `1d04aa5ff`. Stillwell's Linux range is `80c430878..910303d27`; its PR #1 is `4c48eff6f`.
- `linux-research/` holds:
  - `notes/`: the research — 01 efforts, 02 best practices, 03 build/test, 04 architecture review + plan, 05 review of 03, 06 doc fact-check, 07 upstream merge analysis;
  - `docker/` and `scripts/`: the harness;
  - `evidence/`: logs, patches, contact sheets;
  - `cmake/stillwell/`: CMake driver for the Stillwell fork.
- Not in git (regenerate): `cache/` (SDKs, Skia, REAPER — `scripts/fetch-sdks.sh`), `build/`, `wt/` (throwaway worktrees), most screenshots.

**Docker (colima)**

`brew install colima docker docker-buildx` is already installed on this Mac; `~/.docker/config.json` has `cliPluginsExtraDirs`.

```bash
colima start --cpu 6 --memory 8 --disk 50 --vm-type vz --vz-rosetta --mount-type virtiofs --mount /Volumes/Storage/code:w --mount /private/tmp/claude-501:w --mount "$HOME:w"
```

Then follow `docker/README.md`:
1. Build the image: `docker build -t iplug2-linux:24.04 linux-research/docker`.
2. `scripts/fetch-sdks.sh`, then `scripts/install-sdks.sh <tree>`.
3. `scripts/build-cmake.sh <tree> <builddir> <targets>`.
4. `scripts/reaper-plugin-test.sh` / `run-gui-test.sh`. These run REAPER 7.82 aarch64 under Xvfb with xdotool steps, screenshots and gdb.

The harness fails on a blank editor, no repaint, a REAPER PID change, `LUA ERROR`, no draw hits, and fewer timer threads than instances. Use `GUI=<name>` for your own container.

**VM:** see `LINUX_VM_SETUP.md`. SSH key: `~/.ssh/claude_linux_vm`. The user sends the IP and user name; then install build deps and REAPER aarch64 over SSH. Use `DISPLAY=:0`, `xdotool`, and `import -window root` for screenshots.

## 1. Decisions already made (do not re-litigate)

- The port branch comes from `hvoya` (many product fixes), not upstream.
- v1 scope:
  - CLAP + VST3 + standalone APP;
  - IGraphics + NanoVG on GL (GL2 like Windows, or GL3 like RR — pick in P3);
  - X11 only (XWayland covers Wayland desktops).
- Out of scope for v1: Skia, native Wayland, LV2 (Ardour loads VST3), WebView.
- **Threading contract — the decisive requirement.** All X event processing, IGraphics drawing, `IPlugTimer` callbacks (→ `OnIdle`, `ISender` draining) and editor-delegate callbacks run on the **host main thread**:
  - CLAP: `timer-support`, plus `posix-fd-support` for the X fd.
  - VST3: `Linux::IRunLoop`, `registerTimer` + `registerEventHandler`.
  - APP: GLib/SWELL main loop.

  Gneiss `OnIdle()` calls `GetUI()` and `SetLatency()` ("message thread only": `Gneiss.cpp:1285-1294`). A plugin-owned worker thread is allowed **only** as a logged fallback when the host provides no loop; Gneiss must then defer `SetLatency`.
- **No GTK in the plugin process.** It clashes with the host's GTK ABI and crashes Ardour; nested `gtk_dialog_run` caused the RR text-entry crash and the hangs.
  - Dialogs: xdg-desktop-portal (libdbus via dlopen), else zenity/kdialog through `posix_spawn` with the pipe fd on the host loop. Never block the host thread.
  - Text entry: in-place, like mac/win. `CreatePlatformTextEntry` must return immediately and commit later, because `IGraphics::CreateTextEntry` dereferences `mInTextEntry` after it returns (`IGraphics.cpp:~1977`).
- GL: one context per editor, make-current/restore around draws, **swap interval 0**; the host timer sets the pace. GLX now, EGL later.
- Binary hygiene:
  - `-fvisibility=hidden` + a version script exporting only `clap_entry`, or `GetPluginFactory`/`ModuleEntry`/`ModuleExit`;
  - `-Wl,--exclude-libs,ALL`, `-static-libstdc++ -static-libgcc`;
  - build on an old glibc (Ubuntu 22.04 container; RR on 22.04 gives a GLIBC floor of 2.34);
  - resources inside the bundle, or embedded — never a shared `resources/` next to the `.clap`.
- Keep Linux code in **new files** with thin `#ifdef OS_LINUX` hooks in shared code. Do not change shared resize code for all platforms the way RR does. That keeps future upstream syncs (and a possible upstream merge of #1336) tractable.

## 2. Known bugs to NOT import (from RR/Stillwell)

| Bug | Where | Note |
|---|---|---|
| Thread-start race: `mRunning=true` after `pthread_create` | RR `IGraphicsLinux::StartTimer`, `IPlugTimer.cpp` `Timer_impl` (cb8ec371c) | fix: `evidence/review/probes/racefix.patch`; moot once host-driven |
| OS text-entry null deref | RR GTK `CreatePlatformTextEntry` | design it async |
| Close-editor deadlock (lock → join render thread that waits on lock) | RR `IGraphicsEditorDelegate.cpp:52` vs `IGraphicsLinux.cpp:1801` | delete `GfxMutex` with the render thread |
| CLAP `onPosixFd` processes X events without the lock | RR `IPlugCLAP.cpp:1186-1190` | moot when host-driven only |
| CLAP attach starts the private render thread, then `SetHostDriven(true)` | RR | never start it |
| Redraw on every tick | RR | redraw only when dirty |
| Logger `snprintf` FORTIFY abort | fixed in hvoya 46f834a92 | — |
| Stillwell `OnUIClose` before the UI exists | SW editor delegate | — |
| GTK menus/dialogs from plugin | RR | — |

## 3. Work plan

Each phase ends with a commit on `hvoya-linux` and a harness run. Day estimates come from code reading (notes/04 + 06), total ≈ 31–49 dev-days. Treat P0–P2 as validation of the estimate.

### P0. Branch hygiene and build skeleton (0.5–1 d)

1. Bring RR's **build/format scaffolding** onto `hvoya-linux` by cherry-pick, not merge:
   - `27b4a5517` (CMake), `14418467f` (IPlug core), `dda961827` (APP/VST3/CLAP wiring);
   - `4c4354f2e`, `deb25d88b`, `cf407a8bb` (deploy — revisit the resources layout in P7);
   - `55fcbf686` (pkg-config audio), `166c02d1b` (dlopen fontconfig, static C++), `27706feec` (`-fsigned-char`).

   Skip:
   - CLAP fixes `360d84011`, `3580ba00d`, `1e2dab793` — compare with hvoya's CLAP fixes first; hvoya already loops the state write;
   - `3231b87e9` (GfxMutex), the GTK commits `2cc5e6f0a` and `9afbb1524` (take only X11 bits by hand in P3), and chores.

   notes/07 says the merged tree gives ~8 conflicting files (`IPlugPlatform.h` + CMake), all mechanical.
2. Make `IPlugEffect` CLAP/VST3/APP build in Docker with a **stub** `IGraphicsLinux` (no window), so the build matrix is green before UI work.
3. Acceptance: `build-cmake.sh` green for IPlugEffect + IPlugControls; the VST3 validator passes; clap-validator runs. The UI test is expected to fail.

### P1. IPlug core Linux (1.5–2 d)

- `IPlugPlatform`, `IPlugPaths`: take RR's or SW's `IPlugLinuxPathUtils.h` + tests; XDG dirs for presets/settings.
- VST3 `linuxmain.cpp`, `isPlatformTypeSupported(kPlatformTypeX11EmbedWindowID)`.
- CLAP `guiIsApiSupported(CLAP_WINDOW_API_X11)` and `guiSetParent`.
- hvoya utils `preset_manager.hpp` `openFolder()` → `xdg-open` via `posix_spawn`.

### P2. Host-loop timer layer (4–6 d) — the core

- New `IPlug/Linux/IPlugLinuxHostLoop.{h,cpp}`: a per-instance interface `{ addTimer(ms, cb), removeTimer, addFd(fd, cb), removeFd }`. Implementations:
  - **CLAP:** `clap_host_timer_support` / `clap_host_posix_fd_support`. Register in `init()`, not the constructor (as SW does at `IPlugCLAP.cpp:155-163`). Dispatch from `on_timer` / `on_fd`.
  - **VST3:**
    - `Linux::IRunLoop`, obtained from `IPlugFrame` when the view attaches;
    - for closed-editor idle, also from the factory host context (`IPluginFactory3::setHostContext`, as JUCE does at `juce_audio_plugin_client_VST3.cpp:3994-4000`).
    - Reference: SW `VST3RunLoopTimer` (`IPlugVST3_RunLoop.h`, `IPlugVST3_View.h:415-449`) and old upstream `IPlugVST3_RunLoop.cpp:34-165` (`upstrem/linux`, daf40c93b).
    - Watch for hosts that call back for an already-unregistered handler: use refcounted handlers, like SW.
  - **APP:** a GLib `g_timeout_add` / `g_unix_fd_add` on SWELL's GTK loop.
  - **Fallback:** a worker thread, with `DBGMSG` warning. Gneiss must handle this.
- A Linux `Timer_impl` (`IPlugTimer`) on top of the host loop, so `OnIdle` and `ISender` run on the host thread.
- **Probe early** with a trace build (like SW's `IPLUG_TRACE_VST3_RUNLOOP`): which hosts give `IRunLoop` with the editor closed? REAPER in Docker now; Bitwig, Ardour and Carla in the VM / on a PC later.
- Acceptance:
  - harness thread check: Draw, `OnTimer` and `OnIdle` all on the host main thread, for VST3, CLAP and APP;
  - 2 instances → 2 timers;
  - latency change with the editor **closed** reaches the host from the main thread.

### P3. IGraphicsLinux, host-driven only (6–9 d)

- Base: SW's `IGraphicsLinux` structure — pimpl, XIM, pointer grab, close deferral during host popups. It is **Skia-only**, so port the drawing glue to NanoVG/GL.
- Graft from RR (`2cc5e6f0a`, `5a8aed3aa`, `cb8ec371c`, read in `linux-trial/rangerrick` or `rangerrick/feature/linux-support`):
  - ConfigureNotify and host-resize logic;
  - double-click timing;
  - X11 clipboard (serve `SelectionRequest`);
  - Xcursor;
  - llvmpipe FBO fixes.
- Add:
  - X fd registration plus `XPending` drain in both the fd and timer callbacks;
  - GLX context per editor, swap interval 0 (`GLX_EXT_swap_control` / `GLX_MESA_swap_control`);
  - `DrawNow()` (hvoya API, called at `GneissUI.cpp:1180`);
  - `mInOpenWindow` set during open (hvoya, see `IGraphicsMac.mm:103-110`);
  - respect `IGEditorDelegate::mOpening`;
  - `_XEMBED_INFO`, plus `XSetInputFocus` on click and during text entry;
  - in-place text entry;
  - IGraphics popup menus as the default on Linux. Gneiss uses platform menus — consider `AttachPopupMenuControl` for Linux and check clipping at the window edge.
- Do NOT call `XInitThreads`; do not keep a render thread.
- Acceptance: the harness IPlugControls step script passes for VST3 and CLAP (render, knob drag, popups, text entry, reopen ×5, 2 instances), and 100× open/close without leaks (fd count, GL contexts).

### P4. Dialogs (4–6 d)

- `PromptForFile` / `PromptForDirectory`:
  - xdg-desktop-portal `org.freedesktop.portal.FileChooser` via dlopen'd libdbus; parent `x11:<hex XID>`; the response is async on the D-Bus fd registered on the host loop.
  - Fallback: zenity/kdialog through `posix_spawn`, with the pipe fd on the host loop. BN `IGraphicsLinux.cpp:428-776` on `upstrem/linux-bindernews` is a reference for argument building.
- `ShowMessageBox`: portal/zenity, or IGraphics-drawn. `PromptForColor`: zenity or IGraphics.
- The completion handler always fires on the host thread.
- Gneiss callers: `morph/morph_controller.hpp:2114,2126`, `morph/morph_file_router.hpp:115-146`, `hvoya/utils/preset_strip_control.hpp:421-446`, `user_tab_panel.cpp:841-851`.

### P5. HiDPI (2–3 d)

Scale sources, in priority order:
1. host-provided — CLAP `gui.set_scale`, VST3 `IPlugViewContentScaleSupport`;
2. `Xft.dpi`/96;
3. `GDK_SCALE`.

Report physical sizes. Check against hvoya's resize fixes.

### P6. NanoVG GL on Linux (1–2 d)

GL2 vs GL3 decision (made in P3a: GL2, notes/11), RR's FBO fixes for llvmpipe (not needed on GL2 in P3a), and a visual parity check of Gneiss against Metal (mac) and GL2 (win) screenshots.

### P7. Build and packaging + Gneiss CMake (3–4 d)

- Gneiss has only xcodeproj/vcxproj. Write `Gneiss/CMakeLists.txt` for Linux (CLAP, VST3, APP). Port the Xcode/py prebuild steps (`plug_build_info`, resources); `Examples/IPlugEffect/CMakeLists.txt` already shows both for the hvoya utils.
  - Where: the user allowed experiments on a **local-only** Gneiss branch `linux-experiment`, worktree `/Volumes/Storage/code/hvoya/Gneiss-linux` (from Gneiss master d8e95fd). Never push it, and never touch the main checkout `/Volumes/Storage/code/hvoya/Gneiss`. Point its Linux CMake build at `/Volumes/Storage/code/IPlug2-linux`.
- Done in P0: the version script, hidden visibility, `--exclude-libs,ALL` and static libstdc++ (`iplug_linux_restrict_exports` in `IPlug.cmake`). The 22.04 image already exists (`--build-arg UBUNTU=22.04`) and gives GLIBC 2.34.
- Bundles:
  - VST3: `Gneiss.vst3/Contents/{x86_64,aarch64}-linux/Gneiss.so` + `Contents/Resources`;
  - CLAP: a per-product `Gneiss.resources/` beside the `.clap` (done in P0/P1). Embedding is still an option.
- Optionally run an amd64 build via `--platform linux/amd64` (Rosetta).

### P8. Standalone (2–3 d)

RR's SWELL/GDK host + RtAudio (ALSA/JACK/Pulse; PipeWire through its shims), driven by the GLib loop. Fix the segfault when no audio device is present (RR needs `--no-io`). Drop the `IPLUG2_MENU_OFFSET` / `IPLUG2_SCREEN_SCALE` env hacks.

### P9. Gneiss-side work (2–4 d)

- `SetLatency` path when `OnIdle` falls back to a worker thread;
- `openFolder()` (P1);
- the menu strategy (P3);
- a visual QA pass.

### P10. Validation (5–8 d)

- Validators: clap-validator, VST3 validator, pluginval `--skip-gui-tests`.
- Hosts:
  - REAPER (Docker + VM);
  - Carla, Ardour (VST3), Renoise (VM, arm64 permitting);
  - Bitwig — x86_64 only, needs a PC.
- Cases:
  - multi-instance;
  - 100× open/close;
  - state load with the editor open and closed;
  - latency change with the editor closed;
  - Wayland session via XWayland (VM, manual).
- Known open item: clap-validator `state-reproducibility-*` fails on hvoya-based builds. Check whether the mac CLAP of Gneiss fails it too — shared code?

## 4. Risks to probe first

1. VST3 hosts without `IRunLoop` while the editor is closed → decides how much Gneiss fallback is needed. REAPER: has it (factory context, notes/09). Bitwig, Ardour, Carla: probe with `-DIPLUG_TRACE_HOST_LOOP`.
2. XEmbed keyboard focus per host (Bitwig, Carla).
3. Vsync/driver differences (NVIDIA proprietary vs Mesa); XWayland scaling.
4. Upstream merging #1336 later → keep Linux code isolated in new files.

## 5. Where the facts are

| Question | File |
|---|---|
| Who did what, links, PR comments | notes/01-existing-efforts.md |
| How JUCE/DPF/VSTGUI/pugl/Visage/baseview do it (with source paths) | notes/02-how-others-do-it.md |
| Build/test matrix of RR, SW, BN, hvoya trial | notes/03-build-test-results.md (corrected; notes/05 authoritative) |
| Thread model per candidate × format, conflict counts, detailed plan | notes/04-architecture-review.md |
| What the review refuted/confirmed, probes | notes/05-review-of-build-results.md, evidence/review/probes/ |
| Upstream merge contents/conflicts | notes/07-upstream-merge-analysis.md |
| P0–P1 review | notes/08-review-p0-p1.md |
| P2 host loop: design, REAPER IRunLoop finding, acceptance | notes/09-p2-host-loop.md |
| P2 review: findings, fixes, verdict | notes/10-review-p2.md |
| P3a editor window: design, GL2 choice, REAPER fd finding, acceptance | notes/11-p3a-window.md |
| P3a review: findings (destroyed-parent draw crash), fixes, verdict | notes/12-review-p3a.md |
| Patches used to build each candidate | evidence/*/patches/ |
