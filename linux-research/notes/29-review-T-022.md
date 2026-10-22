# Review: T-022 — APP MIDI follows the audio driver (JACK MIDI on JACK) (round 1)

Reviewer with fresh context. Range `hvoya-linux..linux/T-022`: 6e8995437 (code), 8a21c9ce9 (harness, evidence, Log).

**Verdict: request changes.** One blocker (R1-1): with JACK MIDI enabled, a plug-in that sends MIDI while the MIDI
output is "off" crashes the APP when an output is picked later. The rest of the diff is sound: the recreate paths are
thread-safe, the dialog refresh works without duplicates, the INI repair on Cancel is right, and mac/win are
unaffected.

## What was checked

- **Diff against Acceptance and AGENTS §2.**
  - Everything new is under `#ifdef OS_LINUX`. One exception is the shared `ProbeMidiIO` clearing both name lists,
    which is behaviour-neutral on mac/win, where it runs once.
  - Comments explain the WHY only. No product names.
  - mac: I rebuilt `IPlugInstrument-app` with CMake/Ninja (Debug, arm64, scratchpad build dir). The three APP files
    compile with no warnings, and the target links.
- **Thread safety of the recreate.** `TryToChangeMidiApi` has two callers, and both stop the audio first:
  - the driver combo (`TryToChangeAudioDriverType` → `CloseAudio` waits for `mAudioDone`, then closes the stream);
  - Cancel (same call before it).

  So `IPlugAPP::SendMidiMsg` (audio thread) cannot touch the `mMidiOut` being destroyed. The old producer of
  `mMidiMsgsFromCallback` is gone before `InitMidi` creates the next one:
  - `~MidiInJack` calls `jack_client_close`, which stops that client's process thread;
  - `MidiInAlsa::closePort` joins its thread.

  Single producer holds.
- **Driver paths.**
  - Startup: `Init` → `InitMidi` with the normalised driver.
  - Combo: recreate + `RefreshMidiControls`.
  - Apply / OK: the MIDI is already switched, and only the audio restarts.
  - Cancel: restore + recreate + `UpdateINI`.
- **INI writes.** While another driver is selected, `SelectMIDIDevice` / `PopulateMidiDialogs` write the not-yet-applied
  driver. Cancel rewrites the restored state. Confirmed by the harness (`driver=0 indev=off`, then `driver=1` back
  after Cancel).
- **Evidence.**
  - I opened `docker-jack-midi/02, 04, 06, 07` and `vm-jack-midi/03, 05`. Each list shows every port once, and the
    PulseAudio list shows the sequencer names while the JACK list shows `Midi-Bridge:` names.
  - `04` shows the MIDI combos disabled and "off" on ALSA without a sequencer.
  - `checks.txt` and `exit-threads.txt` (Docker and VM) match the Log: only main and `gmain` at `exit()`.
- **Re-run.** `scripts/app-jack-midi-test.sh` → `evidence/t022/review-docker-jack-midi`: **PASS**, the same 25 lines as
  the author's run.
- **Adversarial runs.** `evidence/t022/review-adversarial/adversarial.sh` (Docker, jackd dummy). Results in `log.txt`:
  - **A.** JACK in settings.ini, no JACK server. Audio box, MIDI combos show "off" and stay enabled, no crash. Once
    the server is up, ALSA→JACK lists `seq:out` and picking it connects. Quit exits 0.
  - **B.** `--no-io`: no JACK MIDI clients. Switching drivers and Cancel are fine, and Quit exits 0.
  - **C.** MIDI output off for 30 s while IPlugInstrument echoes notes, then an output picked: **SIGSEGV**, twice
    (R1-1).
  - **D.** JACK↔ALSA toggled 10× while notes flow: no crash. The saved input turns off (R1-2), and Quit exits 0.
  - **E.** Ports off, JACK→ALSA→JACK, Cancel: the audio stays off. This bug was there before T-022 (cross-platform
    findings).
- VM: not used. The Docker runs cover the findings.

## Findings

