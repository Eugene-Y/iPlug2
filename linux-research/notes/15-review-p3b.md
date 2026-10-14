# Review of P3b: keyboard, focus, text entry, cursors, clipboard, menus, fonts (2026-10-12 to 10-14)

Range reviewed: `7bed217fd..f6b57f595` (notes/13). Fix round 1: `8f9617ca2`, `971a73599`, `0142d81e8`, `98ad30601`,
`20a8b0681`, `b8504b006`. Reviewer evidence: `evidence/p3b-review/`.

Sources:
- [R]: re-run in Docker (REAPER 7.82 aarch64, Xvfb, openbox, llvmpipe, container `p3brev`);
- [A]: Debug+ASan IPlugControls APP under Xvfb (`build-asan.sh`), driven by the reviewer's repro scripts;
- [U]: `Tests/LinuxInput` (ASan/UBSan) against Xvfb, layouts `us` and `us,ru`;
- [C]: code reading; [M]: macOS build.

## Re-check of the implementer's results (at f6b57f595, before the fixes)

| Check | Result |
|---|---|
| Full `iplugcontrols-p3b.txt`, CLAP | PASS; all `Draw`/`ProcessXEvents` hits on tid 1; contact sheet matches notes/13 (`reaper-controls-clap/`) [R] |
| Same, VST3 | PASS with the f6b57f595 step file (`reaper-controls-vst3/`). A run that picked up the implementer's newer step file mid-way (stand-in right-click Paste) **crashed REAPER**: H1 (`reaper-controls-vst3-h1-crash/gdb-crash.txt`) [R] |
| APP script | PASS, no failed checks (`app-controls/`) [R] |
| 100× churn, 2 instances, `EDIT=1` | CLAP and VST3 PASS: fds 6/10, sockets 1/3, threads 14/40 before = after (`churn-edit-{clap,vst3}/`) [R] |
| `Tests/LinuxInput` under ASan | PASS in `us` and `us,ru` (`input-test-asan.txt`) [U] |
| Shared files | At f6b57f595, true: only Linux files changed. After round 1, `ITextEntryControl.{h,cpp}` changed (H1); see the mac row below |

The churn's `EDIT=1` and the REAPER script's right-click Paste both ran with IPlugControls' **own** `ITextEntryControl`
attached. So neither exercised a stand-in after its first use. That is why H1 and H2 slipped through.

## Findings

| # | Sev. | Finding | Status |
|---|---|---|---|
| H1 | **High** | **Use-after-free in the text entry's right-click menu.** `ITextEntryControl::OnMouseDown` built its Cut/Copy/Paste menu as a function-local `static IPopupMenu`. Its `[&]` callback captured the `this` of the first control ever right-clicked. Linux stand-ins are destroyed after each edit, so the second edit's Paste called into a freed control. It also bites on every platform after an editor reopen (IGraphics dies with the editor) and with a second instance. Release VST3 in REAPER: SIGSEGV in the lambda under `CollapseEverything`. ASan APP: heap-use-after-free in `Paste` > `GetUI` (`app-s1-asan/`). | Fixed 8f9617ca2 (shared): the menu is a member built with `[this]`. Platform-neutral; mac CLAP Debug builds (`macos/mac-CLAP.txt`). S1 re-run: values 66 then 77 pasted, no report (`app-s1-asan-r1/`, `r2/app-s1-asan/`) [A][M] |
| H2 | **High** (Gneiss) | **Dangling stand-in pointers.** `mStandInTextEntry`/`mStandInPopup` were raw pointers into IGraphics' `unique_ptr`s, dereferenced in `RemoveFinishedStandIns`. Gneiss's layout calls `RemoveAllControls()` (`GneissUI.cpp:153`) from `OnIdle` after a morph or preset change. Choosing such an item from a menu re-lays out while the stand-in popup is still collapsing (≈ 400 ms). ASan: heap-use-after-free in `IPopupMenuControl::GetState` < `RemoveFinishedStandIns` (`app-s2-asan/`, `app-s2.sh`: the plug-in removes the popup control during the collapse). A removal mid-edit also left `mInTextEntry` set with no text entry: every key was kept and the X focus held until the next edit. | Fixed 971a73599: stand-ins are compared with `GetTextEntryControl()`/`GetPopupMenuControl()` before use; a stale `mInTextEntry` is cleared. S2: no report, APP alive [A] |
| H2b | High (introduced by the first H1 draft) | With the menu a member of the text entry control, a focus loss during an edit with its right-click menu open would commit the edit. The stand-in text entry, and with it the open menu's `IPopupMenu`, would then be deleted under the expanded popup. Pre-fix baseline of this sequence: UAF via the old static (`app-s3-asan-head/`). | Fixed 971a73599 + 20a8b0681: a stand-in text entry is not removed while any popup is open; a focus loss first dismisses an expanded menu, then commits. S3 (`app-s3.sh`, xmessage takes the focus): no report, edit committed, the next edit works (`app-s3-asan-r1/sheet.png`) [A] |
| M1 | Medium | **Editor closed with a menu open left a host gesture open.** The mouse-down sends `Begin…FromUI`; `DoCreatePopupMenu` releases the capture, so only the menu's completion sends `End`. On close it never came. In REAPER this is the common path: Escape with a menu open closes the FX window. gdb, ICaptionControl menu open, FX window closed: Begin 1 / End 0 before, Begin 1 / End 1 after (`m1-gesture-rev-p3b*.txt`, `m1-gesture.sh`). | Fixed 971a73599: `CloseWindow` dismisses an open menu once (any popup control, not only stand-ins). Menus now also take the focus like edits, so Escape reaches the menu [R] |
| M2 | Medium | **The clipboard died with the owning editor.** Gneiss workflow: copy rig, close A's editor, open B, paste → nothing (without a clipboard manager). | Fixed 0142d81e8: on close, another open editor of the process takes ownership over; without one the text stays readable in-process as "orphaned" until another owner appears. Unit-tested [U]. SAVE_TARGETS hand-over to a clipboard manager stays open (other apps still lose it once the last editor closes) |
| M3 | Low-Med | **Fonts: named instances of variable fonts.** fontconfig's `index` carries the instance number above bit 16. It went to stb_truetype as a face index, so the face was invalid, and `FindSystemFont` had already returned that candidate. | Fixed 98ad30601: instances skipped; the caller validates each face and falls through to the next. [C] only: the Docker image has no variable fonts (`fc-list :variable=true` is empty) |
| L1 | Low | A second Ctrl+V while the async read was pending (key repeat) fell through to the blocking read on the same property. The two `SelectionNotify` replies were then consumed crosswise (empty paste). | Fixed 971a73599: kept while one is pending [C] |
| L2 | Low | A menu requested while the stand-in popup is still collapsing is ignored by `IPopupMenuControl::CreatePopupMenu`, which expands only from `kCollapsed`. Its owner's gesture and `mInPopupMenu` are left waiting. Same for any plug-in-attached popup control (upstream). | Documented in notes/13 Known issues |
| I1 | Info | `IVMenuButtonControl` (a container) sends `End…FromUI` without a `Begin`: the click lands on its child button. Upstream, all platforms; seen in the M1 probe. | No action |

