# P3a and P3b on a real desktop: UTM VM, GNOME/Mutter on X11, Mesa virgl (2026-10-12/14)

Part 1 is P3a, part 2 (below) P3b. P3a as accepted in notes/12 (`51d211663`), built in Docker (24.04 image, Release) into `build/vm-p3a` from the
detached worktree `wt/vm-p3a`, and run on the VM. Evidence: `evidence/vm-p3a/`. Rig and gotchas:
`scripts/vm/README.md`.

**[V]** = run on the VM: Ubuntu 24.04.5 arm64, 4 CPUs, GNOME Shell 46 (Mutter) on Xorg, screen 1440×863,
OpenGL 2.1 Mesa 25.2.8 virgl over ANGLE/Metal (Apple M3), REAPER 7.82 aarch64 (dummy audio), PipeWire 1.0.5.
**[D]** = the Docker numbers of notes/11–12 (Xvfb, openbox, llvmpipe).

# Part 1: P3a

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

# Part 2: P3b (keyboard, focus, text entry, cursors, clipboard, menus, fonts)

`hvoya-linux` at `9c586203b` (P3b accepted in notes/15), detached worktree `wt/vm-head`, built in Docker (Release)
into `build/vm-head` and, with `-DIGRAPHICS_TRACE_LINUX_INPUT`, `build/vm-head-trace` (staged on the VM under
`~/iplug-builds/vm-head-trace`). IPlugControls CLAP, VST3, APP. Evidence: `evidence/vm-p3b/`. Same VM as part 1;
cursor theme `Yaru` (Xcursor resources; GNOME's `cursor-theme` gsetting says Adwaita), 24 px.

## Results

| # | Check | Result |
|---|---|---|
| 1 | Full `steps/iplugcontrols-p3b.txt` in REAPER, CLAP + VST3 (`vm-reaper-test.sh`): theme cursors, locked knob drag, IGraphics and stand-in menus choosing items, text entry commit/cancel ("13"+Escape: value kept, FX window stays), Ctrl+V/Ctrl+C with xclip **and** xsel, right-click Paste twice, Space reaching REAPER's transport, colour/font stubs, about box, reopen ×5, 2nd instance | **PASS ×2** [V] (`reaper-controls-{clap,vst3}/RESULT.txt`, `sheet.png`). Cursors by name from the real theme: `nwse-resize`, `default` (24×24), `blank` while dragging, pointer back at the drag start. Draw/X processing on tid 1 |
| 2 | P3b paths on the main thread (`input-thread-check.sh`, CLAP, full script under gdb) | `HandleKey` 39, `TakeKeyboardFocus`/`ReleaseKeyboardFocus` 12/12, clipboard get 4 / set 1, stand-in menu 3 / text entry 4, cursor set/hide/move 23/22/20, `ProcessXEvents` 3184: **all tid 1** [V] (`threads-clap/input-threads.txt`) |
| 3 | The 150 ms delayed focus take under Mutter, REAPER (`probe/focus-edit.sh`: 10× double-click the IVNumberBox from an unfocused FX window, type, Escape; human-like 80 ms clicks) | **10/10 CLAP (trace build) and 10/10 VST3**: value unchanged, FX window stays [V]. Focus trail per click of the double-click: Mutter focuses REAPER's FX window, GDK moves it to its 1×1 proxy within ≈ 25 ms; our request lands ≈ 150 ms after the second release and **holds** until the Escape returns it to the proxy. Trace: 10 takes, 0 retries, 10 releases (`focus-edit-*/focus-3.txt`, `editor-focus-trace.txt`). The 150 ms margin is ≈ 6× what Mutter + GDK need here |
| 4 | Timestamped `XSetInputFocus` (notes/15 option 1), observed only, with `probe/focus-ts-probe.c`: a GDK-like globally-active top-level (WM_TAKE_FOCUS → focus to a proxy child with the message's time) and an "editor" child, under Mutter | With press and release in different milliseconds (80 ms holds, plus a run with back-to-back `mousedown`/`mouseup`; 6–12 trials per mode): *no request* → proxy 6/6; *at release, CurrentTime* → editor 6/6; *at release, release time* → editor 12/12; *150 ms later*, either time → proxy at 50 ms, editor at 600 ms, 12/12. With `xdotool click` (press and release in the **same millisecond**) the timestamped request lost 2/5: Mutter's second WM_TAKE_FOCUS carries the same time, and the X server honours an equal time, so the later request wins (`focus-ts-probe-hold*/`). So on Mutter a request at the release works with either time and without delay, as long as the click lasts ≥ 1 ms; the delay is not needed here, but harmless |
| 5 | APP (`steps/iplugcontrols-app-p3b.txt`, with PipeWire audio) | All `expect-change` steps pass. **The final Escape (about box) closed the APP in 4 of 10 plain runs** (0 of 6 with the trace build): issue P3b-V1. `expect-same a03 → a04` fails in every run: harness, P3b-V5 (`app-controls*/`, `sheet.png` in `app-controls-run4/`) |
| 6 | Clipboard with GNOME apps (`probe/gnome-clip.sh`, CLAP) | **PASS both ways**: zenity (GTK4 entry) → Ctrl+V in the editor's edit → gain 37; the edit's Ctrl+C → zenity's entry gets "37"; gnome-text-editor (GTK4) → editor (58) and editor → gnome-text-editor (58, read back with Ctrl+A/C + xclip) [V] (`gnome-clip-clap/sheet.png`). **After the copying editor closes, other apps still read the text**: GNOME Shell's own clipboard manager (owner `0x400004`, no WM_CLASS; targets `text/plain;charset=utf-8 UTF8_STRING TARGETS TIMESTAMP`, not ours) takes over the moment the owner goes; `xclip` alone shows the same (`probe/clip-owner.c`). So notes/13's "an orphaned text is readable in this process only" does not hold on GNOME: every app keeps it there |
| 7 | Menus near screen edges (`probe/menu-edges.sh`: FX window dragged partly off screen by its title bar) | IGraphics popups (own and stand-in) stay inside the **editor**, not the screen. Right edge: the menus are narrow and fit. **Bottom edge: ICaptionControl's dropdown opens downwards into the off-screen part; only its first item is visible** (`menu-edges-clap/sheet-edges.png`): P3b-V2. Mutter keeps programmatic (`wmctrl`) moves on screen; only a user drag puts the editor partly off screen |
| 8 | REAPER's VST3 parameter context menu (IComponentHandler3) | **It does show on the real desktop** with a right click held ≥ ~50 ms; with `xdotool click 3` (same-millisecond release) GTK dismisses it at once, which is very probably what notes/13 finding 5 saw in Docker. "Show track envelope" chosen → the FX envelope exists. `IContextMenu::popup` is a **nested modal loop**: it returned after 4.2 s, when the item was chosen; **0 `Draw` calls during the menu** (the editor freezes, meters stop). Host actions sent meanwhile (ReaScript close FX window / delete FX) run only **after** the menu has closed and our `OnMouseDown` has returned: REAPER alive, no re-entrancy into the editor (`ctx-menu-vst3/`, `ctx-close-vst3/`, `ctx-delete-vst3/`) |
| 9 | Fonts by name with variable fonts (desktop distro, notes/15 carried item; `probe/fc-probe.cpp` with `IGraphicsLinuxFontconfig.h` + NanoVG's stb_truetype) | Named instances skipped, default instance (#0) taken and stb accepts it. **"Ubuntu Sans" Bold gets `UbuntuSans[wdth,wght].ttf #0`, i.e. the regular weight**, ahead of the static Noto Sans Bold: P3b-V4. Times New Roman → Liberation Serif (R/B/I correct), Arial → Liberation Sans, Cantarell (absent) → Noto Sans, DejaVu Sans Italic → DejaVu Sans regular (no italic file installed) (`fonts-by-name-desktop.txt`) |

## Issues (P3b)

| # | Sev. | For | Issue | Repro |
|---|---|---|---|---|
| P3b-V1 | Low–Med | P3b owner | **APP: a key can go to SWELL right after a click into the editor.** The APP takes the focus at the ButtonRelease (`mFocusOnClick`, CurrentTime). Mutter's click-to-focus sends WM_TAKE_FOCUS with the press time and SWELL's GDK then moves the focus to its proxy; when the press and release fall in the same millisecond that request is not stale, and if it arrives after ours the proxy wins. Escape then reaches SWELL and closes the APP. Seen in 4/10 runs of the APP script (always the final Escape on the about box), 1/12 in isolation with `xdotool click`, 0/12 with 50 ms clicks, 0/6 with the slower trace build. Focus trail of a failing trial: editor → proxy right after the click. Real users rarely click in < 1 ms (touchpad taps can be short), so mostly a test artifact, but the edit path already has the cure: also take the focus ≥ 150 ms after the release in the APP when it went to an ancestor (the `UpdateEditingState` retry rule), or use the release time and retry once | `vm-run.sh vm/probe/app-focus.sh evidence/x "~/iplug-apps/IPlugControls" 12 click edit` → "APP CLOSED by Escape"; or the APP script a few times |
| P3b-V2 | Low | P3b / P9 (Gneiss menus) | **IGraphics menus are clamped to the editor, not to the screen.** With the editor dragged partly below the screen, a dropdown near the visible bottom opens downwards into the invisible part; the items cannot be reached with the mouse. Native menus would flip. Option: in `CreatePlatformPopupMenu` (and Gneiss's own attached menus) shrink the bounds `IPopupMenuControl::Expand` uses to the part of the editor that is on screen (root coordinates of our window vs. `_NET_WORKAREA`/the screen) | `vm-run.sh vm/probe/menu-edges.sh evidence/x "CLAP: IPlugControls" "~/.clap/IPlugControls.clap"`, `e02-caption-bottom-edge.png` |
| P3b-V3 | Info | P3b notes / P10 | notes/13 finding 5 should read: REAPER's VST3 context menu **does** show (GTK, held right click); `popup()` blocks in a nested loop during which the editor gets no frames; REAPER defers its own actions until the menu closes, so no close-during-popup guard is needed for REAPER. Re-check in Docker with a held right click | `vm-run.sh vm/probe/ctx-menu.sh evidence/x "VST3: IPlugControls" "~/.vst3/IPlugControls.vst3"` |
| P3b-V4 | Low | P3b owner | **Bold/italic requests on a variable-font family return its default instance** (regular weight), because the variable file is offered first and named instances are skipped. On Ubuntu 24.04 the UI fonts (Ubuntu, Ubuntu Sans) are variable-only. Option: when the candidate is a variable font (`FC_VARIABLE`) and the style is not Normal, prefer the next static candidate of the requested weight/slant, or synthesise bold | `fc-probe "Ubuntu Sans"` (`fonts-by-name-desktop.txt`) |
| P3b-V5 | Low (harness) | harness | `steps/iplugcontrols-app-p3b.txt`: `expect-same a03-textentry-committed a04-textentry-cancelled 870 410 110 40` includes view x 870–885, the right edge of the animated IVDisplayControl; it differs (119 px) whenever the trace moved, independent of the edit. Narrow it to `890 410 90 40` | any APP run on a fast GL (here every run) |
| P3b-V6 | Info | notes/13 | On GNOME the clipboard survives the owning editor (GNOME Shell's clipboard manager); the in-process "orphaned" text matters for desktops without one | row 6 |

## Harness notes from this round

- `xact.sh`'s `click` sends press and release in the same millisecond. That ties X focus timestamps (rows 4, 5) and
  makes GTK menus close on the release (row 8). Probes that care hold the button (`mousedown`, sleep, `mouseup`).
- `set-clipboard` leaves `xclip` running as the selection owner, holding the caller's stdout: a pipeline (or an ssh
  session) waits until something else takes the clipboard. `vm-x.sh` now ends the session on a status line instead.
- `scripts/probe/focuswatch.c` calls `atof` without `<stdlib.h>`: its run time is garbage on gcc 13 (the trails in
  the first focus runs were cut short). The VM probes build it with `-include stdlib.h`; the source is untouched.