### R1-1 — blocker — JACK MIDI output crashes when an output is picked after the plug-in sent MIDI with the output off
- Where:
  - `IPlug/APP/IPlugAPP_host.cpp:841-842` now creates a `MidiOutJack` whenever the driver is JACK.
  - `IPlug/APP/IPlugAPP.cpp:124-138` (`SendMidiMsg`) and `:148-158` (`SendSysEx`) call `sendMessage` whenever
    `mMidiOut` exists, even with no port open.
- Mechanism (RtMidi 4.0.0, which we don't modify):
  - `MidiOutJack::sendMessage` (`RtMidi.cpp:3436`) writes into two 16 KB ring buffers.
  - `jackProcessOut` drains them only while `data->port != NULL` (`:3225`), so with the output "off" they fill up.
    The size buffer holds about 4095 messages.
  - When a port is opened, the next cycle drains the whole backlog into one period.
  - `jack_midi_event_reserve` returns NULL once the port buffer is full, and `jack_ringbuffer_read(..., NULL, ...)`
    at `:3234` segfaults on the JACK thread.
  - Below the crash threshold the backlog is still sent as one burst of stale events. Messages written after the
    size buffer is full leave the two buffers out of step for good.
- Repro (`review-adversarial`, scenario C):
  1. JACK, MIDI input `seq:out`, MIDI output "off", the factory default.
  2. A note on and off every 10 ms for 30 s.
  3. Preferences → MIDI output → `midi-monitor:input` → exit 139.
  - Backtrace in `C-gdb.txt`: `jack_ringbuffer_read` ← `jackProcessOut (RtMidi.cpp:3234)`.
  - At a normal playing rate this needs a few minutes of playing, then picking an output, or turning an output off
    and on again.
- Before T-022 the APP used the ALSA sequencer whenever one existed (`MidiOutAlsa::sendMessage` has no backlog), so
  T-022 newly exposes this path.
- Fix:
  - Don't send while no output port is open, Linux-only so mac's virtual port is untouched. For example, keep a host
    `std::atomic<bool>`:
    - cleared before `mMidiOut->closePort()` in `SelectMIDIDevice(kOutput, …)`;
    - set after a successful `openPort`;
    - checked in `SendMidiMsg`/`SendSysEx`.
  - `RtMidiOut::isPortOpen()` reads a plain `bool` from the audio thread, so it is weaker.
  - Then re-run scenario C (`adversarial.sh <out> <app> C`).

### R1-2 — minor — the saved MIDI ports are lost on a driver round trip and on a start without a JACK server
- `IPlug/APP/IPlugAPP_host.cpp:811-812`: `TryToChangeMidiApi` → `SelectMIDIDevice` turns a port the other API doesn't
  have into "off" in `mState`, and writes it to settings.ini.
- Scenario 1:
  1. JACK with `seq:out` selected.
  2. Look at PulseAudio in the combo.
  3. Go back to JACK.
  4. OK.

  The input is "off" (scenario D; the harness's "back on JACK, the saved ports turned off").
- Scenario 2: start the APP before jackd/QjackCtl. settings.ini gets `indev=off outdev=off` (scenario A), while the
  audio device names survive the same failure.
- This is the documented "unknown → off" rule. Still, JACK being down at start is common, and this loses the
  configuration silently.
- Suggestion:
  - when returning to the API the dialog opened with, reselect `mTempState`'s ports;
  - don't overwrite the saved names when the API has no client (JACK down).
  - Otherwise, record the behaviour in the task as accepted.

### R1-3 — minor — evidence gaps against the Acceptance list
- "Clean quit on every driver": both harnesses quit only on JACK. A quit on PulseAudio/ALSA after
  `TryToChangeMidiApi` recreated the sequencer objects with an input open is not shown. It uses the same classes as
  T-004's quit, so the risk is low, but the item says every driver.
- Some `pass` lines in `scripts/app-jack-midi-test.sh` are unconditional:
  - "the APP's JACK MIDI ports";
  - "settings.ini while ALSA is selected";
  - "back on JACK, the saved ports turned off".

  They read as checks in `checks.txt`. Mark them as `info:` or make them assert.

### R1-4 — nit — comment on the JACK client name
- `IPlug/APP/IPlugAPP_host.cpp:791-792`: "would push the audio client … to JACK's "-01" name". With both RtMidi
  objects named `BUNDLE_NAME`, the audio client would get `-02`, as the Log says.
- Suggestion: "to a numbered name".

### R1-5 — nit — no JACK server: repeated warnings and leaked ring buffers
- Every `getPortCount`/`getPortName` call retries `connect()`. That printed 6 × "JACK server not running?" in
  scenario A.
- `MidiOutJack::connect` (`RtMidi.cpp:3273-3274`) allocates two 16 KB ring buffers before `jack_client_open` and
  leaks them on each failure.
- Small and RtMidi's. Mention it in the task; no change needed.

### R1-6 — nit — `TryToChangeMidiApi`'s precondition is only a comment
- `IPlug/APP/IPlugAPP_host.h:251-253` says "Call while audio is stopped".
- Both callers comply. An `assert(!mDAC || !mDAC->isStreamRunning())` at the top of `TryToChangeMidiApi` would keep
  it true.
- Also, `InitMidi` computes `api` twice (`IPlugAPP_host.cpp:825`, `:841`). One local before the first `try` reads
  better.

## Cross-platform findings (report, not fix)
- **major (pre-existing):** Preferences: change the driver and change it back, then Cancel, and the audio stays off.
  - Cause: the driver combo closes the stream (`TryToChangeAudioDriverType`). Cancel restarts it only if
    `mState != mTempState` (`IPlugAPP_dialog.cpp:403-416`), and after a round trip they are equal.
  - Seen in Docker (scenario E: 2 audio ports → 0 after Cancel).
  - On Windows the same happens with DirectSound → ASIO → DirectSound.
- **minor (pre-existing):** `--no-io` (and screenshot mode) erase the saved MIDI ports from settings.ini at start.
  - `Init` calls `SelectMIDIDevice` without MIDI objects, `GetMIDIPortNumber` returns -1, so the port turns "off" and
    `UpdateINI` writes it (`IPlugAPP_host.cpp:74-75`, `:298`, `:609-612`).
  - Seen in scenario B (`indev=off outdev=off` after start).
- The author's finding (Cancel doesn't undo a MIDI port change) is confirmed by reading the code.

## Note
- My mac CMake configure ran without `-DIPLUG_DEPLOY_PLUGINS=OFF`, so the build copied the APP to
  `~/Applications/IPlugInstrument.app`. The directory's birth time is the build time, so it is most likely new. It
  was left in place for the user to delete. Nothing went to `~/Library`, because only the APP target was built.

# Round 2

Range `55979e325..df033f7a7`: 47951f3a0 (code), df033f7a7 (harness `info:` lines, VM quit on PulseAudio, re-runs,
Log).

**Final verdict: approve.** R1-1 is fixed and verified. R1-3, R1-4 and R1-6 are done (the R1-6 assert is declined,
with a reason that holds). R1-5 is left in RtMidi, which is acceptable. R1-2 is acceptable to hand over as an open
user decision (below).

## The fix (R1-1)
- **Open/close paths.** `mMidiOutPortOpen` (`std::atomic<bool>`, Linux-only member) changes in three places:
  - **Cleared before `closePort`** in `SelectMIDIDevice(kOutput)`. This covers "off", an unknown name (port -1 → 0)
    and a switch from one port to another.
  - **Cleared in `TryToChangeMidiApi`** before `mMidiOut` is reset. If `InitMidi` then fails, `mMidiOut` is null and
    the flag is false. `SelectMIDIDevice` with a null `mMidiOut` doesn't touch the flag, and nothing can open a port
    while `mMidiOut` is null.
  - **Set only after `openPort` returns.** RtMidi's port errors (`DRIVER_ERROR`) throw, so a failed open never sets
    it. Before T-022 such a throw was uncaught on every platform, and it still is.
- **Memory ordering.** Seq-cst by default.
  - Open: the port is registered before the store, so the audio thread can only see `true` after the port exists.
  - Close: the audio thread may have loaded `true` just before the clear, and then writes one block's messages after
    `closePort`. RtMidi's ring buffers outlive `closePort`, so those few bytes reach the next opened port. That is
    harmless and bounded, not the unbounded backlog of R1-1.
  - The recreate paths still run only while audio is stopped (round 1), so the `mMidiOut` pointer itself never races.
- **mac/win.**
  - The checks in `IPlugAPP.cpp:124-128, 152-156` and the clear in `SelectMIDIDevice` are inside `#ifdef OS_LINUX`.
  - The set is inside the `#elif defined OS_LINUX` branch, and `TryToChangeMidiApi`/`InitMidi`'s `api` are Linux-only.
  - The only unconditional change is `#include <atomic>` in `IPlugAPP_host.h`, which is harmless.
  - I rebuilt `IPlugInstrument-app` on macOS (CMake/Ninja, `IPLUG_DEPLOY_PLUGINS=OFF` this time). All four APP files
    compile with 0 warnings, and the target links.
  - ALSA: sending only while a port is open changes nothing that can be observed. The Linux APP has no virtual
    output, so with the output "off" nothing could be subscribed to it.
- **Comments.**
  - The `MidiClientName` comment (R1-4) is right now.
  - Nit, no action needed: the doc on `IsMidiOutPortOpen` says the backlog is written "past the buffer's end". In fact
    `jack_midi_event_reserve` returns NULL and RtMidi copies into it. The WHY is right.

## Re-runs (fresh Debug build in Docker, container t022r, removed)
`evidence/t022/review2-adversarial` (`adversarial.sh` from round 1 plus scenario C2 and a positive check). See
`log.txt`.
- **C (output off for 30 s while about 200 messages/s are echoed, then an output picked):**
  - no crash, and 0 events at the monitor with the source stopped, so no backlog;
  - then the source restarted and reconnected for 3.5 s: 8 events arrive, so the picked output works;
  - Quit exits 0.
- **C2 (the second path: output connected → "off" for 30 s → connected again):**
  - 947 events echoed while connected, and 0 while off;
  - 0 right after reconnecting, so no backlog;
  - 6 in 3.5 s after the source restarted;
  - Quit exits 0.
- My first C2 run showed 0 events after the restart. That was a flaw in my script: a restarted `jack_midiseq` is a new
  port, and the APP's connection to it was gone. The script now reconnects it with `restart_source`, and `log.txt`
  holds only the corrected run.
- **The author's `evidence/t022/fix-adversarial` (A–E on the fixed build):** consistent with my run and with round
  1. E still shows the old Cancel bug from before T-022, as expected.

## New evidence checked
- **`vm-jack-midi/checks.txt`:**
  - the reporting lines are `info:` now;
  - the gdb run is on PulseAudio with the sequencer input picked from the list (virmidi → client 128
    'IPlugInstrument'), and the note moves the meter;
  - main and `gmain` only at `exit()`, so the "every driver" quit gap from R1-3 is closed.
  - I opened `04-pulse-note.png` and `06-jack-again-note.png`: the meter is up and the key is highlighted in both.
- **`docker-jack-midi/checks.txt`:** PASS with the `info:` lines.
- **`macos/`:** the APP files compile, `IPlugAPP.cpp` included, with no APP warnings. This matches my own build.

## R1-2 as an open user decision
Acceptable for handing over.
- It is the task's stated "unknown → off" rule, applied to a new case. It is not a crash or a bad connection.
- The Log names both cases (a driver round trip, and a start without a JACK server).
- A fix changes what settings.ini keeps, which is the user's call.
- When the user decides: a fix can stay Linux-only, inside `TryToChangeMidiApi` or with a Linux branch in
  `SelectMIDIDevice`. "Shared code" alone is not a reason against it.

## Still open, for the coordinator
- The cross-platform findings from round 1 are recorded in the task Log: Cancel after a driver round trip leaves the
  audio off, and `--no-io` erases the saved MIDI ports.
- `~/Applications/IPlugInstrument.app` from my round-1 mac build is still there (unchanged since 15:52). The user can
  delete it.

# Round 3 (R1-2 follow-up)

Range `21ce3ddc0..607bb7250`: 509dbe71a (code: `IPlugAPP_host.cpp/.h`, `IPlugAPP_dialog.cpp`), 607bb7250 (harness,
evidence, Log). Reviewer with fresh context.

**Verdict: approve with fixes.** The R1-2 change does what the user decided: a saved port the current API can't see
stays in `mState` and settings.ini on every path, and Apply/OK bring it back. mac/win are unchanged. One major
finding (R3-1) must be fixed before the merge. It is a T-022 crash that R1-2 didn't create, but its OK restart now
reaches it with a plain OK.

## What was checked

- **Every read/write of `mMidiInDev`/`mMidiOutDev` and settings.ini on Linux:**
  - **Startup:** `Init` → `SelectMIDIDevice`. A missing port keeps its name and stays closed, with no INI write.
  - **`--no-io` and screenshot mode:** `InitMidi` returns early, so `GetMIDIPortNumber` returns -1 for every name but
    "off" and the name is kept. OK with `--no-io` doesn't open a box, because `TryToChangeAudio` returns at once.
    `ReopenSavedMidiPorts` skips the null objects.
  - **Driver combo:** `TryToChangeMidiApi` reselects and keeps.
  - **Apply/OK:** `ReopenSavedMidiPorts`. The MIDI combos: picking writes `mState`.
  - **Cancel:** `mState = mTempState`, then recreate and keep.
  - Nothing writes settings.ini mid-dialog any more. The only writer left is `TryToChangeAudio`'s reset-to-default,
    and that one predates T-022. The OK write is `MainDlgProc` → `UpdateINI`.
- **Clearing a kept port:** SWELL sends `CBN_SELCHANGE` even when the item already selected is picked again
  (`swell-wnd-generic.cpp:3656-3663`). So "off" picked from a list that holds only "off" clears the name. Scenario H
  below confirms it.
- **Thread safety of `ReopenSavedMidiPorts` while audio runs:**
  - It opens only ports that are closed, which is the same as a pick in the combo while audio runs.
  - The output goes through `SelectMIDIDevice`, so `mMidiOutPortOpen` is set only after `openPort`. The audio
    thread's `SendMidiMsg` cannot reach a closed port.
  - The input has no callback while it is closed, so the single-producer rule holds.
  - Scenario K: 20 s of echoed notes with the saved output missing, then the monitor appears and Apply runs while the
    audio runs. Result: 0 stale events, then 6 in 3.5 s, and no crash.
- **The `IsAudioRunning` restart on OK/Cancel:**
  - One `TryToChangeAudio` per press, with no loop.
  - Apply then OK doesn't restart twice: the audio runs and the states are equal.
  - With the audio off for lack of a device, scenario I counted exactly one "Audio is off" box per OK, Cancel or
    Apply, and Preferences closes after OK/Cancel. This is documented in the Log.
- **mac/win:**
  - `TurnOffMissingMidiPort`'s non-Linux branch is byte-for-byte the old code (`OFF_TEXT` is `"off"`, then
    `UpdateINI()`), and it is called from the same four places.
  - Every dialog change is inside `#ifdef OS_LINUX`. `IsAudioRunning` and `ReopenSavedMidiPorts` are Linux-only.
  - `[[maybe_unused]]` is C++17.
  - My own macOS CMake/Ninja Debug build of `IPlugInstrument-app` (scratchpad, `IPLUG_DEPLOY_PLUGINS=OFF`) links. The
    APP files compile with 0 warnings; the 4 warnings are in RtMidi, RtAudio and swell.
