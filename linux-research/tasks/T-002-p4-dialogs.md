# T-002 — P4: file, message and colour dialogs without GTK

- **Status:** in-progress
- **Owner:** worker instance (spawned by the coordinator, 2026-10-14)
- **Branch/worktree:** `linux/T-002` from `hvoya-linux`, worktree `/Volumes/Storage/code/IPlug2-T-002`.
- **Depends on:** —
- **Conflicts:** `IGraphics/Platforms/IGraphicsLinux*.{h,cpp}` (T-007, T-008)

## Goal
Implement on Linux, without GTK in the process:
- `PromptForFile` (open and save, with extension filters);
- `PromptForDirectory`;
- `ShowMessageBox`;
- `PromptForColor`.

They are stubs today ("cancelled"). Gneiss needs them for presets, morph rigs and CC maps:
- `morph/morph_controller.hpp:2114,2126`;
- `morph/morph_file_router.hpp:115-146`;
- `hvoya/utils/preset_strip_control.hpp:421-446`;
- `user_tab_panel.cpp:841-851`.

## Design (PLAN §1, P4)
1. **xdg-desktop-portal `org.freedesktop.portal.FileChooser`** over D-Bus.
   - libdbus is `dlopen`'d.
   - The parent window is passed as `x11:<hex XID>`.
   - The response (`Response` signal) is async on the D-Bus fd, which is registered on `GetHostLoop()`.
2. **Fallback:** zenity, then kdialog, via `posix_spawn`, with the stdout pipe fd on the host loop. Never block the host thread; reap the child.
3. Message box and colour picker use the same path, or are IGraphics-drawn.
4. **Completion handlers:**
   - always fire on the host main thread, asynchronously (like mac);
   - if the editor closes with a dialog open, the dialog is dismissed or orphaned and the handler is **not** called on a dead object.
5. Encoding: UTF-8 paths, spaces, Cyrillic; a default directory and file name; save-dialog overwrite confirmation.

## Acceptance
- [ ] **Docker (zenity path, no portal):**
  - open, save, directory, cancel;
  - editor closed mid-dialog;
  - Cyrillic and space paths;
  - main-thread check;
  - 50× churn with a dialog open at close.
- [ ] **VM (portal under GNOME):** the real dialog appears and is screenshotted; selection via xdotool returns the path to the plugin.
- [ ] No GTK symbols loaded in the plugin process (`/proc/<pid>/maps` check while the dialog is open).
- [ ] Review pass; Log complete.

## Log
