# Build & test results (2026-10-10)

Environment: Docker (colima) on Apple silicon. Everything ran as native **linux/arm64**; nothing was
run under amd64/Rosetta. Ubuntu 24.04 (glibc 2.39, gcc 13). Xvfb + openbox, Mesa llvmpipe.
Hosts and validators: REAPER 7.82 aarch64, clap-validator 0.4.1, VST3 SDK v3.8.1 `validator`.
Plugin SDKs: VST3 SDK v3.8.1, CLAP `main` (a47f6ba), clap-helpers `main`.
Not covered: Carla (no Ubuntu 24.04 package, not built), TomTrnk (worktree checked out, never built), real GPUs and real window managers.

**Revision 2026-10-11 (after review round 1, notes/05):**
- All REAPER rows were re-run with the fail-loud harness (`scripts/rerun-reaper-matrix.sh`; results in `evidence/_env/reaper-matrix-run.txt`, one `RESULT.txt` per run).
- Corrected: failure 1 (not CLAP-only), the new failure 1a (timer start race), the RR VST3 and hvoya-trial REAPER claims, the threading row, and the judgement.

How to re-run everything: `docker/README.md`. Evidence lives in `evidence/<candidate>/`. Every local patch is in `evidence/<candidate>/patches/`.
**[V]** = verified here, **[I]** = inferred.

## Candidates as tested