- **Evidence:**
  - I opened `docker-jack-midi/07`, both `08`s, `10`, `11` and `12`, and `r12-adversarial/A2`. They show what the
    Log says: the lists after the round trip, "off" picked, the port picked again, the Audio box state, the MIDI combos
    back after Apply with the audio combos empty (noted in the Log), and the meter up.
  - `checks.txt` (Docker and VM) and `r12-adversarial/log.txt` match the Log, except the A note under R3-5.
- **Re-run:**
  - Fresh Debug build in Docker (`build/t022r3-debug`, 0 warnings).
  - `app-jack-midi-test.sh` → `evidence/t022/review3-docker-jack-midi`: **PASS**, the same 30 `ok:` lines as the
    author's run.
  - Container `t022r3` removed.
- **Adversarial:**
  - `evidence/t022/review3-adversarial/adversarial3.sh` (scenarios F, G, Gq, Gs, Go, H, I, J, K).
  - Its `log.txt` holds F–K; the G variants are in subdirectories.
  - For the comparisons I built `21ce3ddc0` (before R1-2) and `hvoya-linux` (before T-022) in a scratch worktree;
    both are removed now.
- VM: not used.

## Findings

### R3-1 — major — after a JACK server restart, OK or Apply brings the audio back, but MIDI stays dead and File > Quit segfaults
- **Where:**
  - `IPlugAPP_dialog.cpp:383-391` (IDOK, the new `|| !IsAudioRunning()`) and `:395-401` (IDAPPLY);
  - `IPlugAPP_host.cpp:829-838` (`ReopenSavedMidiPorts`).
