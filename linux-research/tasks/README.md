# Task board

Read `../AGENTS.md` first. **Every port task (Gneiss, Sand, IPlugEffect, IPlugControls, templates) follows AGENTS §2 "Minimal footprint":**
- add a Linux build without changing mac/win/iOS behaviour or their projects/flags;
- touch business logic only when the Linux build cannot work otherwise;
- report cross-platform bugs, don't fix them in the port branch. Statuses: `ready` · `claimed` · `in-progress` · `review-done` (awaiting merge) · `merged` · `blocked` (dependency not met) · `todo` (no task file yet).

The coordinator keeps this table current. A worker updates only its own task file, in its own branch.

| ID | Task | Status | Owner | Depends on | Conflicts with (same files) |
|---|---|---|---|---|---|
| [T-001](T-001-vm-p3b.md) | VM verification of P3a/P3b on GNOME | **merged** (on hvoya-linux, 75e906365) | coordinator | — | — |
| [T-002](T-002-p4-dialogs.md) | P4: file/message/colour dialogs without GTK | **merged** 0846167f8 | coordinator | — | — |
| [T-003](T-003-gneiss-linux-build.md) | P7: Gneiss Linux CMake build (CLAP/VST3/APP) | **merged** e54a43e76 (Gneiss side: local `linux-experiment`, 52cb5d6 + 7e3a1c9) | 2nd instance | — | — |
| [T-004](T-004-app-no-audio.md) | P8: standalone APP robustness (no audio device, devices, settings) | **merged** (see merge log; shared-code fixes approved by the user, CROSS_PLATFORM_FINDINGS "T-004") | 2nd instance | — | — |
| [T-005](T-005-ide-research.md) | P11a: IDE research + presets/IDE configs for examples | **merged** 17675831f (m2: kept as is, documented) | 2nd instance | — | T-011 later |
| [T-006](T-006-clap-state-repro.md) | clap-validator `state-reproducibility` failures (Linux and mac) | **ready — good for a 2nd instance** (a shared-code fix waits for the user's decision before merging) | — | — | — |
| [T-007](T-007-hidpi.md) | P5: HiDPI (CLAP set_scale, VST3 content scale, Xft.dpi) | **merged** 4d413d040 | coordinator | — | T-008 (IGraphicsLinux*): not in parallel |
| [T-008](T-008-p3-polish.md) | P3 polish from the VM run (notes/14), listed below the table | **merged** fac9cc84d (VM re-check of the final focus build still open, see STATUS) | coordinator | — | — |
| [T-009](T-009-visual-parity.md) | P6: visual parity of Gneiss and Sand vs mac/win | **ready** (after T-019) | — | — | — |
| [T-010](T-010-gneiss-linux-needs.md) | P9: Gneiss-side Linux needs (latency without a host loop, platform audit; menus and `openFolder` already done) | **ready** | — | — | Gneiss repo |
| [T-011](T-011-duplicate-templates.md) | P11b: painless duplication (out-of-tree paths in `duplicate.py`) + Gneiss IDE setup | **ready** (user OK'd fixing the shared `duplicate.py`) | — | T-005 | `Examples/duplicate.py`, `gitignore_template`; Gneiss repo |
| [T-012](T-012-sand-linux-build.md) | P11c: Sand Linux build (as T-003, for Sand); reuse the T-005/T-011 Linux template (presets, `-linux.code-workspace`, `.clangd`). Known: `SandUI.cpp` `getOsString()` has no Linux branch (falls off the end: UB) | **merged** (see merge log; Sand side: local `linux-experiment` 4dcd5cd..cec9c1d) | 2nd instance | — | Sand repo |
| [T-013](T-013-host-matrix.md) | P10: host matrix (Carla, Ardour, Renoise on the VM; Bitwig/x86_64 on a PC) | **ready** (best after T-019/T-010) | — | T-002, T-007 (merged) | uses the VM |
| [T-022](T-022-app-jack-midi.md) | APP: JACK MIDI when the audio driver is JACK (split out of T-004) | todo (low priority, after the Gneiss/Sand work; user 2026-10-17) | — | T-004 | `IPlug/APP/*` |
| T-014 | P12: LV2 | blocked | — | everything else | — |
| [T-016](T-016-parity-audit.md) | Feature-parity audit: complete notes/22 (every IGraphics/IPlug platform API, Extras, formats incl. REAPER extension) and file tasks for each gap | **ready** | — | — | — |
| [T-017](T-017-webview.md) | WebView on Linux: `IWebViewEditorDelegate`, `IWebViewControl`, platform child views. **Design decided: out-of-process WebKitGTK helper embedded via XEmbed** (notes/22) | **ready** | — | — | `IPlug/Extras/WebView`, IGraphicsLinux* (platform views) |
| [T-018](T-018-skia.md) | Skia backend on Linux (build/fetch Skia for x86_64 + arm64, GL context) | **ready** | — | — | IGraphicsLinux*, `Scripts/` Skia fetch |
| [T-019](T-019-tooltips-file-drop.md) | Platform features Gneiss/Sand use: tooltips (`SetTooltip`/`EnableTooltips`, now a no-op), files dropped onto the editor (XDND in; Gneiss USER tab `OnDrop`) | **ready** | — | — | IGraphicsLinux* (not in parallel with T-009/T-017/T-018/T-020) |
| [T-020](T-020-platform-gaps.md) | Remaining platform gaps: GL3 context (`glXCreateContextAttribsARB` core 3.2), drag files out, file-path clipboard, dark/light appearance (portal), XInput2 multi-touch | **ready** after T-019 | — | T-019 | IGraphicsLinux* |
| [T-021](T-021-release-packaging.md) | Release packaging of Gneiss and Sand: Release builds in the 22.04 container (glibc floor), CLAP + VST3 (+ APP) with resources, tarball + install notes, symbols split | **ready** (Sand: use `iplug2-linux:22.04-gcc13`, T-012) | — | T-012 (for Sand) | `scripts/`, plug-in repos' local branches |
| T-015 | P13: repo `README.md` — what works on Linux and how (last of all) | blocked | — | everything else, incl. T-014, T-016..T-021 | `README.md` |

## Priority (user, 2026-10-16): Gneiss and Sand as fully working Linux plug-ins first

Gneiss and Sand use, beyond what already works (dialogs, clipboard, menus, text entry, `openFolder`): tooltips,
files dropped onto the editor (Gneiss USER tab), deferred `SetLatency`. Order:

| # | Task | Who | Why this slot |
|---|---|---|---|
| 1 | VM re-check of T-008's final focus build | coordinator | short; closes T-008 |
| 2 | ~~T-012 Sand build~~ merged | 2nd instance | — |
| 3 | T-019 tooltips + XDND drop | coordinator | IGraphicsLinux; used by both plug-ins |
| 4 | T-010 Gneiss-side Linux needs | coordinator | Gneiss `linux-experiment` |
| 5 | T-006 CLAP `state-reproducibility` | 2nd instance | CLAP wrapper; shared-code fix goes to the user before merging |
| 6 | T-009 visual parity, Gneiss and Sand | coordinator | IGraphicsLinux, after T-019 |
| 7 | T-021 release packaging, Gneiss and Sand | 2nd instance | ready now (T-012 merged) |
| 8 | T-013 host matrix with Gneiss and Sand | coordinator | the VM |

Then: developer experience T-011 (2nd), ~~standalone T-004~~ (merged), T-022 JACK MIDI (low); full parity T-016 audit (2nd, read-only, can run
any time), T-018 Skia (2nd), T-017 WebView (2nd: helper and IPC are new files; the IGraphicsLinux platform-view
hook is coordinated), T-020 (coordinator); last T-014 LV2, T-015 README.

**T-008 items:**
- APP focus-on-click race (P3b-V1);
- menus clamped to the editor, not the screen (P3b-V2);
- variable-font bold (P3b-V4);
- flaky APP `expect-same` rect (P3b-V5);
- editor draws while the host is minimized (P3a-V2);
- timestamped `XSetInputFocus` (notes/15, plus the notes/14 probe);
- fix notes/13 finding 5 (P3b-V3).

## Merge log (coordinator)

| Date | Task | Merge commit | Smoke tests |
|---|---|---|---|
| 2026-10-17 | T-012 | (this merge) | Docker 24.04 build Sand CLAP/VST3/APP, Gneiss CLAP, IPlugControls CLAP against the merged tree PASS; REAPER `steps/sand.txt` CLAP + VST3 PASS, `sand-app.txt` PASS (ALIVE), Gneiss `steps/gneiss.txt` CLAP PASS and IPlugControls P3b CLAP PASS (T-012 changed the shared `xact.sh`) (`evidence/merge-smoke/t012-*`). Findings F1–F8 → CROSS_PLATFORM_FINDINGS |
| 2026-10-17 | T-004 | 5aab26db9 | Pulse cookies stripped from the branch history first (T-004 Log). Docker: build IPlugEffect/IPlugInstrument/IPlugControls APP + IPlugControls CLAP/VST3 PASS; `app-audio-test` IPlugEffect + IPlugInstrument PASS (after a harness fix: the shortcut click sat outside IPlugEffect's 450 px window); `app-switch-test` IPlugInstrument JACK↔PulseAudio PASS; IPlugControls APP P3b and CLAP P3b scripts PASS (`evidence/merge-smoke/t004-*`). mac: CMake IPlugInstrument APP links (T-004 evidence) |
| 2026-10-16 | T-005 | 17675831f | Docker: `cmake --list-presets` + `cmake --preset linux-debug` + `cmake --build --preset linux-debug` in `Examples/IPlugEffect` PASS (CLAP, VST3, APP built); root `cmake --list-presets` on Linux lists upstream + new presets. mac (cmake 3.29): root `--list-presets` unchanged (macOS/iOS/visionOS/web), examples list nothing (m2) |
| 2026-10-14 | T-001 | (committed directly on hvoya-linux, last 75e906365) | — (evidence/notes only) |
| 2026-10-16 | T-008 | fac9cc84d | Docker build IPlugControls CLAP+VST3, Gneiss CLAP PASS; REAPER: IPlugControls full P3b CLAP PASS, VST3 PASS, Gneiss `steps/gneiss.txt` CLAP PASS (`evidence/merge-smoke/t008-*`) |
| 2026-10-16 | sync hvoya → hvoya-linux; Gneiss master → linux-experiment | e393b6398; Gneiss 4219350 | User's morph/preset fix (hvoya 0b2d8dbef, Gneiss 3cf2e2e). Docker build Gneiss CLAP+VST3 PASS; REAPER `steps/gneiss.txt` CLAP PASS, VST3 PASS except the known right-click step (`evidence/merge-smoke/sync-1016-*`) |
| 2026-10-15 | T-007 | 4d413d040 (conflict in IGraphicsLinux.cpp: T-003 and T-007 both added "tell the host the opened size"; kept T-007's, which also skips a host-resized window; kept T-003's stand-in menu font) | Docker build Gneiss CLAP+VST3, IPlugControls CLAP+VST3, IPlugEffect VST3 PASS; REAPER: Gneiss CLAP PASS, IPlugControls VST3 P3b PASS; Gneiss VST3 fails only the right-click step, which also fails before T-007 (REAPER's VST3 host menu does not open on a same-ms synthetic click, notes/14) |
| 2026-10-15 | T-003 | e54a43e76 | Docker build Gneiss CLAP+VST3 and IPlugControls CLAP+VST3 PASS; REAPER: IPlugControls P3b script PASS, Gneiss `steps/gneiss.txt` PASS, all Draw/X on tid 1, Gneiss UI renders (`evidence/merge-smoke/t003-*`). Cross-platform bugs found → `../CROSS_PLATFORM_FINDINGS.md` |
| 2026-10-14 | T-002 | 0846167f8 | Docker build IPlugControls CLAP+VST3 PASS; REAPER full P3b script on CLAP: PASS, all Draw/X events on tid 1 (`evidence/merge-smoke/t002-clap/`) |
