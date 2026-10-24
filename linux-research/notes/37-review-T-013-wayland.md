# Review: T-013's GNOME Wayland run (209e24548) and notes/35 (Qtractor CLAP timer report)

Independent reviewer, fresh context, 2026-10-24.

## Round 1

### What I checked

- Commit `refs/heads/hvoya-linux..refs/heads/linux/T-013` = 209e24548 only (base b8079e9df = hvoya-linux). It
  touches only `linux-research/` (task file, evidence logs, four probes); no framework code, so AGENTS §2 (no product
  names in shipped iPlug2 files, mac/win behaviour) is not affected.
- Every screenshot in `evidence/t013/wayland/` (00–07, 10–14, 20–22, 30–33), opened one by one, against each matrix
  cell and finding. Every file in `logs/`.
- `IGraphics/Platforms/IGraphicsLinux.cpp` (key, focus and crossing paths and their traces), and the earlier traces
  of Sand text entries: `evidence/t008/flaky/trace-2/reaper-stderr.txt`,
  `evidence/t013/qtractor/15-sand-vst3-text-input-trace.txt`, `evidence/t013/renoise-fix-r1-docker/sand-clap/`.
- Xwayland's source at tag `xwayland-23.2.6`: `hw/xwayland/xwayland-input.c`.
- **VM spot check** (VM lock 16:20–16:27): one nested shell start with the documented command. It hung (W4), so I
  opened no plug-in. Outer `:0` answered `xdpyinfo` before and after. I removed what I started (details in W4, a side
  effect in W5).
- Part 2: `qtractorClapPlugin.cpp` and `qtractorVst3Plugin.cpp` from github.com/rncbc/qtractor at `v1.5.3`, `v1.6.4`,
  `314600e` and current `main` (4a0729e70, 2026-10-24), plus T-013's `evidence/t013/qtractor/docker-clap/` and the
  Log section "Qtractor CLAP, in Docker".

### Part 1 — GNOME Wayland run

**Confirmed by the evidence:**
- Every cell of the per-case table that cites a screenshot.
  - Opens and renders: `02`, `30`, `10`, `20`.
  - MORPH-off resize, and REAPER's FX window follows (940×620 → 940×200): `06`, `31`.
  - Drags: A hz 457.2 Hz → 20.0 kHz (`02` → `06`); 210.7 Hz → 20.0 kHz (`30` → `31`). Sand comp 0.300 → 0.624
    (`10` → `13`). In Sand VST3 the comp arc grows (`20` → `22`).
  - Text entry: `2000` → 2.00 kHz (`11`/`12`, `21`/`22`), then `31` → 31.0 Hz (`13`). The XI2 trace has keycodes
    12, 10 and 36, i.e. "3", "1", Return, on `xwayland-keyboard`.
  - Help bubble and tooltip: `03`, `32`, `14`, `22`.
  - Space → host transport: `07`, `31` (both "[Playing]").
- Case 7: the `IPLUG_HOSTLOOP` lines match the cells (CLAP timer + fd support; VST3 factory-context `IRunLoop`, all
  on the init tid).
- The VM-state claim before my session was right:
  - no nested processes and no stale files;
  - `:0` answered;
  - the trace builds are staged in `~/iplug-builds/t013-wl/.{clap,vst3}`, and they contain the trace strings;
  - `~/.clap` and `~/.vst3` are dated 2026-10-23 07:45–07:47, right after eda432127 (07:45).
- Finding 1 (wheel) is honestly framed as "not testable", not as a PASS.

**W1 — major. Finding 2's claim about Xwayland is contradicted by Xwayland's source.**

The Log says "Xwayland sends no LeaveNotify when the pointer leaves for a non-X surface … That is Xwayland's behaviour
for every X11 client; nothing to fix in iPlug". Xwayland 23.2.6 (`hw/xwayland/xwayland-input.c`) handles exactly
this case:
- `pointer_handle_leave` (l. 591–609) saves `last_xwindow`, clears `focus_window` and calls `CheckMotion`;
- `xwl_xy_to_window` / `sprite_check_lost_focus` (l. 3077–3140) then return the root window, with the comment
  "the pointer has crossed to another native Wayland window, in this case, pretend we entered the root window so
  that a LeaveNotify event is emitted".

