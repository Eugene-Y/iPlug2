# T-010 — Gneiss-side Linux needs (P9)

- **Status:** ready
- **Owner:** — (planned: coordinator)
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

Explicitly NOT here (user's decisions pending, all platforms): C2, C3, C4, X1, L1, C5 in `CROSS_PLATFORM_FINDINGS.md`.

## Acceptance
- [ ] Latency reaches the host correctly with and without a host loop (CLAP + VST3, REAPER); evidence.
- [ ] Audit table in the Log: item, mac/win behaviour, Linux before/after, file:line.
- [ ] Gneiss diff on `linux-experiment` minimal and Linux-only (`#if defined(OS_LINUX)` or Linux-only files); mac/win untouched. `steps/gneiss.txt` CLAP + VST3 PASS (VST3 except the known right-click step).
- [ ] Independent review (AGENTS §4); evidence in `evidence/t010/`; Log complete.

## Log
(append: date — what was done — commits — next)
