# T-010 — Gneiss-side Linux needs (P9)

- **Status:** review-done (round 1: accept); see Log (3)
- **Owner:** coordinator (chat f91df715), lock `.linux-port-locks/T-010`
- **Branches/worktrees:**
  - Gneiss: local `linux-experiment` in `/Volumes/Storage/code/hvoya/Gneiss-linux`. Never push; never touch `/Volumes/Storage/code/hvoya/Gneiss`.
  - iPlug2: `linux/T-010` from `hvoya-linux`, in `/Volumes/Storage/code/IPlug2-T-010`, if the fix belongs in the framework (preferred when every plug-in would need it).
- **Depends on:** nothing (T-003, T-008 merged; Gneiss master 3cf2e2e synced).
- **Plan phase:** P9.
- **Conflicts:** Gneiss repo; `IPlug/Linux/*`, `IPlug/VST3`/`CLAP` Linux paths if the fix lands in iPlug2.

## Goal
Everything Gneiss needs on Linux beyond the build (T-003), the platform features (T-019) and the look (T-009), with the smallest possible change to Gneiss logic. Already done, don't redo: `openFolder()` (PresetManager `OS_LINUX` → `LinuxOpenWithDefaultApp`), platform menus (IGraphics stand-ins, T-003/T-008), dialogs (T-002).

## Read first
`AGENTS.md` §2 (minimal footprint), Gneiss `CLAUDE.md`, `ARCHITECTURE.md` and the `docs/` spokes it points to for the parts you touch, `CROSS_PLATFORM_FINDINGS.md` (what NOT to fix here), PLAN P2/P9, notes/09.

## Items
1. **Latency when there is no host loop.** `Gneiss::OnIdle` (`Gneiss.cpp` ~1287) calls `SetLatency` on an OS-factor change, "message thread only". On Linux `OnIdle` runs on a plug-in worker thread when the host provides no loop (`GetHostLoop().IsHostDriven() == false`; the logged fallback). P2 measured: REAPER ignores a VST3 latency change reported from the fallback thread (frame-only test build). Find the right fix:
   - preferred, framework-side (Linux-only code, no Gneiss change): when not host-driven, the wrapper defers the latency notification to the next call the host makes on its own thread (e.g. CLAP `on_main_thread`/`request_callback`, a VST3 host call on the UI/main thread), so every plug-in is covered;
   - else a Linux-only guard in Gneiss.
   Test with the fallback forced (the P2 frame-only/no-`IRunLoop` test build) in REAPER CLAP and VST3: PDC follows the OS factor; no call from the audio thread; no `restartComponent` from a non-main thread.