There is a condition, though: `master->lastSlave` must be the Wayland pointer.

So what the run shows is that **in this nested session, with synthetic input, the editor got no LeaveNotify**
(`33`, `04`; `crossing-gneiss-vst3.txt`). The cause is unknown. Candidates:
- the nested X11 backend's pointer handling for warped `xdotool mousemove` jumps;
- the `lastSlave` condition after XTEST input (the wheel test of 2026-10-23 sent XTEST input on `:1`).

None of them supports "every X11 client". If it is real in a Wayland login, it is user-visible: iPlug's tooltip
stays over other applications until the pointer comes back. So "nothing to fix" is premature too.

What should change:
- Reword finding 2 as a nested-harness observation whose cause is unknown, and cite the Xwayland code.
- Check it with a plain X client (`xev`) in the same nested session.
- Add a line to the PC checklist (step 8, Wayland): "hover a control until its tooltip shows, then move the pointer
  straight onto another (native Wayland) window; the tooltip should go away".
- Fix the Summary/Log wording to match.

**W2 — major. Finding 3 (key path) is wrong, and the stderr logs it rests on cannot be complete.**

1. `HandleKey` traces every KeyPress and KeyRelease that reaches the editor's window, including the keys REAPER
   forwards with XSendEvent, marked "(sent by a client)" (`IGraphicsLinux.cpp:1266–1331`). An open edit calls
   `TakeKeyboardFocus`, which traces "take focus from …" (l. 1475–1535). Both kinds of line appear for every
   earlier Sand text entry:
   - REAPER/X11 in `evidence/t008/flaky/trace-2/reaper-stderr.txt`: 38 `keycode` lines and several "take focus";
   - Qtractor in `evidence/t013/qtractor/15-sand-vst3-text-input-trace.txt`.

   CLAP has no other way to bring keys into the edit, yet `sand-clap-stderr.txt` has two edits on screen (`11`–`13`)
   and no "take focus" and no `keycode` line. `sand-vst3-stderr.txt` has no `iPlug2 input:` line at all, although
   that editor took clicks and a text entry (`20`–`22`). Both Sand files are exactly 7 lines long. So the logs are
   partial, or come from another session, or something unexplained happened. Whichever it is, they cannot support
   "the input trace has focus lines but no per-key lines for these edits".
2. "For VST3, REAPER hands keys to the view through `IPlugView::onKeyDown`" contradicts the code's own comment at
   `IGraphicsLinux.cpp:71–73`: REAPER "calls no VST3 onKeyDown".
3. The Docker citation `renoise-fix-r1-docker/sand-clap/reaper-stderr-tail.txt` is a 20-line tail of a run with no
   text entry: its `run.log` steps are knob, pass type, env tab, context menu, about and tooltip. It shows nothing
   about key lines.
4. The XI2 trace (`xi2-sand-clap-text-entry.txt`) shows the keys addressed under the top-level 0x400173. That is
   REAPER's top-level, which also contains the plug-in's window, so it cannot tell "REAPER's FX window" from the
   editor.
5. Finding 4 ("no `OFF-MAIN` in any run") and the case-7 cells rest on the same files.

What should change:
- Re-capture the full `/tmp/reaper.log` for one Sand CLAP text entry in the nested session.
- Say how the `logs/*-stderr.txt` were taken (whole file, filtered, or truncated).
- Rewrite finding 3 from what that capture shows.
- Drop the Docker citation and the `onKeyDown` statement.
- Re-check finding 4 on complete logs.

The on-screen results (text entry works) stand. The problem is the explanation and the trace evidence.

**W3 — minor. Screenshots 03–05 are unexplained.**

Their names say "drag-wheel-help" and "after-drag", but nothing changes from `02` to `05`:
- Main res stays 0.6329 with the pointer on it in `03`;
- A hz is still 457.2 Hz in `05`.

The successful drag is only in `06`, one minute later. If a drag at 08:18 did not register through the compositor,
that is a result to report. If it was only a hover or a wheel, rename the files or say so. `05` is not cited anywhere.

**W4 — minor. The note on the stuck nested-shell start is inaccurate, and its cleanup does not cover the stuck case.**

