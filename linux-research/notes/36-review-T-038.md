# Review of T-038: VST3 host notifications from the fallback thread

Task: `tasks/T-038-vst3-fallback-main-thread.md` (design note and results in its Log). Branch `linux/T-038`
from hvoya-linux 901602a14. Each round is an independent reviewer subagent with fresh context; the owner's fixes
follow each round.

## Round 1

**Verdict: approve with minors.** No blocker, no major. The design holds: deferral only on the instance's own
fallback thread, the queue touched only under the gate, the fallback thread never blocks on the gate, and nothing
on the audio thread changes. The measurements reproduce. mac/win see only empty statements.

### What I read
- `AGENTS.md`, both `CLAUDE.md`s, the task file (design note and Log), notes/09 (host loop).
- The full code diff `901602a14..e3e99d498`: `IPlugLinuxHostLoop.{h,cpp}`, the new `IPlugLinuxVST3HostThread.h`,
  `IPlugVST3.{h,cpp}`, `IPlugVST3_View.h`, `IPlugEffect.cpp`, the unit tests, `linux-development.md` §8/§9.
- Context the diff relies on: `IPlugLinuxHostLoopVST3.h` (the selector and `SetBackend` order),
  `IPlugAPIBase::CreateTimer` (20 ms idle timer on the instance's own `HostLoop`), `IEditorDelegate` (params exist
  before `mHostThread` is built, so `NParams()` is right), and every host call in `IPlug/VST3` and `IGraphics.cpp`
  (`componentHandler`, `beginEdit`…, `restartComponent`, `resizeView`, `createContextMenu`).
- Evidence: `survey-reaper/`, `reaper-deferred-{fix,base}/` (summary, calls-by-phase, pdc, trace), `vm-renoise/`
  (all 5 files; the stderr trace has 4 "deferred" lines on the fallback tid 5323 and 4 "sent" lines on the main tid
  5293), `vm-qtractor/` (2 and 2, fallback tid 7535 and main tid 7521), `smoke-r0/` (summary, every `RESULT.txt`, the
  `pdc` and "deferred" counts of the four host-loop runs), `unit/`. Screenshots I looked at: `gneiss-vst3/g02-os-cycle`,
  `sand-vst3/r03-two-instances`, `controls-p3b-vst3/r02-after-reopen`. All three show a fully drawn editor.

### What I re-ran (Docker; container `t038r` removed afterwards; builds in `linux-research/build/t038r-*`)
- `Tests/LinuxHostLoop`, ASan+UBSan as configured: 3/3 "all host-loop tests passed".
- The same test under TSan (built with g++ `-fsanitize=thread -O1` by hand, run under `setarch -R`): 5/5 passed,
  0 ThreadSanitizer warnings.
- `scripts/t038/reaper-deferred-test.sh` on a fresh Debug IPlugEffect VST3 (`IPLUG_TEST_VST3_FRAME_RUNLOOP_ONLY`,
  `_LATENCY_FROM_IDLE`, `_EDIT_FROM_IDLE`, `IPLUG_TRACE_HOST_LOOP`; CMakeCache confirms Debug and the flags):
  **PASS**. idle: 6 `SetLatency` on the fallback thread and no host call; poll: 12× `beginEdit`/`performEdit`/
  `endEdit`/`restartComponent(8)` all on REAPER's main thread, `pdc` 256 0 256 0 …; play: `getLatencySamples` on main;
  open: the attach sends what waited. This build has the trace lines: 36 "deferred" lines, all on the two fallback
  tids (129, then 454 after the editor closed); 13 "sent" lines, all on the main tid 65. Same counts as the Log.
