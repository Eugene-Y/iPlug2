# Review of P2: host-loop timer layer (2026-10-11)

Range reviewed: `35c03c3fb..5622b6337` (notes/09). Fix round: `b935bbdfb..453074d71`. Reviewer evidence:
`evidence/p2-review/` (`round1-*` = after the fixes; `reviewer-probe.cpp` = the probes below).
[R] = re-run in Docker (REAPER 7.82, container `p2rev`), [U] = unit test or probe (ASan+UBSan and TSan),
[C] = code reading.

## Re-check of the implementer's results

| Check | Before fixes | After fixes |
|---|---|---|
| `host-loop-test.sh`: IPlugControls and IPlugEffect × CLAP and VST3 (trace + latency build) | PASS ×4 [R] | PASS ×4 [R] |
| What PASS covers | every `OnTimer`/`OnIdle` hit on REAPER's main LWP in all phases; 2 instances → 2 objects; 50× churn leaves threads, fds and live registrations (= 2) unchanged; `pdc` toggles 0/256 from the main tid with the editor closed | same |
| Frame-only build (`IPLUG_TEST_VST3_FRAME_RUNLOOP_ONLY`), default expectation (main thread) | **FAIL (7), as designed**: closed/reclose/two/after-churn phases off main, REAPER ignores the latency, `OFF-MAIN` in the trace. The open phase is on main (frame IRunLoop) [R] | — |
| Same build, `EXPECT_CLOSED=worker` | PASS [R] | PASS (exercises the new IRunLoop probe) [R] |
| Unit test | passes under ASan+UBSan and under TSan; no race reported [U] | passes under ASan+UBSan and TSan, with 6 new cases [U] |
| clap-validator / VST3 validator, IPlugEffect | — | unchanged from P0: 35 passed / 3 failed (state-reproducibility) and 536/537. Each `clap_plugin.destroy` stops the loop before the instance is freed [R] |
| APP `--no-io` idle | 0.7 % CPU, ~120 main-thread wakeups/s, the same order as the old `Sleep(10)` loop [R] | — |
| macOS IPlugEffect CLAP + VST3 Debug (DerivedData and install in the scratchpad) | BUILD SUCCEEDED | BUILD SUCCEEDED; `IPlugCLAP.cpp` recompiled. The project's SYMROOT still writes intermediates to `Examples/IPlugEffect/build-mac` (git-ignored) |
| Shared files in the range | every hunk is inside `#ifdef OS_LINUX` / `#elif defined OS_LINUX` or a test define; mac/win code is unchanged in effect [C] | same; the destroy hook is one line under `OS_LINUX` |

## Findings

