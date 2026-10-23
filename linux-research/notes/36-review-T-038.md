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

## Round 2

**Verdict: approve with minors.** No blocker, no major. Every round-1 finding is fixed or answered. The R1-1 fix
cannot deadlock. The re-runs pass. The three new findings are nits and need no further round.

### What I read
- `AGENTS.md`, both `CLAUDE.md`s, the task file (design note, Log up to "(5) review round 1 … and fixes"), round 1
  above.
- The whole code diff `901602a14..42d7d04b2` again, with fresh eyes, and the round-1 fix diff `e3e99d498..HEAD`.
- Context: `IPlugLinuxHostLoopVST3.h` (`Initialize` → `Update` → `SetBackend`, `Terminate`), `HostLoop::SetBackend`,
  `Dispatch`, the fallback thread's `Run` and destructor, `IPlugAPIBase` (one `HostLoop` per instance, so one gate per
  instance), every `beginEdit`/`performEdit`/`endEdit`/`startGroupEdit`/`finishGroupEdit`/`restartComponent`
  call site in `IPlug/VST3` and `IGraphics.cpp`.

### Round-1 findings
- **R1-1, fixed.** The `Call` (`kKeepDeferred`) follows `mHostThread.Initialize()`, so `IsUIThread()` is already true.
  It lives to the end of the `if` block and so covers `OnHostIdentified` and `OnParamReset`.
  - Deadlock check: `mHostLoopSelector.Initialize` → `Update` → `SetBackend(fallback)` starts the thread while the UI
    thread holds the gate.
  - The new thread takes only its own backend mutex (never held across a join) and try-locks the gate. It reaches
    `Dispatch`, and with it `HostLoop::mMutex`, only after the try-lock succeeds. So while the UI thread holds the
    gate, the thread never waits for anything the UI thread holds. It polls the wake fd for 2 ms and tries again.
  - `SetBackend` joins an old backend outside `mMutex`. In `initialize` there is normally no old backend. If a host
    calls `initialize` twice without `terminate`, `Update` keeps the running fallback (no join). Joining a fallback
    inside a held gate is covered anyway by `TestBackendSwapInsideAHostCallDoesNotWaitForTheGate` (20 swaps with a
    ready fd).
  - `AcceptsTimers` calls the host's `registerTimer` under the gate; that is a host-driven path, with no fallback
    thread involved.
  - The comment ("Held through OnHostIdentified and OnParamReset: the fallback thread starts below") is a WHY.
- **R1-2, answered.** The `VST3HostThread::Initialize` doc and the Log state the risk and the reason. Adequate.
- **R1-3, fixed.** §9 now says that parameter changes are merged into one gesture with the last value.
- **R1-4, fixed.**
  - Design note point 5: dropped ticks vs retried fds; the lock order is qualified.
  - Code comments: the fallback's try-lock comment; `GetHostCallGate` ("A format wrapper (so far VST3)");
    `IsFallbackThread`.
- **R1-5, fixed.** `DeferEdit`'s doc names bypass and MIDI CC.
- **R1-6, R1-7: recorded in the Log** (as a watch item and as the evidence scope).
- **R1-8, fixed.** `smoke.sh` prints the name first; `smoke-r0/summary.txt` carries a note.
- **R1-9, fixed.** The task id is gone from `IPlugEffect.cpp`.

### Fresh look at the whole diff
- **Deadlock:** none.
  - The fallback thread never blocks on the gate.
  - The UI thread waits on the gate for at most one round of callbacks, and that round makes no host call.
  - Lock order with a fallback thread present: gate → `HostLoop::mMutex` → the backend mutex on both threads.
  - The reverse order (`mMutex` → gate) only arises in a host-driven `Dispatch`. No fallback thread exists then:
    `SetBackend` unregisters the old backend under `mMutex` and joins it before it registers the new one.
- **Race / lost or duplicate notification:**
  - The flags and the edit array are written only on the fallback thread under the gate, and read and cleared only on
    the UI thread under the gate.
  - Each entry is cleared before its host call, and only the outermost call sends, so a host that calls back cannot
    get a duplicate.
  - The edits go out before the restart, and a guarded `setParamNormalized`/`setState` sends what waited before it
    applies the host's own value. The host's value therefore wins, as it should.
  - `terminate` drops what waits after the thread is joined.
- **Re-entrancy:** a restart inside the flush makes the host call back into us; the unit test covers it, and so does
  Renoise's `setProcessing`/`setActive` sequence in the VM evidence. Processing-state calls never flush.
