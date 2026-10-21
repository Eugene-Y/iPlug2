# T-013 — Host matrix (P10)

- **Status:** in progress (Carla, Ardour, Renoise done; drops, item 9 next)
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
- [ ] Matrix table (host × product × format × case) in the Log with evidence paths (`evidence/t013/`).
- [ ] Every failure: fixed (Linux-only code, own commit) or ticketed with a minimal repro; host bugs documented as such.
- [ ] PC checklist for Bitwig/x86_64 written.
- [ ] Independent review (AGENTS §4); Log complete.

## Log
(append: date — what was done — commits — next)

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
Renoise's `restartComponent` and the lambda's `_M_invoke`; the first versions came from an earlier script. While the transport is stopped the plug-in is suspended. Renoise then re-reads the latency
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
     - **R1-1 downgraded.** Ardour never sent a FOCUS_OUT while the X focus was ours. At the dropdown the X focus was
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
