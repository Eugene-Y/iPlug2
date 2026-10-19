# T-020 — Remaining platform gaps (GL3, drag out, file-path clipboard, appearance, multi-touch)

- **Status:** in progress — items 1, 3, 4 done; 2 (drag out) next; 5 (multi-touch) open
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
