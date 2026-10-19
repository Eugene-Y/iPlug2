# T-013 — Host matrix (P10)

- **Status:** in progress (Carla done; Ardour, Renoise next)
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
