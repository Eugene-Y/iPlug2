# T-013 — Host matrix (P10)

- **Status:** in review — every host done: Carla, Ardour, Renoise, Qtractor (VST3 on the VM, CLAP in Docker), a GNOME
  Wayland session (nested GNOME Shell, REAPER on its Xwayland, 2026-10-24), drops (case 8), item 9, PC checklist;
  Zrythm N/A (no arm64 package).
- **Owner:** coordinator (chat f91df715), lock `.linux-port-locks/T-013`
- **Branch/worktree:** `linux/T-013` from `hvoya-linux`, in `/Volumes/Storage/code/IPlug2-T-013` — for harness/docs and any host-compat fix.
- **Depends on:** T-002, T-007 (merged). Products: Gneiss (T-003), Sand (T-012).
- **Plan phase:** P10.
- **Conflicts:** uses the VM (lock, AGENTS §5). A host-compat fix in IGraphicsLinux/wrappers is coordinated with whoever holds those files.

## Goal
Gneiss, Sand, IPlugControls and IPlugInstrument work in the Linux hosts people use, not only REAPER: load, editor, automation, state, latency, multi-instance, close.

## Hosts
**User (2026-10-19): installing Carla, Ardour and Renoise on the VM is approved** (packages and the vendors' Linux downloads).
- **VM (arm64, GNOME X11):** Carla (CLAP + VST3; apt), Ardour 8 (VST3; CLAP if the version has it; apt or ardour.org arm64), Renoise (arm64 build; demo), Qtractor (CLAP/VST3), Zrythm if packaged for arm64. Also a GNOME **Wayland** session (XWayland) for one host.
- **Not possible here:** Bitwig (x86_64 only), x86_64 in general, NVIDIA/AMD drivers. Write a short checklist for a PC run.
- Installing on the VM needs `sudo`. Key auth only, never a password. If sudo asks for a password, stop and ask the user (through the coordinator) to install the packages.

## VM rules (it hangs under GL load — STATUS "Environment gotchas")
Short batches; `timeout 8 xdpyinfo` before and after each batch; kill everything you started; no display-mode or scale changes; churn only in Docker. Release the lock between batches.

## Cases per host × product × format
1. Scan/load; editor opens at the right size, renders (screenshot reviewed by eye), resizes (host- and plug-in-initiated), HiDPI where the host offers it.
2. Keyboard: text entry, host shortcuts still work with the editor focused; mouse: drag, wheel, right-click menus.
3. Automation write/read; parameter names/values/text in the host's UI.
4. Save/reload the project with the editor open and closed; presets (CLAP/VST3 state).
5. Latency change (Gneiss OS factor) with the editor closed — host PDC follows.
6. Two instances; close the editor while a menu or text entry is open; remove the plug-in while playing.
7. Host-loop trace build (`-DIPLUG_TRACE_HOST_LOOP`): which backend the host gives us (CLAP timer/fd support, VST3 `IRunLoop` and from where), everything on the main thread. **Decides T-010's open point:** if a host (Bitwig, Renoise) gives VST3 `IRunLoop` only through the plug frame, item 5 for VST3 runs on the fallback thread, and REAPER-style hosts drop that latency change (T-010 Log). Report it to the user; the user chose no framework change until such a host shows up.
8. Files dropped from GNOME Files (and a Qt file manager if available) onto the editor (`scripts/vm/probe/files-drop.sh`
   with Gneiss' USER tab, or `steps/igraphicstest-drop.txt` in Docker). The editor's window is XdndAware; whether a
   drop reaches it depends on the host: REAPER forwards drags to XdndAware children itself (but only a source's first
   three types, T-019 R6). Record per host whether it forwards, is XdndAware itself, or neither (then file managers
   cannot drop onto plug-ins); a trace build (`-DIGRAPHICS_TRACE_LINUX_INPUT`) logs `iPlug2 drop:` lines. Also a
   tooltip (Sand) in each host.

9. **From T-020** (merged aa42322c2, Docker-tested only): paste a file put on the clipboard by `SetFilePathInClipboard` into GNOME Files; drag a file out of the editor (`InitiateExternalFileDragDrop`, IPlugControls `-DIPLUG_TEST_DRAGOUT`) into Files and into each host; the real GNOME portal (Settings → Appearance dark/light switch, IPlugControls `-DIPLUG_TEST_APPEARANCE`); a GL3 build (`-DIGRAPHICS_RENDERER=GL3`) on virgl; the drag feedback cursor.

## Acceptance
- [x] Matrix table (host × product × format × case) in the Log with evidence paths (`evidence/t013/`). *Carla,
  Ardour, Renoise, Qtractor (cases 1–8), item 9, GNOME Wayland (REAPER on Xwayland, 2026-10-24); Zrythm N/A.*
- [x] Every failure: fixed (Linux-only code, own commit) or ticketed with a minimal repro; host bugs documented as such.
  *The XEmbed text-entry fix (c86d82e5d). Host bugs, documented with repros: Carla main's VST3 IRunLoop threading;
  Qtractor's CLAP timers stopping when the last CLAP editor closes. The hosts that drop no files on plug-ins are
  documented. **Open framework point, for the user (T-010 item 1):** with the editor closed, iPlug calls VST3
  `restartComponent` from its fallback thread, which the VST3 contract does not allow; Qtractor 1.5.3 then runs a
  state round trip on that thread (Log 2026-10-22, case 5).*
- [x] PC checklist for Bitwig/x86_64 written (section below; review approved, notes/28).
- [x] Independent review (AGENTS §4): approved in round 2, `notes/33-review-T-013-final.md` (R2-1, R2-2 fixed). The
  Wayland run: `notes/37-review-T-013-wayland.md` (verdict at the end of the Log).

## PC checklist (Bitwig, x86_64)
For a tester with an x86_64 PC running Linux, Bitwig Studio 5 and, if possible, REAPER. It covers what our test VM
cannot: Bitwig, x86_64, real graphics drivers and OpenGL 3.2. Start your host from a terminal and keep that window
open; the plug-ins print their diagnostics there.

**What you get from us.** Most testers get archives from us; nothing needs to be built:
- the release archives of Gneiss and Sand (`<Name>_v<version>_linux_x86_64_<edition>.tar.gz`), installed with
  `./install.sh` as in `notes/25-tester-checklist.md`;
- "trace" archives of Gneiss, Sand and the IPlugControls example: the same plug-ins with diagnostic output on
  stderr (host-loop and input traces). IPlugControls also has two test hooks: an orange square that starts a file
  drag, and a dark/light printout. Install a trace archive the same way; it replaces the release build of the same
  name;
- an IPlugControls archive built for OpenGL 3.2 (the "GL3" one), installed only for step 6;
- `collect-linux-report.sh`.

Advanced testers who build themselves need:
- the iPlug2 fork (branch `hvoya-linux`) and the plug-in repo (branch `linux-experiment`), side by side; we give
  you access to both;
- the packages from `Documentation/linux-development.md` §1;
- the SDKs: `cd <iPlug2>/Dependencies/IPlug && ./download-iplug-sdks.sh`.

Then build these (a build installs into `~/.clap` and `~/.vst3`):
```sh
I=<path to the iPlug2 fork>
# Gneiss or Sand with traces, from the plug-in's folder (Sand: replace Gneiss with Sand)
cmake -S . -B build-trace -G Ninja -DCMAKE_BUILD_TYPE=Release -DIPLUG2_DIR=$I \
      "-DCMAKE_CXX_FLAGS=-DIPLUG_TRACE_HOST_LOOP -DIGRAPHICS_TRACE_LINUX_INPUT"
cmake --build build-trace --target Gneiss-clap Gneiss-vst3
# IPlugControls with traces and the two test hooks, from $I; add -DIGRAPHICS_RENDERER=GL3 (and use build-gl3) for the GL3 one
cmake -S $I -B build-controls -G Ninja -DCMAKE_BUILD_TYPE=Release \
      "-DCMAKE_CXX_FLAGS=-DIGRAPHICS_TRACE_LINUX_INPUT -DIPLUG_TRACE_HOST_LOOP -DIPLUG_TEST_DRAGOUT -DIPLUG_TEST_APPEARANCE"
cmake --build build-controls --target IPlugControls-clap IPlugControls-vst3
```
The release archives themselves come from the plug-in repo's `scripts/makedist-linux.sh --arch x86_64`.

**Steps.** Run each in Bitwig, once with the CLAP and once with the VST3 version. Start with the user checks 1–11
of `notes/25-tester-checklist.md` (find and open, use, resize, type a value, tooltips, presets, automation, save and
reopen, oversampling, two at once, closing), using the trace archives. Then:
1. **Bitwig's plug-in hosting.** In Bitwig's Settings > Plug-ins, choose the plug-in hosting mode that runs each
   plug-in in its own process. As far as we know Bitwig 5 calls it "Individually". If your version names it
   differently, pick the closest one and tell us its name. Start Bitwig from a terminal: `bitwig-studio`.
   *Expected:* lines starting with `IPLUG_HOSTLOOP` appear in the terminal when a plug-in loads. If none do, look
   in Bitwig's log folder, usually `~/.BitwigStudio/log/`, and send it. If the lines are in neither place, tell us:
   that is useful to know too.
2. **Which event loop Bitwig gives the plug-in.** Load Gneiss, open its window, close it, open it again.
   *Expected:* the `IPLUG_HOSTLOOP` lines name a backend: `backend=clap-timer-support`,
   `backend=vst3-irunloop(…)` or `backend=fallback-thread`. Every `first … callback` line contains `on main`. Lines
   saying `OFF-MAIN` while the window is open are a problem; report them.
3. **Delay compensation while the window is closed.** This shows whether Bitwig still hears a latency change that
   happens while the plug-in window is closed. Put Gneiss on a track with a click or drum loop, and the same loop
   dry on a second track. Close Gneiss' window, then change its oversampling (OS) from 1x to 8x through Bitwig's
   own parameter view, while playing.
   *Expected:* the two tracks stay in time (no flam). Bitwig's latency figure for the device, if it shows one,
   changes. Report both, for CLAP and VST3, and the `IPLUG_HOSTLOOP` lines from that moment.
4. **Files dropped onto the plug-in.** In Gneiss, click USER (left column), then drag a `.hvoya` layout file (we
   send one) from GNOME Files onto an empty spot of the USER area, between the knobs. Repeat with Dolphin (KDE)
   if you have it. In a terminal, run `xprop XdndAware` and click Bitwig's plug-in window.
   *Expected:* the USER area changes to the dropped layout, and the terminal shows `iPlug2 drop: enter`, `drop`
   and `finished 1`. If nothing happens, say which of these you saw:
   - the drop icon turns into a "not allowed" sign over the plug-in (Bitwig accepts no drops there);
   - the drop is accepted, but Gneiss does not change (Bitwig keeps the drop itself);
   - nothing at all.
5. **A file dragged out of the plug-in.** Close Bitwig and start it with a file to drag:
   `IPLUG_TEST_DRAG_FILE=$HOME/some.wav bitwig-studio` (any short WAV file). Open IPlugControls (trace archive)
   and drag its small orange square, bottom right, onto a GNOME Files window, then onto an empty Bitwig track.
   *Expected:* Files copies the file; Bitwig creates a clip from it. While dragging, the pointer shows "not
   allowed" where nothing accepts and a hand where the drop is possible. The terminal shows
   `IPLUG_TEST_DRAGOUT start '…' -> 1`. If it shows `IPLUG_TEST_DRAGOUT start '' -> 0`, the variable did not reach
   the plug-in (Bitwig's plug-in process did not inherit it): tell us.
6. **Graphics.** On every graphics card you can use (NVIDIA with its own driver, AMD, Intel): open and close each
   plug-in window 20 times. Then install the GL3 IPlugControls and open it; hover the horizontal SVG slider for a
   tooltip.
   *Expected:* every window is fully drawn each time, nothing flickers or stays black, and the GL3 build draws like
   the normal one. If the GL3 window stays empty, send the line `no OpenGL 3.2 core context`.
7. **Typing and the keyboard.** In Sand, click a value under a knob, type a number, press Enter. Do it again and
   press Escape. Open an entry once more and click into Bitwig's own window. Then click the plug-in window (not a
   value) and press Space.
   *Expected:* Enter sets the value; Escape keeps the old one; a click into Bitwig ends the entry; Space reaches
   Bitwig (play/stop) unless an entry is open. Send the `iPlug2 input:` lines if not.
8. **Display scaling and Wayland.** Repeat steps 1–2 of `notes/25` (open, use) at 100 %, 150 % and 200 % display
   scaling (GNOME or KDE), and with Bitwig's own UI zoom. Then once in a Wayland session (choose it at login) and
   once in an X11 session.
   *Expected:* the plug-in window has the right size for the scale (not tiny, not cut off), text is sharp, and
   clicks land where you click.
9. **Dark and light.** With IPlugControls (trace archive) open, switch the desktop between dark and light style
   (Settings > Appearance).
   *Expected:* the terminal shows `IPLUG_TEST_APPEARANCE dark main=1` and then `light main=1` (or the reverse).

**Send back:**
- pass or fail per step, for CLAP and VST3, with your notes;
- the whole terminal output, or Bitwig's log folder (usually `~/.BitwigStudio/log/`). We need the lines starting
  with `IPLUG_HOSTLOOP`, `iPlug2 input:`, `iPlug2 drop:` and `IPLUG_TEST_`;
- the `xprop XdndAware` output from step 4;
- screenshots or a short screen recording of anything that looks wrong;
- the output of `sh collect-linux-report.sh` (in our repo: `linux-research/scripts/collect-linux-report.sh`).

## Log
(append: date — what was done — commits — next)

### Summary matrix (2026-10-22)
PASS = every case that was run passed; the cases not run are listed in the section. n.a. = the host has no such
format.
Case 7 = which event loop the host gives iPlug (all callbacks on the main thread unless said).

| Host (where) | Format | Gneiss | Sand | IPlugControls | Case 7 | Section |
|---|---|---|---|---|---|---|
| REAPER 7.82 (Docker, VM) | CLAP | PASS | PASS | PASS | timer + posix-fd support | notes/09; drops: T-019 |
| REAPER 7.82 (Docker, VM) | VST3 | PASS | PASS | PASS | factory host-context `IRunLoop`, editor open or closed | notes/09 |
| Carla 2.5.10 (VM) | VST3 | PASS | PASS | PASS | frame `IRunLoop` with the editor open; fallback thread when closed | 2026-10-19 |
| Carla 2.5.10 (VM) | CLAP | n.a. | n.a. | n.a. | — | 2026-10-19 |
| Carla main 97a9e07 (VM) | CLAP | PASS | PASS | PASS | timer + posix-fd support | 2026-10-19 |
| Carla main 97a9e07 (VM) | VST3 | **FAIL (host bug)** | not run | not run | frame `IRunLoop` called from Carla's engine thread | 2026-10-19, 2026-10-21 (2) |
| Ardour 8.4 (VM) | VST3 | PASS | PASS | PASS | frame `IRunLoop` open; fallback closed | 2026-10-20 |
| Ardour 8.4 (VM) | CLAP | n.a. | n.a. | n.a. | — | 2026-10-20 |
| Renoise 3.5.4 (VM) | VST3 | PASS | PASS (after c86d82e5d) | PASS (after c86d82e5d) | frame `IRunLoop` open; fallback closed | 2026-10-21 |
| Renoise 3.5.4 (VM) | CLAP | n.a. | n.a. | n.a. | — | 2026-10-21 |
| Qtractor 1.5.3 (VM) | VST3 | PASS | PASS | PASS | frame `IRunLoop` open; fallback closed | 2026-10-22 |
| Qtractor 1.5.3 (Docker) | CLAP | PASS, but host bug | PASS | PASS (case 7 only) | timer + posix-fd support; **all CLAP timers stop once the last CLAP editor closes (host bug)** | 2026-10-22 |
| Zrythm | — | n.a. | n.a. | n.a. | not packaged for arm64 | 2026-10-22 |
| REAPER 7.82 on GNOME Wayland (nested GNOME Shell 46, Xwayland 23.2.6, VM) | CLAP | PASS | PASS | not run | timer + posix-fd support | 2026-10-24 |
| REAPER 7.82 on GNOME Wayland (same) | VST3 | PASS | PASS | not run | factory host-context `IRunLoop` | 2026-10-24 |
| Bitwig 5 (x86_64) | CLAP, VST3 | PC checklist | | | | PC checklist |

Item 9 (T-020's VM checks), section 2026-10-21 (2): all PASS except the GL3 build, which virgl cannot render
(OpenGL 2.1 only); Renoise received a dragged-out file without making an instrument of it; drag-out to Carla N/A.

### 2026-10-19 — hosts installed; Carla
**Hosts on the VM** (aarch64, Ubuntu 24.04, GNOME X11; sudo is passwordless for the key user):
- Ardour 8.4.0 (apt `ardour`).
- Renoise 3.5.4 demo, arm64 (`~/hosts/Renoise_3_5_4_Demo_Linux_arm64`, run from its folder; files.renoise.com).
- Carla: Ubuntu 24.04 has no arm64 `carla` package (only `carla-data`) and upstream ships no Linux builds, so built from source with apt build deps: **2.5.10** (`~/hosts/Carla`, release; VST3, no CLAP) and **main** 97a9e07 (`~/hosts/Carla-main`; CLAP and VST3).
- Qtractor, Zrythm: not yet.

**Builds:** hvoya-linux 86b87408c (T-025 in), Release with `-DIPLUG_TRACE_HOST_LOOP -DIGRAPHICS_TRACE_LINUX_INPUT`: Gneiss, Sand (linux-experiment 2cab58b / 87ccdbf), IPlugControls; deployed to `~/.clap`, `~/.vst3`.

**Carla harness:** `scripts/vm/probe/carla-probe.py` drives Carla's backend library (`libcarla_standalone2`, the engine without Carla's Qt GUI) on a dummy `jackd`: load; editor open (Carla's own X11 host window) + screenshot; parameter dump; set a parameter from the host with the editor open; state save → change → load (value restored); a second instance with its editor; editor close/reopen ×5; remove the second instance with its editor open; remove the first. `carla-run.sh` wraps it for `vm-run.sh`.

| Carla | Plug-in | Format | Result | Host loop (trace) |
|---|---|---|---|---|
| 2.5.10 | Gneiss | VST3 | PASS 13/13; editors render (`03-two-editors.png`) | factory/initialize: no IRunLoop; editor open: frame IRunLoop on the main thread; closed: fallback thread |
| 2.5.10 | Sand | VST3 | PASS | same |
| 2.5.10 | IPlugControls | VST3 | PASS | same |
| main | Gneiss | CLAP | PASS | timer-support, all on the main thread |
| main | Sand | CLAP | PASS | same |
| main | IPlugControls | CLAP | PASS | same |
| main | Gneiss | VST3 | **core dump** after the second instance's editor opened (`carla-main-gneiss-vst3/`) | frame IRunLoop callbacks (timers **and the X connection fd**) arrive on a Carla thread (tid ≠ the thread that attached the view) |

- **Carla main VST3 is a Carla bug, not the port's:** `CarlaPluginVST3.cpp` (main) dispatches the frame IRunLoop's timers and fds in `runIdleCallbacksAsNeeded`, called from `idle()` (Carla's engine thread) and from `getChunkData`/`setChunkData`/`reload`/`activate`, not only from `uiIdle()`. The VST3 contract is that IRunLoop handlers run on the UI thread; the plug-in's editor (X events, drawing) is then driven from two threads. 2.5.10 delivers them on the main thread. Report upstream (the user decides). Nothing in iPlug can make a host's own thread safe.
- **T-010 item 1 (latency from the fallback thread):** Carla gives VST3 a loop only through the frame, so with the editor closed `OnIdle` runs on the fallback thread, as the source survey said; Carla ignores VST3 latency restarts anyway. Not a regression.
- Evidence: `evidence/t013/carla-*/` (steps, stderr traces, screenshots; PNGs local only).

### 2026-10-19 (2) — Ardour started; the VM's virgl GPU hung
- Ardour: `ardour-vst3-scanner` (with `LD_LIBRARY_PATH=/usr/lib/ardour8`) finds Gneiss, Sand, IPlugControls and caches them. `scripts/vm/probe/ardour/make-session.lua` (headless `ardour8-lua`, "None (Dummy)" backend) builds `~/t013-ardour` with one track per plug-in (VST3) and saves it. In this headless session iPlug reports "the host provides no event loop" (fallback thread); the GUI's behaviour is still to be checked.
- Launched the Ardour GUI with that session: its first-run wizard. While stepping through the wizard with clicks, the **VM's virgl GPU hung** (dmesg: `dma_fence_wait_timeout` ← `drm_atomic_helper_wait_for_fences`, a kworker in D state; X stopped answering, `xdotool`/`import` hang). This is the known hang (STATUS "Environment gotchas", now the 4th time), this time after the Carla runs (≈ 12 editor opens) and Ardour's start; no churn ran on the VM.
- **Paused: the VM needs Force Stop + Start in UTM (the user).** Then: Ardour wizard → session → open each plug-in's editor (mixer strip), host-loop trace, automation, save/reload; Renoise; drops; tooltips.
- Note for the hang question: the trace builds open a GL context per editor; Ardour itself is GL-free (GTK2/Cairo).

### 2026-10-20 — Ardour 8.4 (VST3), after the VM restart
Same builds as Carla (hvoya-linux 86b87408c + trace flags). No first-run wizard: `touch ~/.config/ardour8/.a8` (Ardour's
"been here before" file). Scripts in `scripts/vm/probe/ardour/` (`open-gneiss.sh`, `quit.sh`, `gneiss-reopen.sh`,
`gneiss-relaunch.sh`, `gui-probe.lua` for Window > Scripting — Ardour's GUI Lua is sandboxed, no `dofile`/`io`, so the
probe is typed in as one line). Evidence `evidence/t013/ardour/` (PNGs local).

| Case | Gneiss | Sand | IPlugControls |
|---|---|---|---|
| 1 editor opens, renders | PASS (`08`) — but see the logo bug below | PASS (`12`) | PASS (`13`) |
| 2 mouse: knob drag | PASS (A hz 316 Hz → 9.39 kHz, `09`) | — | — |
| 2 text entry | — (no text entry in Gneiss) | PASS: click value, type `2000`, Enter → 2.00 kHz; Ardour takes no keys | — |
| 2 host shortcut with the editor focused | Space starts Ardour's transport (`09`) | | |
| 2 tooltip | | PASS (`a-law compander…`) | |
| 2 right-click on a param control | | | no menu: Ardour gives no `IComponentHandler3` context menu, iPlug returns silently (same on every OS) |
| 4 save with the editor open, quit, reopen | PASS: value and preset restored, Ardour reopens the editor (`11`) | | |
| 5 latency, editor closed | PASS: OS 1x→8x from Ardour Lua: insert latency 0→8, route playback latency 1040→1048; back to 1x → 0/1040 (`20`, `21`) | | |
| 6 close the editor with an iPlug menu open | | | PASS (`15`, `16`): first click on the close button closes it, Ardour lives |
| 7 host loop | editor open: frame `IRunLoop` on the main thread (fd + timer); closed: fallback thread | same | same |

- Ardour's Lua parameter indices are the plug-in's + 1 (Ardour puts Bypass at 0); values are plain.
- **Found: SVG logo drawn as light boxes on an editor's first frame** (the user's report) → **T-034** (iPlug's NanoVG
  main FBO stencil is never cleared; fix verified here: unfixed 3 of 4 first opens broken, fixed 0 of 5).
- **VM/GNOME gotchas (not iPlug):** `xdotool windowclose` *destroys* a window (XDestroyWindow) — never use it on a
  host's windows: it crashed `mutter-x11-frames`, gnome-shell did not restart it, and every server-decorated window
  (Ardour is GTK2) stayed unmapped, then Ardour exited on BadWindow. Recovery without a session restart:
  `setsid /usr/libexec/mutter-x11-frames &` in the session. Close windows with their close button. Ardour's ALSA
  backend hung in `ardour-request-device` (PipeWire device reservation) after a killed run; switched the VM's Ardour
  to the Dummy backend (`~/.config/ardour8/config`, backup `config.t013-alsa`).
- Next: Renoise; drops from GNOME Files (case 8) in Carla/Ardour; item 9 (T-020's VM checks); automation lanes.

### 2026-10-21 — Renoise 3.5.4 (VST3; no CLAP in Renoise)
**Builds:** linux/T-013 after merging hvoya-linux (merge 92738e8c7: T-034 stencil fix, T-023 CLAP rescan), Release with
`-DIPLUG_TRACE_HOST_LOOP -DIGRAPHICS_TRACE_LINUX_INPUT`: Gneiss (linux-experiment 773867e), Sand (052a0bc),
IPlugControls, built in Docker against this worktree, deployed to `~/.clap`, `~/.vst3`. Sessions 1–2 ran that build;
session 3 the same plus the XEmbed fix below.

**Renoise 3.5.4 on Linux hosts VST2, VST3, LADSPA and DSSI, not CLAP** (no CLAP path, setting or string in the
binary; it scans `~/.vst3` only). So every CLAP cell is N/A. It scans in a separate `AudioPluginServer_arm64` process
but runs plug-ins in its own process (the trace's pid is Renoise's). An editor is a Renoise top-level window
("HvoyaAudio: Gneiss") that embeds ours via XEmbed ("Plugin window attached via `_XEMBED_INFO`"). Renoise calls
`XInitThreads` itself. Driving: `scripts/vm/probe/renoise/` (below); evidence `evidence/t013/renoise/` (PNGs local).

| Case (VST3) | Gneiss | Sand | IPlugControls |
|---|---|---|---|
| 1 scan, editor opens, renders | PASS (`06`), logo fine | PASS (`20`) | PASS (`28`), instrument |
| 1 resize, plug-in-initiated | PASS: MORPH off 940×620→940×200 (`08`); corner resizer 940×200→1020×217 (`47`); Renoise's window follows | — | — |
| 1 resize, host-initiated; HiDPI | N/A (`PLUG_HOST_RESIZE 0` in all three); HiDPI not tried (VM at scale 1) | | |
| 2 mouse | knob drag PASS (`09`, A hz 933 Hz→20 kHz), wheel PASS (`10`) | tooltip PASS (`27`) | IVMenuButton menu PASS (`29`) |
| 2 text entry | — (none in Gneiss) | **FAIL → fixed** (Linux-only commit below): `53`–`55` value 10 kHz→`2000`→2.00 kHz; click into Renoise commits (`56`) | same bug and fix: IEditableTextControl (`58`, `59`) |
| 2 host shortcut (Space) with the editor focused | no: host design (finding 2); with Renoise's main window focused it plays (`11`) | | |
| 2 right-click on a control | no menu (`12`): Renoise implements `IComponentHandler` only, no `IComponentHandler3` | | no menu (`48`) |
| 3 names/values in the host | PASS: Lua lists 63 names and value strings | PASS (28) | PASS |
| 3 automation read | PASS: an A hz envelope plays, the knob follows (`31-*`) | | |
| 3 automation write | gestures reach Renoise: begin/perform/end on the main thread, right id (`param-gestures.txt`), Renoise's value follows; Renoise wrote no envelope, but not for its own Gainer either: my recording setup, not settled | | |
| 4 save with the editor open / closed, change, reload | PASS: Main hz 12.7 kHz and the MORPH-off layout restored (`43`); Renoise does not reopen editors | PASS (closed): 0.770 | PASS (closed): Gain 33 % |
| 5 latency, editor closed (OS 1x↔8x while playing) | restart honoured, latency re-read (PDC value not observable): `SetLatency` on the fallback thread; Renoise moves the restart to its main thread (`latency-editor-closed.txt`) | | |
| 6 two instances | PASS, both editors draw (`45`) | | |
| 6 close the editor with a menu open | | | PASS (`29`→`30`) |
| 6 remove the plug-in while playing, editor open | PASS, no hang (`46`) | | |
| 6 load another song / quit with editors open | PASS, clean teardown (`stderr-session2.txt`) | PASS | PASS |
| 7 host loop | see below | same | same |

**Case 7 (decides T-010's open point).** VST3 in Renoise, the same for all three:
- factory `setHostContext` and `initialize`: no `IRunLoop` → fallback thread;
- editor attached: the plug frame's `IRunLoop`; timers and the X connection fd are called on Renoise's main thread;
- editor closed: back to the fallback thread.

So Renoise is a host that reaches the fallback with the editor closed. **It does not drop the latency change.**
`restartComponent(kLatencyChanged)` (flag 8) arrives on the fallback thread; Renoise queues it: its
`std::function` lambda (`TVst3Plug::restartComponent(int)::{lambda()#1}`) runs on the main thread and calls
`restartComponent` again there, which then does `setProcessing(0)`, `setActive(0)`, `setActive(1)`,
`getLatencySamples`, `setProcessing(1)` (`latency-editor-closed.txt`, lwp = pid). Editor open
(`latency-editor-open.txt`): `restartComponent` arrives on the main thread and does the same at once, no lambda.
Both files were re-run in review round 1 with the committed `call-trace.sh` and `latency.syms`, which now also lists
Renoise's `restartComponent` and the lambda's `_M_invoke`; the first versions came from an earlier script.
While the transport is stopped the plug-in is suspended. Renoise then re-reads the latency
when processing resumes (`getLatencySamples` on an audio thread). The PDC value itself is not visible: Renoise has
no API for it. **For T-010: no framework change needed for Renoise.** Bitwig remains the only unknown (x86_64 only, PC
checklist).

**Findings**
1. **Fixed (Linux-only): Renoise's XEmbed `FOCUS_OUT` ended every text entry as it opened.** A click on an editable
   value creates the entry and takes the X focus (`XSetInputFocus` + XEmbed `REQUEST_FOCUS`). Renoise then sends
   `FOCUS_OUT` (5), `WINDOW_DEACTIVATE` (2) and `FOCUS_IN` (4) ×2, and keeps the X focus on our window. iPlug ended
   the edit on `FOCUS_OUT` (`DeclineKeyboardFocus` → `CommitEdit`), so the typed keys went "to host"
   (`textentry-sand-unfixed.txt`, with gdb backtraces and the XEmbed message codes).
   - **Fix 196036808** (round 0): `FOCUS_OUT` ends an edit only when the X focus is no longer ours.
   - **Replaced by c86d82e5d** (review R1-1). A `FOCUS_OUT` now ends the edit only if no `FOCUS_IN` follows within
     150 ms, decided in `UpdateEditingState`. XEmbed messages are traced (`XEmbed message N`).
   - **VM, session 4** (`stderr-session4-r1.txt`; `60`–`63`): `5, 2, 4 → "FOCUS_IN answers the FOCUS_OUT: the edit
     stays"`. Typing 3000 and Enter gives 3.00 kHz; a click into Renoise commits 700 Hz.
   - **Ardour, with the new build** (`evidence/t013/ardour/r1-input-trace.txt`; `34`–`40`):
     - Ardour's GtkSocket sends EMBEDDED_NOTIFY, FOCUS_IN and WINDOW_ACTIVATE at attach, and nothing on the edit's
       focus take. The edit works: 1500 → 1.50 kHz.
     - A click on a header button of the plug-in window (an ArdourButton) moves the X focus to Ardour's window,
       an ancestor of ours (`FocusOut mode 0 detail 0`), and sends no FOCUS_OUT. The edit stays open, as before any of
       these fixes: a focus move into the host's own window keeps an edit (`LoseKeyboardFocusToHost`). Ardour still
       sees the keys first: Space starts its transport (`37`), and only the release is forwarded to us.
     - Opening the preset dropdown then sends MODALITY_ON, FOCUS_OUT and WINDOW_DEACTIVATE, and no FOCUS_IN. The
       edit ends (`XEmbed FOCUS_OUT unanswered: the edit ends`; 1.50 kHz committed, `38`).
     - Afterwards a typed `9` goes to Ardour, which forwards it to us; we drop it because the edit is closed (`39`).
       Space starts and stops Ardour's transport (`40`).
     - **R1-1 downgraded.** In the sequences tried, Ardour never sent a FOCUS_OUT while the X focus was ours. At the dropdown the X focus was
       already on Ardour's window, so 196036808 would also have ended the edit there. The reviewer's case (a real
       FOCUS_OUT with the X focus still ours) did not occur in Ardour 8.4. The pairing rule stays: it does not depend
       on where the X focus is, and it costs no X round trip.
   - **Docker REAPER regression** with c86d82e5d: IPlugControls `iplugcontrols-p3b.txt` and Sand `sand.txt`, CLAP +
     VST3: all PASS (`evidence/t013/renoise-fix-r1-docker/`; round 0: `renoise-fix-docker/`).
2. **Host shortcuts with the editor focused do not work in Renoise (host design, nothing to fix in iPlug).** With
   Renoise's plug-in window focused, Renoise forwards every key to the plug-in as a synthetic event
   (`(sent by a client): dropped` in the trace). iPlug drops unhandled synthetic keys, which avoids a loop with
   REAPER. With the pointer over the editor the key comes to iPlug directly; iPlug forwards it to its parent, and
   Renoise ignores it. Space works once Renoise's main window has the focus.
3. Gneiss: with MORPH on, a knob drag before a morph point is selected changes nothing (`07`, `34`). That is Gneiss's
   morph UX, not the host: with MORPH off the same drag works (`36`).

**Probes** (`scripts/vm/probe/renoise/`, run through `vm-x.sh`):
- `launch.sh [log] [song]`: starts Renoise from its folder with stderr in the log and clicks "Ok" on its start-up
  notices.
- `rn-lua.py '<lua>'`: Lua in the running Renoise over OSC `/renoise/evaluate`. Insert devices
  (`insert_device_at("Audio/Effects/VST3/<uid>")`, an instrument with `plugin_properties:load_plugin`), open and
  close editors (`external_editor_visible`), set parameters, automation, transport, save and load. The sandbox has no
  `io` and its `print` does not reach stdout, so a result goes to the status bar (`show_status`), to be read off a
  screenshot.
- `call-trace.sh <out> <secs> <syms> <actions…>`: gdb (the sudo wrapper) logs hits with the caller's LWP while it
  runs Lua (`lua:`) or shell (`sh:`) actions.
  Symbol lists:
  - `latency.syms`: iPlug's VST3 latency path. Renoise calls through the interface thunks (`_ZThn…`), and a tiny body
    such as `getLatencySamples` is duplicated into them, so a break on the plain method misses those calls.
  - `textentry.syms`, `mouse.syms`: IGraphics text entry and mouse.
  - `edits.syms`: Renoise's own `TVst3Plug::beginEdit/performEdit/endEdit/restartComponent` (its binary is not
    stripped).

**VM/host gotchas**
- The process is `./renoise`: use `pgrep -x renoise`, not a path pattern.
- The second and later starts show an RT-thread warning dialog (no RT rights); `launch.sh` clicks Ok.
- `Ctrl+Q` does not quit: use File > Quit, then "No" in "Save changes?".
- The demo saves songs, and no nag dialog appeared.
- Audio: ALSA `default` (PipeWire), picked by itself.
- `~/.config/Renoise/V3.5.4/Config.xml`:
  - `ShowWelcomeDialog` is now false.
  - The OSC server (UDP 8000 on all interfaces, evaluates Lua) is **off** again. Turn it on for `rn-lua.py`
    (`OscServerEnabled`, or Preferences > OSC) and off afterwards.
  - The original is kept as `Config.xml.t013-orig`.
- Renoise logs to stdout and to `~/.config/Renoise/V3.5.4/Log.txt`.
- While gdb holds Renoise, mutter greys its windows ("not responding"); this is harmless.
- `~/t013-renoise/t013.xrns` holds Gneiss (track 1), Sand (track 2) and IPlugControls (instrument 1).
- Recording plug-in or Gainer tweaks into automation did not work as set up: edit mode and the transport were
  started from Lua; `record_parameter_mode` was tried in both modes.

### 2026-10-21 (2) — drops (case 8) and T-020's VM checks (item 9)
**Builds** (worktree at f9644cb91):
- Gneiss, Sand and IPlugControls trace builds as in the Renoise session (`-DIGRAPHICS_TRACE_LINUX_INPUT`, deployed).
- IPlugControls `-DIPLUG_TEST_DRAGOUT -DIPLUG_TEST_APPEARANCE` + input trace, GL2 and `-DIGRAPHICS_RENDERER=GL3`, staged
  in `~/iplug-builds/t013-t020-{gl2,gl3}`. For the drag into Ardour and Renoise the GL2 one was put in `~/.vst3`; the
  trace build is back there since.
- `evidence/t020/clipboard/clipboard-file-test.cpp`, built in Docker.

**Probes** (`scripts/vm/probe/t013/`, through `vm-x.sh`):
- `reaper-open.sh`: the harness REAPER with one plug-in in a floating FX window.
- `drop-chain.sh`: the window chain under a point, with XdndAware/XdndProxy.
- `editor-view.sh`: the editor's own window and its root position.
- `drop-from-files.sh`: a real mouse drag from GNOME Files.
- `carla-hold.py`: Carla's backend with the editor open for N seconds.
- `dragout-files.sh`, `dragout-to.sh`: drag-out with XFixes cursor samples.
- `clip-paste-files.sh`.

xdotool's press/move/release from a Files icon starts a real GTK XDND session; Files' `GDK_DEBUG=dnd` log shows
Enter/Position/Drop. The scripted source `scripts/probe/xdnd-source.c` (`deep` = Qt's choice of the deepest
XdndAware window) stands in for a Qt file manager. None is installed on the VM, and no packages were added.

**Case 8 — a `.hvoya` layout dragged from GNOME Files onto Gneiss' USER tab** (a gap between its controls; the layout
loaded is the visible result). Evidence `evidence/t013/drops/<host>/` (`RESULT.txt` with the window chain, the drop
trace and Files' XDND lines; PNGs local).

| Host, format | Plug-in window XdndAware | Files → editor | What the host does | Qt-like source (deepest aware window) |
|---|---|---|---|---|
| REAPER (T-019) | yes, 5 | PASS | forwards to our child as its own v3 source (first three types only, T-019 R6) | — |
| Ardour 8.4, VST3 | yes, 5 (GTK2 top-level) | **PASS**: layout loaded; trace `enter v5 / drop / finished 1`; Files gets XdndFinished from Ardour | forwards to the embedded editor | not needed |
| Carla 2.5.10, VST3 | yes, **3** (Carla's own host window) | **no**: Carla answers and finishes the drop itself (Files logs XdndFinished from it); nothing reaches the editor | XdndAware itself, does not forward | PASS: layout loaded |
| Carla main, CLAP | no | **no**: GTK finds no target | neither | PASS |
| Carla main, VST3 | not checked: Carla crashes after loading Gneiss (below) | — | — | — |
| Renoise 3.5.4, VST3 | no (its main window is: 5, for its own file drops) | **no**: on the way the drag crosses Renoise's main window, which accepts (`Entering Xdnd window 0x2c0009b`, Status 461); over the plug-in window GTK finds no target ("async callback for #2c000b0", not XdndAware), and the release finishes nothing | neither | PASS: layout loaded (`after-qt-like-deep-gap.png`) |

- **Carla main VST3 crashes after loading Gneiss VST3 alone: a host bug, documented (user 2026-10-22: no framework guard against individual hosts' bugs; `Documentation/linux-development.md` §8).**
  - The first run ended in apport (SIGSEGV, no stack). Re-run under gdb, it reproduced
    (`evidence/t013/drops/carla-main-vst3/crash.txt`).
  - Carla main calls the plug frame's `IRunLoop` timer from its engine thread (`CarlaEngineRunner`); the trace says
    `first timer callback … on OFF-MAIN`. On that thread Gneiss' `OnIdle` rebuilds its layout and loads a font into
    NanoVG (`IGraphics::LoadFont` → `nvgCreateFontFaceMem`). Meanwhile the main thread is inside GLX: gdb shows
    `libGLX_mesa` without symbols, and that it is opening the editor is inferred from the trace order.
    It crashes there.
  - The frames are iPlug's and Gneiss', but the cause is the Carla-main bug of 2026-10-19: VST3 run-loop handlers
    must run on the UI thread. That time it took a second editor; this time the timer fires during the first open.
  - Minimal repro: `carla-hold.py ~/hosts/Carla-main vst3 ~/.vst3/Gneiss.vst3 "" 15` under a dummy jackd.
  - Reported to the coordinator. No iPlug change made. A framework option would be to ignore run-loop callbacks
    that arrive off the thread that attached the view; that is a decision for the user.
- **Conclusion.** Every editor accepts drops from a source that addresses it, so iPlug's side works everywhere.
  Whether a GTK file manager (GNOME Files) can drop onto a plug-in depends on the host:
  - **Works:** REAPER and Ardour forward the drop.
  - **Swallowed:** Carla 2.5.10 is XdndAware itself and keeps the drop.
  - **No target:** Carla main (CLAP) and Renoise are neither.
  - For iPlug, the only lever is `XdndProxy` on the host's top-level, rejected in T-019: GTK 4 follows it for the
    first position only. No change proposed: it is host behaviour, and a Qt source reaches the editor in every host.
- Tooltip (Sand): Renoise PASS (Log above), Ardour PASS (2026-10-20). Carla not checked.

**Item 9 — T-020's VM checks.** Evidence `evidence/t013/t020/` (text committed; screenshots local, as everywhere).

| Check | Result | Evidence |
|---|---|---|
| GL3 build on virgl | **cannot render here**. virgl offers OpenGL 2.1 only (`Max core profile version: 0.0`). The open fails with the intended message ("no OpenGL 3.2 core context …", plus the trapped `GLXBadFBConfig`); REAPER stays alive and shows its generic UI. Rendering GL3, and a tooltip under GL3, go to the PC checklist. | `gl3-virgl.txt`, `01-gl3-clap-open.png` |
| `SetFilePathInClipboard` → paste into GNOME Files | **PASS**. The clipboard offers `text/uri-list` and `x-special/gnome-copied-files` (`copy`, percent-encoded UTF-8); Ctrl+V in Files copies `Preset #1 ünï.fxp` into the folder. | `clipboard-paste/` |
| Real GNOME portal, dark/light | **PASS**. IPlugControls (`IPLUG_TEST_APPEARANCE`, REAPER CLAP): `dark` at open, `light` after `color-scheme` → `default` (what Settings → Appearance writes), `dark` after the restore; all `main=1`. Re-run with `scripts/vm/probe/t013/appearance-toggle.sh /tmp/reaper.log`, which puts the user's setting (`prefer-dark`) back on any exit. | `appearance-portal.txt`, `reaper-gl2-stderr.txt` |
| Drag out → GNOME Files | **PASS**. `drag me.wav` is copied into the folder (`after:` in RESULT.txt); Files, the target, logs XdndEnter, Positions, Drop (`files-dnd.txt`). | `dragout-files/` |
| Drag out → REAPER | **PASS**. REAPER's tracks go from 1 to 2; track 2 is "drag me", with the dragged file as its media (`reaper-tracks.txt`). The run used `dragout-reaper.sh`; the first runs had dropped onto REAPER by accident. | `dragout-reaper/` |
| Drag out → Ardour | **PASS**. Ardour imports the file: the copy appeared in the session's `interchange/…/audiofiles/` (`NOTE.txt`); a region shows on a track (local screenshot). The session was not saved, and the copy was removed afterwards. | `dragout-ardour/` |
| Drag out → Renoise | **delivered**. Renoise's log: `Dropped file (1 from 1): '…/drag me.wav'`; dropped on its song browser, which makes no instrument from it. | `dragout-renoise/` |
| Drag out → Carla | N/A: Carla's backend without its GUI has no drop target. | — |
| Drag feedback cursor | **PASS** (XFixes cursor names along the drag): `default` over the square before; `not-allowed` where nothing accepts; the hand `pointer` over accepting targets (REAPER's windows, our own editor, Files); `default` again after the release. | `dragout-files/RESULT.txt`, `dragout-to` RESULTs |

**VM notes**
- GNOME Files' first start (Tracker) can take several seconds before its window maps. A drag started before that
  lands on whatever lies below, so `dragout-files.sh` now expects Files to be open already.
- A press in REAPER's FX window also raises REAPER's main window over Files: move the main window aside first.
- `pgrep -f '[c]arla-hold'` inside a `vm-x.sh bash -c` matches that bash's own command line, so the loop waits
  forever. Match the interpreter's argv instead.
- `xwininfo -id ""` waits for a click; `reaper-open.sh` guards against it.
- `rpr.sh` run from a later `vm-x.sh` session started a second REAPER instance instead of reaching the running one,
  so `dragout-reaper.sh` does everything in one session.
- Commands as run, on the VM:
  - Renoise's OSC server, for `rn-lua.py`. On, while Renoise is not running:
    `sed -i 's#<OscServerEnabled>false</OscServerEnabled>#<OscServerEnabled>true</OscServerEnabled>#' ~/.config/Renoise/V3.5.4/Config.xml`
  - Off afterwards: `pgrep -x renoise || sed -i 's#<OscServerEnabled>true</OscServerEnabled>#<OscServerEnabled>false</OscServerEnabled>#' ~/.config/Renoise/V3.5.4/Config.xml`
  - The IPlugControls test build into `~/.vst3` and `~/.clap` for the drag-out in Ardour and Renoise (from the Mac):
    `scripts/vm/vm-deploy.sh build/t013-t020-gl2 IPlugControls`
  - The trace build back afterwards: `scripts/vm/vm-deploy.sh build/t013-rn-controls IPlugControls`, then
    `LD_LIBRARY_PATH=/usr/lib/ardour8 /usr/lib/ardour8/ardour-vst3-scanner -f ~/.vst3/IPlugControls.vst3`, so that
    Ardour's cache matches.
- Clean-up: Renoise quit without saving and its OSC server is off again. Ardour quit with "Just quit" (the session
  file is unchanged). Nautilus and jackd are stopped, apport's dialog is answered "Don't send", and the GNOME
  appearance is back to `prefer-dark`. The VM lock is released.

### 2026-10-22 — Qtractor 1.5.3 (VST3 on the VM, CLAP in Docker), Zrythm; the VM's virgl hung again; Wayland not run
**Branch:** merged hvoya-linux e3709bd89 into linux/T-013 (29d9410c0). No framework code changed in this session.

**Hosts.**
- Qtractor: Ubuntu 24.04 has 0.9.39 (noble) and **1.5.3** (noble-backports). Installed 1.5.3 on the VM (approved
  host install): `sudo apt-get install -t noble-backports qtractor`. It hosts CLAP, VST3, LV2, VST2, LADSPA, DSSI.
- **Zrythm: N/A.** Neither apt (noble, backports) nor snap has an arm64 package; nothing else was installed.

**Builds.**
- Trace builds (Release, `-DIPLUG_TRACE_HOST_LOOP -DIGRAPHICS_TRACE_LINUX_INPUT`, `build/t013-rn-*`): this branch at
  29d9410c0, Gneiss linux-experiment 6246fb7, Sand 95be3b0. Deployed to the VM's `~/.clap`, `~/.vst3`, replacing a
  Debug Gneiss left by another task.
- **Left on the VM at the end: Release builds without traces** (`build/t013-rel-*`, same commits:
  iPlug2 29d9410c0, Gneiss 6246fb7, Sand 95be3b0) of Gneiss, Sand and IPlugControls in `~/.clap` and `~/.vst3`. They
  were deployed over ssh after the hang (no X needed). Ardour's VST3 cache still describes the trace builds; rescan
  there if it complains.

**Probes** (`scripts/vm/probe/qtractor/`):
- `launch.sh [log] [session]`: a dummy `jackd` and Qtractor, with stderr in the log.
  - Qtractor sends stdout/stderr to its Messages window by default. On the VM its `Display\StdoutCapture` is now `false`
    in `~/.config/rncbc.org/Qtractor.conf` (first config Qtractor wrote kept as `Qtractor.conf.t013-orig`).
- `make-session.py`: builds a `.qtr` with one track per plug-in from the session Qtractor saved
  (`evidence/t013/qtractor/t013vst3.qtr`).
- `latency.syms` (VST3) and `latency-clap.syms` (CLAP), for `renoise/call-trace.sh`. That script now takes `PROC=`
  (the process name), stamps every hit with the time, and line-buffers its output.
- `latency-closed-seq.sh` and `restart-seq.sh`: the case-5 sequences used on the VM.
- `count-hits.sh`: counts the calls of given functions over N seconds.
- Harness: `dock.sh` passes `DOCK_ARGS` to `docker run`. The Docker Qtractor needs the ALSA sequencer:
  `DOCK_ARGS="--device /dev/snd" scripts/gui-up.sh t013q`, then `apt-get install -t noble-backports qtractor` in it.

**Qtractor on the VM, VST3** (GNOME X11, virgl). Evidence: `evidence/t013/qtractor/` (PNGs local); traces in
`traces/vst3-s1.log`, `vst3-s2.log`.

How Qtractor shows an editor: a Qtractor top-level ("Gneiss - Track 1 — Qtractor") with the plug-in's view embedded.
It opens the editor when a plug-in is added, and on a double click on the plug-in in the track list. It does not
reopen editors when a session loads.

| Case (VST3) | Gneiss | Sand | IPlugControls |
|---|---|---|---|
| 1 scan, editor opens, renders | PASS (`01`, `06`), logo fine | PASS (`02`) | PASS (`03`) |
| 1 resize, plug-in-initiated | PASS: MORPH off, 940×620 → 940×200, Qtractor's window follows (`08`) | — | — |
| 1 host-initiated resize, HiDPI | N/A (`PLUG_HOST_RESIZE 0`; VM at scale 1) | | |
| 2 mouse | knob drag 171 Hz → 15.7 kHz, wheel 51.2 → 113.4 Hz (`09`; with MORPH on a drag changes nothing, Gneiss' UX, as in Renoise) | tooltip PASS (`17`) | IVMenuButton menu PASS (`04`) |
| 2 text entry | — (none in Gneiss) | PASS: 10 kHz → `2000` → 2.00 kHz (`15`, `16`, input trace `15-…txt`) | |
| 2 host shortcut (Space) with the editor focused | no: iPlug passes the key on to Qtractor's window ("to host" in the trace), Qtractor does nothing with it (`10`). With Qtractor's main window focused Space plays (`11`) | | |
| 2 right-click on a control | | no menu (`18`): Qtractor's VST3 host has no `IComponentHandler3` | |
| 3 names, values in the host | PASS: Qtractor's plug-in form lists them with iPlug's value text (`14`) | | |
| 3 host sets a value, editor open | PASS: the form sets oversampling 8x → 1x and the editor shows `os 1x` (`latency/editor-open-os1x.png`) | | |
| 4 save with the editor open, quit, reopen | PASS: values and the MORPH-off layout restored (`12`, `13`) | | |
| 5 latency, editor closed | see below | | |
| 6 two instances | PASS, both editors draw (`19`, `20`) | | |
| 6 close the editor with a menu open | | | PASS: one click on Qtractor's close button, Qtractor lives (`04` → `05`) |
| 6 remove while playing, editor open | **not run**: the VM hung on the way (below) | | |
| not run | case 3 automation write/read; case 4 with the editor closed, presets | | |
| 7 host loop | see below | same | same |
| 8 drop from GNOME Files | see below | | |

**Case 7, VST3.**
- At factory `setHostContext` and at `initialize`, Qtractor 1.5.3 gives no `IRunLoop`, so iPlug uses its fallback
  thread.
- With the editor open, the plug frame's `IRunLoop` is used: timers and the X connection fd are called on the main
  thread.
- With the editor closed, iPlug is back on the fallback thread.
- Gneiss and Sand: `vst3-s*.log` (`first … backend=vst3-irunloop(frame) on main`). IPlugControls: its editor ran in
  the first session, whose trace went to Qtractor's Messages window and was not kept; `05` shows its "VST3 view
  removed … vst3-irunloop(frame) -> fallback-thread" on the main tid. Off-main lines come only from fallback threads.
- Qtractor 1.6.4's source also answers `IRunLoop` on the host context, which iPlug would prefer. But that loop is
  driven by the same shared `QTimer` that closing the last VST3 editor stops (`startTimer(200)`/`stopTimer()` around
  editors, as in the CLAP bug below; 1.5.3's frame loop has it too, unnoticed because iPlug leaves the frame loop when
  the editor closes). By that reading, in 1.6.x with every VST3 editor closed `OnIdle` would not run at all. Untested:
  no arm64 package.

**Case 5, VST3: Qtractor re-reads latency when playback starts; iPlug's restart request comes off the UI thread.**
Setup: Gneiss' oversampling changed with Qtractor's plug-in form (Track 1 > Gneiss > Properties, Page 2), under
`call-trace.sh` (`latency/`). `latency-editor-{closed,open}.txt` come from the earlier `call-trace.sh` (no time
stamps, hits printed after all actions) and the `latency.syms` without `getState`/`setState`;
`restart-editor-closed.txt` from the committed versions. Which `getLatencySamples` hit belongs to the Track dialog
or to playback start follows from Qtractor's source and the dialog screenshots, not from the traces.
- With the editor closed, `SetLatency` runs on the fallback thread (`latency-editor-closed.txt`, lwp ≠ pid). With the
  editor open, it runs on the main thread (`latency-editor-open.txt`).
- **iPlug breaks the VST3 contract here (T-010 item 1):** `IComponentHandler::restartComponent` is `[UI-thread]`
  (`ivsteditcontroller.h`), and `IPlugVST3::SetLatency` calls it from `OnIdle`, which runs on the fallback thread when
  the host gives no loop.
- Qtractor 1.5.3's `restartComponent` does the same state round trip for every flag (`getState` + `setState`, plus its
  parameter refresh; `kReloadComponent` adds deactivate/activate), synchronously on the calling thread, as the
  contract lets it. With the editor closed that is iPlug's fallback thread: `restart-editor-closed.txt` shows
  `getState`/`setState` with the fallback lwp right after each `SetLatency`, while Qtractor's main thread runs on.
- How the hosts here take the off-thread request: Renoise moves it to its main thread (2026-10-21); Qtractor runs it
  in place; Carla ignores latency restarts; Ardour honours the change (2026-10-20), on which thread is not known.
- Qtractor reads the latency (`getLatencySamples`, main thread) when playback starts (`resetAllMonitors` →
  `resetLatency`) and when the Track dialog shows it. It shows "(no latency)" at 1x and "0.2 ms (8 frames)" at 8x,
  editor closed both times (`lat-1x.png`, `lat-8x.png`). So the compensation follows at the next transport start,
  whichever thread the change came from. **For T-010 (the user decides):** Qtractor 1.5.3 drops no latency change,
  but it runs the plug-in's state round trip on iPlug's fallback thread. That is iPlug's own contract breach, not a
  host bug, so the "no guard for single hosts" rule does not settle it. Not fixed here: a framework change.

**Case 8, VST3** (`drop-vst3/`). The drop target was Gneiss' USER tab.
- **A drag from GNOME Files does not reach the editor.** GTK addresses Qtractor's top-level, which is XdndAware (v5,
  Qt). Qtractor answers `XdndStatus` "refuse" and does not forward the drop (`RESULT.txt`: chain, Files' XDND log, no
  `iPlug2 drop:` line).
- **A Qt-like source (`xdnd-source deep`) reaches the editor**: `enter v5 / drop … over editor 1 / finished 1`. The
  layout loads when the drop lands in a gap of the USER panel (editor 360,100, `after-qt-like-deep-gap.png`).
- On a knob, or at 251,71 or 360,74, the drop is accepted but Gneiss ignores it. T-019 described this:
  `UserTabPanel::OnDrop` runs only where the panel is the topmost control, and the knobs sit in other places when
  MORPH is off.
- `scripted-source.txt` has every scripted run: `top` (a GTK-like source, refused like Files), and `deep` at 251,71,
  360,74 (twice), then 360,100, 444,21 and 251,131 (the last three from the console).

**Qtractor CLAP, in Docker** (Xvfb + openbox + llvmpipe, Qtractor 1.5.3 from backports). It was not run on the VM,
which had hung. Same trace builds; session from `make-session.py`. Evidence: `evidence/t013/qtractor/docker-clap/`.

| Case (CLAP) | Gneiss | Sand | IPlugControls |
|---|---|---|---|
| 1 editor opens, renders | PASS (`01`), logo fine | PASS | — |
| 1 resize, plug-in-initiated | PASS: MORPH off → 940×200 (`02`). Qtractor warns "could not get the resize hints of the plugin GUI": iPlug's CLAP editor is not host-resizable | | |
| 2 text entry, tooltip | | PASS: `2000` → 2.00 kHz, tooltip (`sheet-sand-clap-text-tooltip.png`) | |
| 3 names, values in the host | PASS (`03`) | | |
| 7 host loop | timer + posix-fd support, all on main (`stderr-run*.txt`, no `OFF-MAIN`) | same | same |

- Before it creates the real instance, Qtractor creates and destroys one with a host that offers no timer or fd
  support. iPlug briefly starts its fallback thread for it and stops it again.
- **Qtractor bug: CLAP plug-ins' timers stop when the last CLAP editor closes.**
  - Cause: one shared `QTimer` drives every plug-in's registered timers. Opening an editor does `startTimer(200)`
    (a reference count); closing it does `stopTimer()`, which stops that shared `QTimer` at count 0, although the
    plug-ins' timers are still registered (`qtractorClapPlugin.cpp`, 1.5.3 and v1.6.4 alike).
  - Measured (`clap-timer-counts.txt`, `timers-fresh-session.txt`, `onidle-count-editor-closed.txt`):
    - before any editor was opened: about 150 `clapOnTimer` calls/s for the three plug-ins;
    - with an editor open: the same rate;
    - after it closed: **0** `clapOnTimer` and 0 `Gneiss::OnIdle` calls for every plug-in, while `process` keeps
      running.
  - Effect for iPlug: everything that runs from the idle timer stops until a CLAP editor opens or a plug-in
    registers a new timer (which restarts the shared `QTimer`): `OnIdle` (Gneiss reports its latency there) and the
    processor-to-editor queues. Output to the host is unaffected: it goes through `process`.
  - Case 5 for CLAP is not observable in Qtractor for this reason, and also because Qtractor ignores `latency.changed`
    and skips `request_restart` while its engine runs. Qtractor reads `latency.get` on the main thread when playback
    starts (`latency-clap.txt`), as it does for VST3.
  - Report it upstream to Qtractor (the user decides). An iPlug workaround would need a guard for a single host,
    which the user ruled out.
- Not run in Docker: IPlugControls' editor; case 3 automation; cases 4 and 6; drops.

**The VM's virgl GPU hung again.** At 16:53, during a right-click into Qtractor's track list:
- Before it: about 10 editor opens over 20 minutes, the last a second Gneiss instance (940×620) with two Gneiss
  editors open.
- X stopped answering (`xdpyinfo` timed out). Xorg sits in `drm_atomic_helper_swap_state`, and a kworker is in D state
  in `dma_fence_wait_timeout` ← `drm_atomic_helper_wait_for_fences` (`hang/dmesg-hung-task.txt`, `vm-state.txt`).
- Qtractor's main thread waits in `xcb_wait_for_reply` (`QBackingStore::beginPaint`, `hang/hang-qtractor-bt.txt`).
- Same signature as 2026-10-19. Killed what I started (xdotool, Qtractor, the dummy jackd) and released the lock.
- **The VM needs Force Stop + Start in UTM (the user).** It still answers ssh.

**Wayland: not run** (the VM hung first). Two ways need no password and no user (ssh key, passwordless sudo, GDM
autologin). Both start after the user's VM restart; take the VM lock.
1. **Nested, preferred** (the X11 session stays): `vm-x.sh bash -c 'dbus-run-session -- gnome-shell --nested --wayland
   > ~/t013-nested.log 2>&1 &'`.
   - GNOME Shell 46 has `--nested` and starts Xwayland on demand (`xwayland` is installed).
   - Read `~/t013-nested.log` for the Xwayland display (`:1` …).
   - Start REAPER with `DISPLAY=:1` (`reaper-open.sh` with that `DISPLAY`) and check: the editor opens and draws;
     mouse; text entry in Sand; the trace's `iPlug2 input:` lines.
   - Close the nested window with its close button. This is XWayland under Mutter, not a real Wayland login.
2. **A real Wayland session.**
   - Back up `/etc/gdm3/custom.conf`, set `WaylandEnable=true`.
   - Set `Session=ubuntu` in `/var/lib/AccountsService/users/gene` (it is empty now, and `ubuntu.desktop` is the
     Wayland session).
   - `sudo systemctl restart gdm3`: autologin brings up GNOME on Wayland. That ends the X11 session and every app in
     it.
   - `vm-display-env` then needs Xwayland's `XAUTHORITY` (`/run/user/1000/.mutter-Xwaylandauth.*`) and its `DISPLAY`.
   - Run the same checks with REAPER.
   - **Restore:** put back `custom.conf` (`WaylandEnable=false`), clear `Session=`, restart `gdm3`, and check
     `timeout 8 xdpyinfo` on `:0` under X11.
   - Both ways load virgl like any GL test: keep it to a few opens.
   - If the user prefers to do it by hand: log out, pick "Ubuntu" (not "Ubuntu on Xorg") under the gear on the login
     screen, start REAPER from a terminal, open Gneiss and Sand, click, type a value, then log back into "Ubuntu on
     Xorg".

**VM state left:**
- X11 session hung (needs Force Stop + Start).
- Qtractor 1.5.3 installed, with `StdoutCapture=false`.
- Release builds in `~/.clap` and `~/.vst3`.
- Sessions and logs in `~/t013-qtractor/`.
- No process of mine running; VM lock released.
- Not changed: GDM (X11, autologin), Renoise and Ardour configs, the GNOME appearance.

**Cross-platform findings:** none new.

### 2026-10-23/24 — GNOME Wayland: nested GNOME Shell, REAPER on its Xwayland
**Branch:** hvoya-linux b8079e9df (T-038 merged) fast-forwarded into linux/T-013. No framework code changed.

**Builds.** Trace builds (Release, `-DIPLUG_TRACE_HOST_LOOP -DIGRAPHICS_TRACE_LINUX_INPUT`, `build/t013-rn-{gneiss,sand}`)
from this worktree at b8079e9df, Gneiss linux-experiment 6246fb7, Sand 95be3b0. Staged on the VM in
`~/iplug-builds/t013-wl` (`vm-deploy.sh -p`); `~/.clap` and `~/.vst3` were not touched and still hold the Release
builds from T-038 eda432127.

**Session.** The nested way (the X11 session stays; GDM and AccountsService untouched):
`MUTTER_DEBUG_DUMMY_MODE_SPECS=1280x768 setsid dbus-run-session -- gnome-shell --nested --wayland` inside the X11
session. GNOME Shell 46.0 (mutter 46.2) runs as a Wayland compositor in an X11 window and starts Xwayland 23.2.6 on
`:1` (auth `/run/user/1000/.mutter-Xwaylandauth.*`, Wayland display `wayland-0`). REAPER started with `DISPLAY=:1`,
`WAYLAND_DISPLAY=wayland-0`, `XDG_SESSION_TYPE=wayland` still runs as an X11 client of Xwayland (its windows are in
`:1`'s tree), as it would in a Wayland login. Input was sent with xdotool to the **outer** X11 screen, so it reaches
REAPER and the plug-in through the nested compositor, as a person's mouse and keyboard would.
- Probes (`scripts/vm/probe/t013/`, through `vm-x.sh`): `nested-env.sh` (the nested `DISPLAY`/`XAUTHORITY`/bus),
  `nested-open.sh` (the harness REAPER with one plug-in, update notice and nag closed), `nested-view.sh` (the editor's
  position on the outer screen), `crossing-watch.c` (Enter/Leave of another client's window).
- Evidence `evidence/t013/wayland/` (PNGs local), stderr and XI2 traces in `logs/`.

| Case | Gneiss CLAP | Gneiss VST3 | Sand CLAP | Sand VST3 |
|---|---|---|---|---|
| editor opens, renders (logo fine) | PASS `02` | PASS `30` | PASS `10` | PASS `20` |
| plug-in-initiated resize (MORPH off, 940×620 → 940×200), REAPER's window follows | PASS `06` | PASS `31` | — | — |
| mouse drag | A hz 457 Hz → 20.0 kHz `06` | A hz 211 Hz → 20.0 kHz `31` | comp 0.300 → 0.624 `13` | comp, arc grows `22` |
| wheel | see below | | | |
| text entry (click value, type, Enter) | — (none in Gneiss) | — | 10 kHz → `2000` → 2.00 kHz `11`, `12`; a second entry `31` → 31.0 Hz (`13`) | `2000` → 2.00 kHz `21`, `22` |
| tooltip / Gneiss hover help | help bubble `03` | help bubble `32` | tooltip `14` | tooltip `22` |
| Space with the editor clicked → host transport | plays `07` | plays `31` | | |
| case 7 (host loop) | timer + posix-fd, on main | factory-context `IRunLoop`, on main | same as Gneiss CLAP | same as Gneiss VST3 |

Findings:
1. **Wheel: not testable through the nested compositor (harness).** `xdotool click 4` on the outer screen did not
   reach Xwayland at all: its XI2 root trace shows the pointer motion and no button or scroll event
   (`logs/xi2-wheel-via-compositor.txt`); mutter's nested X11 backend did not forward the synthetic wheel. The same
   wheel sent by XTEST into Xwayland (`:1`) reaches the editor: B hz 535.8 → 853.6 Hz
   (`logs/xi2-wheel-xtest-on-xwayland.txt`). Nothing points to a Wayland-specific problem; a real wheel in a Wayland
   login stays a PC-checklist item (step 8).
2. **Xwayland sends no LeaveNotify when the pointer leaves for a non-X surface.** Moving from the editor onto
   REAPER's arrange view gives `Leave` and hides Gneiss' help bubble (`32`). Moving onto the Wayland desktop, or out of
   the nested compositor, gives no `Leave` (`logs/crossing-gneiss-vst3.txt`: one Leave/Enter pair for three exits),
   so the bubble stays (an iPlug tooltip too: it hides on LeaveNotify) until the pointer comes back (`33`, `04`). That is Xwayland's behaviour for
   every X11 client; nothing to fix in iPlug.
3. **Keys reach the edit through REAPER, as on X11.** The input trace has focus lines but no per-key lines for these
   edits; the XI2 trace has the key events addressed to REAPER's FX window (`logs/xi2-sand-clap-text-entry.txt`).
   For VST3, REAPER hands keys to the view through `IPlugView::onKeyDown`, which is not traced; for CLAP the Docker
   X11 REAPER trace has no key lines either (`renoise-fix-r1-docker/sand-clap/reaper-stderr-tail.txt`). The edits'
   results are on screen.
4. No `OFF-MAIN` line in any run (`logs/*-stderr.txt`).

Not run under Wayland: IPlugControls, right-click menus, drops (case 8), automation, state; one host only (REAPER),
as the task asked. A real Wayland login (GDM) was not needed: the nested shell ran.

**Harness notes (nested GNOME Shell on the VM)**
- The first start (2026-10-23 08:12, `MUTTER_DEBUG_DUMMY_MODE_SPECS=1280x760`) never mapped its window (640×480,
  unmapped) and its Xwayland never answered (polling only its Wayland socket; `logs/nested-shell-run1-stuck.log`). A
  start without the variable (800×600) and the next with `1280x768` worked (`logs/nested-shell.log`). Not reproduced;
  if it sticks, kill it and start again.
- A killed nested shell leaves `/tmp/.X1-lock`, `/tmp/.X2-lock`, `/tmp/.X11-unix/X1`, `X2`,
  `/run/user/1000/wayland-0{,.lock}`, `.mutter-Xwaylandauth.*`, and `/run/user/1000/gnome-shell-disable-extensions`
  (which makes the next GNOME Shell start without extensions). Remove them. Closing its window with the title bar's
  close (`wmctrl -i -c`) ends it cleanly and removes all of them; the services it started on its private bus exit with
  it.
- REAPER 7.82 now shows a modal "New Version Notification" (7.83) over the evaluation nag at every start;
  `nested-open.sh` closes both with `wmctrl -c`.
- Between the 2026-10-23 and 2026-10-24 sessions (the worker stopped at a usage limit, leaving the nested shell and
  REAPER running), the outer GNOME opened its Activities overview and Gneiss CLAP's FX window moved and its A/B res went
  to 1.0000; the take/release-focus pairs in `logs/gneiss-clap-stderr.txt` date from then. Not caused by the probes.
- `xinput test-xi2 --root` on Xwayland warns that it is Xwayland and works.

**VM state left:** the nested shell, REAPER and the dummy jackd are stopped, the stale files above removed, X11 `:0`
answers `xdpyinfo`; GDM, AccountsService and the GNOME settings unchanged; the trace builds stay staged in
`~/iplug-builds/t013-wl`; `~/.clap` and `~/.vst3` hold the Release builds of T-038 eda432127. VM lock released.

**Cross-platform findings:** none.