My start (2026-10-24 16:22) used exactly the documented command, `MUTTER_DEBUG_DUMMY_MODE_SPECS=1280x768 setsid
dbus-run-session -- gnome-shell --nested --wayland`. It hung like run 1:
- the window mapped at 1280×768 but showed only the wallpaper, without the shell's panel;
- Xwayland `:1` did not answer `xdpyinfo` for more than 2 minutes;
- the log had no "GNOME Shell started" line, and `org.freedesktop.impl.portal.desktop.gtk` activation timed out;
- `nested-shell-run1-stuck.log` shows the same signature.

So the hang is intermittent (2 of 4 known starts) and not tied to the size variable. "Not reproduced" should go.
`wmctrl -i -c` did not end the hung shell either (20 s). To clean up I had to:
- SIGTERM the services on its private bus (found through `DBUS_SESSION_BUS_ADDRESS` in `/proc/*/environ`);
- SIGKILL `gnome-shell --nested` and its Xwayland;
- remove `/tmp/.X1-lock`, `/tmp/.X2-lock`, `/tmp/.X11-unix/X1`, `X2`, `wayland-0{,.lock}`, the
  `.mutter-Xwaylandauth.*` file and `gnome-shell-disable-extensions`.

Document how to detect the hang (Xwayland answers within about 10 s; "GNOME Shell started" appears in the log) and
this recipe.

**W5 — minor. The nested shell breaks the outer session's accessibility bus (VM state).**

The nested shell starts its own `at-spi-bus-launcher` on the same `/run/user/1000/at-spi/bus_0`. dbus-daemon unlinks
an existing socket when it binds, and removes its own on exit. After my run, `/run/user/1000/at-spi/` was empty: it
changed at 16:26:30, when the nested daemon stopped. The outer daemon (pid 1262) still listens, but on an unlinked
inode. The author's runs must have done the same, so "VM state left … unchanged" misses it.

The effect is small: outer-session apps started later get no a11y bus (warnings only) until a re-login or a restart
of the outer `at-spi-bus-launcher`. Note it in the harness notes and the VM state; whether to restore it is the
user's call.

**W6 — minor. The crossing evidence cannot be checked.**

`crossing-gneiss-vst3.txt` records neither the watched window nor the pointer path, so "one Leave/Enter pair for
three exits" cannot be verified. The coordinates do fit: Leave at (626,405) on `:1` is over REAPER's arrange view,
and the Enter is on B hz. The third exit, "out of the nested compositor", has no screenshot: `04` and `33` both show
the pointer on the nested desktop. Record the moves with times and the window id (`crossing-watch.c` could print a
timestamp).

**W7 — minor. "Not caused by the probes" is asserted without support.**

Overnight, Gneiss CLAP's window moved and A/B res went to 1.0000. The Log says this was "not caused by the probes",
but there is no evidence either way. It also says the take/release-focus pairs "date from then", but the stderr has
no timestamps. Say the cause is unknown. Also note that `07` (Space) and the wheel's after-value (B hz 853.6 Hz,
visible only in `07`) come from after that change. Finding 1 cites only the log for the after-value.

**W8 — minor. Probe robustness.**

- `nested-env.sh` hard-codes `DISPLAY=:1` and takes `ls /run/user/1000/.mutter-Xwaylandauth.* | head -1`. A killed run
  leaves stale cookie files, so this can pick the wrong one. Read the display and `-auth` from the running nested
  Xwayland's command line instead (`pgrep -a Xwayland`).
- `nested-env.sh` does not check that `NP` was found.
- The UID is hard-coded as 1000 in `nested-env.sh`, but `nested-view.sh` uses `$(id -u)`.
- `nested-view.sh` takes the first X window of the gnome-shell pid as the output window, and the largest child as
  "the view". Neither assumption is stated, and the `:0` `XAUTHORITY` path appears twice in it.
- `nested-open.sh` and `nested-view.sh` have no shebang and no `set -u`.

Comments are within the rules: the update-notice and "reaper-up first stops a running REAPER" comments give a WHY.

**W9 — nit. `crossing-watch.c` style.**

- `if (argc < 3) return 2;` exits without a usage line.
- `printf(...), fflush(stdout);` uses the comma operator; two statements read better.
- The crossings have no timestamp (see W6).

