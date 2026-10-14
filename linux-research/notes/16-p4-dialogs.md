# P4: file, folder, message and colour dialogs without GTK (2026-10-14)

Task T-002, branch `linux/T-002`. Evidence: `evidence/p4/`. **[D]** = Docker (REAPER 7.82 aarch64, Xvfb, openbox,
zenity 4.0.1, no portal, container `t002`). **[V]** = the VM (GNOME 46 on X11, xdg-desktop-portal 1.18.4 +
xdg-desktop-portal-gnome 46.2, FileChooser version 3, zenity 4.0.1). **[K]** = a throwaway container with
`apt-get install kdialog` (kdialog 23.08.5, Qt without a KDE desktop). **[U]** = `Tests/LinuxDialogs` (ASan/UBSan).

## What was built

All new code is Linux-only; the shared touch points are the IGraphicsLinux overrides, two CMake source lists and a
test block in IPlugControls behind `IPLUG_TEST_DIALOGS`.

| File | What |
|---|---|
| `IGraphics/Platforms/IGraphicsLinuxDialogs.{h,cpp}` | `LinuxDialogs`: one per open editor. `Show(request, handler)` returns at once; every dialog is a job that tries its backends in order and completes from the instance's `HostLoop` (a 50 ms timer while any job runs, plus the job's fd). `DismissAll()`/destructor: dialogs closed, **no handler**. Backends: portal FileChooser, zenity, kdialog |
| `IPlug/Linux/IPlugLinuxDBus.{h,cpp}` | libdbus `dlopen`ed (`libdbus-1.so.3`, never closed), own declarations (no dbus headers needed to build). Session bus address from `DBUS_SESSION_BUS_ADDRESS` or `$XDG_RUNTIME_DIR/bus`; never `autolaunch:` (it would start a bus daemon) |
| `IPlug/Linux/IPlugLinuxSubprocess.{h,cpp}` | `LinuxBackgroundProcess`: a program in the background, stdout on a non-blocking pipe, exit status as the pipe's last line. Started detached (below). Own process group, signal mask and dispositions reset, the host's other fds closed (`addclosefrom_np`, glibc 2.34 = the floor), stderr to /dev/null, a UTF-8 `LC_CTYPE` when the host's locale is not UTF-8 |
| `IGraphicsLinux.{h,cpp}` | `ShowMessageBox`, `PromptForFile`, `PromptForDirectory`, `PromptForColor`; the dialog's parent is the editor's client top-level (the ancestor with `WM_STATE`); `CloseWindow` dismisses open dialogs before anything else goes |
| `Tests/LinuxDialogs` | unit test with fake zenity/kdialog scripts and a fake host loop pumped on the test thread |
| `Examples/IPlugControls` | `-DIPLUG_TEST_DIALOGS`: eight `t-*` buttons (open, save, folder, Yes/No/Cancel, OK with Cyrillic text, OK/Cancel, colour, a call without handler); every answer to stderr as `IPLUG_TEST_DIALOG <kind> tid= main= first= second=` |

### Choices

