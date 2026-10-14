# P3a on a real desktop: UTM VM, GNOME/Mutter on X11, Mesa virgl (2026-10-12/13)

P3a as accepted in notes/12 (`51d211663`), built in Docker (24.04 image, Release) into `build/vm-p3a` from the
detached worktree `wt/vm-p3a`, and run on the VM. Evidence: `evidence/vm-p3a/`. Rig and gotchas:
`scripts/vm/README.md`.

**[V]** = run on the VM: Ubuntu 24.04.5 arm64, 4 CPUs, GNOME Shell 46 (Mutter) on Xorg, screen 1440×863,
OpenGL 2.1 Mesa 25.2.8 virgl over ANGLE/Metal (Apple M3), REAPER 7.82 aarch64 (dummy audio), PipeWire 1.0.5.
**[D]** = the Docker numbers of notes/11–12 (Xvfb, openbox, llvmpipe).

## Results

| # | Check | Result |
|---|---|---|
| 1 | `reaper-plugin-test.sh` IPlugControls (`steps/iplugcontrols-p3a.txt`) and IPlugEffect (`steps/iplugeffect-p3a.txt`) × CLAP, VST3: first open, steps, reopen ×5, 2nd instance | **PASS ×4** [V]. Every editor area is painted; layouts, AA, SVG/bitmap knobs, meters, IGraphics popups, caption popup, text entry and about box look as in Docker (`reaper-controls-*/sheet.png`, `sheet-open-reopen-two.png`) |
| 2 | Knob drag changes the value | IVKnob 100 % → 72.32 % → wheel 75.32 %; IPlugEffect "lim thresh" 0.0 → −19.5; all `expect-change` steps pass [V] |
| 3 | Main-thread check (gdb) | All `IGraphics::Draw` / `ProcessXEvents` hits on tid 1 in all 4 runs [V]. With the 2nd editor hovered: Controls CLAP 178 Draws / 268 drains in 5 s, VST3 234 / 469; Effect 16–17 Draws (dirty-only), 256 (CLAP) / ≈ 1000 (VST3) drains. [D] had 71 Draws for Controls: llvmpipe draws slower; the VST3 drains-per-tick pattern (POLLOUT every tick, notes/12 finding 1) is the same |
| 4 | Plug-in initiated resize (IPlugEffect corner resizer) | CLAP and VST3: REAPER's container goes 450×305 → 565×382, UI scaled (`reaper-effect-*/sheet-resize.png`) [V], same as [D] |
| 5 | Open/close churn, 2 instances, 50 cycles (`editor-churn-test.sh`) | CLAP and VST3 PASS: fds 6/16, sockets 1/3, threads 10/12 (closed/open) identical before and after [V]. **RSS grows linearly**, see issue V3 |
| 6 | Host-loop trace build (`-DIPLUG_TRACE_HOST_LOOP`, `host-loop-test.sh`) | PASS ×2. CLAP: `backend=clap-timer-support`, host `timer-support=1 posix-fd-support=1`. VST3: `backend=vst3-irunloop(factory-context)`, `initialize` context IRunLoop `(nil)`, view frame offers its own IRunLoop. 50× churn: threads 10 → 10, fds 6 → 6, no `OFF-MAIN`. **Identical to [D]** (`host-loop-trace-controls-*`) |
| 7 | Swap interval | virgl exposes `GLX_EXT_swap_control`, `_tear`, `GLX_MESA_swap_control`, `GLX_SGI_swap_control` in the effective GLX list. Our code takes the EXT branch; while the editor draws, `glXGetSwapIntervalMESA()` (called from gdb with our context current) returns **0**. Control: a bare GLX program without any swap-control call gets **1**. So the call is applied [V] (`swap-interval-controls-clap`). [D] never took the branch (Xvfb has no such server extension) |
| 8 | Frame rate and CPU (`probe/cpu-fps.sh`, pidstat 10 s, 1-s samples; noisy in a VM) | Controls CLAP open: 28.9 Draws/s idle (animated meters), 26.2/s while dragging the IVKnob (REAPER's ≈ 26–30 Hz timer; no vsync blocking). Effect VST3: **0** Draws/s idle, 20.3/s while dragging. CPU: REAPER 8–14 % with the editor closed, 11–17 % open or dragging; Xorg 4–16 %; gnome-shell 4–18 % [V] |
| 9 | Compositor embedding and stacking | Our window is a child of REAPER's container inside Mutter's frame (`mutter-x11-frames`); no X errors at all in any run. A 2nd instance stacks above the 1st; REAPER's main window stays below its FX windows (they are transients) [V] (`wm-controls-clap/sheet-wm.png`) |
| 10 | Window moves | `wmctrl` move and an interactive title-bar drag: the editor moves with the window, keeps 1024×768, keeps rendering, no resize [V] |
| 11 | Focus clicking between REAPER and the editor | Click on REAPER's track panel → REAPER main active, FX window still on top (transient). Click into the editor → the FX window becomes active (Mutter's click-to-focus on the frame) [V]. Keyboard focus itself is P3b |
| 12 | Minimize/restore | Mutter ignores minimize on the FX window (transient of REAPER's main). Minimizing REAPER's main window hides the FX window too (WM_STATE Iconic); after restore the editor repaints correctly. **While minimized the editor keeps drawing at full rate**, see issue V2 [V] |
| 13 | APP IPlugControls with audio | Opens ALSA `default` = PipeWire ("PipeWire ALSA [IPlugControls]" stream; `alsa-pipewire`, `pw-data-loop` threads); renders below the menu bar; all `expect-change` steps pass; exits on a WM close [V] (`app-controls/`) |
| 14 | NanoVG object balance per editor open/close (`probe/nvg-balance.sh`) | Per cycle 11 texture creates, 10 `renderDeleteTexture` calls; the 11th is NanoVG's 1×1 dummy texture, freed in `glnvg__renderDelete` with `glDeleteTextures`. 1 `renderCreate` / 1 `renderDelete` per cycle. Balanced [V] |

## Differences from Docker

| | Docker [D] | VM [V] |
|---|---|---|
| Threads per open editor | 13 (llvmpipe rasterizer threads per Display) | 1 (12 with two editors vs 10 closed) |
| fds per open editor | 2 (socket + 1) | 5 (socket + virgl render-node fds) |
| RSS over open/close churn | +200 MB once, then flat | +≈ 0.85 MB per cycle (2 editors), linear; flat with `LIBGL_ALWAYS_SOFTWARE=1` on the same VM |
| Swap control | not offered by Xvfb, call skipped | EXT branch taken, interval 0 in effect |
| Draws/s, Controls, 2 editors, gdb attached | ≈ 14/s | ≈ 36–47/s |
| Window manager | openbox, windows always mapped | Mutter: frames are a separate client; minimize keeps child windows viewable; screen 863 px tall clips the editor by 2 px |
| Backends (trace) | CLAP timer-support, VST3 factory-context IRunLoop | identical |

## Issues

| # | Sev. | For | Issue | Repro |
|---|---|---|---|---|
| V1 | Low (harness, fixed) | harness | `reaper-up.sh` closed REAPER's evaluation nag with `xdotool windowclose` (XDestroyWindow). Under GNOME/X11 that killed `mutter-x11-frames` (X error in the journal); afterwards every new window, REAPER's and xmessage's, stayed `Iconic`/unmapped and the desktop looked empty. Fixed in 70fc65719: `wmctrl -c` with retries (the nag ignores a close during its countdown); Docker keeps the old path when `wmctrl` is missing | old `reaper-up.sh` on GNOME X11; `journalctl --user -b \| grep mutter-x11-frames`; recover with Alt+F2 `r` |
| V2 | Low | P3a owner | **The editor keeps drawing while its top-level is minimized.** Mutter (X11, compositing) sets `WM_STATE` Iconic on the FX window but leaves the client and our child window *viewable*, so no `UnmapNotify` reaches us and the frame timer draws and swaps as usual: 109–113 `Draw` calls in 4 s with REAPER minimized, the same as visible (`wm-controls-clap/RESULT.txt`). Wasted CPU/GPU for every minimized project with animated editors. Option: also stop on `_NET_WM_STATE_HIDDEN`/`WM_STATE` Iconic of the top-level ancestor (PropertyNotify on it, our connection), or on `VisibilityFullyObscured` where no compositor redirects | `vm-run.sh vm/probe/wm-behaviour.sh evidence/x "CLAP: IPlugControls" "~/.clap/IPlugControls.clap"` → `main-minimized: N Draw calls` |
| V3 | Info (driver) | P10 | **Mesa virgl leaks ≈ 270 KB per GL context lifetime that used texture/FBO/shader/VBO**, even with every object deleted before `glXDestroyContext`. Reproduced without iPlug by `scripts/vm/probe/glx-churn.c objects` (65 → 133 MB over 200 cycles, per-Display and shared-Display alike; a context that only clears and swaps does not grow; llvmpipe on the same VM is flat at 131 MB). In REAPER: ≈ 425 KB per editor open/close, 200 cycles → +240 MB (`churn200-controls-clap`); flat with llvmpipe (`churn100-llvmpipe-controls-clap`). RSS is flat while an editor stays open (`rss-open-controls-clap`). Not ours; worth one check on real Mesa hardware drivers and NVIDIA in P10. If a host or driver turns out to leak per context, keeping one GL context per Display/process instead of per editor would avoid it | `vm-x.sh bash -c 'gcc -O1 -o /tmp/glx-churn scripts/vm/probe/glx-churn.c -lX11 -ldl && /tmp/glx-churn 200 per-display objects'` (`glx-churn-probe.txt`) |
| V4 | Info | harness / P5 | The 1440×863 screen is too short for IPlugControls in REAPER: Mutter constrains the FX window to the work area, REAPER's container becomes 1024×766, our window stays 1024×768 and its bottom 2 px are clipped. Correct behaviour (no host resize); just a VM limit | `xwininfo-tree.txt` of any Controls run |

Observed, not issues: Mutter's frame windows carry the client's title, so `xdotool search --name` returns two windows
per REAPER window; the GNOME Activities overview (hot corner, Super) hides all windows and was open once during a run
(cause not pinned down), `vm-run.sh` now closes it before each run; `xwininfo -id ""` waits for a click on the screen.
After a VM reboot `jackdbus auto` was running in the session (D-Bus activated, from the jackd2 package); harmless.

## Not run

- `GDB_CATCH_EARLY`, ASan and the external-resize probe: nothing on the VM changes their outcome compared to Docker.
- The 100-cycle churn of notes/11 was replaced by 50 cycles per format plus one 200-cycle CLAP run (V3).
