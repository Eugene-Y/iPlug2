# Review: T-013 host matrix, final part (Qtractor, Zrythm, summary matrix)

Scope: `git diff 29d9410c0..HEAD` on `linux/T-013` (4bebc23ba harness, f2bebf316 task file + evidence, 894a08707
`Documentation/linux-development.md` §8). Read the task file's Status, Acceptance, the summary matrix and every
Log section; AGENTS §2/§4.

## Round 1

### What I checked and re-ran
- `bash -n` on `dock.sh`, `renoise/call-trace.sh` and the four `qtractor/*.sh`: OK. `make-session.py` compiles.
- Docker, own container `t013r` (`DOCK_ARGS="--device /dev/snd" gui-up.sh t013r`, Qtractor 1.5.3 from
  noble-backports; removed afterwards), trace builds `build/t013-rn-*` unchanged:
  - `make-session.py` with `t013vst3.qtr` as template and the three `.clap` files: output identical to the committed
    `docker-clap/t013clap.qtr` (apart from the session dir).
  - `launch.sh` starts jackd + Qtractor and lists the window. All three plug-ins: `backend=clap-timer-support`, first
    timer callbacks on main. Note: on a fresh config the key is `[Options] Display\StdoutCapture`, not a `[Display]`
    group.
  - `count-hits.sh` (clapOnTimer, IPlugAPIBase::OnTimer, Gneiss::OnIdle), 5 s each: before any editor 686/685/–;
    editor open 739/444/148; **after closing it with its close button: 0/0/0.** The timer-stop observation reproduces.
  - `call-trace.sh` with `PROC=qtractor`, the new `t=` stamps and `--line-buffered`: 0 hits from attach until the
    editor reopens, then ≈157 clapOnTimer/s and ≈31 OnIdle/s on the main LWP; HIT lines interleave with ACTION lines
    as intended. Default `PROC=renoise` path unchanged; the Python `import time` sits in the same gdb `python` block
    as before, so the Renoise use still works (not re-run against Renoise: VM hung). Local output:
    `build/t013r-shots/` (not committed).
  - `apt-cache policy zrythm` on arm64 noble: no package. Qtractor: 0.9.39 (noble), 1.5.3 (backports). Matches.
- Qtractor source (v1.5.3 and v1.6.4, `qtractorClapPlugin.cpp`, `qtractorVst3Plugin.cpp`, plus 1.5.3
  `qtractorSession.cpp`/`qtractorAudioEngine.cpp`):
  - CLAP: `register_timer` starts the shared `QTimer`; `openEditor` → `startTimer(200)` (ref count); `closeEditor` →
    `stopTimer()` stops it at count 0 although `m_timers` still holds the plug-ins' timers. The claim is right, and
    the CLAP file is identical in 1.6.4 apart from casts.
  - CLAP: `plugin_latency_changed` is a no-op (commented out); `plugin_request_restart` returns while
    `qtractorAudioEngine::isProcessing()`. Right.
  - VST3 1.5.3: host context answers no `IRunLoop`; the frame's `RunLoop` forwards to `g_hostContext`.
    `restartComponent` does `getState`/`setState` + param refresh in place. Right.
  - Play start: `qtractorSession::setPlayHeadEx` → `resetAllMonitors` → `pPluginList->resetLatency()`. Right.
  - No `IComponentHandler3` in 1.5.3. Right.
