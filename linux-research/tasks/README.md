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
| [T-004](T-004-app-no-audio.md) | P8: standalone APP robustness (no audio device, devices, settings) | **ready — good for a 2nd instance** | — | — | — |
| [T-005](T-005-ide-research.md) | P11a: IDE research + presets/IDE configs for examples | **ready — good for a 2nd instance** | — | — | T-011 later |
| [T-006](T-006-clap-state-repro.md) | clap-validator `state-reproducibility` failures (Linux and mac) | **ready — good for a 2nd instance** | — | — | — |
| [T-007](T-007-hidpi.md) | P5: HiDPI (CLAP set_scale, VST3 content scale, Xft.dpi) | **merged** 4d413d040 | coordinator | — | T-008 (IGraphicsLinux*): not in parallel |
| [T-008](T-008-p3-polish.md) | P3 polish from the VM run (notes/14), listed below the table | ready | — | — | T-007 (IGraphicsLinux*): not in parallel |
| T-009 | P6: Gneiss visual parity vs macOS | todo (unblocked; needs task file) | — | — | — |
| T-010 | P9: Gneiss-side changes (deferred `SetLatency`, menus, `openFolder`) | todo (unblocked; needs task file; see CROSS_PLATFORM_FINDINGS for what NOT to mix in) | — | — | Gneiss repo |
| T-011 | P11b: `duplicate.py` + templates produce Linux CMake/presets/IDE config | blocked | — | T-005 | — |
| T-012 | P11c: Sand Linux build (as T-003, for Sand) | todo (unblocked; needs task file) | — | — | Sand repo |
| T-013 | P10: host matrix (Carla, Ardour, Renoise on the VM; Bitwig/x86_64 on a PC) | blocked | — | T-002, T-007 | uses the VM |
| T-014 | P12: LV2 | blocked | — | everything else | — |

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
| 2026-10-14 | T-001 | (committed directly on hvoya-linux, last 75e906365) | — (evidence/notes only) |
| 2026-10-16 | sync hvoya → hvoya-linux; Gneiss master → linux-experiment | e393b6398; Gneiss 4219350 | User's morph/preset fix (hvoya 0b2d8dbef, Gneiss 3cf2e2e). Docker build Gneiss CLAP+VST3 PASS; REAPER `steps/gneiss.txt` CLAP PASS, VST3 PASS except the known right-click step (`evidence/merge-smoke/sync-1016-*`) |
| 2026-10-15 | T-007 | 4d413d040 (conflict in IGraphicsLinux.cpp: T-003 and T-007 both added "tell the host the opened size"; kept T-007's, which also skips a host-resized window; kept T-003's stand-in menu font) | Docker build Gneiss CLAP+VST3, IPlugControls CLAP+VST3, IPlugEffect VST3 PASS; REAPER: Gneiss CLAP PASS, IPlugControls VST3 P3b PASS; Gneiss VST3 fails only the right-click step, which also fails before T-007 (REAPER's VST3 host menu does not open on a same-ms synthetic click, notes/14) |
| 2026-10-15 | T-003 | e54a43e76 | Docker build Gneiss CLAP+VST3 and IPlugControls CLAP+VST3 PASS; REAPER: IPlugControls P3b script PASS, Gneiss `steps/gneiss.txt` PASS, all Draw/X on tid 1, Gneiss UI renders (`evidence/merge-smoke/t003-*`). Cross-platform bugs found → `../CROSS_PLATFORM_FINDINGS.md` |
| 2026-10-14 | T-002 | 0846167f8 | Docker build IPlugControls CLAP+VST3 PASS; REAPER full P3b script on CLAP: PASS, all Draw/X events on tid 1 (`evidence/merge-smoke/t002-clap/`) |