**W10 — nit. Finding 1 (wheel).**

"Mutter's nested X11 backend did not forward the synthetic wheel" is an inference. The trace only shows that no
button reached Xwayland. The XTEST-into-`:1` check bypasses Wayland, so it re-tests the X11 path only. That is fine
as long as the text says so. The wheel is covered by the PC checklist through notes/25 step 2.

**Verdict part 1: changes needed** (W1, W2 major; W3–W8 minor).

### Part 2 — notes/35 (draft Qtractor report)

**Verified against Qtractor's source.** v1.5.3, v1.6.4, 314600e and current main; the last three are byte-identical.

| Claim | Where (v1.6.4 / v1.5.3) | Result |
|---|---|---|
| One shared `QTimer` drives all registered plug-in timers | `class qtractorClapPluginHost::Timer`, `process_timers()` | ✓ |
| `register_timer()` starts it | l. 1127–1141 / 1129–1143 | ✓ |
| `unregister_timer()` stops it once `m_timers` is empty | l. 1144–1157 / 1146–1159 | ✓ |
| `openEditor()` → `g_host.startTimer(200)`, ref-counted | l. 3155 / 3157; `startTimer` l. 1117 | ✓ |
| `closeEditor()` → `g_host.stopTimer()` | l. 3187 / 3189 | ✓ |
| The quoted `stopTimer` body | l. 1120–1121 / 1122–1123 | ✓ verbatim |
| Timers stay stopped "until an editor opens or a plug-in registers a new timer" | `register_timer` calls `m_pTimer->start` | ✓ |
| Suggested fix | `stopTimer` is a member, so `m_timers` is in scope; tab style matches | ✓ |
| `latency_changed` ignored; `request_restart` skipped while the engine runs | `plugin_latency_changed` empty; `plugin_request_restart` returns on `isProcessing()` | ✓ |
| VST3 1.6.4: shared `QTimer` behind the host-context `IRunLoop`, editor start/stop | `queryInterface` exposes `g_hostRunLoop`; `startTimer`/`stopTimer` l. 952–956; editor l. 2617/2631; `unregisterTimer` stops when empty | ✓ |

**Measurements against the evidence:**
- `clap-timer-counts.txt`: 1188 `clapOnTimer` hits in about 8 s with the editor open (≈150/s); none after the close.
- `timers-fresh-session.txt`: 150/s before any editor, 155–158/s while one is open.
- `onidle-count-editor-closed.txt`: `process` 8429 hits in 10 s, and `OnIdle`/`SetLatency` 0, with the editor closed.
- These match the "Actual" lines and the T-013 Log.

**Q1 — major. "Any plug-in built with iPlug2 does" is wrong, so a stranger cannot reproduce.**

CLAP timer registration is in this fork's unmerged Linux port (`IPlug/Linux/IPlugLinuxHostLoopCLAP.h`). Upstream
iPlug2 has no Linux support. Qtractor's author cannot get such a plug-in, and steps 1–2 need one.

What should change:
- Say "plug-ins built with an in-development Linux port of iPlug2".
- Give a reproducer he can run. Best: attach a minimal CLAP plug-in (about 80 lines; it registers a 100 ms timer in
  `init` and prints `on_timer` calls per second to stderr). Or name a public CLAP plug-in, but only one that has been
  verified to keep a timer registered with its GUI closed.

**Q2 — minor. Version references.**

"main (314600e)" is already stale: main is 4a0729e70 (2026-10-24), with an unchanged file. Cite the tag and the
lines instead, e.g. "v1.6.4 (unchanged on main as of 2026-10-24), `qtractorClapPlugin.cpp` l. 1120, 1144–1157, 3155,
3187". 1.5.3 has the same code at l. 1122, 1146–1159, 3157, 3189.

**Q3 — minor. The measurements lack context a stranger needs.**

- "Three plug-ins" are unnamed, and "≈150 calls/s in total" means nothing without their registered periods. State
  them, or give the rate per plug-in.
