# Review: T-013 Renoise results + XEmbed FOCUS_OUT fix (round 1)

Range `92738e8c7..cd1904d3c` on `linux/T-013`: 196036808 (IGraphicsLinux XEmbed fix, notes/13) and cd1904d3c (Renoise
Log, probes, evidence). Fresh-context reviewer, 2026-10-21.

**Verdict: changes requested** (R1-1; the rest are small).

## What was checked

- `git diff 92738e8c7..HEAD --stat`: one code file, `IGraphics/Platforms/IGraphicsLinux.cpp` (+8/-1). No shared or
  mac/win file touched. Linux-only: yes.
- `scripts/coord/check-product-names.sh HEAD`: clean.
- No secrets, keys, the VM's address or the user's mail in the diff. `/home/gene/...` paths appear only in VM evidence
  under `linux-research/`, as in earlier evidence.
- Docker build (Release, `-DIGRAPHICS_TRACE_LINUX_INPUT`, build dir in my scratchpad): IPlugControls CLAP + VST3
  build with no warnings in IGraphicsLinux*. REAPER smoke `steps/iplugcontrols-p3b.txt` (text entry, focus, menus,
  clipboard, key forwarding), CLAP and VST3: **PASS** for both. I opened `c07-textentry-committed`: 42.00 committed.
  REAPER sends no XEmbed, so this run only shows that the unchanged paths still work. The changed branch runs only
  in an XEmbed host.
- Evidence vs the Log:
  - `textentry-sand-unfixed.txt`: after `TakeKeyboardFocus`, FOCUS_OUT (5) → `CommitEdit`, with a backtrace from
    `ProcessXEvents`. Then 2, 4, 4. The X `FocusOut` before it gave `LocateFocus` = Ours, otherwise
    `mHoldsFocus` would have been false. This matches the claim that the X focus stays on our window.
  - `stderr-session3-fixed.txt`: `check focus: at 0x2c00002 (ours)`, the typed keys `handled`. The click-away is
    `FocusOut mode 0 detail 3` followed by the commit, i.e. the `Elsewhere` path.
  - Screenshots I looked at: `54`/`55` (2000 → 2.00 kHz), `56` (500.0 Hz committed by the click into Renoise),
    `58` ("renoise" typed into IEditableTextControl).
  - Case 7: `stderr-session2.txt` shows the fallback thread before attach, the frame IRunLoop on main while attached
    (fd and timer callbacks), and the fallback thread again after close. In `latency-editor-closed.txt`, `SetLatency`
    runs on lwp 24843, which `stderr-session2.txt:101` names as Gneiss' fallback thread. The `setProcessing`/
    `setActive`/`getLatencySamples` restart follows on lwp 22008 = pid = main. The conclusion "no framework change
    for Renoise" is supported, apart from R1-5.