| Topic | Choice and why |
|---|---|
| Order | portal (files and folders only) → zenity → kdialog; kdialog before zenity when `XDG_CURRENT_DESKTOP` contains KDE. `IPLUG_LINUX_DIALOGS="zenity,kdialog"` etc. overrides it (tests; a user with a broken portal) |
| Portal | One private session-bus connection per request, so every `Response` on it is ours. Fully async: `Hello` sent by hand (libdbus' `dbus_bus_register` would block the host thread on the round trip), then `AddMatch` for the request path computed from our unique name and `handle_token` (before the call, so the Response cannot be missed), then `OpenFile`/`SaveFile`. Folders: `Properties.Get(version)` first; below 3 the `directory` option would be ignored silently, so such portals go to zenity. Options: `modal`, `handle_token`, `directory`, `current_name` (save, with the extension), `current_folder` (bytes, NUL-terminated), `filters` with case-insensitive globs (`*.[fF][xX][pP]`, the portal documents its globs as case-sensitive). Parent `x11:<hex>` of the client top-level. An error reply (`ServiceUnknown`: no portal; `UnknownMethod`: no FileChooser backend), a lost connection or 10 s without a reply before the dialog → the next backend. Response 1 = cancel, 2 = cancel with a stderr line. `file://` URIs percent-decoded; a non-`file:` URI is refused with a stderr line. Dismiss sends `Request.Close`, then closes the connection |
| Programs | argv only; the program is started through `/bin/sh -c '{ "$@"; printf "\nIPLUG_EXIT=%d\n" "$?"; } &' sh <program> <args…>` — the arguments are positional parameters, never parsed (the `IPlugLinuxOpen.h` pattern). The launcher exits at once and is reaped (blocking `waitpid` on a shell that only forks); the program and its waiting subshell are reparented to init or the session's subreaper. So no zombie is ever left for the host, even when the editor closes mid-dialog, and a host that reaps all children (or ignores SIGCHLD) cannot steal the exit status: it arrives on the pipe. This replaces the planned "`waitpid(WNOHANG)` from the loop": that leaves a killed child to be reaped by someone after the editor is gone |
| Close mid-dialog | `kill(-pgid, SIGTERM)` (the group still exists while we hold the pipe open: the subshell writes the status line), pipe closed, fd and timer unregistered, handler dropped. Portal: `Request.Close`, connection closed |
| Results | As macOS/Windows: file → full path as `fileName`, its folder with a trailing `/` as `path`; folder → `path` with a trailing `/`; cancel → both empty. Message box: `kNoResult` returned, the answer through the handler (like iOS/web). Colour: `false` returned, the colour through `func` only when chosen (`false` keeps a caller that expects a synchronous answer from using an unchanged colour) |
| Calls without a handler | `PromptForFile`/`PromptForDirectory`/`PromptForColor` without a completion handler return empty at once and warn once on stderr. A blocking dialog would stall the host's main thread, whose loop is not ours to run nested (PLAN §1); iOS has the same rule. A message box without a handler is still shown |
| Save extension | The proposed name gets the first extension. An answer with none of the extensions gets the first one appended (NSSavePanel and `lpstrDefExt` do the same); if that file exists, our own overwrite question follows (zenity/kdialog Yes/No), because the dialog's own confirmation saw the other name |
| Message boxes | zenity: `--info`, `--question` with explicit labels (OK/Cancel, Yes/No), `--warning` Retry + extra Cancel, Yes/No/Cancel as `--switch` with three extra buttons (the label comes back on stdout; Escape prints nothing → Cancel, as on macOS). `--no-markup` (plug-in text is not Pango markup). kdialog: `--msgbox`, `--yesno`, `--warningyesno`, `--yesnocancel` (exit 0/1/2), labels via `--yes-label`/`--no-label`; text starting with `-` gets a leading space (kdialog would parse it as an option) |
| Colour | zenity `--color-selection --color=rgba(...)` / kdialog `--getcolor --default #rrggbb`. Not IGraphics-drawn: iPlug has no colour-picker control (only swatches), writing one is its own task, and the portal has no chooser (Screenshot's `PickColor` is an eyedropper). Alpha is kept from the input unless the answer is `rgba(...)` |
| Start folder | Empty folder → the portal's own choice; zenity/kdialog get `$HOME` (zenity would otherwise use the host's cwd). zenity 4 shows a bare `--filename=/dir/` selected inside its parent; a name inside the folder (the proposed name, or `.iplug2-start-folder`, which need not exist) opens the folder itself |
| Parent window | Client top-level found with `XQueryTree` + `WM_STATE` on the editor's own connection. Portal: `x11:<hex>`. zenity: `--attach=<decimal> --modal` (honoured by zenity 3; zenity 4 ignores both with a warning on its stderr, which goes to /dev/null). kdialog: `--attach` |

## Findings

1. **zenity prints through the locale's charset.** In REAPER under Docker (no `LANG`, so "C") zenity printed the chosen
   Cyrillic path as `?`s, and GLib converts option strings from the locale too. Real desktops have a UTF-8 `LANG`, but
   hosts may run in C. Fixed by giving the child `LC_CTYPE=C.UTF-8` only when the host's effective `LC_CTYPE` is not
   UTF-8 (messages and numbers untouched; `LC_ALL` moves to `LANG`). [D][U]
2. **zenity 4 (GTK 4)** — Ubuntu 24.04's: `--attach` and `--confirm-overwrite` are deprecated no-ops (GTK 4 always asks
   before overwriting), so its dialogs are not transient for the host's window; the proposed save name is **not**
   shown (`--filename=/dir/name` sets only the folder); a bare folder in `--filename` is shown selected in its parent
   (worked around, see Start folder). [D][V]
