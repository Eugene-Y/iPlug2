# P3b: keyboard, focus, text entry, cursors, clipboard, menus and fonts on X11 (2026-10-12)

Commits `7bed217fd..` on `hvoya-linux`. Evidence: `evidence/p3b/`. **[R]** = Docker (REAPER 7.82 aarch64, Xvfb,
openbox, llvmpipe, container `p3b`); **[U]** = unit test against Xvfb (`Tests/LinuxInput`).

## What was built

Linux-only: `IGraphicsLinux.{h,cpp}` plus four new headers. One shared fix came with the review (H1):
`ITextEntryControl`'s right-click menu is a member instead of a function-local static (platform-neutral; mac
IPlugEffect CLAP Debug builds, `evidence/p3b/review-fixes/macos/`, diff against `hvoya` in `review-fixes/`).

| Topic | Choice |
|---|---|
| Keys | `IGraphicsLinuxKeys.h`. Keysym → `kVK_*` with Windows semantics (what `ITextEntryControl` and the keyboard controls test): keypad by NumLock state, F1–F24, modifiers. Letters and digits get the VK of the **physical** key, searched across XKB groups: Ctrl+C is `'C'` in a Russian layout too, Shift+1 is `'1'`. Text: `Xutf8LookupString` through Xlib's built-in input method (compose/dead keys, any keysym → UTF-8), else Latin-1/Unicode keysyms; with Ctrl/Alt the unmodified character (as mac's `charactersIgnoringModifiers`), control characters as the VK only; one `IKeyPress` per character an IM commits. `XkbSetDetectableAutoRepeat` (per connection). Locale and `XMODIFIERS` are not touched: process-wide, the host's |
| Routing | `KeyPress`/`KeyRelease` → `IGraphics::OnKeyDown/OnKeyUp` at the pointer position. Keys pressed during an edit or an open menu are always kept (Escape/Return must not reach the host). Unused real keys are sent on to the parent window (`XSendEvent`); their releases follow their presses (a 256-bit kept-keys set). Synthetic keys (sent by a host) are never sent back: loops |
| Focus | Plug-ins: the host keeps the X focus outside text entry. During an edit the editor takes it (`XSetInputFocus`, after the opening click has settled, see finding 2), restores the previous focus on commit/Escape, and ends the edit (commit) when the focus leaves for another top-level window. APP: focus on click (the editor is the app's content) |
| XEmbed | `_XEMBED`: `EMBEDDED_NOTIFY` remembers the embedder, which then also gets `REQUEST_FOCUS` when an edit takes the focus; `FOCUS_OUT` ends an edit. `WINDOW_ACTIVATE` etc. ignored. Nothing depends on XEmbed (REAPER sends none) |
| Text entry | `CreatePlatformTextEntry` attaches IGraphics' `ITextEntryControl` for this one edit ("stand-in") and returns at once; the control commits later (Return, a click outside, focus loss) and is removed after the edit and after any menu has collapsed, with mouse-over/capture cleared first. A stand-in the plug-in removed itself (`RemoveAllControls` in a layout, `Remove*Control`) is only forgotten; an edit whose control vanished is cleared (`ClearInTextEntryControl`), or every key would count as an edit's. No GTK. `ForceEndUserEdit` dismisses. Ctrl+V in an edit reads the clipboard **asynchronously** and replays the key (finding 4); further Ctrl+V while one is pending are dropped |
| Menus | `CreatePlatformPopupMenu` attaches `IPopupMenuControl` likewise (`isAsync = true`), removed once collapsed. An open menu (stand-in or the plug-in's own) takes the keyboard focus like an edit, so Escape reaches it: dismissed without a choice. Closing the editor with a menu open completes it first (no choice), so the gesture its owner began on the host is ended. A focus loss to another window dismisses an expanded menu, then commits an edit |
| Cursors | `IGraphicsLinuxCursors.h`: Xcursor theme by CSS name, then legacy names (`XcursorLibraryLoadCursor`, libXcursor `dlopen`ed), else the X cursor font; cached per editor. `HideMouseCursor(hide, lock)`: blank pixmap cursor; with lock, each drag step warps back to where the drag began (not on the warp's own motion event, which would loop) and the release puts the pointer there, as on Windows |
| Clipboard | `IGraphicsLinuxClipboard.h`: CLIPBOARD owner with `UTF8_STRING`/`TEXT`/`STRING`(ASCII)/`TARGETS`, served from the event loop. Text owned by any editor of the process is read directly (a blocking read would starve the other editor's connection). When the owning editor closes, another open editor of the process takes ownership; with none, the text stays readable in the process ("orphaned") until something else owns CLIPBOARD (Gneiss: copy a rig in A, close A, paste in B). Reading: `XConvertSelection` and a bounded wait (200 ms) on our own Display, taking only its own event (`XCheckIfEvent`); INCR received (blocking and async); serving refuses texts over one request (no INCR sending) |
| Fonts | `IGraphicsLinuxFontconfig.h`: fontconfig `dlopen`ed; `FcFontSort` for family/weight/slant; TrueType-outline candidates in order, variable-font named instances skipped (stb cannot select them), and the first face `IFontData` accepts wins. A missing family gets the system's substitute, like Windows |
| Tooltips | `UpdateTooltips` stays a no-op (no native X tooltips; an IGraphics-drawn one is left for later) |
| Trace | `-DIGRAPHICS_TRACE_LINUX_INPUT`: every key (handled / kept / to host / dropped), FocusIn/Out, focus take/release on stderr. For probing hosts in P10 |

NEEDED is unchanged: `libX11.so.6` + libc/libm (`inspect.txt`); GLIBC floor 2.34 on 22.04 (`cmake-22.04.log`).

## Findings

1. **REAPER forwards keys itself and keeps some.** Its X focus sits on GDK's 1×1 focus proxy of the FX window, which
   is not an ancestor of our window, so X never delivers keys to the editor by pointer position. REAPER sends the
   keys it does not use to our window with `XSendEvent` (every key event in the trace is `send_event`), and calls
   no VST3 `onKeyDown` at all (gdb). Space and **Escape it keeps**: Escape closes the FX window. So outside text
   entry a plug-in in REAPER sees keys REAPER lets through, and never Escape (the about box's Escape is tested in
   the APP instead).
2. **Focus during an edit, and the click that opened it.** Taking the focus inside the double-click lost it again
   within milliseconds (openbox's click-to-focus, then GDK moving it to its proxy: `FocusOut NotifyAncestor`), so
   Escape went to REAPER and closed the window in ≈1 of 3 runs. Now the focus is requested ≥150 ms after the
   button release, with up to two retries when it is moved to an ancestor within 500 ms. 4/4 and every later full
   run kept the focus for the whole edit (`focuswatch`, trace build). Once we hold it, REAPER/GDK leave it alone
   (`xdotool windowfocus` test: stays).
3. **Synthetic keys: REAPER takes them, GDK3 apps do not.** `xdotool key --window <container> space` starts
   REAPER's transport; the same to the APP's SWELL window (system GDK3) is ignored, while a real Escape closes the
   APP's dialog. So forwarding unused keys reaches the host in some hosts only. In the APP it costs SWELL's Escape
   only: SWELL's menu shortcuts (Ctrl+F etc.) do nothing on Linux even with SWELL focused (P8).
4. **The host's own clipboard cannot be read with a blocking wait.** Text copied in REAPER (track name field) is
   served by REAPER's main thread — the thread our wait blocks. Measured: paste failed. Ctrl+V in an edit now
   starts an async read and replays the key when the text is in; `host-clipboard-probe.sh` pastes REAPER's "77"
   in CLAP and VST3 [R]. Other callers of `GetTextFromClipboard` (Gneiss's rig paste, the text entry's
   right-click Paste) still block: fine for other processes and other editors of ours, empty for host-owned text.
5. **REAPER's VST3 context menu shows with a held right click** (corrected by T-008; this first read "does not
   show"). Right click on a parameter control reaches `IComponentHandler3::createContextMenu`, and REAPER's
   `IContextMenu::popup` opens a GTK menu in a **nested modal loop**: it returns when an item is chosen, and the
   editor gets no `Draw` meanwhile. REAPER runs its own actions (close/delete FX) only after the menu has closed
   and our `OnMouseDown` has returned, so no close-during-popup guard is needed for REAPER. The first test used
   `xdotool click 3`, whose release lands in the same millisecond as the press; GTK takes that release as a
   choice outside the menu and closes it at once (`popup` returned after ≈0.1 s). With the button held 150 ms
   the menu shows and "Show track envelope" creates the envelope, in Docker (`evidence/t008/ctx-menu-docker-vst3/`)
   and on the VM (notes/14 part 2, row 8). Re-check grabs/nesting in other hosts (P10).
6. **NanoVG cannot draw CFF fonts.** Its stb_truetype is v1.09; fontconfig maps "Times New Roman" to Nimbus Roman
   (`.otf`, CFF) here, and `LoadAPIFont` refused it, so IGraphicsTest fell back to its bundled font. Fonts by name
   now take the best TrueType candidate (DejaVu Serif here), `fonts-by-name-igraphicstest.txt` [R].
7. **INCR.** The owner's write of the INCR property leaves a `NewValue` event before the first chunk; taken as a
   chunk it read as "empty = end". Such events find the property deleted and are skipped now. A blocking wait also
   must not drop other `PropertyNotify`s and then sleep in `poll` while its event sits in Xlib's queue (fixed with
   `XCheckIfEvent`). 3 MB from xclip arrives both ways [U].
8. **Menus stay inside the editor.** `IPopupMenuControl::Expand` clamps panels to the IGraphics bounds (above or
   below the anchor, scrolling when too long), unlike native menus. A menu near the bottom of a small editor opens
   upwards or scrolls; nothing is clipped by the window edge. Gneiss's platform menus become these.

## Acceptance

| # | Check | Result |
|---|---|---|
| 1 | Full IPlugControls script `steps/iplugcontrols-p3b.txt`, CLAP + VST3: theme cursors (corner resizer `nwse-resize`), knob drag with hidden cursor (`blank`) and lock (pointer back at the start), XY pad, keyboard, wheel; IGraphics popup and caption menu choose items (param = three, two); text entry: "42"+Return commits, "13"+Escape cancels and the FX window stays; Ctrl+V from xclip (55), Ctrl+C read by xclip **and** xsel, right-click menu Paste (66); Space over the editor still toggles the transport; OS Text Entry + OS Menu toggles → stand-in menu (four) and text entry ("7" commits, "99"+Escape cancels); right click; colour/font buttons (P4 stubs); about box; reopen ×5; 2 instances | PASS ×2 [R] (`reaper-controls-{clap,vst3}/RESULT.txt`, `sheet.png`) |
| 2 | APP (`steps/iplugcontrols-app-p3b.txt`): cursors, locked drag, text entry commit/cancel (APP alive after Escape), Ctrl+V/Ctrl+C with xclip/xsel, stand-in menu and text entry, the about box gets Escape | PASS [R] (`app-controls/`) |
| 3 | Host-owned clipboard → Ctrl+V | PASS CLAP + VST3 [R] (`host-clipboard-*`) |
| 4 | New paths on the main thread: `HandleKey`, focus take/release, clipboard get/set, stand-in creation, cursor set/hide/move, `ProcessXEvents`, gdb during the full scripts | all tid 1: CLAP, VST3, APP [R] (`threads-*`) |
| 5 | 100× open/close, 2 instances; plain and `EDIT=1` (an edit holding the focus at every close, every other one also owning CLIPBOARD) | fds 6/10, sockets 1/3, threads 14/40 equal before/after, PASS ×4 [R] (`churn-*`) |
| 6 | Unit test: key tables, events with/without IM, Cyrillic group (`setxkbmap us,ru`), fontconfig, clipboard serve/read/INCR | PASS [U] (`input-test.txt`) |
| 7 | Validators | identical to P3a: clap-validator Controls 33/3, Effect 35/3; VST3 Controls 537/537, Effect 536/537 (`validators/`) |
| 8 | macOS | IPlugEffect CLAP Debug BUILD SUCCEEDED with the shared `ITextEntryControl` fix (`review-fixes/macos/`), nothing installed into `~/Library`, no `.rc_mac` changed [M] |

## Harness

`xact.sh`: `expect-same`, `expect-param` (REAPER formatted value), `expect-pointer`, `expect-cursor`
(`cursor-probe.c`, XFixes), `drag-check`, `move`, `lua-expect`, `expect-window`, `set-clipboard`,
`expect-clipboard` (xclip + xsel); failures go to `xact-fails.txt`. New: `steps/iplugcontrols-p3b.txt`,
`steps/iplugcontrols-app-p3b.txt`, `input-thread-check.sh`, `app-input-thread-check.sh`, `host-clipboard-probe.sh`,
`editor-churn-test.sh EDIT=1`. Docker image: `xclip xsel` in a separate last layer.

## Known issues / left for later

- Outside an edit, keys reach a plug-in only where the host delivers them (REAPER: what it forwards; no Escape).
  Hosts where the pointer-over path applies get them directly; unused ones are forwarded, which some toolkits
  ignore (finding 3). Probe Bitwig, Carla, Ardour with the trace build (P10).
- While an edit is open in REAPER and the user clicks into REAPER's own window, the focus may stay with the edit
  (no FocusOut leaves the host); Return/Escape/a click in the editor end it.
- Blocking clipboard reads of host-owned text return nothing (finding 4); serving large texts (INCR) is not done.
- No tooltips. Host context menus: see finding 5 (REAPER's blocks the editor's drawing while open).
- Fonts by name: TrueType outlines only until NanoVG's stb_truetype is updated. A variable font draws at its default
  instance only (stb_truetype applies no variation axes), so Bold/Italic of a variable-only family come out regular
  (Ubuntu Sans and Ubuntu on Ubuntu 24.04, notes/14 P3b-V4). Won't fix (T-008): fontconfig already ranks a static
  face of the same family with the requested style ahead of the variable file when one is installed (measured with a
  fixture, `evidence/t008/vm/fc-variable*/`), and taking another family's static bold instead would change the look
  more than the missing weight does. Synthetic bold would need NanoVG changes.
- Focus retries: after two retries `mHoldsFocus` means "requested", not "held"; keys then come only as the host
  forwards them. Alternatives in notes/15.
- A menu requested while a stand-in menu is still fading out (≈400 ms after a choice) is ignored by
  `IPopupMenuControl::CreatePopupMenu` (it expands only when collapsed); its owner keeps `mInPopupMenu` and the
  host gesture begun on its mouse down until the next menu. Same as a plug-in's own popup control on mac/win.
- No SAVE_TARGETS hand-over to a clipboard manager: an orphaned text is readable in this process only.

## Review round 1 (notes/15)

| # | Fix |
|---|---|
| H1 | `ITextEntryControl`'s right-click menu was a function-local static whose callback kept the first control's `this`: Cut/Copy/Paste after that control died (every stand-in edit after the first, any editor reopen, a 2nd instance, on every platform) used freed memory. Now a member built with `[this]` (shared, platform-neutral). Stand-in text entries stay while any menu is open (the menu refers to them). The P3b scripts paste through the stand-in's right-click menu twice |
| H2 | Stand-in pointers are checked against `GetTextEntryControl()`/`GetPopupMenuControl()` before any use, and forgotten when the plug-in removed them; a vanished edit is cleared |
| M1 | `CloseWindow` (once, not from the destructor) completes an open menu: expanded → as a click outside, still fading in → `SetControlValueAfterPopupMenu(nullptr)` |
| M2 | Clipboard hand-over / orphaned text, unit-tested |
| M3 | Variable-font instances skipped; the caller validates each face and falls through |
| L1 | Ctrl+V while an async read is pending is dropped |
| S3 | New: focus loss while the stand-in edit's menu is expanded (`app-focus-loss-menu.sh`, ASan APP): the menu is dismissed, then the edit commits; no report |
