# Review of P3a: host-driven X11 editor window, NanoVG on GL2 (2026-10-12)

Range reviewed: `ec828eacc..a679b91e1` (notes/11). Fix rounds: `a0e15883a..35b142063` (round 1), `5ccd97d22..112262f36`
(round 2). Reviewer evidence: `evidence/p3a-review/`.

Sources:
- [R]: re-run in Docker (REAPER 7.82 aarch64, Xvfb, llvmpipe, containers `p3arev`, `p3arev-mini`);
- [H]: the reviewer's minimal CLAP host, `mini-clap-host.cpp`:
  - a level-triggered `poll()` reactor with timer-support, posix-fd-support and params;
  - its own X parent window;
  - run by `mini-run.sh`, Release and Debug+ASan builds (`build-asan.sh`);
- [C]: code reading; [M]: macOS build.

## Re-check of the implementer's results

| Check | Result |
|---|---|
| REAPER harness, IPlugControls CLAP + VST3 (`steps/iplugcontrols-p3a.txt`): render, knob drag diff, wheel, XY pad, popups, reopen ×5, 2 instances, main-thread check | PASS ×2 at a679b91e1 and again at 112262f36. Every `Draw` / `ProcessXEvents` hit is on tid 1. VST3 shows ≈ 2 drains per draw: the fd fires every tick, see finding 1 [R] |
| Rendering, judged from the contact sheet (`reaper-controls-clap/sheet.png`) and 1:1 crops | Correct layout, anti-aliasing, SVG/bitmap knobs, meters, IGraphics popups and the about box; nothing stale after reopen or for the 2nd instance [R] |
| 100× open/close, 2 instances (`churn-controls-{clap,vst3}`) | CLAP and VST3: fds 6/10, sockets 1/3, threads 14/≈40 (closed/open), identical before and after; RSS flat after the first 25 cycles [R] |
| Redraw only when dirty | IPlugEffect idle, 4 s: 2 `Draw` calls (map + expose) against ≈ 230 drains (`idle-draws-effect.txt`) [H] |
| Binary | NEEDED `libX11.so.6`, libm, libc, ld-linux; no libGL. Exports `clap_entry` / `GetPluginFactory`, `ModuleEntry`, `ModuleExit` only (`binary.txt`) [R] |
| No libGL.so.1 / X server without GLX | The plug-in loads; one stderr line; the host lives. After round 1, `set_parent` returns false (`mini-host-nogl.txt`, `mini-host-noglx.txt`, `mini-host-asan-round1.txt`) [H] |
| Two instances | Each editor has its own Display: 3 sockets with 2 editors open, 1 after close; threads back to the start count [H] |
| mac | IPlugEffect CLAP Debug from this worktree, DerivedData/SYMROOT/OBJROOT/DSTROOT in the scratchpad: BUILD SUCCEEDED at a679b91e1, and again after round 1 with `IPlugCLAP.cpp` recompiled (`macos/mac-CLAP.txt`). No tracked file under `Examples/` changed, `.rc_mac` included [M] |
| Shared files vs `hvoya` | `IPlugAPP_host.cpp`: only the `#ifdef OS_LINUX` branch of `OpenWindow` (plus P0's Linux hunks). `IGraphics.cmake`: Linux-only branches. Round 1 added `#ifdef OS_LINUX` hooks to `IPlugCLAP.cpp` (`GUIWindowAttach`) and to the Linux branch of `IPlugVST3_View.h` (`attached`). mac/win paths are unchanged [C] |

## Findings

| # | Sev. | Finding | Status |
|---|---|---|---|
| G1 | **Medium** | **The editor kept drawing after the host destroyed the parent.** When an ancestor is destroyed, our window gets only `DestroyNotify`, never `UnmapNotify`, so `mMapped` stayed true. Every frame then made the context current, drew and swapped into a dead drawable; Mesa returns True from `glXMakeCurrent` after swallowing the `GetGeometry` BadDrawable. Before round 1 this was silent (BadDrawable on request 130.3, MIT-SHM PutImage, i.e. a swap). After round 1 the timing changed and the mini host's `parent-death` scenario crashed 5/5 in a llvmpipe thread under `glXSwapBuffers ← DrawDirtyControls`. Any host that destroys its container before `removed()` / `gui.destroy` would hit it. | Fixed 5ccd97d22. `mWindowAlive` is cleared on `DestroyNotify` of our window or the parent. While it is false there is no draw, no `DrawNow`, no resize and no `XDestroyWindow`. Make-current counts an X error during the call as failure, and `DrawDirtyControls` draws only when our context is current. Re-run: `parent-death` 4/4 in Debug+ASan and 2/2 in Release, no SEGV and no X error at all (`mini-host-asan-final.txt`) [H] |
| F1 | Low | Make-current had no X error trap when NanoVG's `ScopedGLContext` ran from host calls outside a drain: CLAP `set_size` / VST3 `onSize` → `DrawResize`, and bitmap loads. With our window gone, the round trip's BadDrawable would reach the host's handler, and Xlib's default handler `exit()`s. | Fixed a0e15883a: a trap inside `Activate`/`DeactivateGLContext`, around `MakeCurrent` only |
| F2 | Low | `DrawNow` drew only dirty controls. hvoya's mac `DrawNow` forces a full repaint, which `GneissUI.cpp:1180` relies on after `Resize` + `LayoutUI`. | Fixed a0e15883a (all dirty, then draw; skipped while unmapped) |
| F3 | Low | An external `ConfigureNotify` on our own window called `OnParentWindowResize` without checking `GetHostResizeEnabled()`. The parent path and VST3 `onSize` do check it. | Fixed a0e15883a: with host resize off, the window goes back to the editor's size. Implementer's re-run: IPlugEffect returns to 450×305 [R] |
| F4 | Low | Editor open failure (no libGL, no GLX visual) was not reported: CLAP `set_parent` and VST3 `attached` returned success, so the host showed an empty frame. | Fixed a0e15883a with `OS_LINUX` hooks. REAPER CLAP now shows its generic UI. REAPER VST3 ignores `kResultFalse` and keeps an empty frame (REAPER behaviour, notes/11) |
| F5 | Low | Release builds swallowed X errors without any trace. | Fixed a0e15883a: the first error per editor connection is printed; debug builds print every error. The single static "reported" Display means two editors raising errors alternately print each time; harmless |
| F6 | Nit | The serial comment said our echoes carry an older serial. Only *earlier* echoes do; the latest echo carries `mResizeSerial` and is caught by the size check. The logic itself is right: after the server processes our resize, every event carries a serial ≥ ours, so an external resize processed before ours is correctly dropped as stale. | Fixed a0e15883a |
| F7 | Nit | `dlerror()` can be NULL after the `dlclose` and was passed to `%s`. | Fixed a0e15883a |

### Finding 1 (Info): REAPER and the X fd

The implementer reported that REAPER CLAP almost never calls `on_fd`. This is REAPER's polling model, not our
registration.

**Our side [C/H].**
- `AddFd(ConnectionNumber(display), kFdRead)` runs after `XOpenDisplay` and window creation.
- The CLAP spec says `on_fd` is level-triggered, and READ is the right mask.
- The mini host is a level-triggered `poll()` reactor. In it, `on_fd` fires about once per input event: 22–24 calls for ≈ 23 synthetic events, with the frame timers running and with them disabled (`fd-only`). There is no spinning, so the drain empties the socket.
- If a host never calls `on_fd`, the timer drain still carries input.

**REAPER [R]** (`strace-{clap,vst3}/classified.txt`, `strace-fd.sh`).
- REAPER's GLib poll set holds only its own fds. It checks each plug-in fd separately, once per tick, with `ppoll(timeout 0)`.
- CLAP: REAPER polls POLLIN only. The frame timer's drain, and the round trips inside a swap that pull pending events into Xlib's queue, have usually emptied the socket by then: 38 of 39 checks found nothing.
- VST3: `registerEventHandler` has no event mask, so REAPER polls POLLIN|POLLOUT. An X socket is always writable, so `onFDIsSet` fires every tick. The "≈ 30 calls/s" in `fd-probe-vst3` is POLLOUT, not input.
- Either way, input latency is bounded by REAPER's timer period, the same in both formats. Nothing to fix and nothing worth reporting to Cockos. notes/11 and PLAN were reworded in round 1.

### Other observations (Info, no action)

- **Re-entrancy.** Every Linux frame on the stack (`ProcessXEvents`, `HandleConfigure`, the CLAP/VST3 callbacks) checks the stack copy of the open flag. None touches `this` after a handler that can close the editor. Shared frames above them (`IGraphics::OnMouseDown`, `IGraphics::Resize`) still continue on a deleted object if a host closes the editor synchronously inside them. That is the same on mac/win. `IGEditorDelegate::mOpening` covers closes during open.
- **X error trap.**
  - Correctly scoped and LIFO-nested, with `sOuterHandler` chaining errors of other Displays to the host's handler.
  - While a drain is in progress, the trap stays installed across calls into plug-in and host code. GDK's push/pop restores it.
  - A host that calls `XSetErrorHandler` inside our scope and never restores it gets its handler replaced when our scope ends. This is inherent to Xlib's process-wide handler.
- **Swap interval 0: unverified here.** Xvfb + drisw exposes neither `GLX_EXT_swap_control` nor `GLX_MESA_swap_control` through `glXQueryExtensionsString`; `glxinfo` lists them only as client extensions. The token match therefore correctly skips the call (checked in gdb), and swaps do not block on Xvfb anyway. Verify on the VM / real Mesa DRI3 and NVIDIA (P10).
- **GLX visual.** `glXChooseVisual` (RGB8, stencil 8, double buffer) with no FBConfig fallback chain. NanoVG needs the stencil, and every current Mesa/NVIDIA/AMD driver offers it. The open fails cleanly otherwise. EGL comes later.
- **Context hygiene.**
  - The previous context, drawable and Display are saved and restored, depth-counted.
  - Open, draw, `DrawNow`, close and NanoVG's own scopes (`DrawResize`, image loads) all go through it.
  - Font loading needs no GL: fontstash builds its texture at draw time.
- **Close order.** Timer and fd are unregistered, then the open flag is cleared, then `OnViewDestroyed`, then the GL context, window and colormap are destroyed, then `XSync`, then `XCloseDisplay`. The mini host confirms registrations go back to 0 and fds back to the start count.
- **Connection loss.** `kFdError` stops the fd watch. If our private connection is killed (e.g. `xkill` on the editor), the next Xlib read calls the IO error handler, which exits the host. libX11 ≥ 1.7's `XSetIOErrorExitHandler` could avoid that later.
- **Parent-size following.** An editor that follows the parent's `ConfigureNotify` could fight hosts whose container is larger than the editor. It only triggers on size changes with host resize on. Watch for it in P10 (Bitwig, Carla).
- **Pre-existing, shared CLAP code.**
  - `IPlugCLAP::guiShow` after a failed `set_parent` calls `OpenWindow(nullptr)`, the top-level fallback. Harmless today, because every Linux reason for a failed `set_parent` also fails there.
  - iPlug's CLAP wrapper calls the host's params extension without a null check. The mini host crashed on it until it implemented params; every real host implements params.
- **Code style.**
  - 2-space indentation; `m`/`p` prefixes; Linux code in new files, with thin `OS_LINUX` hooks elsewhere.
  - `std::shared_ptr`/`std::unique_ptr` only in the platform file.
  - Comments are WHY-only (X11/GLX quirks, re-entrancy, error-trap rationale), with no stale ones after the rounds.
  - Minor redundancy: the parent path in `HandleConfigure` does its own `dynamic_cast` next to `IsHostResizeEnabled()`.

## Verdict

**P3a accepted.** Two review rounds.
- The one real defect, G1 (drawing into a destroyed window, crashing llvmpipe), is fixed and verified under ASan.
- The low items (unguarded make-current, `DrawNow` semantics, host-resize gating, unreported open failure, silent release errors) are fixed.
- The threading contract holds in REAPER for CLAP and VST3: all X processing and drawing run on the main thread.
- Lifecycle is clean over 100× churn, with 2 instances, with the parent destroyed first, and without libGL or GLX.
- mac/win are unaffected.

Carried forward:
- swap-interval verification on real drivers (P10);
- parent-size following in other hosts (P10);
- keyboard/focus/XEmbed, text entry, cursors, clipboard and menus (P3b).