- Name the environment: Ubuntu 24.04 arm64, Docker, Xvfb + openbox, the Qt version of the backports package.
- Say how the editor was closed (the track menu's Edit toggle).
- The "before any editor" figure comes from a per-second trace, not from the 8 s gdb counts.

**Q4 — minor. The "Effect" paragraph invites "we ignore latency.changed anyway".**

The "Also seen" section says Qtractor ignores `latency.changed`. State the concrete consequence instead: the
plug-in's own main-thread update never runs, so even the `latency.get` that Qtractor reads when playback starts
returns the old value (`SetLatency` was never hit while the editor was closed).

**Q5 — nit. The VST3 note can say more.**

- In 1.5.3 the host context exposes no `IRunLoop`; only the plug frame does (`queryInterface` l. 626–638). So 1.5.3's
  VST3 host is not affected, which fits T-013's VST3 traces ("frame `IRunLoop` open; fallback closed").
- The same one-line fix applies to 1.6.x: `&& m_timerHandlers.isEmpty()`.

**Q6 — nit. Offer to test a patch.**

The tone is factual and polite, and the report asks nothing unreasonable. A closing line offering to test a patch
would help.

**Verdict part 2: changes needed** (Q1 major; the rest minor or nit). The source analysis and the fix are correct.

## Author's response to round 1 (2026-10-24, commits a6ce313c2 and the next on linux/T-013; notes/35: d210dba81 on hvoya-linux)

Part 1:
- **W1:** re-checked in a fresh nested shell with no XTEST input on `:1` (`evidence/t013/wayland/recheck/`): xev, a plain
  X11 client, gets one Leave only for the move onto REAPER, none for the nested desktop or out of the compositor; Sand's
  view the same, its tooltip stays (`tooltip-after-leave-to-desktop.png`). Finding 2 reworded as a nested-session
  observation, Xwayland's source cited, cause open (warped synthetic motion the likeliest; a glide test,
  `nested-xev-glide.sh`, could not run: both starts hung). PC checklist step 8 checks the tooltip and the wheel in a
  Wayland session.
- **W2:** re-captured (`recheck/reaper-stderr.txt`, whole file): take focus, keycodes handled, release focus, as on
  X11; counts equal before and after gdb's `fflush(0)`, on X11 too (`logs/x11-sand-clap-flushcheck*.txt`). Finding 3
  rewritten; the onKeyDown and Docker citations dropped; the first session's missing lines stated as unexplained.
  Finding 4 re-checked on the whole re-check log; host-loop lines are flushed one by one.
- **W3:** `03`–`05` renamed (MORPH on, no change; help bubble stays / hides); wheel shots `06a`, `06b` added (from
  before the overnight change).
- **W4:** hang now 4 of 7 starts; `nested-start.sh` detects it, `nested-stop.sh` cleans up.
- **W5:** `nested-start.sh` gives the nested shell its own `XDG_RUNTIME_DIR`; the deleted outer sockets are in the
  VM state for the user. While fixing this, the first `nested-stop.sh` deleted the outer `/tmp/.X11-unix/X0` (it
  took the outer display number); fixed and documented.
- **W6:** `crossing-watch.c` prints times; the re-check logs the pointer path and the window id.
- **W7:** "not caused by the probes" replaced by "cause unknown; no command of this worker ran then".
- **W8, W9:** `nested-env.sh` reads the shell's runtime dir, log and cookie, checks the shell exists; `nested-view.sh`
  states its choices and finds the viewable nested window; shebangs added; `crossing-watch.c` usage line, no comma
  operator. **W10:** finding 1 says the cause is not known and that the XTEST check covers the X11 side only.
- Also: `reaper-up.sh` writes `verchk=0` (REAPER's 7.83 update notice is modal and blocked `rpr.sh`).

Part 2 (notes/35):
- **Q1:** a minimal CLAP reproducer (`timer-probe.c`, inline in the report) was built and run in Qtractor 1.5.3
  (Docker): 8–9 calls/s before and with the editor, none for 17 s after it closed, back on reopening
  (`evidence/t013/qtractor/timer-probe/` on linux/T-013). The framework is named as an in-development Linux port.
- **Q2–Q6:** v1.6.4 line references (unchanged on main as of 2026-10-24) and 1.5.3's; environment and close method;
  the concrete latency effect; the VST3 1.5.3/1.6.x note with its one-line fix; an offer to test a patch.
