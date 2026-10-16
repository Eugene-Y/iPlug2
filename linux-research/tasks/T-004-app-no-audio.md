# T-004 — Standalone APP robustness on Linux

- **Status:** review-done
- **Owner:** 2nd instance (claimed 2026-10-16)
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
- [x] Starts with no audio device: UI shown, no crash, clear status.
- [x] Plays audio through PipeWire (Pulse or JACK API) on the VM: the APP's own meter shows signal, `pw-top` shows the stream.
- [x] Device switch works without restart; settings persist.
- [x] MIDI input reaches the plugin (IPlugInstrument plays a note).
- [x] Clean quit; no leaked threads in gdb at exit.
- [x] mac APP unaffected (Linux-only changes or mac build proof). Evidence in `evidence/t004/`; self-review; Log complete.

## Log

- 2026-10-16 (from T-005, not a claim of this task): on the VM, the APP under gdb in VS Code logs `RtApiAlsa::probeDeviceOpen: unable to synchronize input and output devices` at start (ALSA `default` = PipeWire); audio still runs. Worth checking here (evidence `evidence/t005/vm/23-vscode-app-bp-processblock.png`).

### 2026-10-16 — work log (2nd instance)

**The "no-device segfault" was not about audio.** Without `~/.config`, `InitState` failed its non-recursive
`mkdir`, `Init()` returned before creating the RtAudio object, and `main()` ignored that and crashed in
`GetAudioDeviceID` (null `mDAC`). With `~/.config` present the APP started and showed the shared modal "Please check
the audio settings" *before* any window. A second segfault: with no ALSA sequencer (containers, snd-seq not loaded)
`mMidiIn` is null and an ini without `[midi]` (default name "no input") crashed `GetMIDIPortNumber`.

**Changes** (all in `IPlug/APP/*`; `#ifdef OS_LINUX` unless marked *shared*):
- Settings folder from `INIPath` (XDG), created with parents; failure → run on unsaved defaults (warning on stderr).
  *Shared:* `UpdateINI` returns on an empty path (never empty on mac/win).
