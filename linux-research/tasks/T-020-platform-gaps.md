# T-020 — Remaining platform gaps (GL3, drag out, file-path clipboard, appearance, multi-touch)

- **Status:** review next — items 1–4 done (VM checks pending); item 5 split into T-030
- **Owner:** coordinator (chat f91df715), lock `.linux-port-locks/T-020`
- **Branch/worktree:** `linux/T-020` from `hvoya-linux`, in `/Volumes/Storage/code/IPlug2-T-020`.
- **Depends on:** T-019 (XDND groundwork, same files).
- **Conflicts:** `IGraphics/Platforms/IGraphicsLinux*`.

## Goal
Close the remaining gaps from notes/22 (and whatever T-016 adds), each matching mac/win behaviour, Linux-only code, main thread, no new NEEDED libraries.

## Items
1. **NanoVG GL3.** Create the context with `glXCreateContextAttribsARB` (core 3.2) when `IGRAPHICS_GL3`; fall back to GL2 is not possible at runtime for a GL3 build, so: if a 3.2 context is unavailable, fail the editor open with a clear message. Update the comment in `IGraphicsLinux.cpp` (context creation) and `Documentation/linux-development.md` ("only GL2 tested"). Test IPlugControls GL3 with the P3b scripts.
2. **Drag files out** (`InitiateExternalFileDragDrop`): XDND source (`XdndSelection` ownership, `text/uri-list`, status/finished, Escape cancels), drop into Files and into a host track (REAPER) on the VM.
3. **`SetFilePathInClipboard`**: `text/uri-list` + `x-special/gnome-copied-files` + `UTF8_STRING`, using the clipboard code; paste into Files works.
4. **UI appearance** (`GetUIAppearance`, dark/light, mac-only today): xdg-desktop-portal `org.freedesktop.appearance` `color-scheme` (Settings `Read`, `SettingChanged` signal) through the dlopen'd libdbus already used by the dialogs (T-002); fallback `gtk-theme` name heuristics only if cheap. Notify like mac does on change.
5. **Multi-touch** (`PlatformSupportsMultiTouch`, win only today): XInput2 touch events (dlopen libXi), mapped to IGraphics touch IDs like win; test with `xinput test-xi2` / a virtual touch device if available, else document.

## Acceptance
- [ ] Each item done with evidence (Docker steps where scriptable, VM where it needs a real desktop: short, light, with the lock) or split into its own task with a reason.
- [ ] Full P3b scripts PASS; NEEDED unchanged; mac/win untouched.
- [ ] Independent review (AGENTS §4); evidence in `evidence/t020/`; Log complete.

## Log
(append: date — what was done — commits — next)

### 2026-10-19 — items 1, 3, 4
Worktree `IPlug2-T-020` (`linux/T-020` from hvoya-linux, fast-forwarded to b9c864d91+).