| # | Sev. | Finding | Status |
|---|---|---|---|
| 1 | **Medium** | **CLAP + fallback: `OnIdle` on a half-destroyed plug-in.** `~IPlugCLAP` stopped the loop only after the most-derived destructor (`~IPlugEffect`, `~Gneiss`) had run. With the fallback thread (clap-validator, or any host without timer-support), `OnTimer → OnIdle` could run while the plug-in's members were being destroyed. The probe (`derived`) reproduces it: ASan reports heap-use-after-free in `Derived::OnIdle` from the fallback thread. VST3 is not affected, because `terminate()` comes before the last `release()`. This also answers "unregister in `destroy()`?": yes. The host is valid in both places, but only `destroy` comes early enough. | Fixed 38cd38313: `CLAPHostLoopDestroyHook` wraps `clap_plugin.destroy` (via `const_cast` on the helper's own non-const member) to call `SetBackend(nullptr)` and then clap-helpers' destroy. Verified with the clap-validator and REAPER traces. |
| 2 | Low | `IsHostDriven()` took `mMutex`, which is held across fallback callbacks. P9 will query it from main-thread paths, so the query would block behind `OnIdle`, and would deadlock if `OnIdle` waits on the main thread. The comment on `mMutex` did not state the load-bearing invariant: it spans the callback so that removal and destruction wait out a running callback. | Fixed 7e9a2c653: atomic flag, WHY comment, unit test. |
| 3 | Low | A timer refused by a backend after the backend was installed was lost silently (trace only); probe `late-refuse` shows this. The VST3 refusal rule also only worked if a timer already existed at `initialize()`. | Fixed 51189a81d: one-time stderr warning. The VST3 selector probes each IRunLoop with a detached handler; unit test added. The refused timer still never fires. This is acceptable: no known host accepts some timers and refuses others. |
| 4 | Low | `Timer::Create`: the fallback started before `mId` was stored, so a `Stop()` inside the first callback read 0. TSan flagged the same pattern in a probe. | Fixed 5e436a788. |
| 5 | Nit | The "same values as `CLAP_POSIX_FD_*`" comment was unchecked; a failing `poll()` stopped the fallback silently; the callback rules were undocumented (re-entrancy, `RemoveFd` on `kFdError`, since a hung-up fd re-fires forever in GLib, poll and CLAP). | Fixed d02b36cd0. |
| 6 | Info | `Timer::Create` timers (OSC, ReaperExt) run on a private fallback thread, so ReaperExt calls the REAPER API off REAPER's main thread. They also have the destructor race from #1 in their owners. Out of v1 scope. | Documented in notes/09 (971acf2bd). A future ReaperExt fix is `plugin_register("timer")`. |
| 7 | Info | Debug CLAP builds use clap-helpers `Terminate`/`Maximal`, so any `[main-thread]` host call from the fallback thread aborts (e.g. `SetLatency` in `OnIdle`). | Documented (notes/09 finding 6); one more reason for P9's deferral. |
| 8 | Info, P3 | With a factory or initialize IRunLoop, the frame's IRunLoop is never used. In REAPER the two are different objects, both serviced on the main thread. Mixing them is therefore safe in REAPER: only one is ever active per instance, and handlers are unregistered on the loop they were registered with, while that loop is still held by an `IPtr`. P3 will put the editor's X fd on the factory loop. In each host, check that `registerEventHandler` on that loop delivers; if a host's context loop ever does not, prefer the frame loop for the editor fd. Unlike Stillwell, the VST3 handlers do not reject calls arriving on a foreign thread; the trace's `OFF-MAIN` line is the only check. | Open, for P3/P10. |

Checked without findings [C/U]:
- The recursive `WDL_Mutex` allows callbacks to add and remove entries.
- Self-removal and removal of another entry, including nested dispatch, use the deferred delete.
- Backend migration releases the lock before the old backend's destructor joins its thread, so a dispatch blocked on `mMutex` cannot deadlock. The new backend registers only after the join, so stale fallback handles (entry ids, never reused) find nothing.
- Lock order is consistent: loop → backend on the main thread; the fallback thread never holds both.
- `mRunning` is set before `pthread_create`; the eventfd is `CLOEXEC`.
- VST3 handlers: FUnknown identity is consistent; refcounts are balanced (one owned reference, an extra reference held across dispatch, `Detach` before unregister); late host calls are no-ops.
- The factory context is retained once per module, with one `IPtr` per instance, and released in `DeinitModule`.
- `terminate`, then `initialize` again, works.
- CLAP: the backend is installed in `init()` (the host proxy is initialised first), and `on_timer`/`on_fd` are dispatched by timer id / fd.
- A host without timer-support goes to the fallback.
- APP: GDK events dispatched inside the blocking iteration still reach SWELL's handler (`gdk_event_handler_set`).
- Code style: 2-space indentation, no tabs or trailing blanks, `m`/`p` prefixes, Linux code in new files. Comments are WHY-only after the round, with no stale ones left (the `~IPlugCLAP` mention was removed from the note, the table and PLAN).

## Unit test additions (reviewer, b935bbdfb)

- removing another entry from a callback;
- a nested callback removing the outer entry;
- fallback self-removal and re-add;
- destroying the loop while a fallback callback is in flight.

The implementer added two more: a refusing IRunLoop with no timers yet, and `IsHostDriven` returning during a long callback.

## Verdict

**P2 accepted.** One review round. The single real defect (#1, the CLAP fallback destruction race) is fixed and
verified; the low items are fixed too. The threading contract holds in REAPER for CLAP, VST3 and APP. The fallback
behaves as documented and the harness detects it. mac/win are unaffected. Carried forward:
- #8 (fd delivery on the chosen VST3 loop, P3);
- the Gneiss `SetLatency` deferral (P9);
- probing other hosts (P10).