- **Lifetime:** `mHostThread` refers to the base's `componentHandler`, which is constructed earlier and destroyed later.
  `NParams()` is final when `mHostThread` is built. A `Call` points at `mHostThread` for one host call.
- **Realtime:** `process`, `setProcessing` and `getLatencySamples` are untouched. A guarded entry called off the UI
  thread costs one acquire load plus `pthread_equal`, and never waits or sends. Nothing allocates after construction.
- **mac/win:** the round-1 fixes are all inside `#ifdef OS_LINUX` or in comments and docs. Round 1's unifdef result
  and the worker's xcodebuild still hold.
- **Product names:** `check-product-names.sh`: clean (HEAD 42d7d04b2).
- **Merge:** `git merge-tree hvoya-linux linux/T-038` is clean. Since the base, hvoya-linux changed only
  `linux-development.md` among the files this branch touches (T-018).

### What I re-ran (Docker; my GUI container `t038r2` removed afterwards; builds in `linux-research/build/t038r2-*`)
- `Tests/LinuxHostLoop` as configured (ASan+UBSan), `build/t038r2-unit`: 3/3 "all host-loop tests passed".
- The same sources with g++ `-fsanitize=thread -O1`, run under `setarch -R`, `build/t038r2-tsan`: 3/3 passed,
  0 ThreadSanitizer warnings.
- Fresh Debug IPlugEffect VST3, `build/t038r2-dbg`. CMakeCache confirms Debug and the four flags
  (`IPLUG_TEST_VST3_FRAME_RUNLOOP_ONLY`, `_LATENCY_FROM_IDLE`, `_EDIT_FROM_IDLE`, `IPLUG_TRACE_HOST_LOOP`).
- `scripts/t038/reaper-deferred-test.sh` on that build, output in `build/t038r2-out/reaper-deferred/`: **PASS**.
  - idle: 6× `SetLatency` on the fallback thread and no host call; `pdc` stays 0.
  - poll: 12× `beginEdit`/`performEdit`/`endEdit`/`restartComponent(8)`, all on REAPER's main thread; `pdc` goes
    256 0 256 0 …
  - play: `getLatencySamples` on main.
  - open: what waited goes out on main.
  - The calls by phase contain no `OFF-MAIN HOST` line.
  - Trace: 36 "deferred" lines, all on the two fallback tids (128, then 453 after the editor closed), and 13 "sent"
    lines, all on the main tid 64.
- Not re-run: the VM (as instructed) and macOS (the round-1 fixes are Linux-only).

### Findings

**R2-1 (nit): `SendDeferred` scans all `NParams()` entries on every outermost `kSendDeferred` call, even when nothing
waits.**
This also happens in hosts with a loop, where nothing can ever be deferred. A host that sweeps `getParamNormalized`
over all parameters therefore pays O(N²) comparisons per sweep (e.g. 500 parameters: 250 000 per sweep, well under a
millisecond). It is not a correctness issue. One option: a pending flag or count, set in `DeferRestart`/`DeferEdit`
under the gate and checked first in `SendDeferred`. That makes the empty case O(1) and lets the counting loop go.

**R2-2 (nit): the file header of `IPlugLinuxVST3HostThread.h` overstates the guard.**
It says "each host call on the UI thread holds the loop's host-call gate". Only the listed entries hold it:
`createView`, `terminate`, `setProcessing`, the static getters and the view's sizing calls don't. §9 lists them
correctly ("state, parameters, activation, the editor attaching"). Suggested wording: "the host's UI-thread calls that
reach plug-in state hold …".

**R2-3 (nit): the §8 lead-in no longer fits two of its bullets.**
§8 opens with "Host behaviour seen while testing on Linux; iPlug2 does not work around it". The Renoise and Qtractor
1.5.3 VST3 bullets now describe what iPlug2 does about the missing loop (it holds the notification until the host's
next UI-thread call). Either drop "iPlug2 does not work around it" from the lead-in, or keep only the host behaviour in
those bullets and point to §9 for iPlug2's side.

### Acceptance status (round 2)
- [x] Design note with options, costs and the choice.
- [x] No `restartComponent`/`performEdit` off the UI thread in Renoise, Qtractor (VM, worker's evidence, checked in
  round 1) and REAPER with the fallback forced (re-run in rounds 1 and 2); latency honoured where the host supports it.
- [x] mac/win unchanged.
- [x] Independent review: round 2 approves with minors (R2-1…R2-3 are nits, optional). Docs §8/§9 accurate, apart
  from the R2-3 wording.