- **Scenario G** (`review3-adversarial/log.txt`, `pre-G-*`, `base-G-*`, `gdb-G`):
  1. JACK, with both MIDI ports connected.
  2. Stop jackd and start it again, as with QjackCtl Stop/Start. The sources come back.
  3. Preferences → Apply → OK.

  Result:
  - The audio comes back (2 ports).
  - Both MIDI ports stay disconnected. The RtMidi JACK clients belong to the dead server, `isPortOpen()` is still
    true, so `ReopenSavedMidiPorts` skips them.
  - Then File > Quit → **exit 139**.
- **Scenario Go:** the same with OK only. On 607bb7250 it crashes (139). On 21ce3ddc0 it exits 0, because OK didn't
  restart the audio there; it left the audio silently off.
- **Isolation:**
  - **Gq** (server restart, no Preferences, then Quit): exit 0, 2 of 2 runs.
  - **G on `hvoya-linux`** (no JACK MIDI clients): exit 0, 2 of 2 runs.
  - **G on 21ce3ddc0:** exit 139, 2 of 2 runs.
  - **Gs** (server restart, then JACK → ALSA → JACK in the driver combo before OK): **MIDI reconnects to both saved
    ports**, thanks to R1-2, and Quit exits 0, 2 of 2 runs.

  So the crash needs JACK MIDI clients from the dead server that are still alive when a new JACK audio client opens
  in the process. It shows when they are destroyed at exit. gdb (`gdb-G/G-gdb.txt`) has the PC in heap memory under
  `sInstance.reset()` → `~IPlugAPPHost`, which looks like libjack state that is stale once the server is gone.