| id | ref | build path used |
|---|---|---|
| rangerrick | `rangerrick/feature/linux-support` @03d3c1a (PR #1336) | its own CMake (`scripts/build-cmake.sh`) |
| stillwell | `stillwell/stillwellaudio/master` @1d04aa5 (2026-10-09). It already contains `linux-clap-current`, `linux-vst3-current` and every `fix/linux-*` branch, so it is the most complete. | The fork has **no Linux plugin build system**. I wrote an out-of-tree CMake driver (`cmake/stillwell/`) and built Skia chrome/m130 for arm64 myself (`scripts/build-skia-linux.sh`). |
| bindernews | `upstrem/linux-bindernews` @a6a8c6e | upstream-style CMake |

## Results matrix

| test | rangerrick (PR #1336) | stillwell | bindernews |
|---|---|---|---|
| configure/build APP | PASS after 2 tiny patches [V] | not built (no APP build path) | PASS after `-fsigned-char` + 2 patches [V] |
| build VST3 | PASS (Effect/Controls/Instrument) [V] | PASS with my CMake [V] | builds a bare `.so`, but it is **not loadable**: no `ModuleEntry`, undefined Steinberg symbol, no bundle [V] |
| build CLAP | PASS [V] | PASS with my CMake [V] | FAIL: `#error Not Implemented!` in IPlugCLAP.cpp [V] |
| VST3 validator | 537/537 pass ×3 [V] | 537/537 ×3, after the logger patch [V] | n/a |
| clap-validator | 4–5 fail / 44: `state-reproducibility-*` ×3, `layout-audio-ports-config`, Instrument `param-fuzz-bounds` subnormal [V] | **0 fail** for Effect and Controls; Instrument 1 fail (subnormal) [V] | n/a |
| exported dyn symbols | ~7.6–8k (no `-fvisibility=hidden`) [V] | ~5.3k (Skia/FreeType leak through, although iPlug code is hidden) [V] | 4.4k |
| libstdc++ | static (no GLIBCXX refs) [V] | static (my flags) | dynamic (GLIBCXX_3.4.32) |
| GLIBC floor | 2.38 on 24.04 → **2.34 when built on 22.04**. The 22.04 build passes the validators on 24.04. [V] | 2.38 (24.04 build only) | 2.38 |
| NEEDED | libGLX, libX11, libc/libm only; fontconfig and GTK are dlopen'ed [V] | libfontconfig, libX11, libGLX | – |
| bundle layout | `X.vst3/Contents/aarch64-linux/X.so` + `Contents/Resources/{ttf,png,svg}`. CLAP is a flat `X.clap` that looks for a **shared** `resources/` next to it. [V] | VST3 the same (`Resources/`, flat). CLAP uses a per-plugin `X.resources/` next to `X.clap`. [V] | no bundle; APP looks for `<exe dir>/resources/{img,fonts}` |
| APP GUI under Xvfb | renders, knob drag OK, IGraphics popups OK, GTK native menu OK, GTK file dialog OK [V] | – | renders with resources copied by hand; IGraphics popups OK; zenity file dialog OK [V] |
| REAPER VST3 editor | IPlugEffect: renders, reopen ×5 and 2 instances PASS. IPlugControls: IGraphics popups, IGraphics text entry and GTK menu OK, then **REAPER crashes after the OS "Edit Value" dialog is committed** (failure 1). Reopen ×5 and 2 instances PASS only in a separate no-steps run. **The first open can stay black** (timer-start race, failure 1a) [V] | renders (Skia GL), popups and text entry OK, reopen ×5 and 2 instances OK, **after** the OnUIClose example patch [V] | n/a |
| REAPER CLAP editor | renders once `resources/` is installed. Reopen ×5 and 2 instances PASS (no-steps run). **Same OS-text-entry crash** as VST3 (failure 1) [V] | renders, popups OK, text entry OK, reopen ×5 and 2 instances OK [V] | n/a |
| thread drawing the UI | APP: private pthread. VST3: private pthread per editor (`IGraphicsLinux::TimerThreadProc`), **if that thread wins the start race** (failure 1a). CLAP: **host main thread** via `timer-support` [V]. In **every** format, `IPlugTimer`/`OnIdle` runs on its own pthread (`Timer_impl::ThreadProc`), **never on the main thread** [V by review, notes/05 §3] | VST3: **host main thread** via `IRunLoop` timer. CLAP: **private `std::thread` per editor** (`LinuxTimerWorker`) [V] | APP: private `iPlug2Loop` thread (`IPlugTaskThread`) [V] |

## Key failures (with excerpts)

1. **rangerrick: host crash on OS text entry — VST3 and CLAP** [V]. *(The first version of this note said "CLAP-only; VST3 is fine". That was wrong: the old harness missed the VST3 crash.)*
   - It fires after "OS Text Entry" is toggled on, the number box is double-clicked, and the GTK "Edit Value" dialog is committed.
   - Cause [V by review, code-read here]:
     - `CreatePlatformTextEntry` runs a synchronous `gtk_dialog_run`.
     - It commits through `SetControlValueAfterTextEdit`, which leads to `ClearInTextEntryControl`, so `mInTextEntry` becomes null.
     - `IGraphics::CreateTextEntry` then calls `mInTextEntry->SetDirty(false)` (IGraphics.cpp:1963 in RR, :1987 in the trial).
   - IGraphics-drawn text entry works in both formats.
   - Backtraces:
     - CLAP (host main thread): `CreateTextEntry ← OnMouseDblClick ← ProcessX11Events ← OnDisplayTimer ← IPlugCLAP::onTimer`. See `evidence/rangerrick/gui/reaper-clap-IPlugControls/gdb-crash.txt`.
     - VST3 (private thread 2): `… ← OnDisplayTimer ← TimerThreadProc`. See `evidence/rangerrick/gui/reaper-vst3-IPlugControls/gdb-crash.txt`.
     - The hvoya trial shows the same in both formats.
   - The bigger problem is the nested synchronous GTK loop. In VST3 it runs a GTK dialog off the main thread.
1a. **rangerrick: timer-thread start race (PR commit `cb8ec371c`)** [V].
   - `IGraphicsLinux::StartTimer` and the Linux `Timer_impl` ctor both do `if (pthread_create(...) == 0) mRunning = true;`. The new thread checks `while (mRunning)` first, so it can exit immediately.
   - When it does: no render thread (a black editor that never processes input), and no `IPlugTimer`, so no `OnIdle`/`ISender`.
   - Measured here:
     - hvoya-trial VST3, 2 instances: 1 `TimerThreadProc` thread, and the 2nd editor is blank (`render[second-instance] … stddev 0`; `evidence/hvoya-trial/gui/reaper-vst3-IPlugControls-nosteps/`).
     - RR VST3, 2 instances: only 1 `Timer_impl` thread, so one instance has no `OnIdle` (`evidence/rangerrick/gui/reaper-vst3-IPlugControls-nosteps/threads.txt`).
   - The reviewer reproduced it standalone (1/400) and under ptrace (nearly always), and a set-flag-first patch fixes it (notes/05 §0.2).
   - How often it hits [V by review]: rare without a debugger. In REAPER without gdb, 64/64 timer threads were alive (16 instances × 4 runs) and 8/8 first opens rendered. Under ptrace it is near-certain. A real bug, likely rare in the field [I].
   - The harness's `IPlugTimer` thread-count check came later than the matrix runs. So `rangerrick/…/reaper-vst3-IPlugControls-nosteps` and `hvoya-trial/…/reaper-clap-IPlugControls-nosteps` say PASS even though each has only 1 `Timer_impl` thread for 2 instances: one instance had no `OnIdle`. Re-running now would FAIL them.
   - **The original hvoya-trial VST3 "PASS" was vacuous**: all 15 shots were black and byte-identical, the gdb catcher was attached before the open, and the race lost. That evidence has been replaced.
2. **rangerrick: missing resources take the host down** [V]. If `resources/` is not next to the `.clap`, IPlugEffect shows a blank editor (the font fails, so nothing is drawn) and IPlugControls segfaults REAPER in `ISVGSliderControl::OnResize` (null SVG). The shared `~/.clap/resources/` directory will also collide between products [I].
3. **rangerrick: keyboard focus** [V]. In the APP the text entry opens, but typed keys never arrive: X focus stays on the GDK toplevel and IGraphicsLinux never calls `XSetInputFocus`. After `xdotool windowfocus` on the plugin child, typing works ("42"). REAPER happens to deliver keys.
4. **rangerrick: redraw on every tick** [V]. In a static IPlugEffect, `Draw` hits equal `OnDisplayTimer` hits (VST3 ×2 instances 128+127 Draw / 128+128 timer, CLAP ×2 254/254 in 5 s under gdb; `evidence/rangerrick/gui/reaper-*-IPlugEffect-nosteps/draw-threads.txt`). It never idles [I: CPU cost on hosts with many editors].
5. **rangerrick: APP segfault with no audio device** [V]. `IPlugAPPHost::GetAudioDeviceID ← TryToChangeAudio`. `--no-io` avoids it.
6. **rangerrick: APP misc** [V]. Escape closes the whole APP (SWELL IDCANCEL), including while an IGraphics popup is open. The colour-swatch click shows no dialog. The build only works with current WDL after 2 patches: `SWELL_DLG_WS_DEFAULT_SCALING` is only defined for OSX (also broken on upstream master), and gio is missing from the APP link.
7. **stillwell: build effort** [V]. No CMake/Makefile/Linux Skia script in the fork, and the editor is Skia-only (`IGraphicsLinux.cpp` includes Ganesh unconditionally). The first build hit:
   - wdltypes `unsigned char` assert on arm64, needs `-fsigned-char`;
   - `IPlugCLAP.h` includes `plugin.hh` unqualified;
   - VoiceAllocator lacks `<climits>`/`<memory>`.
8. **stillwell + bindernews (and upstream master, and `hvoya`): `*** buffer overflow detected ***` at plugin construction** [V].
   `IPlugLogger.h CurrentTime(): snprintf(&cStr[i], 32, …)` trips `_FORTIFY_SOURCE=3`, the default of Ubuntu 24.04's gcc 13. With `=2`, or on 22.04/gcc 11, the extracted code does not abort [V by review, notes/05 §4]. The site is in `IPlugAPIBase::IPlugAPIBase` via `Trace(... CurrentTime())`. rangerrick and TomTrnk already fixed this; **hvoya still has the bug**.
9. **stillwell: crash on first editor open** in IPlugControls [V]. Its `IGEditorDelegate::OpenWindow` calls `CloseWindow()` first (a 2023 EUCON workaround), so `OnUIClose()` runs before a UI exists, and the example dereferences `GetUI()`. I patched the example. Not Linux-specific [I].
10. **bindernews** [V]: CLAP is `#error Not Implemented`, the VST3 is not a loadable module, the APP shows a black SWELL menu bar, typed text does not commit, and no OS-native menu appears. This matches "APP only" — and even the APP needs resources copied by hand.
11. **OS-native popups and dialogs inside hosts**: rangerrick's GTK menu and "Edit Value" dialog do appear in REAPER [V]. stillwell's do not appear; an "iPlug2 Plugin" window is listed instead [V, not investigated]. No hangs were observed under openbox, but bsvi's report of KDE GTK hangs was not reproduced either [I].

## hvoya trial (`linux-trial/rangerrick`, worktree `wt/hvoya-trial-rangerrick`)

- Cherry-picked the 22 non-chore PR commits onto `hvoya` (b502242). `hvoya` itself is untouched; nothing pushed.
- **4 of 22 commits conflicted, across 11 files and 15 hunks.** All were mechanical (`evidence/hvoya-trial/cherry-pick.log`):
  - CMake `elseif(UNIX…)` placeholders: take the PR side.
  - `CMakePresets.json`: add the linux presets alongside hvoya's `wam` preset.
  - `IPlugPlatform.h`: the `OS_LINUX` guard.
  - `IPlugCLAP.cpp`: keep hvoya's own chunked state write.
  - `IGraphicsEditorDelegate`: keep both hvoya's `mOpening` guard and the PR's Linux gfx mutex.
- Build [V]: **IPlugControls APP/VST3/CLAP and IPlugInstrument VST3/CLAP PASS**. The hvoya WDL does not even need the SWELL/gio patches.
- IPlugEffect on hvoya cannot build outside the user's environment. It needs Xcode-generated `plug_build_info.hpp` and `hvoya/utils/log/logger.hpp` from another repo, and its CMake source list is stale on hvoya itself.
- Validators [V]: VST3 537/537. CLAP fails 3 (`state-reproducibility-*`), one fewer than the PR alone, because hvoya's chunked state write fixes the buffered case.
- REAPER [V] (re-run with the fail-loud harness, `evidence/_env/reaper-matrix-run.txt`):
  - With steps, VST3 **and** CLAP crash at the OS text entry (failure 1).
  - No-steps runs:
    - CLAP: PASS (render, reopen ×5, 2 instances). But only 1 `Timer_impl` thread for 2 instances, so one instance had no `OnIdle` (failure 1a).
    - VST3: first open and reopen render, but the **2nd instance stays blank** (failure 1a).
  - *(The earlier "VST3 passes everything" was vacuous; see failure 1a.)*

## Judgement

- **PR #1336 (rangerrick)** is the best base to *build on* [V]:
  - It uses upstream's CMake with presets and CI.
  - APP, VST3 and CLAP all build with tiny or no patches.
  - The glibc floor is 2.34 when built on 22.04; libstdc++ is static and fontconfig/GTK are dlopen'ed.
  - It cherry-picks onto hvoya with only mechanical conflicts.

  To ship it, it needs:
  - the timer-thread start race fixed (set the flag before `pthread_create`). Without it an editor can stay black and `OnIdle` can be missing;
  - the OS-text-entry crash fixed (VST3 + CLAP), and no synchronous GTK dialog loop inside host callbacks;
  - `OnIdle`/`IPlugTimer` on the host main thread. Today it is a pthread in every format;
  - VST3 moved onto `IRunLoop` instead of its private 60 fps pthread;
  - XEmbed/`XSetInputFocus` keyboard focus;
  - idle-skip redraw;
  - symbol visibility / a version script;
  - per-plugin CLAP resources;
  - GTK-in-process dialogs replaced or kept off the host thread [I].
- **stillwell** is the most host-correct design for VST3 (`IRunLoop`) and has a hardened CLAP (0 clap-validator failures) [V]. But its CLAP editor still runs on a private thread [V]. It is Skia-only on Linux, has no public build path, sits ~500 commits behind upstream, and carries product-specific delegate changes. Use it as a **source of patches** (`IPlugVST3_RunLoop.h`, CLAP lifecycle and contract fixes, `IPlugLinuxPathUtils.h`, `LinuxTimer`), not as the base [I].
- **linux-bindernews**: APP only, with a private UI thread and no working plugin formats [V]. Ignore it as a base; its PlatformX11/zenity ideas are superseded by the two above [I].
