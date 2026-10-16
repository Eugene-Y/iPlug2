# T-019 — Tooltips and files dropped onto the editor (XDND)

- **Status:** ready
- **Owner:** — (planned: coordinator; IGraphicsLinux is shared with T-009/T-017/T-018/T-020, so not in parallel with them)
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
