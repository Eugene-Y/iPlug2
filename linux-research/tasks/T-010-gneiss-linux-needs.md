# T-010 — Gneiss-side Linux needs (P9)

- **Status:** in progress — decisions taken; independent review next
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
- [ ] Latency reaches the host correctly with and without a host loop (CLAP + VST3, REAPER); evidence.
- [ ] Audit table in the Log: item, mac/win behaviour, Linux before/after, file:line.
- [ ] Gneiss diff on `linux-experiment` minimal and Linux-only (`#if defined(OS_LINUX)` or Linux-only files); mac/win untouched. `steps/gneiss.txt` CLAP + VST3 PASS (VST3 except the known right-click step).
- [ ] Independent review (AGENTS §4); evidence in `evidence/t010/`; Log complete.

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