3. **GNOME's portal dialog** (xdg-desktop-portal-gnome, GTK 4) is transient for the REAPER FX window (`WM_TRANSIENT_FOR`
   = the FX window, checked with xprop), attached modally under Mutter, and shows the proposed name, the folder and the
   `*.fxp` filter (`vm-portal-clap/d01-save-dialog.png`). `Request.Close` takes it away when the editor closes, and the
   portal drops it when the process exits (APP). It preselects only the base name of the proposed `test preset.fxp`,
   so typing a name keeps the extension, and GTK asks about an existing file itself: the "save onto an existing name"
   step got GTK's question on the VM (`vm-portal-clap/sheet.png`, d02), our own one only with zenity. [V]
4. **REAPER loads no GTK at all**: its maps hold `libgdk-3` and `libgdk_pixbuf` (SWELL draws with GDK only), no
   `libgtk*`. While a zenity or portal dialog shows, the set of GTK/GDK libraries in REAPER's process is unchanged
   (`gtk-libs-before.txt` = `gtk-libs-dialog-open.txt` in every run). The plug-in's NEEDED is unchanged:
   `libX11.so.6`, libm, libc. [D][V]
5. Test harness: GTK 4's location entry completes inline while xdotool types, which garbles typed names
   (`стрррарый…`); the harness pastes names instead. Clicking dialog buttons by window geometry misses on GNOME (GTK 4's
   client-side shadow is part of the window); keyboard instead (Tab moves Yes → No). The GTK 4 colour dialog's Select is
   Alt+S. [D][V]

## Acceptance

