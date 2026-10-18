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

**XDND (c35d5f415).** `IGraphicsLinuxDrop.h` (`LinuxFileDrop`), target side, version 5.
- Finding (Docker, REAPER 7.82): GTK sources (Files/Nautilus) look for `XdndProxy`/`XdndAware` on the **top-level
  client window** under the pointer only; REAPER's FX windows (and the main and FX-chain windows) carry
  `XdndAware` = 5 themselves. So an `XdndAware` on our child alone is never seen by GTK sources; Qt sources
  (deepest aware window) would reach it.
- Design: `XdndAware` + `XdndProxy`=self on our window; **`XdndProxy` on the host's top-level** naming our window,
  unless the host has a proxy of its own (then nothing is changed). Removed on close if it still names us (verified
  with REAPER's FX-chain window, which outlives the editor: `evidence/t019/xdnd/proxy-lifetime.txt`).
  Drags outside the editor are **relayed** to the top-level unchanged (XdndEnter re-sent on the way out, XdndLeave
  on the way in), so the host's own drop targets keep working (REAPER answered relayed positions itself).
- Drop: `text/uri-list` read asynchronously (SelectionNotify, INCR via PropertyNotify), XdndFinished sent before the
  plug-in sees the files (its handler may open a dialog or close the editor), then `IGraphics::OnDrop` for one file,
  `OnDropMultiple` for several, at the drop position / total scale — as on mac/win. Only `file://` URIs of this
  machine (no host or `localhost`), `%XX` decoded, a decoded NUL rejects the path; a source that never sends the data
  is given up after 2 s (XdndFinished not accepted).
- Accepts anywhere over the editor when files are offered (mac's `draggingEntered` does the same).

**Harness.** `scripts/probe/xdnd-source.c` (scripted source: `top` = GTK's target search incl. proxy, `deep` = Qt's),
xact verb `xdnd-expect`, steps `igraphicstest-drop.txt` (IGraphicsTest's DragNDrop test: one file, three files,
leave, relay over REAPER's FX header then drop, Qt-like direct target), `sand-tooltip.txt` (scale-independent),
`gneiss-drop.txt` + `gneiss-user-layout.hvoya`, tooltip check at the end of `sand.txt`; `input-thread-check.sh`
lists the tooltip functions and `IGraphics::OnDrop`/`OnDropMultiple`.

**Results so far (Docker, Ubuntu 24.04 arm64, REAPER 7.82):**
- Sand `sand.txt` CLAP + VST3 PASS with the tooltip check; `sand-tooltip.txt` CLAP at REAPER_UI_SCALE 1 and 2 PASS
  (scale 2 crisp, screenshots looked at), the close/reopen ×5 loop then closes the editor with the tooltip shown.
- `igraphicstest-drop.txt` CLAP + VST3 PASS; paths with UTF-8 decoded (screenshots looked at).
- URI-list parser cases: `evidence/t019/xdnd/uri-list-parse.txt` (comment, `%20`, UTF-8, localhost, other host,
  http, `%00`, invalid `%zz`, last line without CRLF).
- Main thread: tooltip functions and `OnDrop`/`OnDropMultiple` all tid=1 (`evidence/t019/threads-*`).
