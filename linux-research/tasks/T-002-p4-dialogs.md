# T-002 — P4: file, message and colour dialogs without GTK

- **Status:** review-done
- **Owner:** worker instance (spawned by the coordinator, 2026-10-14)
- **Branch/worktree:** `linux/T-002` from `hvoya-linux`, worktree `/Volumes/Storage/code/IPlug2-T-002`.
- **Depends on:** —
- **Conflicts:** `IGraphics/Platforms/IGraphicsLinux*.{h,cpp}` (T-007, T-008)

## Goal
Implement on Linux, without GTK in the process:
- `PromptForFile` (open and save, with extension filters);
- `PromptForDirectory`;
- `ShowMessageBox`;
- `PromptForColor`.

They are stubs today ("cancelled"). Gneiss needs them for presets, morph rigs and CC maps:
- `morph/morph_controller.hpp:2114,2126`;
- `morph/morph_file_router.hpp:115-146`;
- `hvoya/utils/preset_strip_control.hpp:421-446`;
- `user_tab_panel.cpp:841-851`.

## Design (PLAN §1, P4)
1. **xdg-desktop-portal `org.freedesktop.portal.FileChooser`** over D-Bus.
   - libdbus is `dlopen`'d.
   - The parent window is passed as `x11:<hex XID>`.
   - The response (`Response` signal) is async on the D-Bus fd, which is registered on `GetHostLoop()`.
2. **Fallback:** zenity, then kdialog, via `posix_spawn`, with the stdout pipe fd on the host loop. Never block the host thread; reap the child.
3. Message box and colour picker use the same path, or are IGraphics-drawn.
4. **Completion handlers:**
   - always fire on the host main thread, asynchronously (like mac);
   - if the editor closes with a dialog open, the dialog is dismissed or orphaned and the handler is **not** called on a dead object.
5. Encoding: UTF-8 paths, spaces, Cyrillic; a default directory and file name; save-dialog overwrite confirmation.

## Acceptance
- [x] **Docker (zenity path, no portal):**
  - open, save, directory, cancel;
  - editor closed mid-dialog;
  - Cyrillic and space paths;
  - main-thread check;
  - 50× churn with a dialog open at close.
- [x] **VM (portal under GNOME):** the real dialog appears and is screenshotted; selection via xdotool returns the path to the plugin.
- [x] No GTK symbols loaded in the plugin process (`/proc/<pid>/maps` check while the dialog is open).
- [x] Review pass; Log complete.

## Log

- 2026-10-14, worker instance. Worktree `/Volumes/Storage/code/IPlug2-T-002`, Docker container `t002`, build dirs
  `build/t002-*`. Notes: `notes/16-p4-dialogs.md` (design, findings, acceptance table, known issues). Evidence:
  `evidence/p4/`.
- 1a249af60: claim; `build-cmake.sh`/`build-skia-linux.sh` tracked (the `build-*` ignore rule hid them from every
  worktree but the main one).
- 961e342a0: `LinuxDialogs` (portal FileChooser via `dlopen`ed libdbus, zenity/kdialog fallback, all async on the
  HostLoop), `LinuxBackgroundProcess`, `linux_dbus`; IGraphicsLinux overrides; IPlugControls `IPLUG_TEST_DIALOGS`.
- c531c43da: zenity 4 starts inside the folder; UTF-8 `LC_CTYPE` for helper programs (REAPER in C locale got `?`s);
  `dialog-test.sh`. Docker CLAP PASS.
- following commits: VST3 PASS, churn 50× CLAP + VST3 PASS, APP PASS (Docker); kdialog probe (throwaway container);
  VM: portal CLAP + VST3 PASS, portal churn 30× PASS, APP PASS, zenity forced PASS, gdb thread check; unit test
  `Tests/LinuxDialogs` (found and fixed a leak of the extension lists); mac builds; 22.04 floor 2.34; validators
  unchanged.
- Acceptance: Docker zenity path [x], VM portal [x], no GTK in the process [x] (REAPER has no libgtk at all; the set of
  GTK/GDK libraries is unchanged while dialogs show), review pass: see below.
- VM lock taken 17:00–17:28 and 17:53–18:05, released after each batch.
- Review round 1 (fresh-eyes subagent): no High; 1 Medium (colour alpha under a decimal-comma locale), Low items
  (fd watch outlived its fd with CLAP's by-number unregistering, kill of a possibly finished group, Close after the
  Response, two-part extensions, style). All fixed in 73916de60, each with a unit case that fails on the old code;
  table in notes/16 "Review round 1". Verdict after the fixes: acceptance met.
- Re-run on 73916de60 (builds 17:41): unit 88 checks PASS (ASan/UBSan, `de_DE.UTF-8` generated in the run container);
  Docker `dialog-test.sh` CLAP + VST3 PASS, churn 50× CLAP + VST3 PASS, APP PASS, gdb threads all tid 1; VM portal
  CLAP + VST3 PASS, portal churn 30× PASS, APP PASS, zenity forced PASS, gdb threads tid 1. The coordinator's WIP
  commit 05164d63f holds these results; all of them had finished (last at 18:05) before it, so it is complete; the
  contact sheets that `vm_pull`/re-runs had deleted were regenerated afterwards.
- Hand-over: commits 1a249af60..HEAD on `linux/T-002`. Evidence `evidence/p4/` (sheets: `dialogs-clap/sheet.png`,
  `vm-portal-clap/sheet.png`, `kdialog-app/sheet.png`). Container `t002` removed. Open issues (notes/16 Known issues):
  zenity 4 dialogs not transient and without the proposed save name; calls without a completion handler return
  empty (no blocking dialogs); message boxes/colours need zenity or kdialog; kdialog and the KDE portal untested on a
  real Plasma desktop; helper inherits the host's `LD_LIBRARY_PATH` (P10); `ShowMessageBox` returns `kNoResult`.