- **Why it matters:**
  - A JACK server restart is a normal action.
  - The user asked for the most reliable behaviour.
  - With R1-2, a plain OK now reaches the crash, and MIDI stays dead until a driver round trip.
- **Fix (Linux-only):**
  - When the driver is JACK and `!IsAudioRunning()`, recreate the JACK MIDI objects **before** `TryToChangeAudio`
    opens the new audio client, on OK, Apply and Cancel. For example, give `TryToChangeMidiApi` a force flag, or
    call a small `RecreateMidi()` there.
  - The kept names then reconnect the ports, as Gs shows.
  - This also respects the "audio stopped" rule for recreating.
  - Re-run: `adversarial3.sh <out> <app> "G Go Gs"`. Expect MIDI connected after OK and Quit exit 0.

### R3-2 — minor — a port that vanishes while open and comes back is not reopened; the comment promises it is
- **Where:**
  - `IPlugAPP_host.cpp:829-838`: `ReopenSavedMidiPorts` reopens only ports that are closed.
  - `IPlugAPP_host.cpp:606-607`: the comment says the port "reconnects when it is back".
- **Scenario F:** the input is connected to `seq:out`; the source client restarts under the same port name (the same
  happens with a USB re-plug on ALSA). The APP's port is still open, so the input stays disconnected after Apply and
  after OK. The combo shows `seq:out` as selected all the while.
