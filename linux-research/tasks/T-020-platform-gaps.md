# T-020 — Remaining platform gaps (GL3, drag out, file-path clipboard, appearance, multi-touch)

- **Status:** ready after T-019 (same files); split into sub-tasks if any item grows
- **Owner:** — (planned: coordinator)
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