- Probes: `rn-lua.py` sends to `127.0.0.1:8000` only and never enables the OSC server. Nothing in the diff writes
  `Config.xml`, and the Log says the server is off again (I could not check the VM's state: no VM lock).

## How the fix decides, and what it misses

`HandleClientMessage` (`IGraphicsLinux.cpp:1327-1337`): on XEmbed FOCUS_OUT it calls `XGetInputFocus` and declines
(commits the edit) only if `focus != w.mWindow`.

Points that are fine:
- **No race for the Renoise case.** Renoise's FOCUS_OUT is its reaction to our own `XSetInputFocus`, and that request
  was flushed on our connection before our `XGetInputFocus`. So the reply cannot predate our own focus change.
- **No Xlib error.** `XGetInputFocus` cannot raise BadWindow, so the missing `ScopedXErrorTrap` is fine.
- **No child windows to miss.** Our window has no children (the tooltip is a root child), so comparing with
  `mWindow` alone is exact today. A WebView child (T-017) would need `LocateFocus`.
- **A real loss is still handled.** When the X focus leaves, the X `FocusOut` path (`HandleFocusOut`, `LocateFocus`)
  still runs. If the X focus has already gone when FOCUS_OUT is read, the edit ends as before.

What it gets wrong is the case below.

### R1-1 — major — `IGraphicsLinux.cpp:1327-1337`: a logical focus move inside a GtkSocket host no longer ends the edit, and the edit then takes the host's keys

The new rule reads "the X focus is still ours" as "the FOCUS_OUT is spurious". For a spec-following embedder that is
the normal state of a real FOCUS_OUT. XEmbed focus is logical: the embedder keeps the X focus and moves focus
between its widgets without any X focus change. The case that breaks, likely in Ardour:
- Ardour's VST3 X11 view is a `Gtk::Socket` (GTK2). A GtkSocket adopts a foreign child created in its window and
  speaks XEmbed to it: EMBEDDED_NOTIFY, FOCUS_IN/OUT, WINDOW_(DE)ACTIVATE.
- An edit is open, and our `XSetInputFocus` put the X focus on our window. The window manager still regards the
  top-level as focused, because the focus moved to an inferior.
- The user clicks a widget in the same plug-in window, for example the preset combo or the bypass button in Ardour's
  plug-in window header. GTK moves its focus to that widget internally, and the GtkSocket sends FOCUS_OUT.
- Nobody calls `XSetInputFocus`, and the window manager does not refocus an already active top-level. The X focus
  stays on our window.
- Before 196036808 the edit ended on that FOCUS_OUT. Now it is ignored: the edit stays open and the X focus stays on
  our window, so the keys the user types for the host widget go into the plug-in's edit.
- The click was in the host's window, so `ITextEntryControl` sees no "click outside" either. The edit ends only on
  Return/Escape, a click back into the editor, or the top-level losing the X focus.

The earlier Ardour PASS (case 2) does not cover this: it ran click → type → Enter, which never reaches the changed
branch. The fix was verified in Renoise only, but it changes FOCUS_OUT handling for every XEmbed embedder.

What tells the two cases apart is the event sequence, not the X focus. Renoise's spurious FOCUS_OUT comes with
FOCUS_IN in the same burst (`textentry-sand-unfixed.txt`: 5, 2, 4, 4). A real one comes without it.

**Fix:**
- On FOCUS_OUT while `mHoldsFocus`, record a pending embedder focus-out with its time, e.g.
  `std::optional<steady_clock::time_point> mEmbedderFocusOutAt`, or a bool plus a time point as elsewhere in `Impl`.
- Add `kXEmbedFocusIn = 4` and clear the pending focus-out when FOCUS_IN arrives.
- In `UpdateEditingState`, when the pending focus-out is older than `kFocusQuiet` (150 ms): clear it and call
  `DeclineKeyboardFocus()` if still `mHoldsFocus`.
- The `XGetInputFocus` check can then go, or stay as a fast path: X focus not ours → decline at once.
- Verify in Renoise (the textentry probe as in session 3). Verify in Ardour too: open a Sand edit, click the plug-in
  window's preset combo, type, and the edit must have committed with the keys not in it. If Ardour turns out not to
  send FOCUS_OUT there, record that in the Log and downgrade this finding. The pairing is still the safer rule.

### R1-2 — minor — `IGraphicsLinux.cpp:1320-1321`, `1329-1330`: comments no longer match the code

- 1320-1321 still says the embedder's "focus-out ends an edit like a real one". Two reasons it is wrong:
  - It now ends one only conditionally.
  - It was already unlike a real one: an X `FocusOut` into the host's own window keeps the edit open
    (`LoseKeyboardFocusToHost`).
- 1329-1330: the first sentence is a good WHY. "A focus that really left is no longer ours" is cryptic.

**Fix:** after R1-1, say once, in the case's comment, why a FOCUS_OUT followed by FOCUS_IN is ignored (Renoise
answers our focus request that way). Shorten the preamble to "the embedder is remembered for focus requests; its
FOCUS_OUT ends an edit". Update the notes/13 XEmbed row to match.

### R1-3 — minor — `IGraphicsLinux.cpp:1304-1341`: XEmbed messages leave no trace

