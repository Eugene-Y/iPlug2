# T-022 — Standalone APP: JACK MIDI when the audio driver is JACK

- **Status:** todo (low priority: after the Gneiss/Sand work; user, 2026-10-17)
- **Owner:** —
- **Branch/worktree:** `linux/T-022` from `hvoya-linux`, in `/Volumes/Storage/code/IPlug2-T-022`.
- **Depends on:** T-004 (merged).
- **Plan phase:** P8 follow-up.
- **Conflicts:** `IPlug/APP/*`.

## Goal
With the APP's audio driver set to JACK, its MIDI input and output use JACK MIDI ports. With ALSA and PulseAudio
they stay on the ALSA sequencer, as T-004 left them. Switching the driver in Preferences switches the MIDI API too,
without a restart. mac/win unchanged (AGENTS §2).

## Why it is low priority
On PipeWire (the default on current desktops) hardware and virtual MIDI ports already reach the APP through the
ALSA sequencer; T-004 verified that on the VM (`evidence/t004/vm-midi`). JACK MIDI matters mainly for setups running
plain jackd without the a2jmidid bridge.

## Context (from T-004)
- RtMidi is already compiled with `__UNIX_JACK__` (`Scripts/cmake/APP.cmake`); `RtMidiIn(RtMidi::UNIX_JACK, name)`
  works without build changes.
- `IPlugAPPHost::InitMidi()` runs once in `Init()`. On Linux it constructs RtMidi with `RtMidi::UNSPECIFIED` and
  `BUNDLE_NAME` as client name, so ALSA is used whenever the sequencer exists.
- `ProbeMidiIO()` appends to `mMidiInputDevNames`/`mMidiOutputDevNames` without clearing them; it would list ports
  twice after a second call.
- `PopulateMidiDialogs()` fills the MIDI combos only on `WM_INITDIALOG`; the driver-change handler
  (`IDC_COMBO_AUDIO_DRIVER` in `PreferencesDlgProc`) refreshes only the audio controls.
- Port names differ per API (`system:midi_capture_1` or PipeWire's `Midi-Bridge:…` against ALSA's
  `Client:Port 20:0`); a saved name from the other API is not found and falls back to "off" (existing behaviour).
- The JACK audio client is already named `BUNDLE_NAME`; a second JACK client with the same name gets JACK's
  `-01` suffix.
- RtMidi's JACK input runs its callback on a JACK thread. It still has one producer for `mMidiMsgsFromCallback`
  (single-producer queue), but check that the old RtMidi object is gone before the new one starts.

## Steps
1. Choose the MIDI API from the audio driver (Linux only): JACK → `RtMidi::UNIX_JACK`, otherwise `RtMidi::LINUX_ALSA`.
2. On a driver change, close and recreate the MIDI objects, clear and re-probe the port lists, refresh the MIDI
   combos, and reselect the saved ports (unknown → "off").
3. Tests:
   - Docker: `jackd -d dummy` plus a JACK MIDI source, e.g. a test client from `jack-example-tools` if the image has
     one, or a tiny libjack program under `linux-research/scripts/probe/`. Extend `scripts/app-switch-test.sh`.
   - VM: JACK through PipeWire (`APP_ENV=LD_LIBRARY_PATH=/usr/lib/aarch64-linux-gnu/pipewire-0.3/jack`, as in
     T-004): snd-virmidi appears as a JACK MIDI port; IPlugInstrument plays a note (`vm/probe/app-midi.sh` pattern).
4. No MIDI thread left at `exit()` (gdb, as in `app-switch-test.sh`).

## Acceptance
- [ ] On JACK, the MIDI combos list JACK MIDI ports; a note from a JACK MIDI source plays IPlugInstrument.
- [ ] ALSA ↔ JACK ↔ PulseAudio in Preferences switches the MIDI port lists without a restart; no duplicate entries.
- [ ] Clean quit on every driver; no leaked threads.
- [ ] mac/win unchanged (Linux-only code, or a mac build proof as in T-004). Evidence in `evidence/t022/`.

## Log