- Screenshots opened: qtractor/01–20, drop-vst3/*.png, latency/lat-1x, lat-8x, editor-open-os1x, docker-clap/01–03
  and the sheet. They show what the tables say (values, layouts, latency "(no latency)" / "0.2 ms (8 frames)", the
  dropped layout in `after-qt-like-deep-gap.png`, both Gneiss editors in `20`).
- Traces: in `vst3-s1.log`/`vst3-s2.log` every `OFF-MAIN` line comes from a thread that logged "fallback thread
  started"; `initialize` shows `factory IRunLoop=(nil) context IRunLoop=(nil)`; SetLatency's lwp 104313 in
  `latency/*.txt` is Gneiss' fallback thread at that time (s2 line 145). `docker-clap/stderr-run*.txt`: no OFF-MAIN.
- The Release builds left on the VM could not be checked (VM hung, not touched).
- AGENTS §2: no plug-in names in `Documentation/` (hosts are named, as elsewhere in §8); no framework code changed.

### Findings

**R1-1 major — the off-main VST3 restart is iPlug's contract violation, not a Qtractor bug.**
`tasks/T-013-host-matrix.md:516-531` (case 5: "That is the host's design…", "a Qtractor-side hazard, reported and not
fixed (the user's rule: no framework guard for single hosts)"), `:46-48` (Acceptance lists "Qtractor 1.5.3's
synchronous VST3 restart on the calling thread" among the host bugs), `Documentation/linux-development.md:195-198`
(listed under "Known host issues … iPlug2 does not work around it").
The VST3 SDK marks `IComponentHandler::restartComponent` `[UI-thread & Connected]`
(`Dependencies/IPlug/VST3_SDK/pluginterfaces/vst/ivsteditcontroller.h:239`). Qtractor is entitled to run it in place;
it is `IPlugVST3::SetLatency` (`IPlug/VST3/IPlugVST3.cpp:277`), reached from `OnIdle` on the fallback thread, that
breaks the contract. The user's "no guard for single hosts" rule is about host bugs, so it does not cover this. This
is T-010 item 1 with a new, concrete consequence: Qtractor runs the plug-in's `getState`/`setState` and its own
parameter refresh on iPlug's fallback thread, concurrently with its main thread. That is exactly what case 7 was to
report to the user. Every host in this matrix except REAPER reaches the fallback with the editor closed.
Fix: (a) In the case-5 section, say that iPlug calls `restartComponent` off the UI thread (a VST3 contract breach, T-010
item 1), and that Qtractor 1.5.3 then runs a state round trip on that thread. List how each host reacts: Renoise
defers to its main thread; Qtractor runs it in place; Carla ignores it; Ardour honours it (did it run in place? say
if unknown). Mark it "for the user (T-010 decision)", not "host hazard". (b) Remove it from the Acceptance list's
host bugs and name it as an open framework point there. (c) In §8, either drop the restart sentence from the
Qtractor VST3 bullet, or reword it as iPlug2's own limitation, e.g. "with no host loop iPlug2 reports a latency
change from its own thread, which VST3 does not allow; Qtractor 1.5.3 then runs its state round trip on that
thread".

**R1-2 major — the 1.6.x remark is misleading: by the same source reading, VST3 timers in 1.6.x stop like CLAP's.**
`tasks/T-013-host-matrix.md:513-514` ("With 1.6.x iPlug would never need the fallback"),
`Documentation/linux-development.md:198` ("(Qtractor 1.6.4's source also offers `IRunLoop` on the host context.)").
In v1.6.4 `qtractorVst3Plugin.cpp`, the host-context `IRunLoop` (`g_hostRunLoop`, `:609-643`, answered at
`:774-777` and by the frame at `:2276`) is driven by `g_hostContext`'s single `QTimer`. `registerTimer` starts it
(`:916-931`). `openEditor` → `startTimer(200)` and `closeEditor` → `stopTimer()` (`:952-956`, `:2617`, `:2631`) stop
it at ref count 0, while the plug-in's timers stay registered: the same pattern as the CLAP bug. iPlug takes the
host-context loop when one is offered (the REAPER row), so with Qtractor 1.6.x and every VST3 editor closed, `OnIdle`
would not run at all. Gneiss' latency change would then wait for an editor, and PDC would not follow at the next
transport start. The present text reads as an improvement, and "Qtractor drops nothing" (`:530`) would be wrong for
the current release.
Fix: in both places say that 1.6.x offers `IRunLoop` on the host context but drives it from the shared timer that
stops when the last VST3 editor closes. By source reading, `OnIdle` would then stop with all editors closed, as in
the CLAP case; untested, since there is no arm64 package. Scope the T-010 conclusion to 1.5.3.

**R1-3 minor — the IPlugControls VST3 case-7 claim cites logs that don't contain it.**
`tasks/T-013-host-matrix.md:511` says "Same for all three plug-ins (`vst3-s*.log`: `first … backend=vst3-irunloop(frame)
on main`)". Neither `traces/vst3-s1.log` nor `vst3-s2.log` has a view attach for IPlugControls: only initialize and
fallback lines. Its editor ran in an earlier session (pid 98477, screenshots `01`–`05`, traces then in Qtractor's
Messages window). Only `05` shows its "VST3 view removed … backend vst3-irunloop(frame) -> fallback-thread" on the main
tid. Fix: cite `05` for IPlugControls and say that session's trace was not kept (or re-run when the VM is back).

**R1-4 minor — the CLAP effect statement misnames what the idle timer carries.**
`tasks/T-013-host-matrix.md:566-567`: "…and parameter and MIDI messages from the processor to the host." For CLAP,
`IPlugAPIBase::OnTimer` (`IPlug/IPlugAPIBase.cpp:149-196`) drains `mParamChangeFromProcessor`,
`mMidiMsgsFromProcessor` and `mSysExDataFromProcessor` into the UI delegate (processor → editor). MIDI and parameter
output to the host goes through `process()` (`IPlugCLAP.cpp:657`, `:698-767`), and the restart request through
clap-helpers' `runOnMainThread`. Fix: "`OnIdle` (Gneiss reports its latency there) and the processor-to-editor
queues; output to the host is unaffected (it goes through `process`)".

**R1-5 minor — the matrix legend lets cases that were not run count as PASS, and the not-run lists are incomplete.**
`tasks/T-013-host-matrix.md:152`, `:173`, `:501`, `:573`.
- "PASS = the cases the host supports work" also covers cases nobody ran.
  - Qtractor VST3: automation write/read (case 3), save/reload with the editor closed and presets (case 4), and
    remove while playing (listed) were not run.
  - Qtractor CLAP: cases 3 (automation), 4, 5 (explained), 6, and the IPlugControls editor were not run.
- Item 9's "all PASS except the GL3 build" also hides "Drag out → Renoise: delivered" (no instrument made) and
  "Drag out → Carla: N/A".

Fix: make the legend "PASS = every case that was run passed; the cases not run are listed in the section". Complete
the "not run" lines of both Qtractor tables (VST3: case 3 automation, case 4 editor closed/presets, case 6 remove
while playing; CLAP: cases 3–6 except names/values, IPlugControls' editor, drops). Say "all PASS except GL3; Renoise
received the file without making an instrument; Carla N/A".

**R1-6 minor — evidence provenance of the VST3 latency traces and the CLAP count file.**
- `latency/latency-editor-closed.txt` and `latency-editor-open.txt` were recorded with the earlier `call-trace.sh`
  (no `t=`, block-buffered: every HIT comes after all ACTIONs) and an earlier `latency.syms` (no `getState`/`setState`).
  `latency-closed-seq.sh` as committed would produce a different file, and the files cannot tie a
  `getLatencySamples` hit to "playback starts" or "the Track dialog". That attribution comes from the source
  (`resetAllMonitors` → `resetLatency`) and the dialog screenshots, which is fine, but say so.
- `docker-clap/clap-timer-counts.txt` has no header (duration, session, how the editor was closed), and its "visible
  editor windows: 0" line sits under "== editor open".

Fix: one line in the case-5 Log naming the script/syms state of those two files and where the attribution comes
from. Give `clap-timer-counts.txt` a header like `onidle-count-editor-closed.txt` has, and move the window-count line
under "== editor closed".

**R1-7 minor — "`scripted-source.txt` has every attempt" is not true.**
`tasks/T-013-host-matrix.md:542`; `drop-vst3/scripted-source.txt` holds two runs: `top` (refused) and one `deep` at
251,71. The gap drop at 360,100 that loaded the layout (`after-qt-like-deep-gap.png`), and the 360,74 and knob drops,
are not in it. Fix: add those runs' output, or reword to "has the `top` run and the `deep` run at 251,71; the gap
drop is shown by `after-qt-like-deep-gap.png`".

**R1-8 nit — §8 wording and wrap.** `Documentation/linux-development.md:192-203`.
- Line 201 is 171 characters: reflow it to the file's ~120 width.
- "Dragging files *out* of the editor works in every host tried": Qtractor is now in the list, but drag-out was not
  tried there. Say "in every host where it was tried (REAPER, Ardour, Renoise)".
- The CLAP bullet's "waits until some CLAP editor is opened again": `register_timer` also restarts the shared timer,
  so the timers also come back when any CLAP plug-in registers a new timer (e.g. another instance is inserted). Say
  "until a CLAP editor opens or a plug-in registers a new timer".

**R1-9 nit — `restartComponent` "ignores what the flags ask for".** `tasks/T-013-host-matrix.md:521`. It honours
`kReloadComponent` (deactivate/activate around the round trip) and treats every other flag alike. Reword: "does the
same state round trip for every flag (`kReloadComponent` adds deactivate/activate); 1.6.4's is identical".

**R1-10 nit — probe comments.**
- `scripts/vm/probe/qtractor/latency.syms:1-2` repeats `renoise/latency.syms`' thunk explanation. Say it once: keep
  the Qtractor-specific part ("Qtractor's binary is stripped, so no host symbols; iPlug's lines as in
  renoise/latency.syms").
- `latency-clap.syms:1`: "the host's deactivate/activate" should be "the plug-in's activate/deactivate (called by the
  host)".

**R1-11 nit — `make-session.py` is not executable.** It has a shebang, and the Log calls it as `make-session.py`, but
its mode is 100644. `chmod +x` it, or write `python3 make-session.py` in the Log. (`count-hits.sh` prints a bare
`bp N` for zero hits; printing "0" would read better, optional.)

### Matrix vs. Log sections
Rows for REAPER, Carla 2.5.10/main, Ardour, Renoise, Qtractor, Zrythm, Wayland and Bitwig agree with their sections
and the section column points to the right headings. The only issues are the legend and the not-run lists (R1-5),
the IPlugControls VST3 citation (R1-3) and the 1.6.x scope (R1-2). Status line and Acceptance agree with the Log
except for the classification in R1-1.

Verdict: changes requested

### Worker's fixes for round 1
- R1-1: the off-main VST3 `restartComponent` is now described as iPlug's contract breach (T-010 item 1), with how
  each host takes it, and marked for the user. It is out of the Acceptance list's host bugs (now an "open framework
  point") and reworded in §8 as an iPlug2 limitation.
- R1-2: the 1.6.x remark now says that the host-context loop runs on the same shared timer, which the last VST3
  editor's closing stops, so `OnIdle` would stop there. Untested. The T-010 conclusion is limited to 1.5.3.
- R1-3: IPlugControls' VST3 case 7 cites `05`, and the Log says its trace was not kept.
- R1-4: the CLAP effect now names the processor-to-editor queues, and says output to the host is unaffected.
- R1-5:
  - the legend reads "every case that was run passed";
  - both Qtractor tables list what was not run;
  - the item 9 line names Renoise "delivered" and Carla N/A.
- R1-6:
  - the case-5 setup says which script and syms versions produced each latency trace, and where the attribution comes
    from;
  - `clap-timer-counts.txt` has a header, and its window-count line is under "editor closed".
- R1-7: `scripted-source.txt` now has every scripted run (pulled from the VM over ssh; the last three are copied from
  the console). The Log wording matches.
- R1-8, §8: the drop bullet is reflowed; drag-out names where it was tried; the CLAP bullet adds "or a plug-in
  registers a new timer".
- R1-9: the Log says "the same state round trip for every flag (`kReloadComponent` adds deactivate/activate)".
- R1-10: the `latency.syms` header is one line pointing to `renoise/`; `latency-clap.syms` says "the plug-in's
  activate/deactivate".
- R1-11: `make-session.py` is executable.

## Round 2

Checked `git diff 894a08707..0aa2fef74` and the files themselves: the task file (Acceptance, legend, item 9 line,
Qtractor case 7/5/8, the CLAP section, the not-run rows), `Documentation/linux-development.md` §8, the evidence edits
(`clap-timer-counts.txt`, `onidle-count-editor-closed.txt`, `drop-vst3/scripted-source.txt`) and the two `.syms`
headers. Nothing to re-run in Docker: no script logic changed (only comments and `make-session.py`'s mode), and the
round-1 Docker re-run still stands. VM not touched.

R1 findings:
- R1-1 **resolved.** Case 5 now says iPlug breaks the `[UI-thread]` contract (T-010 item 1). It lists how each host
  takes the request (Ardour's thread marked unknown) and leaves the call to the user. The Acceptance moves it from
  host bugs to "open framework point". §8 calls it an iPlug2 limitation.
- R1-2 **resolved** in the Log, which matches my reading of v1.6.4, including the note that 1.5.3's frame loop has the
  same timer but iPlug leaves it on close. The T-010 conclusion is limited to 1.5.3. §8: see R2-2 (nit).
- R1-3 **resolved.** IPlugControls cites `05`, and the Log says the trace of that session was not kept.
- R1-4 **resolved.** The effect now names the processor-to-editor queues and says output to the host goes through
  `process`. Matches `IPlugAPIBase::OnTimer`.
- R1-5 **resolved.** The legend says "every case that was run passed". Both Qtractor tables list what was not run.
  The item 9 line names Renoise "delivered" and Carla N/A.
- R1-6 **resolved, one new inconsistency (R2-1).** The provenance of the latency traces and the source of the
  attribution are now stated. The window-count line moved under "editor closed".
- R1-7 **resolved.** `scripted-source.txt` has every run: `top`, `deep` at 251,71, 360,74 twice, and the three-point
  loop from the console. The Log names them and the gap drop's screenshot.
- R1-8 **resolved.** §8 reflowed (no line over 120 in §8). Drag-out names where it was tried. The CLAP bullet adds the
  new-timer restart.
- R1-9, R1-10, R1-11 **resolved** (`latency.syms` header is one line pointing to `renoise/`; `latency-clap.syms` says
  "the plug-in's activate/deactivate"; `make-session.py` is 100755).

New findings:

**R2-1 minor — the new header of `clap-timer-counts.txt` gives a window that contradicts the Log's "same rate".**
`evidence/t013/qtractor/docker-clap/clap-timer-counts.txt:1-3` says the counts are "calls in ~5 s (8 s minus gdb's
attach)". The open-editor count of 1188 over ~5 s would be ≈ 240 clapOnTimer/s. But `timers-fresh-session.txt`
shows ≈ 155/s with the editor open, and the Log says the open-editor rate is "the same" as before. My round-1
re-run of the same `count-hits.sh` showed that the attach takes almost none of the window: 739 calls in a 5 s run
with the editor open, ≈ 148/s. So 1188 calls fit the whole 8 s (≈ 148/s). Fix: "calls in ~8 s (count-hits.sh
qtractor 8 …)", or drop the duration from the header and keep only the per-second rate from
`timers-fresh-session.txt`.

**R2-2 nit — the §8 Qtractor VST3 bullet stops short of the consequence.** `Documentation/linux-development.md:197-198`:
"…drives it from the same kind of shared timer, stopped when the last VST3 editor closes (untested)". Add the
effect, as the Log does: "…so with every VST3 editor closed `OnIdle` would not run at all (from the source;
untested)".

Verdict: approve

### Worker's fixes for round 2
- R2-1: the header of `clap-timer-counts.txt` now says ~8 s.
- R2-2: the §8 sentence on Qtractor 1.6.4 now says that, with every VST3 editor closed, `OnIdle` would not run at all
  (read from the source; untested).
- On R1-7, for the coordinator: `scripted-source.txt` was pulled from the hung VM with `scp` under the VM lock. ssh
  still worked, and nothing touched X.
