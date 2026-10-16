# 23 — Independent review of T-004 (standalone APP robustness), round 1

Reviewer: fresh-context subagent. Range: `hvoya-linux...linux/T-004` (merge-base a348fd749; note that the
two-dot range `hvoya-linux..linux/T-004` also shows the integration branch's later task-file deletions,
which are not this branch's work). Code: `IPlug/APP/IPlugAPP_{host.h,host.cpp,dialog.cpp,main.cpp}`.

## What I re-ran (Docker, own container `t004r`, own build dirs)

| Run | Result | Evidence |
|---|---|---|
| Debug build `IPlugInstrument-app` | PASS | `build/t004r-debug` |
| `app-audio-test.sh` (no device, fresh HOME without `~/.config`) | PASS, 8/8 | `evidence/t004/review-nodev` |
| `app-switch-test.sh` JACK(dummy) → PulseAudio(no server) → JACK | PASS, gdb at `exit()`: main + `gmain` | `evidence/t004/review-jack-switch` |
| Extra signal probes (my `review-extra/probe.sh`) | see m1 | `evidence/t004/review-extra` |
| ASan Debug build: `app-audio-test.sh` + extra probes | PASS, no ASan report | `review-asan-nodev`, `review-asan-extra` |
| ASan + JACK switch | could not drive: under ASan+llvmpipe the main thread is saturated by drawing (270 % CPU, gdb in `nvgStroke` from the display timer), menu clicks are not served in time; not attributed to T-004 | `review-asan-jack-switch`, `review-asan-menu` |
| Release build: Debug menu gone, SIGTERM with File menu open exits 0 | PASS | `evidence/t004/review-release/01-release-menu.png` |

Screenshots I opened: worker's `04-nodev-start`, `05-nodev-prefs`, `docker-nodev/02-prefs`,
`docker-nodev/03-before-sigTERM`, `docker-jack-switch/01-jack-note`, `vm-pipewire-alsa/02-prefs`,
`vm-midi/01-midi-note`, `vm-midi/03-prefs-midi-inputs`, `vm-logo/02-5s` + `04-away`; and all of mine.
They show what the Log claims: the UI behind the "Audio is off (Pulse)…" box, the Preferences title
`Preferences - audio off: no device found`, labels unclipped at scale 1.9 (Xvfb and GNOME), disabled "off"
MIDI combos without a sequencer, the meter moving on a JACK note and on a virmidi note, the VirMIDI port
listed and selected, the logo white rectangle at 5 s and correct after hover. `checks.txt`,
`exit-threads.txt`, `pw-top-*.txt` (`+ IPlugInstrument`, Pulse quantum 1024) match the Log.

## Signal path analysis (`IPlugAPP_main.cpp:546-570`)

Read against SWELL generic: `EndDialog` sets `has_ret` and destroys the dialog synchronously, so
`DialogBoxIsActive()` is NULL right after it, but `SWELL_DialogBox` leaves its loop on the very next check,
before any further `SWELL_MessageQueue_Flush`. `WM_CLOSE` is posted at the earliest 20 ms later and flushed
only once the stack has unwound to a loop. After `DialogBox` returns `IDCANCEL`, `ID_PREFERENCES` does not
touch the host; HWNDs and menus are retained by `TrackPopupMenu`/`runMenuBar`. A message box over
Preferences (Apply) is closed first, then Preferences. I found no use-after-free path, and ASan agrees
(SIGTERM over Preferences + box, SIGINT over About, SIGTERM with File menu open, SIGINT+SIGTERM together).
`GetAudioOffReason()` returns a pointer into a member used immediately: fine. `ReportAudioOff` dereferences
`mDAC`, which on Linux always exists when it is reached (`--no-io`/screenshot return earlier; unknown driver
types fall back to ALSA).

## Shared-code (mac/win) check

| Change | mac/win effect | Flagged |
|---|---|---|
| `UpdateINI` returns on empty path (`IPlugAPP_host.cpp:214`) | none (path never empty) | yes |
| ini `driver` default = `mState.mAudioDriverType` (`:151`) | none (still 0) | yes (commit + Log) |
| `ProbeAudioIO` resets default IDs (`:334-335`) | only after a driver switch / vanished default; RtAudio flags a default whenever a device exists | yes, separate commit 013328ba4 (A3) |
| `GetMIDIPortNumber` null guards (`:301`, `:318`) | only where it used to crash | yes, 013328ba4 (A2) |
| `kDevicePulse` in the shared header; commented-out `streamName` line removed | none | n/a |

All other changes are inside `#ifdef OS_LINUX`. macOS proof is the CMake/Ninja link plus clean Xcode
compiles of the three touched files (Xcode fails only in the untouched `IPlugAPP.cpp`, pre-existing);
acceptable given that pre-existing failure.

## Findings

### Blocker
None.

### Major
None.

### Minor
- **m1** `IPlug/APP/IPlugAPP_main.cpp:605-608` — the SIGINT/SIGTERM sources are installed *after*
  `TryToChangeAudio()`, which on a no-device system runs the modal "Audio is off" box. A signal while that box
  is up takes the default action: reproduced, `SIGTERM over startup box: exit 143`
  (`review-extra/checks.txt`), while every other signal path exits 0. No hang and no data at risk, but the
  "quits cleanly" claim has this hole. Fix: move the two `g_unix_signal_add` calls above
  `TryToChangeAudio()` (the box's loop already iterates GLib, which is how the Preferences case works).
- **m2** `IPlug/APP/IPlugAPP_main.cpp:548-549` — the comment's rationale looks wrong against SWELL's source:
  `TrackPopupMenu` destroys all menu windows and empties `m_trackingMenus` *before* it sends `WM_COMMAND`
  (`WDL/swell/swell-menu-generic.cpp:1327-1338`), so while Preferences runs no menu is open and
  `DestroyPopupMenus()` returns false; only the `TrackPopupMenu` frame is on the stack. The earlier hang that
  motivated it cannot be checked: `vm-sigterm-probe/hung-bt.txt` was overwritten by the passing run. The
  code (close menus without waiting) is fine; reword the comment to what is verifiable, or restore
  the gdb evidence that shows the menu still tracked.
- **m3** `IPlug/APP/IPlugAPP_host.cpp:197-203` — the Linux `mkdir` branch in the "folder doesn't exist" else
  is now dead: `create_directories` either succeeded (the shared `stat` then finds the folder) or the function
  returned early. Dead code next to the new path misleads; drop it or fold the Linux case into the early
  block.
- **m4** Git history: the VM PulseAudio cookies were committed in fa7fc2e09 and 0e986f5d4 and only deleted
  in 71a47afe0, so they stay in history after a `--no-ff` merge. It is a VM-local auth token and the branch is
  unpushed; the coordinator may want to drop them from history (e.g. a squashed merge of the evidence) before
  anything is pushed.

### Nit
- **n1** `IPlug/APP/IPlugAPP_host.cpp:572` — the message says `Audio is off (Pulse)` (RtAudio's display name)
  while the driver combo says `PulseAudio`; use the combo's names for consistency.
- **n2** `IPlug/APP/IPlugAPP_host.cpp:573-574` — `getErrorText()` is RtAudio's *last* error text, not
  necessarily the failure being reported (e.g. a stale ALSA "unable to synchronize…" warning from an earlier
  open of the same `RtAudio` object could be appended to a later "no device found"). Low impact.
- **n3** `IPlug/APP/IPlugAPP_host.cpp:530-554` — the Linux `TryToChangeAudio` tail duplicates the shared
  if/else (17 lines of `#ifdef` in a shared function) rather than a thin hook. Acceptable since the Linux
  version also reports `InitAudio` failure, but it could be one `ReportAudioOff`/`MessageBox` hook plus the
  failure branch.
- **n4** `IPlug/APP/IPlugAPP_main.cpp:597` — Debug-menu removal by position 1 matches Windows and every
  `main.rc` I checked (all Examples, Tests, Gneiss, Gneiss-linux, Sand: File/Debug/Help; only Apple-only
  IPlugSwiftUI has no Debug menu). Robust alternative: find the submenu holding `ID_LIVE_EDIT`.
- **n5** Harness: `app-switch-test.sh` exit-thread check covers only File > Quit, not the signal paths; and
  after an early failure the APP can outlive the script (a leftover instance stayed in my container after
  `timeout` killed the script).

## Acceptance
All six items are met, as far as Docker re-runs and the VM evidence show. m1 is a small gap in "clean
quit". The cross-platform findings A1–A4 are correctly reported, not fixed (A2/A3 are fixed in a
separate shared-code commit awaiting the user's decision).

## Verdict
**approve with minor fixes** (m1–m3 in code/comments; m4 for the coordinator).

## Round 2

Reviewer: fresh-context subagent. Commits: b77df0c67 (shared: A1, A4) and 05cf72aa4 (Linux: shortcuts, screenshot,
SWELL INIFILE, JACK rate). Docker only (container `t004r2`, build `build/t004r2-debug`), no VM.

### What I re-ran

| Run | Result | Evidence |
|---|---|---|
| Debug build `IPlugInstrument-app` | PASS, no warnings | `build/t004r2-debug` |
| `app-audio-test.sh` (no device, fresh HOME) incl. `Ctrl+,` from the plug-in UI, `Ctrl+Shift+S`, signals | PASS 11/11 | `evidence/t004/review2-nodev` |
| `jackd -d dummy -r 48000 -C 0 -P 2`, settings `[audio] driver=1` only | no "Audio" box, `IPlugInstrument:outport 0/1` connected, INI rewritten with `sr=48000` | `review2-extra/01-jack-start.png`, `stderr-jack.txt` |
| `Ctrl+,` with Preferences open (focus in the dialog; then focus forced on the disabled main window, plus `Ctrl+Shift+S`) | one Preferences, no screenshot, APP alive | (console output; `02-jack-prefs.png`) |
| Pulse (no server) at 44100 → Preferences → JACK → Apply → Cancel | see m5 | `03-driver-combo.png`, `04-after-apply.png`, `stderr-cancel.txt` |
| Debug > Save Screenshot clicked in the menu (Xvfb + openbox, no compositor) | see m6 | `05-debug-menu.png`, `06-menu-screenshot.png` |
| `--screenshot <path>` | exit 0, full 1024x669 UI | `07-cli-screenshot.png` |

Screenshots I opened: all of mine, and the worker's `minor/release-debug-menu-crop.png` (Debug menu = "Save
Screenshot" only, no stray separator).

### b77df0c67 (shared code)

- `!mDAC` guard: reached on mac/win only where the old code dereferenced null (Init() returned before
  `TryToChangeAudioDriverType`). Behaviour change is crash → silent "no audio"; flagged in the commit. OK.
- A4: `InitAudio` passes `iParams` only when `nChannels > 0` (`IPlugAPP_host.cpp:754`), and `nChannels` is
  `MaxNChannels(kInput)`, the same value as `needsInputDevice`. So `inputID = outputID` is only a placeholder that
  RtAudio never sees (CoreAudio, DS, ASIO alike; it only appears in a `DBGMSG`). ASIO already took the output's ID.
  Effects (inputs > 0) follow the old path exactly. The only mac/win change is the claimed one: an instrument with
  no input device now starts audio instead of "Please check the audio settings". OK.

### 05cf72aa4 — shortcuts

- The grab activates only while the focus is the SWELL main window or a descendant (the plug-in's X window), so
  keys typed in Preferences or a message box are untouched; with the main window disabled the filter swallows
  the key and posts nothing (verified above).
- With XInput2 GDK selects no core key events, so the filter sees only grabbed keys and synthetic (`XSendEvent`)
  ones; it cannot eat normal keys of other windows. KeyRelease is swallowed with the same match, consistent.
- A text entry in the plug-in UI loses exactly these combos (the grab wins), as on mac/win. Acceptable.
- GdkWindow recreation: SWELL destroys a top-level's GdkWindow only when it is hidden
  (`swell_oswindow_manage`, `swell-generic-gdk.cpp:596-603`); the APP never hides the main window, and doing so
  would also destroy the plug-in's child X window. Not a practical risk.
- `ParseShortcut`: `,` → `XK_comma` (0x2c), `Ctrl+Shift+S` → `s` + Shift, correct.

### Findings

#### Blocker / Major
None.

#### Minor
- **m5** `IPlug/APP/IPlugAPP_host.cpp:584` — `UseSampleRateTheDeviceSupports` calls `UpdateINI()` from inside
  the Preferences dialog, so Apply writes the *uncommitted* dialog state (driver, devices, rate) to the INI and a
  later Cancel does not undo it. Reproduced: INI `driver=2 sr=44100` → choose JACK → Apply → INI `driver=1
  sr=48000` → Cancel: the running state goes back to PulseAudio (no JACK ports), the INI stays JACK/48000, so the
  next start uses the cancelled driver. (The upstream `resetToDefault` branch has the same pattern but needs a
  vanished device; this one fires on every first switch to JACK.) Fix: drop the `UpdateINI()` here — the rate is
  re-derived on each start and OK already saves the INI.
- **m6** `IPlug/Linux/IPlugAPPLinux.cpp:231-246` + `IPlugAPP_dialog.cpp:749` — Debug > Save Screenshot chosen
  from the menu on an X server without a compositor captures a black rectangle where the menu was: `XGetImage`
  runs before the plug-in has repainted the exposed area (`06-menu-screenshot.png`). The keyboard path and
  `--screenshot` are fine; composited desktops (GNOME/KDE) are fine. Fix: on Linux defer the capture (e.g. a
  ~200 ms timer, as `--screenshot` already does), or say in the Log that it needs a compositor.
- **m7** (cross-platform finding, report — shared code) `IPlug/APP/IPlugAPP_dialog.cpp:56-63` —
  `PopulateSampleRateList` lists only rates common to input *and* output device; with no input devices
  (`mAudioInputDevIDs` empty) the input info is empty and the Sampling Rate combo stays empty
  (`02-jack-prefs.png`, `04-after-apply.png`). A4 makes this case reachable on mac/win too (Mac mini without a
  microphone). The task Log's "Preferences shows it after Apply" is therefore not true for an output-only device;
  correct the Log and add it to the cross-platform findings.

#### Nit
- **n6** `IPlug/Linux/IPlugAPPLinux.cpp:204-221` — `XQueryTree`/`XGetWindowAttributes` in `LargestChildWindow` run
  outside the GDK error trap; an untrapped X error is fatal in GDK3. Practically unreachable (the only child is
  owned by the same thread), but moving `gdk_x11_display_error_trap_push` above the call costs nothing.
- **n7** `IPlug/Linux/IPlugAPPLinux.h:28-33` — "Items removed from the menu … take their shortcuts with them" holds
  only for removals before `SetMenuShortcuts` (grabs are taken once, never refreshed); "Save the window's client
  area" — it saves the largest child X window and fails when there is none (no IGraphics editor). Reword both.
- **n8** `IPlug/Linux/IPlugAPPLinux.cpp:83` — `// ControlMask | ShiftMask | Mod1Mask` restates
  `kShortcutModifiers`; drop it.
- **n9** `IPlug/Linux/IPlugAPPLinux.cpp:33-47` — `SWELL_ExtendedAPI("INIFILE")` also raises `RLIMIT_NOFILE`'s soft
  limit to min(hard, 16384), writes `max_open_files` and reads `ui_scale` (`WDL/swell/swell.cpp:1090-1140`); none of
  that ran before. Harmless (REAPER does the same) and calling it before `SWELL_initargs` is right, but worth one
  line in the Log. The folder creation duplicates `InitState`'s; on failure SWELL silently falls back to
  `~/.libSwell.ini` while `InitState` prints the error — acceptable.
- **n10** `IPlug/Linux/IPlugAPPLinux.cpp:115` — a shortcut on a shifted character (`Ctrl+?`) would grab the base
  key without Shift; and only Caps/Num Lock variants are grabbed. Neither occurs in the APP menus today.

Comments otherwise follow the rules (WHY only). Shared files: every 05cf72aa4 change outside `IPlug/Linux/` is
inside `#ifdef OS_LINUX` or the CMake Linux branch.

### Verdict (round 2)
**approve with minor fixes**: m5 should be fixed (one line); m6 fix or document; m7 goes to the cross-platform
findings and the Log claim is corrected. Nits optional.