| # | Check | Result |
|---|---|---|
| 1 | Docker, zenity path, `dialog-test.sh` (CLAP, VST3): save (extension added), save onto an existing name (our overwrite question, Yes), open, open cancel, folder, folder cancel, Yes/No/Cancel by button and by Escape, OK with Cyrillic text, OK/Cancel both ways, colour, the call without handler (empty at once + warning), Cyrillic and spaces in every path (`/tmp/dlg/Пресеты с пробелами/новый пресет.fxp`) | **PASS ×2** [D] (`dialogs-{clap,vst3}/checks.txt`, screenshots `d0*.png`) |
| 2 | Main thread | every handler line `main=1` (tid = REAPER's pid) in every run, CLAP, VST3, APP; gdb: `LinuxDialogs::Show/Poll`, `LinuxBackgroundProcess::Start/Pump` all on tid 1 [D] (`threads-clap/`), `Show/Poll` on tid 1 with the portal [V] (`vm-threads-clap/`) |
| 3 | Editor closed mid-dialog (save, message box, colour) | dialog program gone, no window of that title left, no handler, REAPER alive, dialogs work after reopening [D][V] |
| 4 | 50× churn with a dialog open at every close (save, folder, message, colour in turn; every 5th closes right after the click, before the dialog maps) | CLAP and VST3 PASS: fds 6/8, sockets 1/2, threads 14/27 (closed/open) equal before and after, 0 children, 0 helpers, 0 handler calls, 0 zombies [D] (`churn-dialog-*`); 30× with the portal: PASS, no portal window left after any close [V] (`vm-churn-dialog-clap`) |
| 5 | VM, portal under GNOME: real dialog screenshotted, selection via xdotool returns the path | `dialog-portal-test.sh` CLAP + VST3 **PASS**: dialogs shown by `/usr/libexec/xdg-desktop-portal-gnome` with no zenity running, transient for the FX window; save/open/folder/cancel paths returned as in row 1 [V] (`vm-portal-{clap,vst3}`) |
| 6 | VM, zenity forced (`IPLUG_LINUX_DIALOGS=zenity`) | PASS [V] (`vm-zenity-clap`) |
| 7 | APP (GLib loop): save, message, the APP closed with a dialog open | PASS: handlers on the main thread; the APP exits, the zenity/portal dialog goes with it, no handler [D][V] (`dialogs-app`, `vm-portal-app`) |
| 8 | kdialog path (APP, `IPLUG_LINUX_DIALOGS=kdialog`) | all seven kinds answer correctly on the main thread: save with the extension added, open, folder, Yes/No/Cancel Escape → CANCEL, OK, OK/Cancel, colour (`kdialog-app/lines.txt`, `kdialog-sheet.png`) [K] |
| 9 | No GTK in the plug-in process | finding 4 [D][V] |
| 10 | Unit test, ASan/UBSan under `dbus-run-session`: argv verbatim (a title with `$(…)`, quotes and backticks; nothing expanded), proposed name and filters, Cyrillic/space paths, overwrite question Yes/No, every message mapping, colour rgba/rgb/cancel, fallback zenity missing / exit 255 / exit 127 → kdialog, nothing available (cancelled, asynchronously; an OK box reads OK), `DismissAll` and destruction kill the program and call no handler, a handler that destroys the dialogs, child locale, no bus and a bus without a portal → zenity without waiting | 75 checks PASS, no ASan report [U] (`unit/dialogs-test-asan.txt`). The one UBSan line is WDL's own `memcpy(null, null, 0)` when an empty `WDL_String` is copied |
| 11 | macOS | IPlugEffect CLAP and IPlugControls VST3 (it has no CLAP scheme) Debug BUILD SUCCEEDED, build and install dirs in the scratchpad (`macos/mac-builds.txt`) [M] |
| 12 | Binary, validators | NEEDED unchanged (`libX11.so.6`, libm, libc); 22.04 build: GLIBC floor **2.34** (unchanged, `addclosefrom_np` is 2.34) (`cmake-22.04.log`); validators identical to P3b: clap-validator Controls 33/3 (state-reproducibility), VST3 537/537 (`validators.log`) [D] |

## Harness

New in `scripts/`: `dialog-fixture.sh` (Cyrillic/space folders), `dialog-up.sh` (REAPER + editor, prints the view
origin), `dialog-test.sh` (the REAPER checks above; `EXPECT_PORTAL=1` adds the portal checks), wrappers
`dialog-portal-test.sh` / `dialog-zenity-test.sh` for `vm-run.sh` (which passes no environment),
`dialog-churn-test.sh`, `dialog-app-test.sh`, `dialog-thread-check.sh`, `dialog-kdialog-probe.sh`. `build-cmake.sh` and
`build-skia-linux.sh` are now tracked: the `.gitignore` rule `build-*` had kept them out of git, so no worktree but the
main one had them. The Docker image already had zenity; kdialog is not added (Qt/KF5, several hundred MB) — probe it in
a throwaway container.

```sh
R=linux-research
DOCK_ENV="EXTRA_CMAKE_ARGS=-DCMAKE_CXX_FLAGS=-DIPLUG_TEST_DIALOGS" $R/scripts/dock.sh -- scripts/build-cmake.sh .. build/t002-test IPlugControls-clap IPlugControls-vst3 IPlugControls-app
$R/scripts/gx.sh t002 scripts/dialog-test.sh evidence/p4/dialogs-clap "CLAP: IPlugControls" build/t002-test/out/IPlugControls.clap
$R/scripts/gx.sh t002 scripts/dialog-churn-test.sh evidence/p4/churn-dialog-clap "CLAP: IPlugControls" build/t002-test/out/IPlugControls.clap 50
$R/scripts/vm/vm-deploy.sh -p iplug-builds/t002 $R/build/t002-test IPlugControls
$R/scripts/vm/vm-run.sh dialog-portal-test.sh evidence/p4/vm-portal-clap "CLAP: IPlugControls" "~/iplug-builds/t002/.clap/IPlugControls.clap"
$R/scripts/dock.sh -- bash -c 'cmake -S ../Tests/LinuxDialogs -B build/t002-dialogs-test -G Ninja && cmake --build build/t002-dialogs-test && dbus-run-session -- build/t002-dialogs-test/iplug-linux-dialogs-test'
```

## Known issues / left for later

- **zenity 4 dialogs are not transient for the host window** and can open behind it; the proposed save name is not
  filled in (finding 2). zenity is the fallback; on GNOME/KDE desktops the portal is used.
- **Calls without a completion handler get an empty answer** (no blocking dialogs). Upstream's
  `IVPresetManagerControls` load button uses the synchronous form and does nothing on Linux; Gneiss passes handlers
  everywhere.
- Message boxes and colours need zenity or kdialog. Without either (or for a save whose added extension hits an
  existing file, which needs our overwrite question) the dialog is answered as cancelled and the message goes to stderr.
- kdialog verified only in a container (Qt without Plasma); the KDE portal backend and kdialog on a real Plasma
  desktop are untested (P10).
- The program inherits the host's environment: a host that ships its own libraries in `LD_LIBRARY_PATH`
  (Bitwig?) could break zenity. Check in P10; the fix would be to drop such variables for the child.
- A host killed hard leaves an open zenity behind (its parent is init); a normal close or exit removes it.
- Blocking pieces, all local and short: `connect()` to the session-bus socket when a portal request starts, the
  launcher's `waitpid` (a shell that only forks), `dbus_connection_flush` of the portal's `Close` on dismissal, and the
  `XQueryTree` walk for the parent window.
- Portal response 2 ("other") is reported as a cancel with a stderr line, not retried with zenity.
- zenity that cannot open its window (no display) exits 1 like a user's cancel: no kdialog fallback then, and its
  message went to /dev/null.
- `ShowMessageBox` returns `kNoResult`; a caller that uses the return value instead of the handler sees nothing
  (allowed by `IGraphicsConstants.h`, the same as iOS and web). With no editor open no box is shown at all.
- `PromptForColor` calls `func` once, when the colour is chosen; macOS calls it on every change in its panel.
- With the fallback host-loop backend (a host without timer support) `Show` and `Poll` would run on different threads,
  like everything else of the editor there (notes/11 known issue).

## Review round 1 (fresh-eyes subagent, no High findings)

| # | Finding | Fix |
|---|---|---|
| M1 | Colour alpha followed the host's `LC_NUMERIC`: `%.3f`/`%f` write and read a decimal comma under a German locale, so zenity got an unparsable colour and the answer's alpha read as 0 | Alpha written with integer arithmetic, read by a small parser; unit case under `de_DE.UTF-8` |
| L2 | A backend's fd was closed before its watch was removed; with hosts that unregister by fd number (CLAP) a reused number could keep or drop the wrong watch; `mFdFailed` was never reset | Each job owns its watch and removes it before closing the fd (`LinuxBackgroundProcess::CloseOutput` separated from `Pump`); `mFdFailed` reset per backend. The unit test's fake backend now fails on any watch whose fd changed identity (dev/inode) |
| L3 | `Kill()` could signal a process group whose members had all exited | `Kill` reads first and signals only while the status line has not arrived; the status line itself ends a program (a child of it holding the pipe no longer delays the answer) |
| L4 | `Request.Close` and a blocking flush were sent after the Response | skipped once answered |
| L5 | Two-part extensions (`tar.gz`) were compared by their last part only | whole-suffix, case-insensitive comparison |
| L6 | zenity's display failure reads as cancel | documented (Known issues) |
| L7 | Style: several statements per `case` line, a misleading reuse of `SplitExtensions` for backend names, `fileName` carrying the path to confirm, one shared warning flag | `ArgList::Add({ ... })`, `SplitList`, `mPathToConfirm`, a flag per method |
| I8–I12 | `kNoResult`, colour callback count, fallback-thread races, blocking `connect()`, `<unistd.h>` in the test block | documented; the test block's POSIX calls are Linux-only now |

All six new unit cases fail on the code before the fixes and pass after (mutation check in a throwaway worktree). Re-run
after the fixes: unit test 88 checks PASS under ASan/UBSan; Docker `dialog-test.sh` CLAP + VST3, churn 50× CLAP + VST3,
APP, gdb thread check PASS; VM portal CLAP + VST3, portal churn 30×, APP, zenity forced, thread check: see the task log.
