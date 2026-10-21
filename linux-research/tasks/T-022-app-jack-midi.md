# T-022 — Standalone APP: JACK MIDI when the audio driver is JACK

- **Status:** in progress (the user asked for it on 2026-10-21; earlier: low priority, after the Gneiss/Sand work)
- **Owner:** T-022 worker (chat 6db96ccf)
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
- 2026-10-21 — implementation and tests (commit 6e8995437, then the harness/evidence commit).
  - **Code (Linux-only unless noted):**
    - `InitMidi` constructs RtMidi with `UNIX_JACK` when the audio driver is JACK, `LINUX_ALSA` otherwise, not
      `UNSPECIFIED`. `UNSPECIFIED` also opened a throwaway JACK client to probe, and could pick JACK on a machine
      without a sequencer.
    - `TryToChangeMidiApi()`: when the driver's API differs from the current one, it destroys both RtMidi objects
      first, then calls `InitMidi`, `ProbeMidiIO` and `SelectMIDIDevice` for the saved ports. A port the other API
      doesn't have turns off (existing behaviour). The driver combo calls it with `RefreshMidiControls` (resets and
      refills the four MIDI combos), and Cancel calls it too.
    - Cancel also calls `UpdateINI()`. While another driver is selected, `SelectMIDIDevice` turns the unknown port off
      and writes the whole state, the not-yet-applied driver included. Seen in Docker: `driver=0 indev=off` in
      settings.ini while ALSA was only selected.
    - The JACK MIDI clients are named `<app> MIDI in` / `<app> MIDI out`. RtMidi opens one JACK client per object at
      construction, before the audio stream. With `BUNDLE_NAME` they would take the name and push the audio client to
      `<app>-02`. The ALSA sequencer keeps the plain `BUNDLE_NAME`.
    - Shared: `ProbeMidiIO` clears both name lists first. On mac/win it runs once, so behaviour there is unchanged.
  - **Docker** `scripts/app-jack-midi-test.sh` → `evidence/t022/docker-jack-midi` **PASS**. Setup: jackd dummy,
    `jack_midiseq` → APP, echo → `jack_midi_dump`, no ALSA sequencer in the container. Checks:
    - The audio client keeps its name, and both MIDI connections come up from settings.ini.
    - The meter moves and the note-ons are echoed.
    - Selecting ALSA leaves no JACK MIDI ports and disables the MIDI combos (`04`).
    - Cancel restores the JACK connection and settings.ini.
    - PulseAudio → JACK: the saved ports are off, the lists show the JACK ports once (`06`, `07`), and a port picked
      from the list plays.
    - File > Quit exits 0. At `exit()` only main and `gmain` are left.
  - **VM** `scripts/vm/probe/app-jack-midi.sh` → `evidence/t022/vm-jack-midi` **PASS**. Setup: PipeWire's libjack,
    snd-virmidi, `amidi`. Checks:
    - JACK: Midi-Bridge port → `IPlugInstrument MIDI in`, no sequencer client, the note plays and is echoed.
    - PulseAudio: the list shows the sequencer's ports (`03`). The virmidi port picked there connects to sequencer
      client 128 'IPlugInstrument' and plays.
    - JACK again: the sequencer client is gone, and the Midi-Bridge port picked from the list (`05`) plays.
    - File > Quit exits 0. Under gdb at `exit()`: main and `gmain` only.
  - **macOS:** CMake/Ninja `IPlugInstrument-app` compiles the APP sources and links
    (`evidence/t022/macos/cmake-ninja-IPlugInstrument-app.txt`). The only warnings are in RtMidi, RtAudio and swell.
  - **Deviation:** a separate `app-jack-midi-test.sh` instead of extending `app-switch-test.sh`, which stays about the
    audio switch.
  - **Cross-platform findings** (report, not fixed):
    - **minor:** Preferences > Cancel doesn't undo a MIDI port change. Picking a MIDI device opens it at once
      (`IPlugAPP_dialog.cpp`, `IDC_COMBO_MIDI_IN_DEV`/`OUT_DEV` → `SelectMIDIDevice`). Cancel restores
      `mState.mMidiInDev` but reselects only the audio. Repro on any platform: pick another MIDI input, then Cancel.
      The app keeps listening on the new port while the state and the next settings.ini say the old one. On Linux,
      Cancel after a driver change with another MIDI API does reselect, through `TryToChangeMidiApi`.
  - **Next:** independent review.
