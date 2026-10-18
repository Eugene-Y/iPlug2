# T-019 — Tooltips and files dropped onto the editor (XDND)

- **Status:** review-done (independent review: round 1 accept with fixes, round 2 accept, 2026-10-18)
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
- Timing like Windows' initial delay: shows after the pointer rests 500 ms within 4 px on a control with a tooltip; once shown it
  stays while the pointer is on that control; hidden on any button or key press, LeaveNotify, menu open, text entry,
  a minimized host window, editor close. Re-armed only on another control (as `TME_HOVER` on Windows). No auto-hide
  after a few seconds (Windows has one, mac does not).
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

### Review round 1 (independent reviewer, 2026-10-18)
Fresh context, read-only on code. Range `hvoya-linux..linux/T-019` (28ae356c8 … 0a96072ba). Shared-code rule holds:
outside `linux-research/` only `IGraphics/Platforms/IGraphicsLinux.{h,cpp}` and the new `IGraphicsLinuxDrop.h` change,
so mac/win/iOS/web are untouched. No product names in framework code. Style fine (2-space, `mCamelCase`, `pCamelCase`).
Main thread only by construction (everything comes from the X drain / display timer). NEEDED unchanged.

Findings:
- **R1 — major — `IGraphicsLinux.cpp:1840-1850` (tooltip window), `:900` (LeaveNotify).** The tooltip window takes
  pointer input. It sits 20 logical px below the pointer, and stays shown while the pointer is on the same control.
  So moving the pointer down on any control taller than about 20 px enters the tooltip. The editor then gets a
  LeaveNotify, which hides the tooltip, resets `mTooltipIdx` and calls `OnMouseOut()`. The control's hover drops, then
  comes back on the next motion. While the pointer rests, the tooltip stays hidden: it is re-armed only on the next
  motion, and then reappears 20 px lower. A click that lands on the tooltip before the next drain hides it is lost.
  On mac, native tooltips ignore mouse events, so the pointer passes through them; Windows not checked.
  Reproduced in Docker on Sand CLAP: `evidence/t019/review/tooltip-overlap/` (steps
  `review/tooltip-overlap.txt`). `o01` shows the tooltip; `o02` (pointer 34 px lower, same knob) has none; `o03`
  still none 1.5 s later; `o05` shows it again after a 1 px move.
  *Fix:* make the window input-transparent with an empty input shape: `XShapeCombineRectangles(…, ShapeInput, …, 0
  rects, ShapeSet, …)` via `dlopen("libXext.so.6")`, or `XFixesSetWindowShapeRegion` via libXfixes. Either keeps
  NEEDED unchanged. If neither library is available, place the tooltip below the control's rect instead of below the
  pointer, or treat a LeaveNotify whose root position lies inside the tooltip as "still inside": hide the tooltip,
  but skip `OnMouseOut` and the index reset. Add the overlap steps to `sand-tooltip.txt`.
- **R2 — minor — `IGraphicsLinuxDrop.h:69-74` (`Release`).** When the editor closes while a drop's data is still being
  read (between `XConvertSelection` and SelectionNotify/INCR end), the source never gets `XdndFinished`. GTK then
  waits for its own timeout, with the drag icon stuck until then. *Fix:* in `Release()`, call
  `if (mDisplay && mDrag.source) SendFinished(false);` before clearing (the display is still open there).
