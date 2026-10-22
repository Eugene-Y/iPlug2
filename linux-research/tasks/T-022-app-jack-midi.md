# T-022 — Standalone APP: JACK MIDI when the audio driver is JACK

- **Status:** in progress (R1-2 per the user's decision, 2026-10-22; the rest was approved in review round 2, `notes/29-review-T-022.md`)
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
- [x] On JACK, the MIDI combos list JACK MIDI ports; a note from a JACK MIDI source plays IPlugInstrument.
- [x] ALSA ↔ JACK ↔ PulseAudio in Preferences switches the MIDI port lists without a restart; no duplicate entries.
- [x] Clean quit on every driver; no leaked threads.
- [x] mac/win unchanged (Linux-only code, or a mac build proof as in T-004). Evidence in `evidence/t022/`.

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
- 2026-10-21 — review round 1 (`notes/29-review-T-022.md`): request changes, one blocker. Fixed in 47951f3a0.
  - **R1-1 blocker, fixed:** a JACK MIDI output object now exists while the output is "off". IPlugInstrument echoes
    every note into it, and RtMidi queues them; picking an output later dumped the backlog into one cycle and crashed
    the APP (`jack_midi_event_reserve` → NULL). Now `SendMidiMsg`/`SendSysEx` send only while
    `IsMidiOutPortOpen()`, a Linux-only atomic flag cleared before `closePort` and set after `openPort`.
    - Re-run of the reviewer's `adversarial.sh` A–E → `evidence/t022/fix-adversarial`:
      - C (30 s with the output off, then an output picked): no crash, and 0 stale events reach the monitor.
      - A, B, D: as in the review.
      - E: the existing Cancel bug, below.
  - **R1-3, done:**
    - `checks.txt` lines that only report are `info:` now, in both harnesses.
    - Quit on every driver: the VM probe's gdb run is now on PulseAudio, with the sequencer input picked from the
      list. It checks the connection to client 'IPlugInstrument' and the note, then quits; main + `gmain` at `exit()`.
      The JACK quit stays covered by the VM's File > Quit run and by Docker's gdb run. ALSA uses the same RtMidi
      sequencer objects as PulseAudio.
  - **R1-4, R1-6 (computed once), done.** The assert for "audio stopped" is not added: both callers stop audio first
    (`TryToChangeAudioDriverType` → `CloseAudio`), and the reviewer checked that.
  - **R1-5, not done:** in RtMidi (third-party, not modified).
  - **R1-2, open, for the user to decide:** the saved MIDI ports are lost when the API cannot see them. The cases are a
    driver round trip through the other API, and a start on JACK while no JACK server is running: settings.ini gets
    `indev=off`. This is the task's "unknown → off" rule. Keeping a saved name the current API doesn't list can stay
    Linux-only (reviewer, round 2).
  - Re-runs after the fix: Docker `evidence/t022/docker-jack-midi` PASS, VM `evidence/t022/vm-jack-midi` PASS,
    macOS CMake `IPlugInstrument-app` links with no warnings in the APP files.
  - **Cross-platform findings from the review** (bugs that predate T-022; reported, not fixed):
    - **major:** after a driver change and back in Preferences, Cancel leaves the audio off
      (`IPlugAPP_dialog.cpp`, the IDCANCEL branch). The reviewer's scenario E: 2 JACK audio ports before, 0 after
      Cancel.
    - **minor:** `--no-io` writes `indev=off outdev=off` over the saved MIDI ports at start
      (`IPlugAPP_host.cpp`: `Init` → `SelectMIDIDevice` with no RtMidi objects).
- 2026-10-22 — review round 2: **approve** (`notes/29-review-T-022.md`, 67e04f9f1). The R1-1 fix holds on every
  open/close path. Re-run of scenario C, plus the reviewer's new C2 (output on → off for 30 s → on): no backlog, and the
  output works again afterwards (`evidence/t022/review2-adversarial`). mac/win unchanged.
  - **Hand-over:**
    - Commits: 6e8995437, 8a21c9ce9, 55979e325, 47951f3a0, df033f7a7, 67e04f9f1.
    - Evidence: `evidence/t022/{docker-jack-midi,vm-jack-midi,macos,fix-adversarial,review-*}`.
    - Open: R1-2 (user decision) and the two cross-platform findings above.
- 2026-10-22 — **R1-2 per the user's decision:** keep a saved MIDI port the current API can't see, Linux-only. Code
  in 509dbe71a.
  - **`TurnOffMissingMidiPort`** now holds the four places that turned a missing port into "off" and saved the
    settings. On Linux it keeps the name; on mac/win it does what it did before. The port stays closed, and the combo
    shows "off" until the port is back.
  - **`ReopenSavedMidiPorts()`** runs on Apply and OK on Linux. It opens saved ports that weren't open and refreshes
    the MIDI combos on Apply. Typical case: the APP started before jackd, then the user starts the server and presses
    Apply.
  - **The Cancel `UpdateINI()` added in round 0 is removed.** Nothing writes settings.ini mid-dialog on Linux any
    more.
  - **This exposed bug E** (it predates T-022): after JACK → PulseAudio → JACK the settings equal the running ones,
    so OK didn't restart the audio. With the ports turned off, the settings used to differ, which hid it. On Linux,
    OK and Cancel now also restart the audio when `!IsAudioRunning()`. When the audio is off for lack of a device,
    OK/Cancel retry it, and the "Audio is off" box may show again.
  - **Docker** `docker-jack-midi` PASS. New checks:
    - Selecting ALSA leaves settings.ini untouched.
    - Back on JACK the saved ports reconnect by themselves.
    - Picking "off" and then the port from the list still works.
    - The audio runs after the round trip and OK.
    - Started with no JACK server: the ports stay in settings.ini; after the server starts, Apply brings back the
      audio and both MIDI ports (`11-server-up-apply.png`), and the note plays.
  - **Reviewer's `adversarial.sh` A–E** → `evidence/t022/r12-adversarial`:
    - A: the ports are kept at start without a server.
    - B (`--no-io`): settings.ini keeps the ports.
    - C: no backlog.
    - D: the input reconnects after 10 toggles.
    - E: 2 audio ports after Cancel (was 0).
  - **VM** `vm-jack-midi` PASS. **macOS:** CMake `IPlugInstrument-app` links, no warnings in the APP files.
  - **Seen, not changed** (audio, T-004 territory, minor): Apply on the same driver doesn't re-list the audio devices.
    After the server starts, the audio device combos stay empty though the audio runs (`11-server-up-apply.png`). They
    fill after a driver switch or on reopening Preferences.
  - **Cross-platform findings, updated:**
    - Bug E and the `--no-io` port erasing are fixed on Linux by this change.
    - They remain on Windows: DirectSound ↔ ASIO round trip, then OK/Cancel.
    - On mac/win, `--no-io` still writes `indev=off`.
  - **Next:** an independent review of 509dbe71a.