The input trace (`IGRAPHICS_TRACE_LINUX_INPUT`) logs X `FocusIn`/`FocusOut` but no `_XEMBED` message. Session 3's
log therefore cannot show that the fix acted, and checking which hosts send FOCUS_OUT (R1-1, Carla, Bitwig) needs
the gdb probe and Renoise-only scripts.

**Fix:** add `TRACE_INPUT("XEmbed message %ld, holding focus %d\n", message.data.l[1], w.mHoldsFocus);` before the
switch, plus one line when a FOCUS_OUT is ignored or acted on.

### R1-4 — nit — `IGraphicsLinux.cpp:1331-1335`: a server round trip even when no edit holds the focus

`XGetInputFocus` is synchronous and runs before the `mHoldsFocus` test. Renoise sends FOCUS_OUT on every
deactivation of the plug-in window.

**Fix:** `if (!w.mHoldsFocus) break;` first. If the X check stays after R1-1, name it, e.g.
`const bool xFocusStillOurs = focus == w.mWindow;`.

### R1-5 — minor — T-013 Log case 5/7 and `evidence/t013/renoise/latency-*.txt`: evidence from an older probe, one claim not in it

- `latency-editor-closed.txt`/`-open.txt` have `this=`/`arg=` fields, `LUA` lines and gdb's `Breakpoint N at`
  lines. The committed `call-trace.sh` prints `x0=`/`x1=`, `ACTION` lines and filters `Breakpoint` lines out, so
  those files came from an earlier version of the script and cannot be reproduced with it.
- "`TVst3Plug::restartComponent` defers a lambda" is not in any evidence file: no latency run traced
  `restartComponent`, although `edits.syms` lists it. What the evidence shows is the outcome: the restart runs on
  main.

**Fix:** either re-run with `latency.syms` plus the two `restartComponent` lines, or say in the Log that the latency
files predate the final `call-trace.sh` and that the deferral is inferred from the outcome (or from disassembly, if
that is where it came from).

### R1-6 — nit — T-013 Log, case 5 cell: "PASS" where the acceptance item says "host PDC follows"

The Log itself says the PDC value is not observable in Renoise. What was shown is that the restart is honoured and
`getLatencySamples` is re-read.

**Fix:** "restart honoured, latency re-read (PDC value not observable)" instead of a bare PASS.

### R1-7 — nit — T-013 Log heading "2026-10-21 — Renoise 3.5.4 (CLAP, VST3)"

Renoise has no CLAP (every CLAP cell N/A).

**Fix:** "(VST3; no CLAP in Renoise)".

### R1-8 — nit — `scripts/vm/probe/renoise/call-trace.sh:1-6`, `rn-lua.py:2-5`: probe headers miss two constraints

- `call-trace.sh` reads `$x0`/`$x1` in every hit, and `mouse.syms`/`textentry.syms` use `$x1` offsets. It is
  arm64-only, but the header does not say so.
- `rn-lua.py` says to enable Renoise's OSC server, but not that it evaluates Lua from any interface and must be
  turned off afterwards. That warning lives only in the Log.

**Fix:** one clause in each header.

## Round 1 fixes

