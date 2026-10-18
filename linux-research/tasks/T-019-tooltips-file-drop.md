# T-019 — Tooltips and files dropped onto the editor (XDND)

- **Status:** in-progress
- **Owner:** coordinator (2nd coordinator instance, 2026-10-18; lock `T-019`)
- **Branch/worktree:** `linux/T-019` from `hvoya-linux`, in `/Volumes/Storage/code/IPlug2-T-019`.
- **Depends on:** nothing.
- **Plan phase:** parity (notes/22); first because Gneiss and Sand use both features.
- **Conflicts:** `IGraphics/Platforms/IGraphicsLinux*`.

## Goal
Two IGraphics features that mac and win have and Linux lacks, both used by the hvoya plug-ins:
1. **Tooltips.** `IGraphics::EnableTooltips(true)` + `IControl::SetTooltip()` show the control's tooltip after hovering, as on mac/win. Today `IGraphicsLinux::UpdateTooltips()` is `{}`. Gneiss and Sand call `EnableTooltips`/`SetTooltip`.
2. **Files dropped onto the editor.** Dragging one or several files from a file manager (and from a host, if it offers file drags) onto a control calls `IControl::OnDrop` / `OnDropMultiple` via `IGraphics::OnDrop`/`OnDropMultiple(…, x, y)`, as on mac/win. Gneiss' USER tab (`hvoya/utils/user_tab_panel.cpp` `UserTabPanel::OnDrop`) needs it.

Mac/win code and behaviour unchanged; only Linux files (AGENTS §2).