- This is old behaviour on every API, not a regression. But the comment and R1-2's goal suggest more.
- **Fix:** either
  - on Apply, an explicit user action, reselect the saved ports even when they are open. Closing and reopening the
    output is safe under the flag. Or
  - narrow the comment to what happens: "reopened on Apply/OK or a driver change while it was missing".

### R3-3 — minor (UX) — the combo shows "off" while a port name is kept
- **Where:** `IPlugAPP_dialog.cpp:233-239`, `:248-254` → index 0.
- With JACK down (`review3-adversarial/H1-kept-input-list.png`), the combos read "off". But settings.ini still holds
  `seq:out`, and the port connects by itself at the next Apply, OK or start.
- Clearing works if the user picks "off" explicitly. Scenario H: `indev=off outdev=off` after OK, and no reconnect
  once the server is up. The user can't tell, though, that this is needed.
- **Suggestion, optional and Linux-only:** list the kept name as an extra entry, e.g. `seq:out (not found)`, and
  select it, so the combo shows the setting.

### R3-4 — nit — stale wording
- `IPlugAPP_host.h:252-254`: `TryToChangeMidiApi`'s doc says "(unknown ones turn off)". They now stay saved and
  closed.
- `TurnOffMissingMidiPort` sets nothing on Linux, so the name reads as if it did. A name like `OnMidiPortMissing`, or
  a doc line, would read truer.
