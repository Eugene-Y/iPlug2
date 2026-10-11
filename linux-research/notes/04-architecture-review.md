# Linux port: architecture review of existing efforts (2026-10-10)

Scope: RangerRick PR #1336, stillwellaudio fork (`linux-vst3-current`, `linux-clap-current`, fork PR #1),
upstream `linux-bindernews` and `linux`, judged as **bases for a Linux branch cut from `hvoya`**.
[V] = read in source, with file:line at the reviewed ref. [I] = inference. [M] = measured with git.

Refs reviewed (worktrees under `linux-research/wt/`):

| Candidate | Ref / head | Base vs upstream | Own diff |
|---|---|---|---|
| RangerRick #1336 | `rangerrick/feature/linux-support` 03d3c1af1 (2026-06-16) | mb 584df5a33 (2026-06-12), 57 behind master | 25 commits, 48 files, +3561/−291 |
| Stillwell | `stillwell/stillwellaudio/master` 1d04aa5ff (2026-10-09); Linux = 80c430878..910303d27 (fork PR #1 = 4c48eff6f, merged in c73103a66) | mb d27861e33 (2025-11-16), 503 behind | Linux delta: 54 files, +4813/−134; whole fork: 75 files, +6581 (AAX/AU/Win/Mac/tooltips mixed in) |
| Bindernews | `upstrem/linux-bindernews` a6a8c6edf (2025-12-14) | 359 behind | 310 files (carries an old CMake/RtAudio import) |
| Old upstream | `upstrem/linux` daf40c93b (2022) | 1731 behind | xcbt, LV2, `IPlugVST3_RunLoop` |

## 1. Gneiss-specific requirements this review was measured against

These come from Gneiss and `hvoya`, not from generic Linux concerns, and they decide the outcome:

- **Gneiss `OnIdle()` is UI-thread code.** It calls `GetUI()`, drives controls, persists prefs, and calls
  `SetLatency()`, which the code comments mark as "message thread only"
  (`Gneiss/Gneiss.cpp:1285-1294`, `Gneiss.hpp:764-766`) [V]. So `IPlugAPIBase::OnTimer → OnIdle` **must**
  run on the host main thread, the same thread that handles IGraphics events and drawing. Any design
  that runs `OnIdle` on a plugin-owned thread breaks Gneiss in two ways: it races with the UI, and it
  calls `restartComponent` and `clap_host_latency` off the main thread.
- **Gneiss needs real file dialogs.** It calls `PromptForFile` and `PromptForDirectory` for presets,
  morph rigs and CC maps (`morph/morph_controller.hpp:2114,2126`, `morph/morph_file_router.hpp:115-146`,
  `hvoya/utils/preset_strip_control.hpp:421-446`, `user_tab_panel.cpp:841-851`). It also uses the
  clipboard and popup menus [V].
- **`hvoya` adds platform hooks that a Linux backend must honour:**
  - `IGraphics::DrawNow()`: Gneiss calls it at `GneissUI.cpp:1180`.
  - `IGraphics::mInOpenWindow`: suppresses host resize notifications during open (`IGraphics.cpp:83-115` on hvoya).
  - `IGEditorDelegate::mOpening`: swallows a re-entrant close (`IGraphicsEditorDelegate.cpp`, commit c8234d338).
  - CLAP fixes: looped state write, deferred latency (5ceb702de, eee9468ee).
  - VST3 fix: only send `restartComponent` when latency actually changed (3992bff17) [V].
- Gneiss builds with xcodeproj and vcxproj, not CMake (`Gneiss/projects/`) [V]. A Linux build means
  writing a new CMake project for Gneiss.

## 2. Threading and event-loop model (the decisive axis)

| | X event pump | IGraphics draw | `IPlugTimer` / `OnIdle` / queue drain | Editor-delegate calls | Contract "callback on creating thread" |
|---|---|---|---|---|---|
| **RR, VST3 & APP** | private pthread per editor (`IGraphicsLinux.cpp:1761-1789`, 16.7 ms `usleep`) | same pthread (`:1791-1822`) | **another** pthread per plugin (`IPlugTimer.cpp:146-189`) | host thread, serialized with a recursive `GfxMutex` (`IGraphicsEditorDelegate.cpp:17,36,52…`) | **broken** |
| **RR, CLAP** | host `posix-fd` (`IPlugCLAP.cpp:1186-1190`) | host `timer-support` (`:1179-1184`) | still the pthread `IPlugTimer` | host thread | **broken** for `OnIdle` |
| **Stillwell, VST3** (after PR #1) | host `IRunLoop` **timer only**, no fd (`IPlugVST3_View.h:415-449`) | host timer at editor FPS | host `IRunLoop` timer at `IDLE_TIMER_RATE` **while the editor is attached**; `LinuxTimerWorker` thread otherwise (`IPlugVST3_View.h:428-436`, `IPlugLinuxTimer.h`) | host thread | **partly**: correct with the editor open, broken with it closed |
| **Stillwell, CLAP & APP** | `LinuxTimerWorker` std::thread (`IGraphicsLinux.cpp:551-555`); `SetHostDriven` is compiled only for `VST3_API` (`IGraphicsLinux.h:45`) | worker thread | worker thread (`IPlugTimer.cpp` Linux, `IPlugCLAP.cpp:160-162`) | host thread, guarded by a recursive mutex inside `Impl` | **broken** |
| **Bindernews** | private `IPlugTaskThread` (10 ms) | same | same | n/a (no X11 in VST3/CLAP: the plugin formats don't support it yet) | broken |
| **old `linux` (xcbt)** | VST3 `IRunLoop::registerEventHandler(fd)` + `registerTimer` (`IPlugVST3_RunLoop.cpp:34-165`) | host | `IPlugVST3_RunLoop::CreateTimer` routes timers through the host loop | host | **kept** (VST3 only) |

Concrete defects found:

1. **RR, potential deadlock on editor close.** [V code path; the race window is small but real]
   - `IGEditorDelegate::CloseWindow` takes `GfxMutex` (`IGraphicsEditorDelegate.cpp:52`). It then calls
     `IGraphicsLinux::CloseWindow → StopTimer → pthread_join` (`IGraphicsLinux.cpp:643,1781-1788`).
   - Meanwhile the render thread may have passed its `mTimerRunning` check (`:1767`) and be blocked
     acquiring the same mutex in `OnDisplayTimer` (`:1801`).
   - The join then never returns. This affects VST3 and APP, where the private thread is used.
2. **RR CLAP: unlocked X/IGraphics access.**
   - `onPosixFd` calls `ProcessX11Events()` without `GfxMutex` (`IPlugCLAP.cpp:1186-1190`).
   - That dispatches mouse events into controls, while the `IPlugTimer` pthread concurrently runs
     `SendParameterValueFromDelegate` and `OnIdle` under the lock.
   - The mutex therefore protects only one side. It also never protects plugin code: Gneiss's `OnIdle`
     and `OnParamChangeUI` touch `GetUI()` directly.
3. **RR: nested GTK main loops run on the wrong thread.**
   - In VST3 and APP, `gtk_dialog_run` (`IGraphicsLinux.cpp:849,937,980,1014,1318`) and `gtk_main()`
     for popup menus (`:1252`) run on the private render thread while it **holds `GfxMutex`**.
   - Any host-thread call into the delegate blocks for the entire life of the dialog. This matches bsvi's
     report on #1336 that "GTK popups/file/color pickers hang".
   - GTK calls off the GTK main thread, with `gtk_init_check` executed inside the host process (`:344`),
     are unsafe in GTK3 hosts and fatal in GTK2 hosts (Ardour).
4. **RR: `XInitThreads()` is called in every `IGraphicsLinux` constructor** (`:445`), i.e. after the host
   has already used Xlib. libX11 ≥1.8 makes this a no-op, but on older libX11 it is UB.
   Stillwell wraps it in `call_once` (`IGraphicsLinux.cpp:358-359`), which still has the same problem.
   Both need it only because they touch X from their own threads.
5. **Stillwell: a host-thread model is only half implemented.**
   - The VST3 design is sound:
     - It uses refcounted `ITimerHandler`s, which are safe against unregister-in-callback
       (`IPlugVST3_RunLoop.h:22-126`).
     - It rejects callbacks arriving on the wrong thread (`:56`).
     - It defers `CloseWindow` while a synchronous host popup is on the stack (`IGraphicsLinux.cpp:566-585`).
   - But the closed-editor and CLAP paths fall back to a worker thread. For Gneiss, `SetLatency` from
     `OnIdle` with the editor closed would then fire from that worker. That is a VST3/CLAP threading
     violation; hvoya fixed exactly this class of bug on macOS/Windows.
   - The Linux draw path is welded to VST3:
     - `#if VST3_API` gates in the delegate (`IGraphicsEditorDelegate.h`, `mGraphics` becomes `shared_ptr` only on Linux VST3).
     - A `dynamic_cast<IPlugVST3*>` inside `IGraphics.cpp:1487-1502`.
6. **Neither sets a swap interval.**
   - `glXSwapBuffers` (RR `:1819`, SW `:1412`) uses the driver default, which is usually vsync on
     Mesa/NVIDIA [I].
   - In host-driven mode (RR CLAP, SW VST3) this can block the host main thread for up to one refresh
     period per open editor, per tick.

**Conclusion on threading.** No candidate meets Gneiss's requirement on all formats. The right model is
"all UI and idle work on the host main thread, from host-provided timers and fds", as in DPF, VSTGUI and
old upstream xcbt (see 02-how-others-do-it.md). That model has to be written. The pieces to build it
from exist:

- Stillwell's `VST3RunLoopTimer`.
- RR's CLAP `timer-support` and `posix-fd` registration.
- Stillwell's move of `CreateTimer()` from the constructor to `clap init()`. The host extensions are
  usable from `init()`, not from the constructor (`IPlugCLAP.cpp:155-163` SW).
- JUCE's trick of obtaining `IRunLoop` from `IPluginFactory3::setHostContext`, so idle work keeps running
  on the host thread while the editor is closed
  (`JUCE/.../juce_audio_plugin_client_VST3.cpp:3994-4000`) [V].

## 3. Window embedding, input, HiDPI, lifecycle

| Topic | RangerRick | Stillwell |
|---|---|---|
| Embedding | `XCreateWindow` child of the host window; validates the parent with `XGetWindowAttributes` and falls back to root (`:455-477`); WM hints only when top-level (`:585-599`) | child of the host window, with top-level fallback; also proxies Xdnd on the parent (`HandleXdnd*`, `RegisterXdndProxyWindows`) |
| XEmbed / focus | **none** (no `_XEMBED*`, no `XSetInputFocus`) [V] | **none**, but has XIM/XIC for UTF-8 text (`IGraphicsLinux.cpp:116-146,517,847-849`) [V] |
| Pointer | `XWarpPointer` hide/lock, blank cursor (`:745-801`) | `XGrabPointer` on button press, ungrab on release or before host menus (`:726-800,1230-1239`) |
| Host resize | detailed `ConfigureNotify` coalescing and oscillation guards (`:1520-1582`); rewrites `IGEditorDelegate::OnParentWindowResize`; disables `AttachCornerResizer` on Linux (`IGraphics.cpp` diff) | resize logic is mostly product-specific scale-rounding (`IGraphicsEditorDelegate.cpp` diff), much of it from the cross-platform resize fixes in PRs #6, #8, #9 |
| HiDPI | env vars only: `IPLUG2_SCREEN_SCALE`, `GDK_SCALE` (`:485-494`); upstream CLAP `guiSetScale` | CLAP `guiSetScale` (`IPlugCLAP.cpp:1065`) and VST3 `setContentScaleFactor`; no Xft.dpi |
| Close/reopen | `Display`, GL context, colormap, cursor all freed (`:638-672`) | the same, plus XIM/IC and Xdnd property restore (`:577-653`); deferred close in host callbacks |
| Multi-instance | one `Display` and one thread per editor, plus one thread per plugin instance (IPlugTimer) | one `Display` per editor; one worker thread per instance (CLAP), or none (VST3 while the editor is open) |
| Popup / text entry | GTK menu and GTK entry dialog (`:1186-1330`) | IGraphics-drawn (`AttachPopupMenuControl` and `AttachTextEntryControl` forced in `OpenWindow`, `:406-407`); VST3 host context menu on right-click |
| File, message, colour dialogs | GTK3 via `dlopen`, blocking | **stubs**: PromptForFile returns empty (`:1495-1528`), message box auto-answers (`:1441-1453`) |
| Clipboard | owner + reader, 500 ms poll, no INCR | owner + reader, 100 ms poll, no INCR (`:1312-1395`) |
| OpenURL / reveal | `xdg-open` | `posix_spawnp xdg-open` on a detached reaper thread (`:1455-1475`) |

For Gneiss, IGraphics-drawn menus and text entry are acceptable; it already uses IGraphics controls
heavily. Popup menus drawn by IGraphics are clipped to the editor bounds [I], so long menus need checking.
Missing XEmbed focus handling means text entry may get no key events in some hosts [I]; see the testing
item in §7.

## 4. OpenGL

Both use legacy GLX 1.2 (`glXChooseVisual` + `glXCreateContext`; RR `:1408-1437`, SW `:416,522`), glad
loaders, one context per editor, and make-current/release around each draw. There is no EGL, no FBConfig,
and no swap control.

RR adds two llvmpipe/Xwayland fixes that are worth keeping:

- The `GL_DEPTH_STENCIL_ATTACHMENT` fix in `nanovg_gl_utils.h`.
- A fallback that draws without the FBO (`IGraphicsNanoVG.cpp` diff).

RR also states GL3 is required on Linux, because `NANOVG_FBO_VALID` is not defined under GL2
(`IGraphics.cmake` comment). Gneiss uses GL2 on Windows, so the Linux build would use GL3 and needs a
quick visual check.

Stillwell's IGraphicsLinux has no NanoVG-specific code, because their product is Skia/GLX
(`IGraphicsSkia.cpp` Linux branch). The NanoVG path there is less exercised [I].

## 5. Build and packaging

| | RangerRick | Stillwell |
|---|---|---|
| CMake | extends upstream `Scripts/cmake/*` (PR #1300 layout): VST3 bundle `Contents/x86_64-linux`, `.clap`, `linuxmain.cpp`, resource copy into the bundle, APP with RtAudio ALSA/JACK/Pulse via pkg-config, presets `linux-ninja` and `linux-make`, plus a CI job | **none in the tree**: base predates `Scripts/cmake`, product built by out-of-tree scripts; only test CMakeLists (`Tests/LinuxVST3`, `Tests/LinuxPaths`, `Tests/CLAPAdapter`) |
| Symbols | `-static-libstdc++ -static-libgcc`; **no** `-fvisibility=hidden`, no version script, no `--exclude-libs` [V grep] | n/a |
| glibc floor | CI on `ubuntu-latest` (24.04, glibc 2.39) [V `cmake-ci.yml:743`]: too new for distribution | builds on older Debian (from 01 notes) |
| Deps | X11, GL; fontconfig and GTK3 via `dlopen`; `-include stdlib.h`, `-fsigned-char` | X11, GL, `posix_spawn`; Skia+fontconfig |

## 6. Code quality, intrusiveness, tests

**RangerRick**
- Self-contained `IGraphicsLinux.cpp` plus small hooks.
- Intrusive bits:
  - Linux-only `GfxMutex` in `IGEditorDelegate`.
  - New virtuals `OnBeginHostResize` and `OnEndHostResize` in `IGraphics.h`.
  - `AttachCornerResizer` disabled on Linux.
  - `EditorResizeFromUI` changed for all platforms (`IPlugAPIBase.cpp`).
  - `IGraphics_select.h` replaces the generic `#else <OpenGL/gl.h>` fallback with a Linux glad
    include. The macOS branches are untouched.
- Noise: an edited `CLAUDE.md`, `.llmenv.yaml`, whitespace churn in `IPlugVST3_View.h`.
- No unit tests; CI only builds.

**Stillwell**
- Better engineered core: pimpl, explicit lifecycle and re-entrancy handling, and unit tests for the
  run-loop, menu-host, timer, paths, input and text-entry behaviour, plus a CLAP adapter test harness.
- But it is a **product fork**:
  - The Linux commits are interleaved with transactional-state, bypass, tooltip, AAX and resize-scale
    work.
  - IGraphics core is coupled to VST3 (`IGraphics.cpp:20-25,1487-1569`).
  - Diagnostic tracing via `IPLUG_TRACE_VST3_RUNLOOP` is left in.
  - It is 503 commits behind upstream.
- Both forks show heavy AI-assisted authorship [I]. Stillwell's has more tests behind it; RR's has more
  in-tree build integration.

## 7. Cost of landing each candidate on `hvoya` (primary axis)

hvoya diverged at f98027a70 (2026-03-23). It is 242 commits behind master and has 207 own commits. Of
those, 23 files are in IGraphics/IPlug; the rest are `hvoya/` utils and the example. The hvoya-side
numbers below come from `git merge-tree --write-tree`, run against throwaway worktrees.

| Operation | Conflicted files | Notes |
|---|---|---|
| upstream master → hvoya | **1** (`IPlugAPP_main.cpp`, one hunk: the commented-out screenshot shortcut) | the CMake sync is effectively free |
| RR (full, natural base) → hvoya | **4** files / 5 hunks: `IGraphicsEditorDelegate.cpp/.h` (mOpening vs GfxMutex), `IPlugCLAP.cpp` ×2 (the **same** state-write loop fixed by both), `IPlugAPP_main.cpp` | trivial textually |
| RR own diff only (`--merge-base` = RR's base) → hvoya | 11 (7 are `Scripts/cmake/*`, plus `CMakePresets.json`) | the CMake conflicts come from upstream Mar→Jun CMake changes, not from hvoya; they vanish if master is merged first |
| Stillwell Linux delta (44d9eef19..910303d27) → hvoya | **6** files / 8 hunks: `ITextEntryControl.cpp`, `IGraphicsEditorDelegate.cpp`, `IPlugCLAP.cpp` ×3 (deferred-latency logic written independently by both), `IPlugParameter.cpp/.h` (hvoya param API vs SW inverse-display parsing), `IPlugPaths.h` | it also brings SW's CLAP rewrite, which overlaps hvoya's CLAP fixes semantically |
| Stillwell whole fork → hvoya | 8 (adds `IGraphicsMac.mm`, `IPlugPluginBase.cpp`) | not recommended: product changes |
| Bindernews → hvoya | 71 | not viable |

Semantic interactions with hvoya that would not show up as textual conflicts:

- **`IGEditorDelegate::OpenWindow` and `CloseWindow`.**
  - hvoya's `mOpening` guard swallows closes during open.
  - RR wraps both in `GfxMutex`.
  - SW's `IGraphicsLinux::OpenWindow` calls `CloseWindow()` itself first (`:400`) and defers closes
    during host callbacks.
  - With RR's lock, a host close that re-enters during open is serialized but then swallowed, which is
    fine. With SW, an internal `CloseWindow` in `OpenWindow` runs **below** the delegate guard. That is
    harmless (its own object), but must be reviewed against `mInOpenWindow`.
- **`IGraphics::mInOpenWindow`.** hvoya sets it only in IGraphicsMac (`IGraphicsMac.mm:103-110`). The
  Linux `OpenWindow` must set it as well. Neither candidate does, because the flag postdates them.
  Without it, REAPER-style re-entrant teardown during open can recur on Linux [I].
- **`DrawNow()`.**
  - Gneiss calls it. Neither candidate implements it, so it falls back to the base-class no-op and
    resize glitches return.
  - With RR's render thread, a host-thread `DrawNow` would race GL; it would need the mutex and a
    cross-thread make-current.
  - With a host-driven design it is trivial: make current, draw, swap.
- **CLAP latency.**
  - hvoya defers `latencyChanged` to `activate()` and calls `requestRestart` via `runOnMainThread`.
  - Stillwell independently did the same (972cffa97). Keep hvoya's version; drop SW's.
  - With SW's or RR's worker-thread `OnIdle`, Gneiss's `SetLatency → runOnMainThread` would at least be
    marshalled on CLAP, but on VST3 `restartComponent` would be called from the worker [I].
- **`ISender`, `IPlugTimer`.** hvoya does not modify them [V], so a Linux `Timer_impl` can be added
  without conflict. But every `ISender::TransmitData` and every Gneiss `OnIdle` runs on whatever thread
  that Timer chooses (see §2).
- **`IGraphics.h`.** RR adds virtuals next to hvoya's `DrawNow` and `BringControlToFront`. These merge
  cleanly, but the `IGraphics` vtable changes, which matters if any binary-compat expectation exists.
  It doesn't here.

## 8. Recommendation: a hvoya-based plan

**Decision: write our own Linux layer on a branch cut from `hvoya`.** Take RR's scaffolding (build,
formats, X11 utilities) and Stillwell's host-run-loop and lifecycle pieces. **Rewrite the event-loop and
threading core.**

Adopting either branch wholesale fails Gneiss's `OnIdle`-on-the-main-thread requirement:

- RR: on every format.
- Stillwell: on CLAP, on APP, and on closed-editor VST3.

Both would have to be gutted in exactly the place that matters.

| # | Work item | Source | Est. (dev-days) |
|---|---|---|---|
| 0 | Branch `linux` from hvoya; merge `upstrem/master` (1 conflict) **or** hand-port only the CMake files. Merging is cheaper but pulls 242 upstream commits into the product, so the author decides. | — | 0.5–1 (merge) / 1–2 (port) |
| 1 | IPlug core Linux: `IPlugPlatform`, `IPlugPaths` (take SW `IPlugLinuxPathUtils.h` + tests, or RR's version), VST3 `linuxmain.cpp` and X11 platform type, CLAP `CLAP_WINDOW_API_X11` and `guiSetParent` validation | RR `dda961827`, `14418467f`; SW paths | 1.5–2 |
| 2 | **Host-loop timer layer.** A Linux `Timer_impl` backed by a per-instance host loop: CLAP `timer-support` registered in `init()` (move `CreateTimer` out of the constructor, as SW `IPlugCLAP.cpp:155-163`); VST3 `IRunLoop` taken from **`setHostContext` on a custom factory** (as JUCE does) and from `IPlugFrame`; APP a GLib `g_timeout` on SWELL's GTK loop. Fallback worker thread only when no loop exists, with a logged warning. Delete RR's `GfxMutex` and SW's `Impl::mutex` and `XInitThreads`. | new; SW `VST3RunLoopTimer` (`IPlugVST3_RunLoop.h`) | 4–6 |
| 3 | **IGraphicsLinux core, host-driven only.** Base it on SW's file (pimpl, XIM, pointer grab, close deferral, Xdnd optional) and graft in RR's ConfigureNotify and host-resize logic, clipboard serving and cursor code. Add: X fd registration (CLAP `posix-fd`, VST3 `registerEventHandler`) with `XPending` drained in both fd and timer callbacks; swap interval 0 via `GLX_EXT/MESA_swap_control`; `DrawNow`; `mInOpenWindow`; `_XEMBED_INFO` plus `XSetInputFocus` on click and during text entry. Remove all `VST3_API` gates and the `IGraphics.cpp` `dynamic_cast`. | SW + RR | 6–9 |
| 4 | **Dialogs.** Async `PromptForFile`/`PromptForDirectory` via xdg-desktop-portal (libdbus `dlopen`); fallback zenity/kdialog through `posix_spawn`, with the pipe fd registered on the host loop. The host thread must never block, and the completion handler fires on the host thread. Message box via the same path; colour via zenity or IGraphics. **No in-process GTK.** | new (zenity argument building from BN `IGraphicsLinux.cpp:428-776` as reference) | 4–6 |
| 5 | HiDPI: CLAP `set_scale`, VST3 content scale, Xft.dpi fallback; Scale-mode host resize checked against hvoya's resize fixes | RR + new | 2–3 |
| 6 | NanoVG GL3 on Linux plus RR's llvmpipe FBO fixes; visual parity check of Gneiss GL3 vs GL2/Metal | RR | 1–2 |
| 7 | Build and packaging: Gneiss `CMakeLists.txt` for Linux (CLAP, VST3, APP), `-fvisibility=hidden` + version script + `--exclude-libs,ALL`, static libstdc++, build container on Ubuntu 22.04 / manylinux_2_28, VST3 bundle and resources layout | RR CMake + new | 3–4 |
| 8 | Standalone: RR's SWELL/GDK host + RtAudio (ALSA/JACK/Pulse), switched to GLib-loop driving; drop the env-var hacks `IPLUG2_MENU_OFFSET` and `IPLUG2_SCREEN_SCALE` where possible | RR | 2–3 |
| 9 | Validation and host matrix: clap-validator, pluginval, VST3 validator under `xvfb-run`; REAPER, Bitwig, Carla, Renoise (VST3+CLAP), Ardour (VST3 only); multi-instance, 100× open/close, state load with the editor open or closed, latency change with the editor closed | — | 5–8 |
| | **Total** | | **≈ 29–44 dev-days** (about 6–9 weeks for one developer) |

**Not taken:**

- RR's GTK dialogs, menus and text entry.
- RR's render thread and `GfxMutex`.
- SW's `LinuxTimerWorker` as the primary path.
- SW's product changes: transactional state, bypass, tooltip and AAX work.
- SW's CLAP adapter rewrite (hvoya already has its own CLAP fixes). Its CLAP **tests** are worth
  porting later.
- Bindernews' private task thread.
- LV2.

**Risks:**

1. **Hosts without a usable `IRunLoop` on the host context.** [I] This is unknown per host and must be
   probed early with SW's `IPLUG_TRACE_VST3_RUNLOOP`-style tracing. In those hosts, closed-editor idle
   falls back to a worker thread. Gneiss's `OnIdle` must then guard `SetLatency` and UI access
   (`GetUI()==nullptr` is already the closed case). The latency report needs a main-thread path, or must
   be deferred to the next host-thread callback.
2. **XEmbed focus.** Host implementations vary, and text entry may fail in some (Bitwig, Carla) [I].
3. **Vsync and driver behaviour** on NVIDIA proprietary drivers vs Mesa; Xwayland scaling.
4. **Upstream may merge #1336 later.** If it does, a future hvoya↔upstream sync would conflict in
   `IGraphicsLinux.*`, `IPlugTimer` and the CLAP and VST3 Linux paths. To mitigate, keep Linux code in
   new files with thin `#ifdef OS_LINUX` hooks, and do not modify shared resize code for all platforms
   the way RR does.
5. **Effort.** The estimate assumes no Skia, no Wayland-native and no LV2. Ardour users get VST3 only.

## 9. Verified vs inferred summary

- **Verified in code:**
  - All threading paths in §2 and the RR close-deadlock path.
  - The RR unlocked `onPosixFd`.
  - The GTK nested loops on the render thread.
  - The SW CLAP and APP worker-thread driving and the SW VST3 run-loop design.
  - No XEmbed and no swap interval in either.
  - Visibility flags missing in RR.
  - All conflict counts (`git merge-tree` and `git apply -3` in `wt/review-merge`).
  - Gneiss's `OnIdle`, `DrawNow` and `PromptForFile` usage.
  - JUCE's `setHostContext` run-loop acquisition.
- **Inferred, not run on Linux:**
  - Vsync blocking.
  - Popup clipping.
  - XEmbed focus behaviour per host.
  - Which hosts implement `IRunLoop` on the factory context.
  - The llvmpipe behaviour of SW's NanoVG path.
  - How often the deadlock actually triggers.