**Item 1, NanoVG GL3** (5f6739350): `IGRAPHICS_GL3` builds pick a `GLXFBConfig` (`glXChooseFBConfig`, `glXGetVisualFromFBConfig`, GLX 1.3 from the same dlopen'ed libGL) and create the context with `glXCreateContextAttribsARB` (3.2 core, `GLX_ARB_create_context_profile`); without one the editor open fails with "no OpenGL 3.2 core context (this build renders with NanoVG GL3)". GL2 builds unchanged. `Documentation/linux-development.md` updated.
- IPlugControls `-DIGRAPHICS_RENDERER=GL3`, Docker llvmpipe, REAPER: `iplugcontrols.txt` CLAP + VST3 PASS, renders ok on first open, reopen ×5 and a second instance (`evidence/t020/gl3-controls-*`).
- `MESA_GL_VERSION_OVERRIDE=3.1` (no 3.2 core): 7 opens, each prints the message (plus the trapped `GLXBadFBConfig`), no editor, REAPER alive (`evidence/t020/gl3-no-core-clap*`).
- VM (virgl): not yet (VM down).

**Item 3, `SetFilePathInClipboard`** (459fcb7c3): `LinuxClipboard::SetFilePath` offers `text/uri-list` (`file://` + percent-encoded path, CRLF), `x-special/gnome-copied-files` (`copy\n<uri>`) and the path as UTF-8 text; a later `SetTextInClipboard` replaces it. Test (`evidence/t020/clipboard/`): a window owning the clipboard with `/tmp/a dir/Preset #1 ünï.fxp`, `xclip` from another process reads all four targets correctly (`run.txt`). Paste into GNOME Files: VM, not yet.

**Item 4, appearance** (6a1bd43f8): new `IGraphicsLinuxAppearance.h`: per editor a private session-bus connection (libdbus dlopen'ed, like the dialogs), asynchronous Hello, `AddMatch` for `SettingChanged` (arg0 `org.freedesktop.appearance`), `Settings.Read(org.freedesktop.appearance, color-scheme)`; polled every 250 ms from the display timer (main thread, `read_write(0)`, never blocks); 1 = dark, else light; a change calls `OnAppearanceChanged`, `GetUIAppearance` returns it. The D-Bus header is included before Xlib (its `Bool` macro).
- Test with a fake portal (`evidence/t020/appearance/fake-portal.c`, libdbus; Read → dark, then `SettingChanged` → light → dark) and IPlugControls `-DIPLUG_TEST_APPEARANCE` (new test-only hook): `dark, light, dark`, all on the main thread, CLAP and VST3 (`appearance/{clap,vst3}/`).
- Without a portal (Docker's plain session bus): no callback, no error; `iplugcontrols.txt` CLAP + VST3 PASS on this build (`regress-controls-*`).
- NEEDED unchanged (libX11, libm, libc, ld-linux) for the GL2 and GL3 builds. A real GNOME portal: VM, not yet.

### 2026-10-19 (2) — item 2 (drag out); item 5 split
**Item 2, `InitiateExternalFileDragDrop`** (0775a601a): new `IGraphicsLinuxDragSource.h`, an XDND v5 source offering `text/uri-list`, non-blocking like the mac drag session (Windows' `DoDragDrop` is modal):
- `Start` takes `XdndSelection` and grabs the keyboard (Escape would otherwise go to the host window with the focus: REAPER closes its FX window on Escape — seen in the first run).
- The press that started the drag keeps an implicit pointer grab, so motion and the release keep coming to the editor: while the button is down they go to the drag, not to IGraphics; when it goes up (drop or cancel) the editor's mouse capture ends (`ReleaseMouseCapture`, as Windows does).
- Target: the outermost XdndAware window under the pointer (following a valid `XdndProxy`); Enter (min(target version, 5)), one Position per Status, Leave on target change; Drop only if the target accepted, else Leave; after a drop the target's requests are served until `XdndFinished` or 3 s.
- `LinuxClipboard::AppendFileUri` is now public and shared with the clipboard's file mode.
- Harness (`evidence/t020/dragout/`): IPlugControls `-DIPLUG_TEST_DRAGOUT` (an orange square at 700,690 starts a drag of `$IPLUG_TEST_DRAG_FILE`), `xdnd-target.c` (a minimal v5 target that accepts uri-lists and logs), `run.sh` (xdotool drags). CLAP and VST3 (`{clap,vst3}.log`):
  - onto REAPER's arrange view (FX window moved aside): REAPER creates a media item from `/tmp/drag me.wav`;
  - onto the probe: Enter v5 `text/uri-list` → Positions → Drop → `file:///tmp/drag%20me.wav` → Finished;
  - Escape over the probe: Leave, no Drop, and the FX window stays open.
  - `iplugcontrols.txt` CLAP + VST3 PASS with the drag build (`regress-drag-controls-*`).
- Not yet: dropping into GNOME Files and other hosts (VM).

**Item 5, multi-touch:** split into **T-030** (`tasks/T-030-multitouch.md`): there is no touch device to test with here (Xvfb, the VM), and it is a Windows-only feature in IGraphics today (mac has none). Plan and test method in the task file.

### Review round 1 (independent reviewer)
Fresh context; range `hvoya-linux..linux/T-020` (5f6739350 … 7bedb32e8). Docker only (no VM).

**Re-run by the reviewer.** I made fresh builds of HEAD rather than reusing `build/t020-*`, which predate the commits by a few minutes: `build/t020r-{drag,app,gl3}` (IPlugControls CLAP and VST3, with `-DIPLUG_TEST_DRAGOUT`, `-DIPLUG_TEST_APPEARANCE` and `IGRAPHICS_RENDERER=GL3`). The worktree's VST3 SDK is a set of symlinks into `IPlug2-linux`, so the build needs `DOCK_MOUNTS=/Volumes/Storage/code/IPlug2-linux:ro`. Container `t020r`. My extra scripts are in `build/t020r-scripts/`, which git ignores.
- NEEDED for all 6 binaries: `libX11.so.6 libm.so.6 libc.so.6 ld-linux-aarch64.so.1`. Unchanged.
- `dragout/run.sh`, CLAP: identical to the committed `clap.log`. REAPER creates the item from `/tmp/drag me.wav` (I checked the screenshot myself). On the probe: Enter v5, Positions, Drop, then `file:///tmp/drag%20me.wav`, then Finished. Escape: Leave, no Drop, the FX window stays open.
- `appearance/run.sh`, CLAP and VST3: `dark, light, dark`, all `main=1`. Identical to the committed logs.
- GL3 build: renders correctly in REAPER (I looked at the screenshot). The editor window's visual is 0x21, depth-24 TrueColor, the same as GL2 under Xvfb.
- Extra drag cases (`extra.sh`, CLAP):
  - **Self-drop** onto our own editor: REAPER's XdndAware top-level forwards it to us. No hang, no crash, and the next drag starts at once.
  - **Editor closed mid-drag** over the probe: the probe gets Leave, the keyboard grab is released, REAPER is alive.
  - **Release over nothing**, and **Escape over nothing**: the grab is released.
  - The grab was checked from another client with `XGrabKeyboard`. It reports AlreadyGrabbed during a drag and GrabSuccess after each of these endings.

**Major**
- **M1 — A wheel step during a drag drops the file too early.** `LinuxDragSource::HandleEvent` treats any `ButtonRelease` as the drop (`IGraphicsLinuxDragSource.h:101`). The wheel arrives as button 4/5 press+release on the implicit grab, so the drag ends there. Reproduced with `wheel.sh`: button 1 held over the probe, one `xdotool click 5`, and the probe logs Drop, DATA and Finished *before* the button is released. A second button behaves the same way. Scrolling while dragging is a common gesture in DAWs. Fix: remember the button that started the drag, from the press's state when `Start` runs. End the drag only when that button is released, and swallow every other button.

**Minor**
- **m1 — Appearance `Send` blocks on `connection_flush`.** `IGraphicsLinuxAppearance.h:119`: `dbus_connection_flush` loops with a blocking iteration and no timeout. It waits until the outgoing queue is written, and in `Start` that includes the SASL handshake. A wedged session bus would therefore hang the host's UI thread at editor open, and the file header's "nothing here blocks" is not true. The dialogs call `connection_read_write(conn, 0)` after each send instead. Do the same: `Poll`'s `read_write(0)` finishes the write.
- **m2 — The first appearance value arrives late.** Polling runs every 250 ms, and Hello → Read → reply takes 2–3 polls. The editor therefore always opens light and switches about 0.5 s later. The committed log's first `dark` line is that switch, not an initial value. On mac the value is right at open. A UI that themes itself from `GetUIAppearance()` in its layout function flashes light. Options: watch the connection's fd through the host loop, as the dialogs do (`connection_get_unix_fd`), or poll every tick until the Read reply has arrived.
- **m2b — A portal that starts after the editor is missed until its next change.** Tested with `late-portal.sh`: the fake portal starts dark 3 s after the open. The editor stays light, and only the later change back to dark gets through. This matters only for portals that are not D-Bus-activatable; a real `xdg-desktop-portal` is activated by the Read. Optional fix: Read again on `NameOwnerChanged` for `org.freedesktop.portal.Desktop`. Low priority.
- **m3 — No drag feedback cursor.** The source never sets an accept or refuse cursor; GTK and Qt sources do, and mac and Windows show the OS feedback. The pointer keeps the editor's cursor for the whole drag.
- **m4 — `Start` does not check that a mouse button is held.** If `InitiateExternalFileDragDrop` is called outside a press (from a key handler or a timer), the drag sits in `Dragging` with the keyboard grabbed desktop-wide. The editor swallows clicks and keys until a click in the editor or Escape. Return false when the last event's state has no button down (or check with `XQueryPointer`).
- **m5 — Evidence gaps.** Three things are not shown yet:
  - the VM items: paste into Files, drop into Files, a real GNOME portal, GL3 on virgl;
  - a tooltip under GL3 (it shares the context and uses the FBConfig's visual, so it is expected to work, but nothing exercised it);
  - drops into hosts other than REAPER.

**Nits**
- **n1.** `Release()` in the `Dropped` state sends XdndLeave after XdndDrop. The spec has no Leave after a Drop. This only happens when the editor closes within 3 s of a drop.
- **n2.** `FindTarget` never looks at the root window, so an XdndProxy on the root (old desktop file managers) is not followed. A target that is XdndAware with version < 3 is skipped and the search goes on into its children, where it should stop.
- **n3.** Every MotionNotify costs about 3 X round trips per tree level (`XTranslateCoordinates` plus 2–3 `XGetWindowProperty`), with no motion compression. That is fine locally and slow on remote X. Coalescing pending motion would fix it.
- **n4.** `LinuxClipboard::AppendFileUri` uses the locale-dependent `isalnum`. Under an 8-bit non-UTF-8 locale set by the host, bytes ≥ 0x80 can pass unencoded. Use an explicit ASCII test. The D-Bus address helper has the same pattern.
- **n5.** The `IGraphicsLinuxDragSource.h` header says "include after Xlib and the clipboard". It also needs `<X11/Xatom.h>` (`XA_ATOM`, `XA_WINDOW`), which today arrives only through the include order.
- **n6.** The GL3 FBConfig attributes do not pin `GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR`. A driver that lists depth-32 ARGB configs first could give the editor an alpha visual. The risk is low and the GL2 path has the same property.
- **n7.** GL2 builds now also need `glXChooseFBConfig` and `glXGetVisualFromFBConfig` to load libGL. These are GLX 1.3 and universal, but they could be required only under `IGRAPHICS_GL3`.
- **n8.** `GetUIAppearance()` returns Light while the editor is closed (Impl reset), whereas mac answers at any time. Fine, but worth a line wherever it gets documented.

**Checked and fine**
- **XDND message layout:**
  - Enter: version in bits 24–31, three types inline.
  - Position: (x<<16)|y, then the time, then the action.
  - Drop: the time.
- **XDND protocol flow:**
  - The version is negotiated as min(target, 5).
  - An XdndProxy must point to itself; messages go to the proxy with `window` set to the target.
  - One Position per Status.
  - A Status from an old target is ignored, because `l[0]` must match.
  - A target that never sends Status: Positions stop, and the release sends Leave instead of Drop.
  - After a Drop, the 3 s timeout applies.
- **Keyboard grab:** released on every path: drop, Escape, release over nothing, and editor close (both `Release()` and the connection close). When the grab fails, Escape goes to the host; REAPER then closes the FX window, which ends the drag cleanly, as tested.
- **Mouse capture:** ends through `ReleaseMouseCapture` as soon as the drag stops being `Dragging`. Grab-mode focus events are already ignored (`HandleFocusOut`).
- **Source and target in one window:** dispatch order is drag source first, and it takes only Status and Finished; then clipboard, then file drop. XdndSelection requests go to the source and SelectionNotify to the target.
- **Clipboard file targets:**
  - text/uri-list with CRLF;
  - gnome-copied-files as `copy\n<uri>` with no trailing newline;
  - TARGETS lists the file targets only in file mode;
  - `SetText` clears file mode.
- **Portal:**
  - Read's `v(v(u))` and the signal's `ssv` are unwrapped through any number of variants.
  - Error replies are ignored and leave the value unchanged.
  - The AddMatch rule has no sender, so a later portal's signals still arrive.
  - When the connection drops it is closed, with no reconnect.
- **GL3:** an FBConfig context with a core 3.2 profile, the error trapped by OpenWindow's trap, and a clear failure message (evidence `gl3-no-core-*`).
- **Threading:** everything runs from `ProcessXEvents` or `OnDisplayTimer` on the host main thread, and the appearance callback reported `main=1`. The one blocking call is m1.
- **Scope:** only `IGraphicsLinux*` files changed, plus IPlugControls test hooks guarded by `OS_LINUX` and the doc. No mac/win files touched, and no product names. Comments are WHY-only and accurate, apart from the "nothing here blocks" claim in m1.

**Verdict: changes requested.**
- Fix M1. Fixing m1 and m4 at the same time is recommended; both are small.
- m2/m2b and m3 can be done now or recorded as follow-ups. The nits are optional.
- Items 1–4 are otherwise correct and the Docker evidence holds up. The VM checks (m5) are still needed before `review-done`.

Containers:
- I removed `t020r`.
- The container `t020`, up for about 3 hours when the review started, is the worker's; I left it running.
