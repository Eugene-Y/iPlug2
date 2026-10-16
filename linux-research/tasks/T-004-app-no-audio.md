# T-004 — Standalone APP robustness on Linux

- **Status:** ready
- **Owner:** —
- **Branch/worktree:** `linux/T-004` from `hvoya-linux`, in `/Volumes/Storage/code/IPlug2-T-004`.
- **Depends on:** nothing.
- **Plan phase:** P8.
- **Conflicts:** `IPlug/APP/*` (shared with mac/win — Linux-only changes or proven mac build).

## Goal
The standalone app is usable on a normal Linux desktop:
- starts without an audio device;
- lists and opens ALSA, JACK and PulseAudio devices — PipeWire provides the JACK/Pulse shims;
- the settings dialog works;
- MIDI input works;
- quits cleanly.

## Context
- **APP host:** SWELL on GDK plus RtAudio/RtMidi (notes/03, PLAN P8).
- **Main loop:** the APP main loop already uses GLib (P2, notes/09).
- **Known:** the APP segfaults when no audio device exists, so the harness runs it with `--no-io`.
- **Environment variable hacks:** RR introduced `IPLUG2_MENU_OFFSET`/`IPLUG2_SCREEN_SCALE`; P0 dropped `IPLUG2_SCREEN_SCALE`. Check whether anything similar remains.
- **Test environments:**
  - Docker has no audio. There you can test no-device startup and a dummy ALSA device (`snd-dummy` is not available in a container; consider `ALSA_CONFIG` with a null PCM, or a JACK dummy backend `jackd -d dummy`).
  - The VM has PipeWire (take the VM lock, AGENTS §5).

## Steps
1. Reproduce the no-device crash in Docker. Fix it: the app must start and show the UI with audio off and a clear message in the settings dialog.
2. Device enumeration and switching across ALSA/JACK/Pulse. Use JACK dummy in Docker and PipeWire on the VM.
3. Settings dialog: check SWELL dialog rendering and scaling under GNOME on the VM (screenshots); check that settings persist in XDG config.
4. MIDI: list devices (ALSA seq, and JACK MIDI when JACK is used). Test with `aconnect`/`amidi` virtual ports, e.g. `snd-virmidi` on the VM if available, or `aseqdump`.
5. Quit and close: no hang, no crash, audio thread stopped.

## Acceptance
- [ ] Starts with no audio device: UI shown, no crash, clear status.
- [ ] Plays audio through PipeWire (Pulse or JACK API) on the VM: the APP's own meter shows signal, `pw-top` shows the stream.
- [ ] Device switch works without restart; settings persist.
- [ ] MIDI input reaches the plugin (IPlugInstrument plays a note).
- [ ] Clean quit; no leaked threads in gdb at exit.
- [ ] mac APP unaffected (Linux-only changes or mac build proof). Evidence in `evidence/t004/`; self-review; Log complete.

## Log

- 2026-10-16 (from T-005, not a claim of this task): on the VM, the APP under gdb in VS Code logs `RtApiAlsa::probeDeviceOpen: unable to synchronize input and output devices` at start (ALSA `default` = PipeWire); audio still runs. Worth checking here (evidence `evidence/t005/vm/23-vscode-app-bp-processblock.png`).