- `IPlugAPP_host.h:264-265`: the doc on `IsAudioRunning` describes what OK/Cancel do with it. That WHY belongs at
  the IDOK/IDCANCEL condition, said once.

### R3-5 — nit — evidence notes
- `r12-adversarial/log.txt` A: "APP MIDI ports now:" is empty after Apply with the server up. That reads as Apply
  failing. The cause is the old script's race: Apply comes about 1 s after `jack_midiseq` starts, and `A2` shows a
  list with only "off".
- My scenario J does the same with a 2 s wait. Both ports reconnect (`J1-inputs-after-apply.png`). Worth one line in
  the Log.
- `docker-jack-midi` has two `08-*.png` files.

## Not findings
- **Audio boxes:** OK, Cancel and Apply each show one "Audio is off" box while there is no device (I: 1 per press, no
  loop). This is accepted in the Log.
- **ALSA port names with client numbers** (`… 20:0`) change when a USB device comes back on another client number. A
  kept name then never matches, and the user picks the port again. Before R1-2 the name turned "off" instead, so
  nothing is lost.
- **Apply on the same driver leaves the audio device combos empty** (`11`). The author already noted it as T-004
  territory.

## Round 3, re-check

Range `84ca1d167..a5a937370`: f48e2803f (code), a5a937370 (harness, VM probe, re-runs, Log).

**Final verdict: approve.** R3-1, R3-2, R3-4 and R3-5 are fixed and verified. R3-3 is left as an option, which is
acceptable. One new minor finding (R3b-1) comes from the R3-2 fix. It is JACK-only and doesn't block; fix it or record
it as accepted, whichever the user prefers.

### The fix, by path
- **The "audio stopped" condition** is `mDAC && isStreamRunning()`. It is false only after `CloseAudio`'s `abortStream`
  or RtAudio's JACK shutdown `closeStream`. Either way no audio callback is running, so `RecreateMidi` can't race
  `SendMidiMsg`.
- **OK:** the recreate is inside the restart branch. Audio stopped → recreate, then `TryToChangeAudio`. Audio running
  → nothing is recreated, even when the settings differ and the stream restarts.
- **Apply:** called every time. While audio runs it is a no-op (F: Apply with audio running → the input reconnects,
  nothing is recreated). After a driver-combo switch the audio is stopped, so Apply recreates the JACK MIDI objects
  `TryToChangeMidiApi` has just made. That is redundant but harmless; the saved ports reconnect.
- **Cancel:** the recreate runs after `mState = mTempState`, so it follows the restored driver, and before
  `TryToChangeAudioDriverType`/`TryToChangeAudio`, so the dead clients are gone before a new audio client opens.
  `TryToChangeMidiApi` is then a no-op on JACK.
  - JACK → ALSA → Cancel: harness PASS.
  - JACK → PulseAudio → JACK → Cancel (my scenario N): both ports connected, 2 audio ports, settings.ini restored,
    Quit exits 0.
- **No JACK server:** every OK and Apply recreates objects that can't connect. This only adds the RtMidi warnings and
  leaks of R1-5. It is bounded to one recreate per press.