2. **Audit Gneiss for platform-specific behaviour** that has no Linux branch or a wrong one: `#if OS_MAC/OS_WIN` in Gneiss and `hvoya/` (none found in Gneiss sources on 2026-10-16 except via hvoya PresetManager), preset/user-data/settings paths (XDG on Linux: `$XDG_CONFIG_HOME`, `$XDG_DATA_HOME`, `~/Documents` equivalence), file-type filters, "reveal in file manager", URLs (manual, website), key modifiers (Cmd vs Ctrl in shortcuts/help text), version/OS strings in the about box or crash logs. Fix only Linux gaps, Linux-only.
3. **Licence/feature gate** (`feature_gate.hpp`, `HVOYA_IS_PAID`): confirm both variants build and behave as on mac/win (don't change the mechanism).
4. Anything Gneiss-specific T-003/T-008/T-019 runs found that is Linux-only and not yet fixed (check their Logs).

Explicitly NOT here (user's decisions pending, all platforms): C2, C3, C4, X1, L1, C5 in `CROSS_PLATFORM_FINDINGS.md`. X1 is now T-025 (user decided 2026-10-17 to fix it); item 1 above may be folded into it.

## Acceptance
- [x] Latency reaches the host correctly with and without a host loop (CLAP + VST3, REAPER); evidence. *With a host loop: P2 (notes/09, notes/10). Without one: CLAP yes (`evidence/t010/fallback-latency-clap`); VST3 no in REAPER — waived by the user's decision (Log 2026-10-18 (2)).*
- [x] Audit table in the Log: item, mac/win behaviour, Linux before/after, file:line.
- [x] Gneiss diff on `linux-experiment` minimal and Linux-only (`#if defined(OS_LINUX)` or Linux-only files); mac/win untouched. `steps/gneiss.txt` CLAP + VST3 PASS (VST3 except the known right-click step). *No Gneiss code change; only the MANUAL note. Paid CLAP + VST3 runs are the T-028 merge smoke on the same linux-experiment code (`evidence/merge-smoke/t028-gneiss-*`); the free CLAP run is new here.*
- [x] Independent review (AGENTS §4); evidence in `evidence/t010/`; Log complete.

## Log
(append: date — what was done — commits — next)

### 2026-10-18 — claimed; items 1–4 researched; item 1 needs the user's decision

Worktree `IPlug2-T-010` (`linux/T-010` from hvoya-linux 9f9afa0e9; SDK folders symlinked from `IPlug2-linux`, git-ignored). Gneiss `linux-experiment` f799cb3 (T-028 merged), not changed.

**Item 1 — latency without a host loop.**
- **CLAP is already right.** `IPlugCLAP::SetLatency` asks for the restart through clap-helpers `runOnMainThread`, which queues the call off the main thread and calls `clap_host.request_callback`; `latency.changed` itself waits for `activate()`. Measured with a new test switch `IPLUG_TEST_CLAP_NO_TIMER_SUPPORT` (`IPlugLinuxHostLoopCLAP.h`, the CLAP twin of `IPLUG_TEST_VST3_FRAME_RUNLOOP_ONLY`): IPlugEffect `IPLUG_TEST_LATENCY_FROM_IDLE`, editor closed, OnIdle on the fallback thread, REAPER `pdc` follows every change (256/0/256/0…). The test's "open" phase fails by design in this build (no host loop even with the editor open). `evidence/t010/fallback-latency-clap/`.
- **VST3: REAPER ignores `restartComponent(kLatencyChanged)` from the fallback thread** (P2 finding 4 reproduced: `pdc` stays 0 over 6 changes; `evidence/t010/fallback-latency-vst3/`). A framework fix can only defer the call to the next call the host makes on its own main thread (an `IEditController` call, state load, the editor attaching); VST3 has no way to ask for one.
- **Which real hosts reach the VST3 fallback** (their VST3 hosting source, fetched 2026-10-18):

| Host | Where it gives `Linux::IRunLoop` | Editor closed | Off-thread `restartComponent` |
|---|---|---|---|
| REAPER 7.82 | factory `setHostContext` (P2) | host loop | ignored (measured) |
| Ardour (master `vst3_plugin.cc`) | host context (`HostApplication`), also via its component handler | host loop | — |
| Qtractor (`qtractorVst3Plugin.cpp`) | host application and plug frame | host loop | — |
| JUCE hosts (master `juce_VST3PluginFormatImpl.h`) | host context (`VST3HostContextHeadless`) | host loop | accepted (asserts in Debug, then `ComponentRestarter` posts it to the message thread) |
| Carla (`CarlaPluginVST3.cpp`) | plug frame only | **fallback** | stored in `fRestartFlags`, never acted on: Carla ignores latency changes from VST3 on any thread |
| Bitwig, Renoise | closed source | unknown → T-013 (trace `IPLUG_TRACE_HOST_LOOP` backend) | unknown |
| SDK validator, clap-validator | refuses timers / none | fallback | — |

  So no known host both falls back and drops the call. Recommendation: no framework deferral now. T-013 traces Bitwig and Renoise; revisit only if one of them falls back. The race between `SetLatency` and the audio thread is T-025's.
- **Wider point (for the user):** on the fallback thread the whole `OnIdle` runs concurrently with the host's main-thread calls (state load, `setParamNormalized`, …). On mac/win `OnIdle` is always on the main thread. Gneiss's `OnIdle` does a lot (MIDI CC mediator pushes to the host, morph session, note controller, layout). In the hosts above this only happens in Carla VST3 with the editor closed and in validators.

**Item 2 — audit (Gneiss `linux-experiment` f799cb3 and `hvoya/`):**

| Item | mac/win | Linux | Where |
|---|---|---|---|
| `#if OS_*` in Gneiss | none (only `TARGET_OS_IOS` in the AUv3 header) | — | — |
| `#if OS_*` in `hvoya/` (outside `libs/`) | `PresetManager::openFolder` open/ShellExecute | `LinuxOpenWithDefaultApp` (already) | `hvoya/utils/preset_manager.hpp:473` |
| Settings (`preferences.ini`, `default_layout.hvoya`) | `INIPath`: `~/Library/Application Support/Gneiss`, AppData | `$XDG_CONFIG_HOME/Gneiss` (`~/.config/Gneiss`) | `Gneiss.cpp:59`, `IPlugLinuxPaths.cpp:157` |
| User presets | `AppSupportPath/Gneiss/Presets` | `$XDG_DATA_HOME/Gneiss/Presets` (`~/.local/share`) | `Gneiss.cpp:196`, `IPlugLinuxPaths.cpp:126` |
| File dialogs and filters (`fxp`, `hvoya`) | native | portal / zenity / kdialog with the same filters (T-002) | `preset_strip_control.hpp:421`, `user_tab_panel.cpp:841`, `morph_controller.hpp:2120`, `cc_file_orchestrator.hpp:152` |
| About-box link | `OpenURL` | `xdg-open` (`IGraphicsLinux::OpenURL`) | `GneissUI.cpp:888` |
| Text naming Finder/Explorer/Cmd/Option | none in strings (one code comment says "Finder") | — | `GneissUI.cpp:433` (comment) |
| Modifiers | Alt (Option) + drag/wheel/click on cutoff knobs snaps to a note; Shift for marquee/column; Shift+A | same code; **Alt+drag and Alt+click are taken by the window manager on Xfce, MATE, Cinnamon and KDE Plasma 5 (move window)**; GNOME and Plasma 6 use Super | `ui/freq_knob_control.hpp:90` → finding T10-1 |
| Version/OS strings | build info in the log, edition in the about box | same, nothing OS-specific | `Gneiss.cpp:47` |

No Linux gap needing a Gneiss change was found.

**Item 3 — licence gate:** compile-time only (`feature_gate.hpp`, `HVOYA_IS_PAID`), nothing platform-specific. Both editions × both arches built and smoke-tested in T-021 (`strings` edition check). Here: a free CLAP build (`-DHVOYA_IS_PAID=0`, `edition: free`) passes `gneiss.txt` (`evidence/t010/free-edition-clap/`). The screenshots differ as the gate says: no clipboard copy/paste icons (`ClipboardWorkspace`). The step's ">" click then lands on the preset name, because the strip is wider; that is a harness assumption, not a bug.

**Item 4 — leftovers from T-003/T-008/T-019 Logs:** everything T-003 handed to T-010 is done elsewhere (input-buffer write → T-028; morph after preset load → C2, Gneiss 3cf2e2e); the deferred SetLatency is item 1. T19-1 (USER-tab drop target) is not Linux-specific.

**Finding T10-1 (Linux-only, Gneiss UX):** Alt + drag/click on the cutoff knobs (snap to note) moves the plug-in window instead on desktops whose window manager uses Alt as the move modifier (Xfce, MATE, Cinnamon, Plasma 5 by default). Options: document it (users can change the WM modifier), or a second modifier on Linux only. A decision for the user.

Commits: `IPLUG_TEST_CLAP_NO_TIMER_SUPPORT` and this Log. Next: the user's decision on item 1 and T10-1, then the independent review.

### 2026-10-18 (2) — the user's decisions

- **Item 1, VST3 latency from the fallback thread:** no change ("ничего не меняем"). T-013 traces which loop Bitwig and Renoise give; revisit only if one of them falls back.
- **T10-1, Alt on the cutoff knobs:** a short note in the manual, no code change. Gneiss `linux-experiment` `MANUAL.md` (the paragraph on Alt‑drag / Alt‑wheel / Alt‑click snapping): on Linux, desktops whose window manager moves windows with Alt‑drag (Xfce, MATE, Cinnamon, KDE Plasma 5) need the move modifier switched to Super. Left uncommitted for the author (Gneiss `CLAUDE.md`: no commit without an explicit command).

### Review round 1 (independent reviewer)

Read: AGENTS.md, this file, `git diff hvoya-linux...linux/T-010`, all of `evidence/t010/`, `IPlugCLAP.cpp` `SetLatency`/`activate`, clap-helpers `runOnMainThread`/`clapOnMainThread`, the Gneiss `MANUAL.md` diff, the audit's file:line references.

**Re-run (outputs in `linux-research/build/t010r-*`, git-ignored; containers `t010r-*` removed):**
- IPlugEffect CLAP built from this worktree with only `-DIPLUG_TEST_CLAP_NO_TIMER_SUPPORT -DIPLUG_TEST_LATENCY_FROM_IDLE -DIPLUG_TRACE_HOST_LOOP` (the VST3 switch left out, so the CLAP switch is tested on its own). `host-loop-test.sh` with `EXPECT_CLOSED=worker`: same result as the evidence. Every phase runs on the fallback thread, REAPER `pdc` follows (0/256/0/256/256/0/256/0), churn is flat (threads 16 → 16, fds 9 → 9), and the only FAIL is the expected "open" one.
- Extra gdb check of the mechanism (`build/t010r-mech.sh`, 12 s, one instance): `SetLatency` and `runOnMainThread` run on the fallback thread; `clapOnMainThread`, `deactivate`, `activate` and `latencyGet` all run on REAPER's main LWP. So the claim "CLAP is already right" is shown by the mechanism, not only by `pdc`.
- Carla VST3 claim checked against `CarlaPluginVST3.cpp` (falkTX/Carla main). `IRunLoop` is offered only by the plug frame, and `v3RestartComponent` only ORs `fRestartFlags`. `runIdleCallbacksAsNeeded` reads the flags and writes them back unchanged, so they are never acted on. Matches the table. The other host rows were not re-checked.

**Checked and correct:** the only code change is 3 lines in `IPlugLinuxHostLoopCLAP.h`, which is included only under `OS_LINUX`. mac/win are not touched, no product names. The WHY comment matches the VST3 twin's. Evidence matches the Log: CLAP `pdc` alternates; VST3 `pdc` stays 0 over 6 off-thread `SetLatency` calls, with RESULT PASS under `EXPECT_CLOSED=worker`. The free edition run is real: the `.clap` contains `edition: free`, `CMakeCache` has `HVOYA_IS_PAID=0`, and `g04c` has no copy/paste icons where the paid T-003 merge-smoke shot has them. `g05` opens the preset dropdown, as the Log says. Spot-checked audit references, all accurate: `preset_manager.hpp:473`, `Gneiss.cpp:47/59/196`, `GneissUI.cpp:433/888`, `IPlugLinuxPaths.cpp:126/157`, `preset_strip_control.hpp:421`, `user_tab_panel.cpp:841`, `morph_controller.hpp:2120`, `cc_file_orchestrator.hpp:152`, `freq_knob_control.hpp:90`. A grep for `OS_*`/`__APPLE__`/`_WIN32`/`TARGET_OS` in Gneiss and `hvoya/` finds nothing else that matters. The Gneiss tree has only the `MANUAL.md` change, uncommitted, as required.

**Findings:**
- **R1-1 minor — acceptance item 1 is not met as written; record the waiver.** "Latency reaches the host … without a host loop (CLAP + VST3)" fails for VST3 in REAPER, and the user accepted that ("no change"). Before handover, tick it as "CLAP yes; VST3 waived by the user's decision 2026-10-18, see the decisions section". Point the "with a host loop" half to the P2 evidence (notes/09 row 1, notes/10), since T-010 did not re-measure it. Do the same for the `steps/gneiss.txt` CLAP + VST3 item: no Gneiss code changed, so the paid runs are T-028's, and only the free CLAP run is new here.
- **R1-2 minor — "CLAP is already right" leaves out formal data races on the fallback path.** `IPlugCLAP::SetLatency` writes `mLatencyUpdate` (a plain `bool`) and reads clap-helpers' `_isActive` on the fallback thread, while `activate()` reads `mLatencyUpdate` on the main thread. When the plug-in is active, the mutex in `runOnMainThread` orders the two sides. When it is inactive, the only synchronisation is whatever comes before the next `activate`. `latencyGet` on the main thread also reads `mLatency`, which the fallback thread writes. This is harmless in practice and close to T-025's subject, but T-025 names only the audio-thread race. Add one line to the Log, or to T-025, so that whoever fixes `SetLatency` makes these atomic as well.
- **R1-3 nit — the trace line is misleading when the switch is on.** With `IPLUG_TEST_CLAP_NO_TIMER_SUPPORT` the trace prints `CLAP host 'REAPER' timer-support=0`, and a reader of `trace.txt` may think REAPER lacks timer-support. Optional fix: trace "(forced off)" under the switch. This is test-only code either way.
- **R1-4 nit — the harness cannot express the CLAP switch.** `host-loop-test.sh` documents `EXPECT_CLOSED=worker` only for the VST3 frame-only switch. With the CLAP switch "open" is also on the worker, so RESULT is FAIL by design. An `EXPECT_OPEN=worker` option, or one line in the script header, would let this evidence read PASS. This is `linux-research` tooling only.
- **R1-5 nit — the free-edition screenshots are not in the evidence.** The Log's screenshot claims rest on PNGs in `IPlug2-linux/linux-research/build/t010-free-run/`, which is git-ignored and in another worktree. Copy `g04c-menu-dismissed.png` and `g05-next-preset.png` into `evidence/t010/free-edition-clap/`, or drop the claim.
- **R1-6 nit — the manual sentence.** It is accurate and short. The window manager also takes plain Alt‑click, not only Alt‑drag: the click never reaches the knob. KDE calls the modifier "Meta", not "Super". Possible wording: "…use Alt‑drag to move windows, so Alt‑drag or Alt‑click on a knob goes to the window manager instead. Set its window‑move modifier to Super (Meta in KDE)." This is the author's call. The audit row for `hvoya/` could also mention the two `__APPLE__` toolchain shims (`format_compat.hpp:18`, `filesystem_compat.hpp:19`); they already take the std path on Linux.

**Verdict: accept.** Nothing blocks. R1-1 and R1-2 are Log and acceptance bookkeeping that the owner should do before handover. R1-3 to R1-6 are optional.

### 2026-10-18 (3) — review round 1 fixes

- **R1-1:** acceptance boxes ticked with what each rests on (P2 for "with a host loop", the user's waiver for VST3 without one, T-028's paid Gneiss runs).
- **R1-2:** T-025 now also names the formal races on the fallback path (`mLatencyUpdate`, `mLatency` read by `latencyGet` on the main thread), so the `SetLatency` fix makes them atomic too.
- **R1-3:** the CLAP switch traces "timer-support forced off" before the backend choice.
- **R1-5:** free-edition screenshots copied to `evidence/t010/free-edition-clap/` (PNGs are git-ignored in `evidence/`, as everywhere in this repo; they stay local).
- **R1-6:** the manual sentence now covers Alt-click too and names KDE's "Meta".
- **R1-4** (no `host-loop-test.sh` option for the CLAP switch) not done: the switch is a one-off diagnostic, and the Log explains the by-design "open" FAIL.