## Context
- **Reference behaviour:** `IGraphicsWin.cpp` (tooltip window, `mTooltipIdx`, show/hide on mouse move, hide on click and leave; `TOOLTIPWND_MAXWIDTH`), `IGraphicsMac.mm`/`IGraphicsMac_view.mm` (`registerToolTip`, `stringForToolTip`; hvoya's `mTooltipUpdatePending` liveness fix). Drops: Win `IDropTarget`/`WM_DROPFILES`, mac `draggingEntered`/`performDragOperation`.
- **Threading:** everything on the host main thread via the X event drain (PLAN §1). No new threads.
- **Binary hygiene:** no new NEEDED libraries (dlopen if anything new is needed); `scripts/inspect-binary.sh`.
- **Tooltip rendering — evaluate, then choose, record in the Log:**
  - (a) an override-redirect X window (can extend beyond the editor, like native tooltips), drawn without a second GL context: e.g. text rasterized with stb_truetype into an `XImage`, using the editor's default font;
  - (b) drawn inside the editor by IGraphics (like the stand-in menus; clipped to the editor).
  - Prefer (a) if its cost is moderate; it matches mac/win. Delay, placement near the cursor, screen clamping (`_NET_WORKAREA`, as T-008's `FitMenusOnScreen`), HiDPI scale (T-007), hide on click/leave/close, no leak after editor close.
- **XDND:** set `XdndAware` (version 5) on our window; handle `XdndEnter`/`XdndPosition`/`XdndLeave`/`XdndDrop`, reply `XdndStatus`/`XdndFinished`, request `text/uri-list` via `XConvertSelection(XdndSelection)`, decode `file://` URIs (percent-decoding) into paths, map the position to editor coordinates (scale!). Find out how sources pick the target inside an embedded child window (deepest `XdndAware` window vs top-level, `XdndProxy`): test with Nautilus/Files on the VM, a scripted source in Docker (e.g. a tiny Xlib/Python XDND source or `dragon-drop` if available), and REAPER's own drags. Hosts that set `XdndAware` on their top-level may swallow drops; document what works.
- The clipboard code (`IGraphicsLinuxClipboard.h`) already does selection conversion and INCR; reuse it.

## Steps
1. Tooltips: implement per the chosen design; test on IPlugControls (call `EnableTooltips` in a test build, or a control with `SetTooltip`) and Gneiss/Sand.
2. XDND: implement; test single and multiple files, a drop outside any control, cancel (leave), and a drop while a menu is open.
3. Harness: add steps (hover + wait + screenshot for tooltips; a scripted XDND drop) to `scripts/steps/`, so regressions show up in Docker.
4. Full P3b scripts, Gneiss `steps/gneiss.txt`, churn 100× (no leaked windows/atoms/fds), main-thread census.
5. VM: short, light check (take the VM lock; `timeout 8 xdpyinfo` before/after): tooltip look under GNOME, drag from Files into Gneiss' USER tab (CLAP and VST3 in REAPER).

## Acceptance
- [ ] Tooltips appear/disappear like on win/mac; screen-clamped; correct at scale 1 and 2; no leak after 100× churn.
- [ ] Files dropped from Files (VM) reach `OnDrop`/`OnDropMultiple` at the right control; Gneiss USER tab accepts a dropped file in REAPER CLAP and VST3.
- [ ] Docker steps for both; full P3b + Gneiss scripts PASS; main thread only; NEEDED unchanged.
- [ ] Independent review (AGENTS §4); evidence in `evidence/t019/`; Log complete.

## Log
(append: date — what was done — commits — next)

### 2026-10-18 — tooltips and XDND implemented (coordinator)
**Tooltips (28ae356c8).** Design (a), without a second GL context: an override-redirect X window
(`_NET_WM_WINDOW_TYPE_TOOLTIP`, save-under) with the editor's GL visual and colormap, drawn by making the editor's
own GLX context current on it for one frame and drawing with the editor's NanoVG context and the stand-in menu's
sans-serif face (`StandInMenuText`). Cost: ~150 lines, no new library, no new thread. (b) was rejected: Gneiss and
Sand editors are 170–200 px tall, multi-line tooltips would be clipped.
- Timing as Windows: shows after the pointer rests 500 ms within 4 px on a control with a tooltip; once shown it
  stays while the pointer is on that control; hidden on any button or key press, LeaveNotify, menu open, text entry,
  a minimized host window, editor close. Re-armed only on another control (as `TME_HOVER` on Windows).
- Place: below the pointer (+20 logical px), above it where the work area ends; clamped to `_NET_WORKAREA`.
- Size: max width 250 logical px (Windows' `TOOLTIPWND_MAXWIDTH`), font 13 px, at the **screen** scale (not the
  plug-in's draw scale, like native tooltips).
- `UpdateTooltips()` (IPopupMenuControl calls it when a menu opens/closes) hides a shown tooltip while a menu is
  open and refreshes its text if the control's tooltip changed.
- The swap-interval code moved into `DisableSwapWait()` (used for both windows).

**XDND (c35d5f415, redesigned after the VM run, see below).** `IGraphicsLinuxDrop.h` (`LinuxFileDrop`), target side,
version 5: `XdndAware` on the editor's window, nothing on the host's.
- How sources find a target: GTK and Qt sources address the **top-level client window** under the pointer. REAPER's
  FX, FX-chain and main windows carry `XdndAware` = 5, and **REAPER forwards a drag to an `XdndAware` child window by
  itself**, as its own source (protocol version 3; the data still comes from the original source's `XdndSelection`).
  Verified in Docker (scripted GTK-like source, `XdndProxy` absent) and on the VM (GNOME Files, trace build).
- First design (c35d5f415), dropped: `XdndProxy` on the host's top-level naming the editor, with drags outside the
  editor relayed to the top-level. The VM run with Files (GTK 4.14) showed GTK 4 follows the proxy for the first
  position only: it then sends `XdndLeave` to the proxy and a new `XdndEnter` and everything after it to the top-level
  directly (`GDK_DEBUG=dnd` log + our trace: `evidence/t019/vm/relay-gneiss-*`, `editor-gneiss-trace`). Over REAPER's
  FX header that left the drag unfinished (the drag icon stayed on screen; the baseline build finishes it), and a
  host whose top-level is not `XdndAware` would get GTK 4's messages without understanding them. In REAPER the proxy
  was never needed: the editor drops worked through REAPER's own forwarding. Proxy and relay are removed.
- Hosts whose top-level is not `XdndAware` and that do not forward get no drops from GTK/Qt file managers (a Qt-style
  source that targets the deepest aware window still reaches the editor). To be checked per host in T-013.
- Drop: `text/uri-list` read asynchronously (SelectionNotify, INCR via PropertyNotify), XdndFinished sent before the
  plug-in sees the files (its handler may open a dialog or close the editor), then `IGraphics::OnDrop` for one file,
  `OnDropMultiple` for several, at the drop position / total scale, as on mac/win. Only `file://` URIs of this
  machine (no host or `localhost`), `%XX` decoded, a decoded NUL rejects the path; a source that never sends the data
  is given up after 2 s (XdndFinished not accepted). Accepts anywhere over the editor when files are offered (mac's
  `draggingEntered` does the same); IGraphics hands the files to the topmost control under the drop.
- `-DIGRAPHICS_TRACE_LINUX_INPUT` also traces drag enter/drop/finish (`iPlug2 drop:` on stderr), for T-013.
- Gneiss (not Linux-specific, for the user): `UserTabPanel::OnDrop` only runs where the panel itself is the topmost
  control, i.e. in gaps between its child controls (e.g. view 360,75 in the default layout); a file dropped on a knob
  goes to the knob and is ignored. Same on mac/win (same `IGraphics::OnDrop` path).

**Harness.** `scripts/probe/xdnd-source.c` (scripted source: `top` = GTK's target search incl. proxy, `deep` = Qt's),
xact verb `xdnd-expect`, steps `igraphicstest-drop.txt` (IGraphicsTest's DragNDrop test: one file, three files,
leave, relay over REAPER's FX header then drop, Qt-like direct target), `sand-tooltip.txt` (scale-independent),
`gneiss-drop.txt` + `gneiss-user-layout.hvoya`, `sand-drop-menu.txt`, `vm/probe/files-drop.sh` (a real mouse drag from
GNOME Files, optional `GDK_DEBUG=dnd` log and drop trace), tooltip check at the end of `sand.txt`; `input-thread-check.sh`
lists the tooltip functions and `IGraphics::OnDrop`/`OnDropMultiple`.

**Results (final tree 98a6101f4; Docker Ubuntu 24.04 arm64 + the GNOME/X11 VM, REAPER 7.82):**
- Docker: `igraphicstest-drop.txt` CLAP + VST3 PASS (one file, three files, leave, a drag over REAPER's FX header
  that REAPER answers itself, Qt-like direct target; UTF-8 paths decoded, screenshots looked at); `gneiss-drop.txt`
  CLAP + VST3 PASS (the USER tab loads the dropped layout); `sand-drop-menu.txt` PASS; `sand.txt` CLAP + VST3 PASS
  with the tooltip check; `sand-tooltip.txt` CLAP at REAPER_UI_SCALE 1 and 2 PASS (scale 2 crisp, looked at), the
  close/reopen ×5 loop closes the editor with the tooltip shown.
- Regression on the tooltip + first XDND tree (the redesign only removed code from `IGraphicsLinuxDrop.h`): P3b
  IPlugControls CLAP + VST3 PASS, Gneiss `gneiss.txt` CLAP PASS and VST3 only the known right-click step, churn ×100
  IPlugControls CLAP stable (fds 6, sockets 1, threads 14 closed; RSS flat after 25), `evidence/t019/regress/`.
- Main thread: tooltip functions and `IGraphics::OnDrop`/`OnDropMultiple` all tid=1 (`evidence/t019/threads-*`).
- NEEDED unchanged (libX11, libm, libc, ld-linux): `evidence/t019/needed.txt`.
- URI-list parser cases: `evidence/t019/xdnd/uri-list-parse.{cpp,txt}` (comment, `%20`, UTF-8, localhost, other host,
  http, `%00`, invalid `%zz`, last line without CRLF).
- VM (GNOME Shell/Mutter X11, VM lock taken and released, `xdpyinfo` fine before and after): Sand tooltip steps PASS,
  looked at (`vm/sand-tooltip-clap`); a real mouse drag from GNOME Files into Gneiss' USER tab loads the layout in CLAP
  and VST3, and a drag onto REAPER's FX header finishes (Nautilus logs `XdndFinished` in all four:
  `vm/final-{clap,vst3}-{editor,header}`, `vm/files-drop/`). The first design's failure and the baseline comparison:
  `vm/proxy-design/`, `vm/relay-header-*`.