- **R3 — nit — Log, "Timing as Windows … once shown it stays while the pointer is on that control".** On Windows,
  iPlug's tooltip control keeps the default `TTDT_AUTOPOP`, so it hides by itself after about 5 s (10 × the
  double-click time). Linux never auto-hides. Either add an auto-pop or reword the Log ("stays until the pointer
  leaves the control, like mac").
- **R4 — nit — `IGraphicsLinuxDrop.h:170-201`.** A browser link drag offers `text/uri-list` with `http://` URIs. It is
  accepted over the whole editor (copy cursor) and then refused at the drop. mac/win accept file drags only.
  Acceptable for now. Optionally request the data on the first `XdndPosition` (XDND lets the target convert before
  the drop) and accept only when a local path parses.
- **R5 — nit — comments.** `IGraphicsLinux.cpp:1830` ("Below the pointer, or above it where the screen ends")
  restates the next lines. `:1880` ("With the editor's GL context, made current on the tooltip's window…") repeats
  what the `mVisual` member comment already says ("say it once"). `DrawTooltip` calls `DisableSwapWait` on every draw
  (`:1892`), which queries the GLX extension string each time; once per window is enough.
- **R6 — info for T-013 (no code defect).** With a source that lists more than three types and puts `text/uri-list`
  after the third (`XdndTypeList`, bit 0 of `XdndEnter`), the drop is refused through REAPER's forwarding: `top`
  answers accept=0 on the second position. The same source addressed to the editor directly (`deep`) is accepted, so
  `SourceListsType` works. REAPER evidently forwards only the first three types; GNOME Files lists `text/uri-list`
  third, so it is not affected. Record it among the host notes.
- **R7 — nit — `linux-research/notes/22-feature-parity.md:15,18`** still say "no XDND" / "no-op" for T-019's rows;
  update them at merge.

Checked and found correct (no finding):
- Tooltip lifetime: the window is destroyed in `DestroyWindowResources`, and `w = Impl()` resets all tooltip state.
- GL context switching: the context is made current on the tooltip, then back on `mWindow`, and
  `DeactivateGLContext` restores the host's context. `IGraphicsNanoVG::BeginFrame` resets the viewport.
- Placement and clamping, and the screen scale vs the draw scale.
- XDND message filtering by window and source, the `XdndStatus` flags (bit 1 with an empty rectangle), the drop
  time stamp, and the `XdndFinished` v5 fields.
- `XdndFinished` is sent before the callback. A callback that closes or deletes the editor touches no member
  afterwards: `Deliver` and `HandleEvent` return straight away, and `ProcessXEvents` checks its own `open` copy.
- URI parsing and `%XX` decoding.

Re-run by me in Docker (container `t019r`, builds `build/t019r*`, evidence `evidence/t019/review/`):
- Builds PASS: IPlugControls-clap, IGraphicsTest-clap/vst3, Sand clap+vst3 (release). NEEDED is libX11, libm, libc
  and ld-linux only (`review/needed.txt`).
- `igraphicstest-drop.txt` CLAP: **PASS**. One UTF-8 file, three files, leave, the relay over the FX header, deep.
  I looked at `d01` (`/tmp/dröp/öne.wav` decoded) and `d02`.
- `sand.txt` Sand VST3: **PASS**, including the tooltip check. I looked at `s09` (the tooltip below the pointer).
- `iplugcontrols-p3b.txt` IPlugControls CLAP: **PASS**. I looked at `c04` (popup menu).
- Paths the Log did not cover, with a probe variant (`review/xdnd-source-x.c`, `XDND_MODE=incr|typelist|nofiles|silent`)
  on IGraphicsTest CLAP (`review/drop-probes/probes.txt`, `p-*.png`):
  - INCR transfer in 16-byte chunks, both via REAPER and directly: accepted. The three paths, including one with a
    space, are shown in `p-incr`.
  - More than three types, directly: accepted.
  - No `text/uri-list`: refused.
  - A source that never answers the selection request: `XdndFinished(accepted=0)` after the 2 s timeout, and the
    previous drop result stays.
  - REAPER stayed alive throughout.
- Author's evidence, opened by me: `sand-vst3/s09-tooltip`, `tooltip-clap-x2/t01-tooltip` (crisp at 2×),
  `drop-clap/d02-three-files`, `gneiss-drop-clap/u00`→`u01` (the USER layout changed), `vm/final-vst3-editor/after`
  (layout loaded from Files). The four `vm/final-*` RESULT files and Nautilus logs each end with `XdndFinished`.
  `vm/proxy-design/relay-gneiss-gt019/nautilus-dnd.txt` shows the proxy followed for the first enter (async callback
  to `#2c00002`) and then no `XdndFinished`, while `relay-gneiss-gbase` and the final build do finish. The
  proxy-removal reasoning is supported.
- Not re-run: churn ×100, the VM, Gneiss.
- Cleanup: container removed. Removing `build/t019r` and `build/t019r-sand` (git-ignored) was refused by the
  permission system; delete them by hand.

**Verdict: accept with fixes.** Fix R1 (major) and R2. R3–R7 are optional. Everything else matches the Log, and the
key tests pass on re-run.

### Review round 1 fixes (author, 2026-10-18)
- **R1 fixed:** the tooltip window gets an empty input shape (X Shape 1.1, `XShapeCombineRectangles(ShapeInput)`;
  libXext dlopen'ed like libXcursor, NEEDED unchanged), so the pointer passes through it as on mac. Without libXext
  or the extension the tooltip hides as soon as the pointer moves on (`mTooltipTakesPointer`). The reviewer's case
  is now in `sand-tooltip.txt` (move down the same knob onto the tooltip: it stays): CLAP at scale 1 and 2 PASS.
- **R2 fixed:** `LinuxFileDrop::Release()` sends XdndFinished (not accepted) when the editor closes during a read.
- **R3:** Log reworded (no auto-hide; Windows has one, mac does not). Kept as is: mac-like.
- **R4 kept:** the copy cursor for a link drag over the editor: the URI list is only read on the drop; mac accepts
  by pasteboard type alike. Noted.
- **R5 fixed:** the two comments removed; `DisableSwapWait` once per tooltip window.
- **R6:** for T-013 (REAPER forwards only the first three types of a source; GNOME Files lists text/uri-list third).
- **R7:** `notes/22` is updated by the coordinator at the merge.

### Review round 2 (independent reviewer, 2026-10-18)
Checked the diff `f987ca7a1..91eb15c82`: code changes only in `IGraphicsLinux.cpp` and `IGraphicsLinuxDrop.h`.
- **R1 is fixed.** `MakeInputTransparent` dlopens `libXext.so.6`, the same way libXcursor, fontconfig and libGL
  are loaded, and sets an empty `ShapeInput` region. The constants are right (ShapeInput 2, ShapeSet 0,
  Unsorted 0), and the call runs inside `ShowTooltip`'s X error trap. The fallback `mTooltipTakesPointer` hides the
  tooltip once the pointer moves more than 4 px; it re-arms only on another control, consistent with the click rule.
  In Docker, my round-1 overlap steps (`review/tooltip-overlap.txt`) on Sand CLAP:
  - Before the fix: the tooltip disappeared, the hover dropped, and it came back only after another move.
  - Now the tooltip stays where it was in `o02`, `o03` and `o05`, and the spurious mouse-out is gone. Screenshots
    looked at: `evidence/t019/review2/tooltip-overlap/`.
- **R2 is fixed.** `Release()` sends `XdndFinished(false)` while a read is in progress, before the display closes.
  `SendFinished` still guards against a missing source.
- **R3 and R5 are fixed**, and the reasons given for keeping R4 and for R6/R7 are fine.

Re-run in Docker (container `t019r`):
- Sand clap+vst3 release build: PASS.
- `review/tooltip-overlap.txt` on CLAP: PASS. The tooltip stays while the pointer passes over it.
- `scripts/steps/sand-tooltip.txt` on CLAP: PASS, including the new `t01b` step. I looked at `t01b`: the tooltip is
  still shown with the pointer on it. The close/reopen ×5 loop and the second instance also ran; REAPER stayed alive.
- NEEDED of `Sand.clap`: libX11, libm, libc and ld-linux only (`review2/needed.txt`).

New findings (nits only, no fix required):
- **N1 — nit — `IGraphicsLinux.cpp` `MakeInputTransparent`.** It checks only `XShapeQueryExtension`, not
  `XShapeQueryVersion` ≥ 1.1. A Shape 1.0-only server would reject `ShapeInput` with an error (trapped), yet the
  function returns true, so the fallback would not engage. Shape 1.1 dates from 2006, so this is theoretical.
- **N2 — nit.** The fallback path (no libXext, or no Shape extension) was not exercised. Xvfb, like GNOME, has
  Shape. It reads correct.

**Final verdict: accept.**

### Hand-over (author, 2026-10-18)
- Commits `git log hvoya-linux..linux/T-019`: 28ae356c8 tooltips, c35d5f415 XDND (first design), e6dbc1fb0 steps,
  98a6101f4 XDND redesign (no proxy), 0a96072ba VM evidence, 91eb15c82 review fixes, reviews f987ca7a1 / 1d5aa1a16.
- Code: `IGraphics/Platforms/IGraphicsLinux.{h,cpp}`, new `IGraphicsLinuxDrop.h`; no shared file; NEEDED unchanged.
- Open: per-host drop support (hosts that neither forward nor are XdndAware get no drops; REAPER's 3-type limit) →
  T-013. Review nits N1 (Shape version not checked, 1.1 is from 2006) and N2 (fallback path not exercised) kept.
- Evidence `evidence/t019/` (screenshots local only). VM lock released; containers `t019`/`t019r` to be removed at merge.

