# T-032 — Synchronous dialogs on Linux (PromptForFile/Directory/Color, ShowMessageBox without a completion handler)

- **Status:** waiting for the user's decision (found by the T-016 review, M2)
- **Owner:** —
- **Conflicts:** `IGraphics/Platforms/IGraphicsLinux*`, `IGraphics/Controls/IVPresetManagerControls.h`.

## Problem
On mac and Windows these calls without a completion handler block (a modal loop) and return the result. On Linux (T-002) dialogs are asynchronous only: called synchronously, `PromptForFile`/`PromptForDirectory`/`PromptForColor` return at once with nothing and `ShowMessageBox` returns `kNoResult` (`IGraphics/Platforms/IGraphicsLinux.cpp:2178–2260`). Plug-ins written for mac/win that use the synchronous form silently get nothing. In iPlug2 itself the stock `IVDiskPresetManagerControl` load button calls `PromptForFile` synchronously (`IGraphics/Controls/IVPresetManagerControls.h:243`) and does nothing on Linux.

## Options (for the user)
1. **A nested wait on Linux**, as mac/win do: run the dialog and wait for its answer inside the call, pumping the editor's X events (and the dialog's pipe/D-Bus) meanwhile. Matches mac/win; risk: the host's main thread is blocked as on mac/win, and re-entrancy (the host calling into the plug-in while we wait) must be handled — T-002 chose asynchronous for this reason.
2. **Document it as a Linux difference** and switch iPlug2's own synchronous callers (the stock preset control) to completion handlers, which works the same on mac/win. Plug-ins must use the asynchronous form on Linux.
3. Both: 2 now, 1 later.

Recommendation: 3 — the stock control fix is small and safe everywhere; the nested wait needs care.

## Steps (after the decision)
- Option 2: `IVDiskPresetManagerControl` with a completion handler; grep iPlug2 for other synchronous calls; a note in `Documentation/linux-development.md`. Shared code: the user approves the control change.
- Option 1: a modal wait in `IGraphicsLinux` with a bounded nested loop; tests for host calls during the wait (REAPER automation, editor close).
