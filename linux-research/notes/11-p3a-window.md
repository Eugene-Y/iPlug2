# P3a: host-driven X11 editor window, NanoVG on GL2 (2026-10-11)

Commits `ec828eacc..` on `hvoya-linux`. Evidence: `evidence/p3a/`. **[R]** = run in Docker (REAPER 7.82 aarch64,
Xvfb, Mesa llvmpipe, container `p3a`). **[M]** = macOS.

## What was built

`IGraphics/Platforms/IGraphicsLinux.{h,cpp}` replace the stub; `IGraphicsLinuxGLX.h` holds the GLX loader and an
X error trap. X11 types stay out of the header (pimpl `IGraphicsLinux::Impl`, `union _XEvent` forward-declared), so
plug-in translation units never see Xlib's macros. `IGraphicsNanoVG.cpp` is unity-included *before* the X11
headers for the same reason.

| Topic | Choice |
|---|---|
| Connection | `XOpenDisplay` per editor, closed in `CloseWindow`. Never the host's Display; no `XInitThreads` |
| Window | child of the host's X window (CLAP `x11`, VST3 `kPlatformTypeX11EmbedWindowID`, APP: XID of SWELL's GdkWindow). A parent that `XGetWindowAttributes` rejects gives a top-level window (`WM_DELETE_WINDOW` only unmaps it). `_XEMBED_INFO` = {0, XEMBED_MAPPED}. `CWBorderPixel` set (a GL visual that differs from the parent's is a BadMatch otherwise) |
| GL | GLX 1.2 (`glXChooseVisual` RGB8 + stencil 8 + double buffer, `glXCreateContext`), one context per editor. **libGL is `dlopen`ed** (`libGL.so.1`, `RTLD_LOCAL`, never closed); glad loads GL through `glXGetProcAddressARB`. NEEDED is now `libX11.so.6` + libc/libm only: the plug-in loads and scans on machines without a GL driver, only the editor fails to open |
| Renderer | **NanoVG GL2**, the CMake default on Linux now (as on Windows / Gneiss `IGRAPHICS_GL2`). RR's claim that GL2 has no FBO is wrong for iPlug: `IGraphicsNanoVG.h` defines `NANOVG_FBO_VALID` itself, and glad_GL2 loads `GL_ARB_framebuffer_object`. The main FBO and layers work on llvmpipe without RR's `nanovg_gl_utils.h` and `glFinish` patches |
| Context hygiene | `ActivateGLContext` saves the current context/drawable/display and makes ours current; `DeactivateGLContext` restores exactly that (or releases). Depth-counted, so the nested `ScopedGLContext`s in NanoVG are fine. If make-current fails (window already gone) no context is left current, so GL deletes cannot hit another context's objects |
| Swap interval | 0 via `GLX_EXT_swap_control`, else `GLX_MESA_swap_control` (token match, not `strstr`) |
| Event loop | X fd: `GetHostLoop().AddFd(ConnectionNumber, kFdRead)`; frame timer: `AddTimer(1000 / FPS())`. Both callbacks drain `XPending`/`XNextEvent` fully; the timer then draws only if `IsDirty()`. Both are removed in `CloseWindow` before the GL context and the Display go. No thread, no mutex |
| Re-entrancy | Every X event handler calls into IGraphics as its last statement. A `std::shared_ptr<bool>` "open" flag, copied onto the stack by each callback, stops the drain when a handler closed the editor (the host may then delete the IGraphics object synchronously) |
| X errors | `ScopedXErrorTrap` around open, close, every drain, draw, resize and every make-current (NanoVG's `ScopedGLContext` also runs from host calls such as `set_size`/`onSize` and bitmap loads): errors on our Display are swallowed, others go to the previous handler. Xlib's default handler would `exit()` the host, e.g. on `XDestroyWindow` after the host destroyed the parent. Release builds print the first swallowed error of each editor connection, debug builds every one |
| hvoya APIs | `DrawNow()` = all controls dirty, make current, `Draw`, `glXSwapBuffers`, restore, synchronously (a full repaint, like mac; skipped while unmapped). `mInOpenWindow` is set for the whole `OpenWindow`. `IGEditorDelegate::mOpening` needs nothing extra: `OpenWindow` closes only its own previous window, never through the delegate |
| Plug-in resize | `PlatformResize` → `XResizeWindow` when the physical size changed; host notified through the existing `EditorResizeFromUI` paths (CLAP `request_resize`, VST3 `resizeView`) |
| Host resize | CLAP `set_size` / VST3 `onSize` → `OnParentWindowResize` (shared path, unchanged). ConfigureNotify on **our** window from someone else (XEmbed-style hosts) → the same `OnParentWindowResize` if host resize is enabled; then the window is put back to the editor's size whenever the editor did not take the new one (host resize off, constraints). Our own `XResizeWindow` echoes are told apart by request serial (`NextRequest` before the call; events with an older serial are stale), which replaces RR's in-ConfigureNotify flags and coalescing. ConfigureNotify on the **parent** (we select StructureNotify on it, on our connection only): only when its size changed, host resize is enabled, it differs from ours, and not in the APP (its parent includes the menu bar) |
| Expose / map | Expose (count 0) → all controls dirty. Map/UnmapNotify on our window → drawing is skipped while unmapped |
| Mouse | buttons 1/3 → `OnMouseDown`/`Up` (modifier state adjusted: X reports the state before the event), 4/5 → wheel, double click by timestamp (400 ms, 4 px), motion coalesced only while the next queued event is also motion (so motion never overtakes a release), `LeaveNotify` → `OnMouseOut`. The implicit X pointer grab keeps a drag going outside the window |
| Fonts | `LoadPlatformFont` from a resource file (`LocateResource`) or memory → `IFontData`. Fonts by system name return nullptr (fontconfig: P3b/P4) |
| Open failure | No libGL, no GLX visual, no X display: `OpenWindow` returns nullptr and the host is told (Linux-only hooks: VST3 `attached` returns `kResultFalse`, CLAP `set_parent` returns false), so it shows its own UI instead of an empty frame |
| APP | `IPlugAPPHost::OpenWindow` moves the editor's window below SWELL's menu bar (SWELL draws the menu bar into the same X window; offset from `ClientToScreen` − `GetWindowRect`) |

Not selected on purpose: `KeyPress`/`KeyRelease`/`FocusChange`. While the editor does not handle keys, the host
gets them (transport, shortcuts). P3b adds them together with focus/XEmbed.

## Findings

1. **REAPER polls plug-in fds itself, once per main-loop tick** (reviewer's strace, `evidence/p3a-review/strace-*`):
   its GLib poll set holds only its own fds; each registered plug-in fd gets a separate `ppoll` with timeout 0.
   - CLAP: POLLIN only (our flags). By the time of REAPER's check the frame timer's drain, and the round trips
     inside a swap, have usually emptied the socket, so `on_fd` rarely fires (38 of 39 checks found nothing; one
     call in a whole session in our trace, none in 4-s windows of mouse motion, `evidence/p3a/fd-probe-clap`).
   - VST3: `registerEventHandler` has no event mask, so REAPER polls POLLIN|POLLOUT. An X socket is always
     writable, so `onFDIsSet` fires on every tick: the ≈ 30 calls/s in `fd-probe-vst3` are POLLOUT, not input.
     The factory-context IRunLoop does deliver on the main thread (notes/10 #8 answered for REAPER).
   - Our registration is right: in the reviewer's level-triggered mini CLAP host `on_fd` fires about once per
     input event. So this is REAPER's polling model, not a bug here. In both formats input latency is bounded
     by the frame-timer period (REAPER runs plug-in timers at ≈ 26-30 Hz).
2. **llvmpipe costs threads per editor**: 13 threads per open editor on the 6-CPU VM (Mesa's rasterizer threads per
   GLX screen, i.e. per Display), all joined on close. Hardware drivers will differ; nothing to do in iPlug.
3. RSS grows once (≈ +200 MB over the first editor opens with llvmpipe: shader caches, font cache) and then stays
   flat over 100 cycles.
4. GCC 13 `-Wstringop-overflow` false positive in `IParam::GetNormalized` inlined into `OpenWindow` (shared code,
   `std::atomic<double>` behind a pointer GCC thinks may be null). Harmless.

## Acceptance

| # | Check | Result |
|---|---|---|
| 1 | `reaper-plugin-test.sh` IPlugControls (`steps/iplugcontrols-p3a.txt`) and IPlugEffect (`steps/iplugeffect-p3a.txt`) × CLAP, VST3: editor renders on first open, after reopen ×5, 2nd instance | PASS ×4 [R] (`reaper-summary.txt`) |
| 2 | Knob drag changes the value (screenshot diff, `expect-change` step) | IVKnob 100 % → 72.32 % → wheel 75.32 % (`knob-c00-c01-c03.png`); IPlugEffect "lim thresh" 0.0 → −19.5; XY pad, keyboard, IGraphics popup menus and caption popup also change [R] |
| 3 | Thread check | every `IGraphics::Draw` and `IGraphicsLinux::ProcessXEvents` hit on gdb thread 1 (REAPER main), all 4 runs; the harness now fails on any off-main hit [R] |
| 4 | Plug-in initiated resize | IPlugEffect corner resizer (Scale mode): REAPER grows the FX window to 565×382, the UI scales (`resized-crop.png`) [R] |
| 5 | External resize of our window | before the review round it followed (`IGraphics::Resize` once); now, with IPlugEffect's host resize off, the window goes back to 450×305 (`external-resize-effect-clap`) [R] |
| 6 | APP IPlugControls `--no-io` | renders below the menu bar, all `expect-change` checks pass, Draw and X processing on the main thread (`app-controls/`) [R] |
| 7 | 100× open/close, 2 instances, every 5th cycle closes before any timer of the new window fired | CLAP and VST3: fds 6/10, sockets 1/3, threads 14/40 (closed/open) identical before and after; RSS flat after the first 25 cycles; REAPER alive (`churn-controls-*`) [R] |
| 8 | Host registrations | trace build after churn + FX delete: live registrations 0; no `OFF-MAIN`, no "cannot register" (`churn-trace-*`) [R] |
| 9 | P2 host-loop test with the real editor | PASS CLAP + VST3 (`host-loop-controls-*`) [R] |
| 10 | Validators | unchanged from P2: clap-validator Effect 35/3, Controls 33/3 (state-reproducibility ×3); VST3 Effect 536/537, Controls 537/537 [R] |
| 11 | Binary | NEEDED `libX11.so.6`, libm, libc, ld-linux; exports unchanged; GLIBC 2.34 on 22.04, 2.38 on 24.04 (`inspect.txt`, `cmake-22.04.log`) [R] |
| 12 | macOS | IPlugEffect CLAP Debug BUILD SUCCEEDED; APP fails only in `IPlugAPP.cpp` (pre-existing, P0); `IPlugAPP_host.cpp` compiles (`macos/`) [M] |
| 13 | Visual parity | IPlugControls Linux (GL2, 1×) vs macOS (Metal, 2× downscaled): same layout and colours. Linux text is a little heavier and less smooth (stb_truetype at 1×, no hinting) and the IBKnob shows its 1× bitmap; nothing misplaced (`visual/side-by-side.png`, `zoom-*.png`) [R/M] |

The original full step script (`fullsteps-controls-clap/`, at ec828eacc): IGraphics popups and the IGraphics text
entry open, typed text does not commit, and the "OS menu" / "OS text entry" / colour / font / message-box steps do
nothing. All expected for P3b (keyboard, platform menus and text entry) and P4 (dialogs).

## Harness changes

- `xact.sh`: `expect-change A B X Y W H` (view-relative crop must differ; failures land in `xact-fails.txt`, which
  `reaper-plugin-test.sh` turns into FAILs).
- `reaper-plugin-test.sh`: the Draw count moves the mouse over the 2nd editor (a UI without animations has nothing
  dirty otherwise), counts `ProcessXEvents` too, and fails on any editor hit off REAPER's main thread or no Draw.
- New: `steps/iplugcontrols-p3a.txt` (mouse-only subset), `steps/iplugeffect-p3a.txt`, `editor-churn-test.sh`,
  `fd-probe.sh`, `external-resize-probe.sh`, `sheet.sh` (cropped contact sheets), `compare-shots.sh`.

## Known issues / left for later

- P3b: keyboard, focus and XEmbed messages, text entry, cursors (`HideMouseCursor`/`MoveMouseCursor` are no-ops:
  during a knob drag the cursor stays visible and is not locked), clipboard, platform popup menus
  (`CreatePlatformPopupMenu` returns nullptr), fonts by system name.
- Fallback backend (CLAP host without timer-support, VST3 without any IRunLoop): the editor still registers, so X and
  GL work would run on the fallback thread while the host thread calls into IGraphics. Racy by construction, like
  `OnIdle` there; no REAPER case. Revisit if P10 finds such a host.
- P5: HiDPI (screen scale stays at the constructor's value).
- The top-level fallback window gets no WM size hints.
- Double-click time is fixed at 400 ms (no XSettings lookup).

## Review round 1 (notes/12)

| # | Fix |
|---|---|
| F1 | X error trap inside `ActivateGLContext`/`DeactivateGLContext`, around make-current only (traps nest) |
| F2 | `DrawNow()` repaints everything (all controls dirty), like the mac version Gneiss relies on |
| F3 | External resize of our window: `OnParentWindowResize` only with host resize enabled; otherwise the window goes back to the editor's size. Re-run of `external-resize-probe.sh` on IPlugEffect (host resize off): 600×400 requested, window back at 450×305 [R] |
| F4 | Editor open failure reaches the host: VST3 `attached` → `kResultFalse`, CLAP `set_parent` → false (`#ifdef OS_LINUX`). `no-libgl-probe.sh` (empty `libGL.so.1` first on `LD_LIBRARY_PATH`): one stderr line, plug-in loads and REAPER lives in both formats; **CLAP**: REAPER shows its generic parameter UI; **VST3**: REAPER ignores the failed `attached` and keeps an empty 1024×768 frame (REAPER behaviour; `no-libgl-clap-vst3.png`) [R] |
| F5 | Release builds print the first swallowed X error of each editor connection; debug builds every one |
| F6 | Serial comment: earlier echoes have an older serial, the latest echo fails the size check |
| F7 | `dlerror()` may be NULL after the `dlclose`: guarded ("GLX entry points missing", seen in the probe) |
| G1 (round 2) | Destroying an ancestor sends our window DestroyNotify only, never UnmapNotify, so frames kept drawing into the dead drawable and llvmpipe crashed (reviewer's mini host, parent destroyed before `gui.destroy`, ASan, 5/5). Now DestroyNotify for our window or the parent stops drawing, `DrawNow` and resizing, and `CloseWindow` skips `XDestroyWindow`; make-current counts an X error during the call as failure and draws only with our context current. Re-run: parent-death 6/6 alive under ASan with no X error at all; fd, two, churn (50) clean; REAPER IPlugControls CLAP + VST3 PASS; 100× churn stable [R] |
| notes | Finding 1 rewritten from the reviewer's strace: REAPER polls plug-in fds once per tick; CLAP POLLIN rarely finds input after the timer's drain, VST3 POLLOUT fires every tick |

Re-run after the fixes [R]: REAPER IPlugControls CLAP + VST3 and IPlugEffect CLAP PASS, every Draw/X hit on the main thread. macOS IPlugEffect CLAP + VST3 Debug BUILD SUCCEEDED with the F4 hooks (nothing installed into `~/Library`) [M].