| Finding | Resolution | Commit |
|---|---|---|
| R1-1 major | The XEmbed FOCUS_OUT check is redesigned. A FOCUS_OUT during an edit is now only recorded, with its time. A FOCUS_IN clears it. `UpdateEditingState` ends the edit once it has been pending longer than `kFocusQuiet` (150 ms). The pending flag is also cleared by our own focus request, release and decline, so a stale FOCUS_OUT cannot end a later edit. The `XGetInputFocus` check is gone. Tested on the VM: Renoise (5, 2, 4 → the edit stays; type, Enter, click-away all work) and Ardour. In Ardour, the edit commits on the preset dropdown's FOCUS_OUT, which no FOCUS_IN follows, and host keys reach Ardour afterwards. Docker REAPER smoke (`iplugcontrols-p3b`, `sand`; CLAP and VST3): PASS. **Downgraded:** Ardour 8.4 never sent a FOCUS_OUT while the X focus was ours. A click on a header button moves the X focus to Ardour's window first, so 196036808 would also have ended the edit at the dropdown. The pairing rule stays because it is independent of the X focus. | c86d82e5d; Log and evidence cc282e3da |
| R1-2 minor | The preamble now says the embedder is remembered for focus requests and that its FOCUS_OUT ends an edit. The WHY (Renoise's FOCUS_OUT/FOCUS_IN burst; a GtkSocket keeps the X focus on our window) is stated once, at the FOCUS_OUT case. notes/13 XEmbed row updated. | c86d82e5d |
| R1-3 minor | `TRACE_INPUT("XEmbed message %ld, holding focus %d")` before the switch, plus one line when a FOCUS_IN answers the FOCUS_OUT and one when an unanswered FOCUS_OUT ends the edit. | c86d82e5d |
| R1-4 nit | No X call remains on the message; FOCUS_OUT does nothing unless `mHoldsFocus`. | c86d82e5d |
| R1-5 minor | `latency-editor-{closed,open}.txt` re-run with the committed `call-trace.sh`. `latency.syms` now also lists Renoise's `restartComponent` and its lambda's `_M_invoke`. The closed run shows `restartComponent` on the fallback thread, the lambda on main, then `restartComponent` again on main, followed by the restart. The open run shows `restartComponent` on main, handled at once. The Log states this and notes the re-run. | cc282e3da |
| R1-6 nit | The case 5 cell now reads "restart honoured, latency re-read (PDC value not observable)". | cc282e3da |
| R1-7 nit | Heading changed to "Renoise 3.5.4 (VST3; no CLAP in Renoise)". | cc282e3da |
| R1-8 nit | The `call-trace.sh` header says it is arm64-only. The `rn-lua.py` header warns that the OSC server evaluates Lua from any interface and must be turned off afterwards. | 24b59a29d |
| Also | `Dependencies/IPlug/VST3_SDK/README.md` restored. The Renoise OSC server is off and the VM lock released; Ardour quit with "Just quit", so the session file is unchanged. | — |

## Round 2

Range `45b148cb2..2755ffa37` (c86d82e5d, 24b59a29d, cc282e3da, 2755ffa37). Same reviewer, 2026-10-21.

**Verdict: approve.** R2-1 is a one-line comment fix; it should land before the merge, and no further review round
is needed. R2-2 and R2-3 are optional.

### What was checked

**The pairing logic** (`IGraphicsLinux.cpp:1332-1347`, `1361-1373`):
- A FOCUS_OUT is recorded only while `mHoldsFocus`. Renoise's FOCUS_OUT at the opening click, before we take the
  focus, is ignored as before.
- A FOCUS_IN clears it.
- `UpdateEditingState` declines once the FOCUS_OUT has been pending for more than 150 ms. That function runs after
  every X event and on every display-timer tick, so the deadline is met within about one frame. The `mOpen` guard
  after the decline matches the other call sites.
- The pending flag is also cleared by:
  - `RequestKeyboardFocus`, so a click back into the editor wins;
  - `ReleaseKeyboardFocus`, when the edit ends by Return or Escape;
  - `DeclineKeyboardFocus`;
  - `CloseWindow` (`w = Impl()`), so nothing survives a close and reopen.
- I found no way for a stale FOCUS_OUT to end a later edit, or for an edit to stay open after an unanswered
  FOCUS_OUT while we hold the focus. A menu is ended the same way as an edit (`EndTextEntryOnFocusLoss`), consistent
  with the X path.
- One timing assumption: Renoise's FOCUS_IN must reach us within 150 ms of its FOCUS_OUT. Renoise sends them in one
  burst (`stderr-session4-r1.txt:123-127`: 5, 2, 4), so only a stall in Renoise of more than 150 ms between the two
  sends could end an edit wrongly. That is acceptable.

**R1-1 to R1-8:** all resolved as the "Round 1 fixes" table says.
- R1-1: the redesign is the fix I proposed. The downgrade is fair; the evidence is under "Evidence" below.
- R1-2: resolved, but see R2-1.
- R1-3: the traces are in place and are what made the VM evidence readable.
- R1-4: there is no X call left on the message.
- R1-5: the latency evidence was re-run (see below).
- R1-6, R1-7: the Log wording is fixed.
- R1-8: both probe headers have the clauses.

**Evidence:**
- `stderr-session4-r1.txt`: two edits, each "5, 2, 4 → FOCUS_IN answers the FOCUS_OUT: the edit stays". The keys
  are handled. The click-away is `FocusOut detail 3` (`Elsewhere`).
- `ardour/r1-input-trace.txt`:
  - The header-button click gives `FocusOut detail 0, holding focus 1`, with no "focus lost" line (outside the
    episode), so the edit stays.
  - MODALITY_ON, FOCUS_OUT and WINDOW_DEACTIVATE follow, with no FOCUS_IN, then "FOCUS_OUT unanswered: the edit
    ends".
  - The later forwarded keys are dropped.
- The new `latency-editor-*.txt` match the committed `call-trace.sh` format. In the closed run, `SetLatency` and the
  first `restartComponent` run on lwp 41235, which `stderr-session4-r1.txt:60` names as Gneiss' fallback thread. The
  lambda's `_M_invoke` and the restart run on 41209 (pid, main). In the open run, all of it runs on main after the
  view attached (`:170-174`).
- The Log's claims hold.

**Docker** (scratchpad build dir):
- IPlugControls CLAP + VST3 at 2755ffa37 with `-DIGRAPHICS_TRACE_LINUX_INPUT`: builds, with no IGraphicsLinux
  warnings, and the new trace strings are in the binary.
- REAPER `iplugcontrols-p3b.txt` smoke: **PASS** on both formats.
- REAPER sends no XEmbed, so the new branch is covered only by the VM evidence above.

**Rules:** the change is Linux-only (`IGraphicsLinux.cpp` only) and the style matches the surrounding code.

### R2-1 — minor — `IGraphicsLinux.cpp:1333-1335`: the comment claims something the Ardour evidence contradicts

"(a GtkSocket host moving its focus to one of its widgets keeps the X focus on our window too)" was my round-1
hypothesis. Round 1's own Ardour trace shows the opposite: a click on Ardour's header button moved the X focus to
Ardour's window (`FocusOut mode 0 detail 0`). A comment stating an unverified, contradicted behaviour as fact will
mislead the next reader. The table's R1-2 row repeats the claim ("a GtkSocket keeps the X focus on our window").

**Fix:** end the comment with "a FOCUS_OUT that no FOCUS_IN follows is a real one, wherever the X focus is." and
drop the parenthetical. Optionally, the member comment at `:431` could say "while we hold the focus" instead of
"during an edit": a menu takes the focus too.

### R2-2 — nit (optional) — `IGraphicsLinux.cpp:1364`: an unanswered FOCUS_OUT is dropped if the X focus went to the host during the click episode

If the embedder sends a real FOCUS_OUT and also moves the X focus into its own window within 1.5 s of the
edit-opening click, the following happens:
- `LoseKeyboardFocusToHost` clears `mHoldsFocus`.
- The deadline then finds `!mHoldsFocus` and drops the FOCUS_OUT.
- `AskForKeyboardFocusAgain` takes the focus back.

This is the same as before these commits, which depended on event order, and needs a host click within 1.5 s.
Declining on `mHoldsFocus || mFocusLost` (and clearing `mFocusLost`) would let the embedder's explicit signal win
over the episode heuristic. Leave it unless a host shows it.

### R2-3 — nit — T-013 Log line 153 and notes/27 R1-1 row: wording

- Log line 153 runs on past the wrap ("…from an earlier script. While the transport is stopped…"). Break it, or put
  the re-run note on its own line.
- The R1-1 row says "Ardour 8.4 never sent a FOCUS_OUT while the X focus was ours". The trace covers one sequence:
  a header-button click, then the dropdown. "In the sequences tried" is what the evidence supports, as the Log
  already says ("did not occur in Ardour 8.4").
