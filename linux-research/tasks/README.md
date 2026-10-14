# Task board

Read `../AGENTS.md` first. Statuses: `ready` · `claimed` · `in-progress` · `review-done` (awaiting merge) · `merged` · `blocked` (dependency not met) · `todo` (no task file yet).

The coordinator keeps this table current. A worker updates only its own task file, in its own branch.

| ID | Task | Status | Owner | Depends on | Conflicts with (same files) |
|---|---|---|---|---|---|
| [T-001](T-001-vm-p3b.md) | VM verification of P3a/P3b on GNOME | in-progress | coordinator | — | uses the VM (lock) |
| [T-002](T-002-p4-dialogs.md) | P4: file/message/colour dialogs without GTK | ready | coordinator (next) | — | T-007 (IGraphicsLinux.cpp) |
| [T-003](T-003-gneiss-linux-build.md) | P7: Gneiss Linux CMake build (CLAP/VST3/APP) | **ready — good for a 2nd instance** | — | — | — |
| [T-004](T-004-app-no-audio.md) | P8: standalone APP robustness (no audio device, devices, settings) | **ready — good for a 2nd instance** | — | — | — |
| [T-005](T-005-ide-research.md) | P11a: IDE research + presets/IDE configs for examples | **ready — good for a 2nd instance** | — | — | T-011 later |
| [T-006](T-006-clap-state-repro.md) | clap-validator `state-reproducibility` failures (Linux and mac) | **ready — good for a 2nd instance** | — | — | — |
| T-007 | P5: HiDPI (CLAP set_scale, VST3 content scale, Xft.dpi) | blocked | — | T-002 merged | T-002 |
| T-008 | Focus: timestamped `XSetInputFocus` instead of the 150 ms delay (notes/15) | blocked | — | T-001 findings | IGraphicsLinux.cpp |
| T-009 | P6: Gneiss visual parity vs macOS | blocked | — | T-003 | — |
| T-010 | P9: Gneiss-side changes (deferred `SetLatency`, menus, `openFolder`) | blocked | — | T-003 | Gneiss repo |
| T-011 | P11b: `duplicate.py` + templates produce Linux CMake/presets/IDE config | blocked | — | T-005 | — |
| T-012 | P11c: Sand Linux build (as T-003, for Sand) | blocked | — | T-003 | Sand repo |
| T-013 | P10: host matrix (Carla, Ardour, Renoise on the VM; Bitwig/x86_64 on a PC) | blocked | — | T-002, T-007 | uses the VM |
| T-014 | P12: LV2 | blocked | — | everything else | — |

## Merge log (coordinator)

| Date | Task | Merge commit | Smoke tests |
|---|---|---|---|