## Focus heuristic (task point 2)

**Mechanism.**
- The delayed take is not a separate timer. `UpdateEditingState` runs after every X event and every frame tick, and the request goes out on the first one ≥ 150 ms after the button release.
- So nothing needs cancelling on close: the frame timer and fd watch are removed in `CloseWindow`.
- Retries (2, within 500 ms) only answer `FocusOut NotifyAncestor/NotifyVirtual`. After that, `mHoldsFocus` means "requested", not "held" (now in notes/13).
- Focus is returned on commit, Escape and close (`ReleaseKeyboardFocus` in `UpdateEditingState` and `DestroyWindowResources`). If someone else moved it meanwhile, it is left alone.
- Round 1 adds `mFocusDeclined`: after a non-linear focus loss, the edit or menu does not take the focus back.

**Judgement.** Sound enough for v1, but fragile in principle.
- It encodes openbox + GDK timing in REAPER.
- A slower WM, or a GDK proxy move later than 500 ms, leaves the edit without keys in hosts that keep some keys (REAPER keeps Escape and closes the FX window). That is bounded and degrades to "the edit commits or closes", never to a hang or a focus fight: at most 3 requests per edit.

**Better options for P10, in order of cost:**
1. **Timestamped request.** Call `XSetInputFocus` with the ButtonRelease's server time instead of `CurrentTime`. The X server ignores focus requests older than the last focus change. The WM's click-to-focus and GDK's proxy move (WM_TAKE_FOCUS) carry the press time, so if they arrive after ours they become no-ops. That would remove the delay altogether.
   - Risk: requests made with `CurrentTime` still win.
   - Test on openbox/REAPER first, then on mutter/kwin.
2. **Watch the host's top-level** (`FocusChangeMask` selected on our own connection). Take the focus once its `FocusIn` and the following `FocusOut NotifyInferior` (the proxy move) have arrived, instead of after a fixed delay.
3. Keep the delay as a fallback when neither event sequence is seen.

## Other observations

**Keys** [C][U]
- The keysym → `kVK` table covers every `kVK_*` iPlug defines: F1–F24, keypad by NumLock state, navigation, modifiers, Super → `LWIN`. iPlug has no OEM/Apps VKs, so punctuation travels as text only, as on mac.
- Physical-key VKs across XKB groups verified with `us,ru`.
- The 64-byte IM buffer drops overflow commits; acceptable.

**Forwarding** [C]
- Only real (non-`send_event`) unused keys are forwarded, to the parent with propagation. A host that forwards back sends synthetic events, which are never re-forwarded, so there is no loop.
- Releases follow their presses (kept-key bitset).
- Worst case is one extra `OnKeyDown` for an unused key that comes back from the host.