- `scripts/host-loop-test.sh` on a Release trace build (`_LATENCY_FROM_IDLE`, `_EDIT_FROM_IDLE`,
  `IPLUG_TRACE_HOST_LOOP`, REAPER's factory loop): **PASS**, `pdc` 0 256 256 0 256 256 0 256, no "deferred" line.
- `check-product-names.sh`: clean.
- macOS: not rebuilt. I ran `unifdef -UOS_LINUX -UIPLUG_TEST_EDIT_FROM_IDLE` on the four shared files, before and
  after, and diffed them. The only differences are the empty `VST3_HOST_CALL` define, its 8 uses (each now an empty
  statement) and 2 blank lines. `IPlugVST3_View.h` and `IPlugEffect.cpp` are identical. The worker's xcodebuild
  result stands.

### Design check
- **UI-thread identity, deferral scope:** right. Deferring only on `IsFallbackThread()` keeps hosts with a loop
  byte-for-byte on today's path and keeps the audio thread out of the queue.
- **Deadlocks:** I found none.
  - The fallback thread only try-locks.
  - `attached`/`removed` hold the gate while `SetBackend` joins the fallback thread. That thread can't be in
    `Dispatch` then, because dispatching needs the gate.
  - The backend's own mutex is held only for bookkeeping.
  - A UI thread that waits on the gate waits for one `OnIdle`, which no longer calls the host.
- **Lost or duplicate notifications:**
  - Flags are OR-ed, and edits keep the last value. Both are cleared before the host call, so a nested call can't
    resend them.
  - Only the outermost call flushes (the unit test covers a host that calls back inside `restartComponent`).
  - `terminate` drops what waits, which is right.
- **Realtime:** `process`, `setProcessing` and `getLatencySamples` are untouched. On a non-UI thread a `Call` costs
  one acquire load plus `pthread_equal`, and nothing allocates after construction.
- **Processing-state livelock:** the processing-state calls don't flush. Renoise's restart sequence ran nested in
  the flush (`setProcessing(0)`, `setActive(0/1)`, `setProcessing(1)` on main) and did not hang.
- `std::recursive_mutex` instead of a WDL type is justified: `WDL_Mutex` has no try-lock.

### Findings

**R1-1 (minor): `initialize()` is not covered by the gate, but the fallback thread is already running there.**
`mHostLoopSelector.Initialize(context)` starts the fallback thread. Then `OnHostIdentified()` and
`OnParamReset(kReset)` run on the UI thread without the gate. `OnParamReset` calls `OnParamChange` for every
parameter. The first `OnIdle` fires 20 ms after the timer registers, so a plug-in with a slow `OnParamReset` (many
parameters, a Debug build) runs it concurrently with `OnIdle`. That is the overlap this task removes everywhere else,
and §9 now promises it doesn't happen.

The Log's reason for leaving `initialize` out ("initialize starts the fallback thread") doesn't hold. The gate is
recursive and the fallback thread only try-locks. Fix: build a `VST3HostThread::Call hostCall(mHostThread,
VST3HostThread::kKeepDeferred);` right after `mHostThread.Initialize()`, inside the `OS_LINUX` block. It must come
after `Initialize()`, because before it `IsUIThread()` is false. It then lives to the end of the `if` block and covers
both calls.

**R1-2 (minor): a host that calls `initialize` on another thread never gets the deferred notifications.**
Suppose a host calls `initialize` on a thread other than the one that later makes its UI calls. That breaks VST3's
threading rule, and no traced host does it. Every `Call` is then a no-op, including in `attached`. A deferred restart
or edit then waits forever, where before this branch it was at least sent off-thread. Accepting this is reasonable.
Either note it in the design note as a known risk, or make it visible: for example, a one-time trace or stderr line
when a guarded entry runs on a thread that is neither the recorded UI thread nor a fallback thread while something is
queued.

**R1-3 (minor): §9 doesn't say that parameter changes from `OnIdle` are merged into one gesture.**
This is visible to plug-in code. On the fallback thread, `beginEdit`/`endEdit` are dropped and only the last value
per parameter is sent, as one short gesture per flush. A plug-in that drives a parameter from `OnIdle` (MIDI-CC
learn, morphing) therefore has its intermediate values and gesture boundaries lost in an automation write while the
editor is closed. One clause in the §9 bullet would do: "…parameter changes, merged into one gesture with the last
value per parameter…".

**R1-4 (nit): the wording of the design note and the code comments.**
- Design note point 5 says "skips the round and retries 2 ms later". Only fds are retried after 2 ms. The ready
  timers' `mDueMs` was already advanced, so a timer tick that hits a held gate is dropped until its next interval.
  That behaviour is fine; the note should say so.
- Point 5 also says "the lock order is gate → `HostLoop::mMutex` on both threads". On the UI thread with a host-driven
  backend the order is the reverse: `Dispatch` holds `mMutex`, then `OnIdle` → `SetLatency` → `restartComponent` → the
  host calls `getParamNormalized` → gate. This is harmless, because a loop never has a fallback thread while a
  host-driven backend is set (`SetBackend` joins the old backend before it registers the new one). The statement
  needs that qualification.
- `HostLoop::GetHostCallGate()` says "Format code holds it…". Only the VST3 wrapper does. A CLAP host without
  `timer-support` still runs `OnIdle` concurrently with its main-thread calls (§9 already scopes the guarantee to
  VST3). Say "The VST3 wrapper holds it…".
- `IsFallbackThread()` says "the host's main thread is busy elsewhere", which is unclear. Suggested wording: "not the
  host's main thread: the host's [UI-thread] methods must not be called from here".

**R1-5 (nit): `DeferEdit` silently drops a tag ≥ `NParams()`.**
For example `kBypassParam` (65536): `DeferEdit` returns true without storing it. iPlug never edits such tags from
`OnIdle` today, and dropping one is better than calling the host off-thread. Fine as it is; a test or a word in the
`DeferEdit` doc would make it deliberate.

**R1-6 (nit, watch item): `getParamNormalized` is a flush point, so a host getter can trigger a synchronous
`restartComponent`.**
mac/win plug-ins rarely re-enter a host like this. It is verified harmless in REAPER (poll phase). Renoise and
Qtractor flushed from `setParamNormalized`. If a host ever misbehaves here, this is the first flush point to drop. No
change requested.

**R1-7 (nit): evidence coverage.**
- Deferred edits (`performEdit`) are measured only in the forced REAPER run. The Renoise and Qtractor runs exercised
  only `restartComponent` ("0 edit(s)").
- Carla is in the task's Direction list but was not measured. It isn't in the Acceptance list, and it ignores latency
  changes anyway.

Worth one line in the Log, so the coverage is stated rather than implied.

**R1-8 (nit): `smoke-r0/summary.txt` is easy to misread.**
`smoke.sh` prints each GUI run's `RESULT` line before its `== name` header. As a result "RESULT: FAIL (1)" sits under
"== controls p3b vst3", but it belongs to gneiss vst3. The per-run `RESULT.txt` files are right: `gneiss-vst3` FAIL(1)
is the known `g04b` synthetic-click limitation, and `controls-p3b-vst3` passes. Print the header first next time.

**R1-9 (nit): the task id in the shipped example.**
The comment `T-038:` in `IPlugEffect.cpp` points to a work-area file. The block above it says "Linux port test (P2)",
so the reason alone ("a host edit from OnIdle as well, as a MIDI CC mediator makes them") is enough.

### Acceptance status (as reviewed)
- [x] Design note with options, costs and the choice.
- [x] No `restartComponent`/`performEdit` off the UI thread in Renoise, Qtractor (VM, worker's evidence checked) and
  REAPER with the fallback forced (re-run). PDC follows in REAPER (poll) and Renoise; Qtractor reads the latency at
  transport start.
- [x] mac/win unchanged (unifdef diff above; the worker's xcodebuild).
- [ ] Independent review: round 1 approves with minors. R1-1 is a two-line fix and should go in; R1-2 and R1-3 need a
  note or a clause. Docs §8/§9: accurate apart from R1-3 (and R1-1 for the claim that `OnIdle` never overlaps a
  UI-thread call).