- Drivers ALSA / JACK / **PulseAudio** (`kDevicePulse = 2`, new constant in the shared header). Unknown type → ALSA, so
  `mDAC` always exists. **PulseAudio is the default for new Linux settings** (`AppState` ctor; the ini read now
  defaults to `AppState`'s value, 0 on mac/win as before). Why: ALSA `default` = PipeWire on current desktops, and
  RtAudio's ALSA duplex needs `snd_pcm_link`, which the PipeWire plug-in refuses ("unable to synchronize input and
  output devices"): an effect underruns ~36×/s (`vm-xruns-IPlugEffect-alsa-512`: 359 in 10 s). Pulse and JACK(PipeWire)
  ran 10 s with none. Output-only ALSA (instrument) has none either. Existing ini files keep their driver.
- `options.numberOfBuffers = 4`: Pulse output otherwise buffers to the server default (pw-top quantum 5040 ≈ 105 ms,
  `vm-xruns-effect-2`); now 1024 (`vm-xruns-*-pulse-*`). 4 is RtAudio's ALSA default anyway; JACK ignores it.
- `options.streamName` and the RtMidi client name = `BUNDLE_NAME` (JACK/pw-top/aconnect show "IPlugInstrument"
  instead of "RtApiJack"/"RtAudio"/"RtMidi Input Client").
- The window opens before audio starts; a failure shows "Audio is off (<API>): <reason>. <RtAudio text>. Choose an
  audio driver and device in File > Preferences." over the UI, and Preferences shows `Preferences - audio off:
  <reason>` (or `(--no-io)`) in its title, refreshed on Apply.
- Preferences: SWELL generic scale 1.9 (swell_resgen's own non-Apple default; the macOS 1.5 clipped every label);
  MIDI combos show a disabled "off" when RtMidi is unavailable; "Config..." disabled (it opened alsa-project.org).
- Release builds drop the Debug menu, as on Windows (Linux has no screenshot implementation for its last item).
- SIGINT/SIGTERM close like a WM close: popup menus and modal dialogs first (`DialogBoxIsActive`/`EndDialog`), then
  `WM_CLOSE`. The first version posted `WM_CLOSE` directly and destroyed the host under a running Preferences proc
  (UAF, seen as a garbage title); the second looped while `DestroyPopupMenus()` returned true and hung on the VM with
  Preferences open (gdb: main thread in `SWELL_DialogBox` under `TrackPopupMenu`). Not waiting on menus fixed it
  (VM probe exits 0, repeatedly). **Mechanism unconfirmed:** the reviewer points out that SWELL's `TrackPopupMenu`
  empties its menu list before sending `WM_COMMAND`, so `DestroyPopupMenus()` should have returned false there; the
  first claim in commit fa7fc2e09 is therefore not proven. The probe's later run replaced `hung-bt.txt`. The handler removes its
  source, so GLib restores SIG_DFL: a second signal ends a hung quit (checked with a probe program).
- *Shared (separate commit 013328ba4, for the user's decision):* `ProbeAudioIO` resets the default device IDs (after
  JACK→ALSA the stale JACK default gave "output device ID is invalid"; RtAudio numbers every API's devices from the
  same start value, so it can also pick a wrong device); `GetMIDIPortNumber` treats a null RtMidi object as "port not
  found" (→ "off") instead of dereferencing it, placed after mac's virtual-port answer so that path is unchanged.

**Results**
- Docker (Xvfb, no audio): `evidence/t004/docker-nodev` PASS (fresh HOME without `~/.config`, Pulse "no device found
  / Connection refused" box over the UI, Preferences title, Escape, File > Quit, SIGINT, SIGTERM over Preferences +
  message box). `docker-jack-switch` PASS (jackd dummy: JACK ports named after the APP, note moves the meter,
  JACK→PulseAudio(no server)→JACK in Preferences without restart, `driver=1` persisted, exit threads).
- VM (GNOME/X11, PipeWire): `vm-default-instrument`, `vm-default-effect` PASS (fresh settings → Pulse, no box,
  `driver=2` saved); `vm-jack-pulse`, `vm-jack-alsa` PASS (JACK via `pipewire-jack`'s libjack ↔ Pulse/ALSA live; the
  meter moves on every driver; pw-top rows `+ IPlugInstrument`, `+ alsa_playback.IPlugInstrument`); `vm-midi` PASS
  (snd-virmidi → ALSA seq → IPlugInstrument plays from `amidi`, sequencer client "IPlugInstrument", gone after quit);
  `vm-sigterm-probe` exits 0. Dialog under GNOME: `vm-pipewire-alsa/02-prefs.png`. X answered after the batch.
- Threads at `exit()` (gdb, Docker and VM): main + GLib's own `gmain` worker (started by `g_unix_signal_add`);
  no audio, JACK or MIDI thread left.
- macOS: `xcodebuild -scheme macOS-APP` (IPlugInstrument): `IPlugAPP_host/dialog/main.cpp` compile without errors or
  warnings; the build fails only in the untouched `IPlugAPP.cpp` (pre-existing: compiled as C++ but includes Cocoa, see
  T-003). The CMake/Ninja build of `IPlugInstrument-app` (which marks APP sources Objective-C++) links
  `IPlugInstrument.app` (`evidence/t004/macos/`). The Xcode build regenerated `main.rc_mac_dlg/menu`; I restored them
  with `git checkout` without the snapshot AGENTS asks for (they held only generated output).
- Env hacks: none left (`IPLUG2_MENU_OFFSET`/`IPLUG2_SCREEN_SCALE` absent; only `IPLUG_LINUX_DIALOGS` from T-002).
- Harness: `scripts/app-audio-test.sh`, `scripts/app-switch-test.sh`, `scripts/vm/probe/app-{midi,xruns,
  sigterm-hang,first-frame}.sh`; `vm-run.sh` gains `VM_ENV`; `vm-provision.sh` installs `pipewire-jack`,
  `pulseaudio-utils`, `alsa-utils` (installed on the VM). Test HOMEs drop Mesa caches and the Pulse cookie on exit;
  the VM's Pulse cookies did land in commits d9823b47d..0e986f5d4 (VM-local auth token, removed in the last commit).

**Not done / open**
- JACK MIDI: MIDI stays on the ALSA sequencer for every driver. On PipeWire that reaches the same graph; a pure
  jackd setup needs a2jmidid. Worth a small follow-up if wanted.
- `Ctrl+,` (Preferences) and the Debug-menu shortcuts are shown but do nothing: SWELL generic has no accelerator
  tables and the Linux loop does not translate them.
- "Save Screenshot" (Debug builds) has no Linux implementation (gap for T-016).
- SWELL writes `~/.libSwell.ini` into `$HOME` (pre-existing; `SWELL_ExtendedAPI("INIFILE", …)` could move it to XDG).
- ALSA stays selectable; an effect on ALSA through PipeWire still underruns (RtAudio limitation, `Dependencies/`).

**Seen in passing, not T-004 (for the coordinator):** on the VM (virgl) the hvoya header logo of IPlugEffect
(`ISVGControl`, white strokes on transparency) draws as a **white rectangle on the first frames** and correctly after
the pointer hovers it (`evidence/t004/vm-logo/02-5s.png` vs `04-away.png`; noticed by the user). Docker/llvmpipe draws
it correctly from the start (`logo-01-start.png`). The GLX visual has stencil 8 and NanoVG stencil strokes are off,
so it is not a missing stencil buffer. Belongs to T-009 (visual parity) / IGraphicsLinux.

### Cross-platform findings
- **A1 (crash, low):** `IPlugAPP_main.cpp` (all platforms) ignores `Init()`'s result; if `InitState` fails (mac:
  `mkdir` of `~/Library/Application Support/<BUNDLE>` fails), `TryToChangeAudio` dereferences a null `mDAC`
  (`IPlugAPP_host.cpp` `GetAudioDeviceID`). Linux no longer reaches it; mac/win unchanged (report only).
- **A2 (crash, low):** `GetMIDIPortNumber` null deref when RtMidi failed to initialise — fixed in shared code,
  commit 013328ba4 (needs the user's OK, or wrap in `#ifdef OS_LINUX`).
- **A3 (wrong device, low):** `ProbeAudioIO` kept stale default device IDs across driver changes (Windows DS↔ASIO
  too) — fixed in shared code, commit 013328ba4 (same decision).
- **A4 (minor):** `TryToChangeAudio` requires an input device even for an instrument with no inputs; an output-only
  system reports "no device". Not changed.

### Review round 1 (notes/23-review-T-004.md): approve with minor fixes — fixed
- m1: SIGINT/SIGTERM handlers now installed before `TryToChangeAudio()`; a signal over the startup "Audio is off"
  box exits 0 (was 143). `app-audio-test.sh` gained that case (Docker PASS); VM `vm-sigterm-probe` exits 0,
  `vm-default-effect` PASS after the change.
- m2: the comment that blamed `TrackPopupMenu` is gone; the Log above says the mechanism is unconfirmed.
- m3: the dead Linux `mkdir` branch in `InitState` is replaced by "run on unsaved defaults" for the folder vanishing
  between creation and `stat`.
- Nit: one `kAudioDriverNames` table (`ALSA`, `JACK`, `PulseAudio`) feeds the Preferences list and the message
  ("Audio is off (PulseAudio)", was RtAudio's "Pulse").
- m4 (Pulse cookies of the VM in fa7fc2e09/0e986f5d4): left for the user/coordinator — dropping them needs a history
  rewrite of this unpushed branch. They are VM-local PulseAudio auth tokens.
- Not changed: `getErrorText()` may be an older RtAudio error (shown as detail only); the Linux tail of
  `TryToChangeAudio` stays a separate branch so the mac/win lines are untouched.

**Hand-over:** commits `git log hvoya-linux..linux/T-004`; shared-code fixes isolated in 013328ba4 for the user's
decision (A2/A3); evidence `evidence/t004/` (Docker `docker-*`, VM `vm-*`, macOS `macos/`, review `review-*`).

**User decision (2026-10-16):** the shared-code fixes in 013328ba4 (A2, A3) stay as they are, for all platforms. The
VM Pulse cookies in history (m4) are to be dealt with later.
