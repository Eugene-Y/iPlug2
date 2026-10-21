# T-032 — Synchronous dialogs on Linux (PromptForFile/Directory/Color, ShowMessageBox without a completion handler)

- **Status:** waiting for the user's decision (found by the T-016 review, M2)
- **Owner:** —
- **Conflicts:** `IGraphics/Platforms/IGraphicsLinux*`, `IGraphics/Controls/IVPresetManagerControls.h`.

## Problem
Without a completion handler, mac and Windows **file prompts and message boxes** block in a modal loop and return the result; Linux (T-002, asynchronous only) returns at once with nothing, and `ShowMessageBox` returns `kNoResult` (`IGraphics/Platforms/IGraphicsLinux.cpp:2178–2260`). **Colour prompts:** mac's panel is non-modal and returns at once too (Linux matches mac); only Windows blocks. In iPlug2 itself the stock `IVDiskPresetManagerControl` load button calls `PromptForFile` synchronously (`IGraphics/Controls/IVPresetManagerControls.h:245`): it does nothing on Linux, and on iOS it already asserts (iOS file prompts require a handler) — so it is a cross-platform defect, not only a Linux one. The API already documents `kNoResult` for message boxes that cannot answer inline.

## Options (for the user)
1. **A blocking wait on Linux.** It cannot be "as mac/win do": their modal loops dispatch the whole process's events, so the host stays responsive. A Linux plug-in can only pump its own window's events; the host's windows would freeze while the dialog is open. Acceptable only for the standalone APP, or as an opt-in.
2. **Make iPlug2's own callers asynchronous** (the stock preset control with a completion handler) and document that plug-ins must use the asynchronous form on Linux (as on iOS). Effect on mac: with a handler the file panel becomes a sheet on the plug-in window instead of app-modal — a visible change, for the user to approve. Fixes iOS too.
3. 2 now; 1 only for the APP or as an opt-in.

Recommendation: 3.

## Steps (after the decision)
- Option 2: `IVDiskPresetManagerControl` with a completion handler; grep iPlug2 for other synchronous calls; a note in `Documentation/linux-development.md`. Shared code: the user approves the control change.
- Option 1: a modal wait in `IGraphicsLinux` with a bounded nested loop; tests for host calls during the wait (REAPER automation, editor close).