- **Cost of reselecting the ports on every OK/Apply:**
  - On ALSA the APP's sequencer port is kept across `closePort`/`openPort` (`RtMidi.cpp:2037-2063`). External `aconnect`
    subscriptions survive, and only the input thread restarts.
  - On JACK `closePort` unregisters the port (`RtMidi.cpp:3183-3192`). See R3b-1.
  - RtMidi's JACK `closePort` clears `data->port` without synchronising with its process thread. Before, only a combo
    pick ran into that; now every OK/Apply does. No failure was seen in any run; noting it for completeness.

### Re-runs (fresh Debug build `build/t022r3b-debug`, 0 warnings; container `t022r3b` removed)
- **`app-jack-midi-test.sh`** → `evidence/t022/review3b-docker-jack-midi`: **PASS**. The same 35 `ok:` lines as the
  author's run, with the new source-restart and server-restart phases.
- **`evidence/t022/review3b-adversarial`** (`adversarial3b.sh` = the round 3 script plus K2, L, N):
  - **F:** the source restarts; Apply → reconnected; OK → still connected; Quit exits 0.
  - **G** (2 runs) and **Go** (2 runs): after a server restart, OK or Apply brings back the audio and **both MIDI
    ports**, and Quit exits 0. Before the fix this crashed with 139. **Gq** and **Gs** also exit 0.
  - **Ggdb:** `[Inferior 1 … exited normally]`, then "No threads" / "No stack". The script's "exit 1" is gdb's status
    for the failing `bt`, not the APP's. **I agree with the Log.**
  - **K:** 0 events, and the manual `jack_connect` fails with "not a valid port". **I agree with the Log:** the source
    was stopped at Apply, so the reselect closed the input. I added **K2** for the positive case: with the source back,
    OK reconnects it and 8 events arrive in 3.5 s. Both K and K2 show 0 stale events after 20 s of echoed notes with
    the output missing.
  - **L:** see R3b-1.
- **Evidence opened:**
  - `docker-jack-midi/14-server-restarted-note.png`: the meter is up after the server restart and OK.
  - `12`/`13`: the same content as round 3's `11`/`12`, renumbered.
  - `r3fix-adversarial3/log.txt` and `G-gdb.txt` match the Log.
  - `r3fix-adversarial` A–E: as before.
  - `vm-jack-midi/checks.txt`: PASS, with the new bridge name, and main + `gmain` at `exit()`.
- **mac/win:**
  - `HandleMissingMidiPort` is a rename, and its non-Linux branch is unchanged.
  - Every new call is inside `#ifdef OS_LINUX`, and `RecreateMidi`, `RecreateJackMidiIfAudioStopped` and
    `ReselectSavedMidiPorts` are Linux-only.
  - IDOK's braces around `TryToChangeAudio()` don't change the mac/win branch.
  - My macOS CMake rebuild of `IPlugInstrument-app` links, with 0 warnings in the APP files.
- **Comments:** they are the WHY only now. The IDOK comment covers Cancel once, and the R3-4 docs are corrected.

### R3b-1 — minor — on JACK, every Apply/OK drops connections made outside the APP
- **Where:** `IPlugAPP_host.cpp` `ReselectSavedMidiPorts` → `SelectMIDIDevice` → `closePort()`. On JACK that
  unregisters the APP's MIDI port.
- **Scenario L:**
  1. A second source is connected in a patchbay: `jack_connect seq2:out "IPlugInstrument MIDI in:RtMidi Input"`.
  2. Apply: seq2 is **dropped**, while the saved `seq:out` comes back.
  3. Connect seq2 again, then OK: dropped again.

  The output's extra connections go the same way. This is new with the R3-2 fix: before it, OK/Apply didn't touch an
  open port.
- **Suggested fix (JACK-only):** for a saved port that is found and already open, call `openPort(n)` without
  `closePort()`. RtMidi's JACK `openPort` reuses the registered port when `data->port != NULL` and only
  `jack_connect`s the source again (`RtMidi.cpp`, `MidiInJack::openPort` and `MidiOutJack::openPort`). So the source
  comes back and patchbay connections stay. Keep close + open on ALSA, whose `openPort` refuses an open port and
  whose close keeps subscriptions anyway.
- **Alternative:** record it as accepted. QjackCtl/Carla users then lose their extra MIDI connections on each
  Preferences OK.