**Clipboard** [C][U]
- Serving: TARGETS, UTF8_STRING, TEXT, STRING (ASCII only); MULTIPLE and TIMESTAMP are refused. ICCCM asks for TIMESTAMP, but nothing in practice needs it.
- No INCR serving; large texts are refused.
- The blocking read cannot deadlock: the wait is bounded at 200 ms, and owners inside this .so are read directly. It does stall the host thread 200 ms and returns empty for host-owned text, and for text owned by another plug-in served on the host thread.
- The async Ctrl+V path is correct; its INCR handling is covered by the unit test.

**Cursors** [R][C]
- Xcursor (`dlopen`ed once, never closed), with font-cursor fallback; cursors and the blank cursor are cached per connection and freed before `XCloseDisplay`.
- The locked drag warps back on every step with a delta, so the warp's own motion cannot loop. Release restores the pointer.
- Leaving the window during a locked drag is harmless: the implicit grab keeps motion and release coming, and the grab window's blank cursor stays.
- Coalesced motion that was already queued at the time of a warp can overstate one step; the same is true on Windows.

**Fontconfig** [C][U]
- `dlopen` failure means fonts by name are not found, and the plug-in still loads.
- Patterns and font sets are destroyed; the default `FcConfig` is process-wide by design (LSan suppression in the test only).
- No result caching is needed: IGraphics loads each font ID once.

**Threading** [R]
- All new paths are on tid 1 in the CLAP/VST3 runs (`draw-threads.txt`).

**Code style** [C]
- 2-space indentation, `m`/`p` prefixes; WDL containers in the new code (`WDL_PtrList`, `WDL_String`, `WDL_TypedBuf`). `std::` is limited to `chrono`/`function`/smart pointers in the platform file, as P3a was accepted.
- Comments are WHY-only (X/ICCCM quirks, host findings, lifetime rules). The one stale comment about the menu's Escape handling was replaced in round 1.
- Linux code lives in new headers. The single shared change (H1) is a genuine cross-platform bug fix.

## Round 1 results (`r1/` at 70fc65719, `r2/` at b8504b006, `final/` at e21ebc5f1)

| Check | Result |
|---|---|
| Full `iplugcontrols-p3b.txt` (now with two stand-in right-click Pastes), CLAP + VST3 | PASS ×2 at r1 and r2 [R] |
| APP script (same additions) | PASS at r1; PASS under ASan at r2 with no report [R][A] |
| S1 / S2 / S3 under ASan | no report at r1 and r2 [A] |
| M1 gesture probe | Begin 1 / End 1 [R] |
| 100× churn `EDIT=1`, CLAP + VST3 | PASS ×2, fds/sockets/threads equal before/after [R] |
| `Tests/LinuxInput` ASan, `us` + `us,ru`, incl. clipboard hand-over/orphan | PASS [U] |
| mac IPlugEffect CLAP Debug with the changed `ITextEntryControl` | BUILD SUCCEEDED, scratchpad build dirs, no tracked file changed (`macos/mac-CLAP.txt`) [M] |
| **Final re-verification**, fresh build dirs (`build/final-p3b*`), code unchanged since b8504b006 (`final-verify.sh`, `final/final-verify.log`) | S1/S2/S3 ASan: no report. Full scripts CLAP + VST3: PASS, all `Draw`/`ProcessXEvents` on tid 1. APP script: no failed check. `EDIT=1` churn ×100 CLAP + VST3: PASS (fds 6/10, sockets 1/3, threads 14/40 before = after). `Tests/LinuxInput` ASan `us` + `us,ru`: PASS. This run regenerated `evidence/p3b/` (84dc38c5e): the implementer's own re-verification had hung on dead containers, leaving partial files [R][A][U] |

## Verdict

**P3b accepted after one fix round.**
- The two High defects are fixed and verified under ASan: H1 is a use-after-free in shared code that the stand-in design made routine; H2 is dangling stand-in pointers, which Gneiss's `RemoveAllControls` layout would have hit. So is the follow-on H2b.
- The Medium items are fixed: gesture left open on close, clipboard lost with the editor, variable-font instances.
- The threading contract and churn hygiene hold. Keyboard, clipboard, cursor and font behaviour match notes/13.
- The claim "no shared file changed" no longer holds: `ITextEntryControl.{h,cpp}` changed, with no behaviour change on mac/win other than the fix.

Carried forward:
- focus by timestamp or top-level events instead of the 150 ms delay (P10, with Bitwig/Carla/Ardour);
- SAVE_TARGETS hand-over to a clipboard manager;
- L2 (menu request during a collapse);
- verify the variable-font path on a desktop distro;
- Gneiss (P9): style menus by attaching its own `IPopupMenuControl` on Linux, since stand-ins use IGraphics defaults.
