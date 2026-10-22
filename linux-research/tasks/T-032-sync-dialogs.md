# T-032 — Synchronous dialogs on Linux (PromptForFile/Directory/Color, ShowMessageBox without a completion handler)

- **Status:** in progress (option 2, user 2026-10-22)
- **Owner:** T-032 worker (coordinator chat f91df715)
- **Branch:** `linux/T-032` in `/Volumes/Storage/code/IPlug2-T-032`
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

## Decision
**User, 2026-10-22:** "if there are no weighty reasons why they were synchronous, make them asynchronous, but test it."
Option 2: iPlug2's own callers use the completion-handler form; plug-ins are told to do the same on Linux (as on iOS).

## Acceptance
- [x] Every synchronous dialog call in `IGraphics/`, `IPlug/`, `Examples/`, `Tests/` listed, each with a decision (table below).
- [x] Callers without a weighty reason converted: `IVDiskPresetManagerControl` (the only one).
- [x] mac/win behaviour equivalent apart from the documented sheet: Windows unchanged (handler runs before
      `PromptForFile` returns); macOS panel is a sheet on the plug-in window instead of app-modal.
- [x] `Documentation/linux-development.md` §9: dialogs are asynchronous only on Linux; use the handler.
- [x] Linux Docker, REAPER, CLAP + VST3: the Load dialog opens, a chosen file loads and its folder becomes the list,
      cancel changes nothing, closing the editor with the dialog open is clean. Baseline (old control) fails the same checks.
- [x] P3b IPlugControls CLAP + VST3 PASS (regression).
- [x] macOS: builds (IPlugControls APP with the test define, IPlugEffect CLAP); the Load button opens an `NSOpenPanel`
      sheet on the window; cancel/OK through the handler keep the APP alive. Choosing a real file on mac not driven.
- [ ] Windows: not run (no machine); reasoned from `IGraphicsWin.cpp` below.
- [ ] Independent review approved.

## Log

### 2026-10-22 — worker
**Setup.** Lock `T-032`; worktree from `refs/heads/hvoya-linux`; SDKs `cp -cR` from the integration worktree.

**Callers without a completion handler** (grep of `PromptForFile|PromptForDirectory|PromptForColor|ShowMessageBox`
in `IGraphics/ IPlug/ Examples/ Tests/`; no wrappers exist — `IPlug/` has no calls, the APP's own dialogs are SWELL):

| Caller | Form before | Result used in the same frame? | Context that must finish synchronously? | Decision |
|---|---|---|---|---|
| `IGraphics/Controls/IVPresetManagerControls.h` `IVDiskPresetManagerControl` Load button | `PromptForFile` sync | yes, but only to list the folder and load the file — movable into the handler | no: a button's animation-end action on the UI thread, not a host callback or destructor | **converted** |
| `Examples/IPlugControls/IPlugControls.cpp` `t-sync` button (`IPLUG_TEST_DIALOGS` only) | `PromptForFile` sync | — | — | **kept on purpose**: the Linux harness's check that a call without a handler returns empty at once and warns |
| `IGraphics/Controls/IControls.cpp:1428` (`IVColorSwatchControl`) | `PromptForColor` with handler | — | — | already async |
| `IGraphics/Controls/Test/TestImageControl.h`, `TestSVGControl.h`, `Skia/ISkLottieControl.h` | `PromptForFile` with handler | — | — | already async |
| `Examples/IPlugControls/IPlugControls.cpp` font buttons, message box, `t-*` test buttons | with handler | — | — | already async |
| `IGraphicsMac/Win/IOS/Web/Linux` | implementations | — | — | n/a |

No caller has a weighty reason to stay synchronous. Nothing to flag for the user.

**Change** (098ebce5a): the Load button passes a handler; the old post-call code is `OnPresetFileChosen(fileName,
folder)`. The handler holds a `weak_ptr` to a token owned by the control and returns if the control is gone: on macOS
the panel is a sheet on the host's window and can outlive the editor (the host can remove the plug-in from its main
window while the sheet is open); before, `runModal` made that impossible. On Linux closing the editor already drops the
handler. `IPlugControls` gets an `IVDiskPresetManagerControl` above the test buttons, only under `IPLUG_TEST_DIALOGS`
and only when `IPLUG_TEST_DIALOG_DIR` is set (its path must be non-empty: `AddPath` asserts).
`scripts/dialog-test.sh` gains: Load + choose `подпапка два/пресет А.fxp` → handler line and label; `>` → `пресет Б.fxp`
(the chosen folder became the list); Load + Escape → no load; `>` → `пресет А.fxp` (cancel kept the folder);
closing the editor with the Load dialog open (dialog gone, no handler, REAPER alive).

**Docs** (2d8b0e302): `linux-development.md` §9 "Plug-in code: differences from macOS and Windows" (new section after
§8, so the existing §8 references stay valid). The `IGraphics.h` doc comments of the four dialog methods said they
block (wrong for macOS with a handler, iOS and Linux) and had a typo'd `completionHanlder`; they now state each
platform's behaviour.

**Equivalence, mac/win, from the code:**
- *Windows* (`IGraphicsWin.cpp:1481-1587`): with a handler the same `GetOpenFileNameW` modal call runs and the handler
  is called with the same `fileName`/`path` before `PromptForFile` returns, so the control does exactly what it did; the
  only reorder is `ReleaseMouseCapture()` now running after the handler instead of before the post-call code, which
  touches no capture state. Cancel: `fileName` empty, `path` unchanged (empty, the control passes empty) → no-op, as
  before. Not run (no Windows machine).
- *macOS* (`IGraphicsMac.mm:510-593`): without a handler `runModal` (app-modal); with one
  `beginSheetModalForWindow:` on the view's window, handler later with fresh strings: OK → full path + folder with
  trailing `/` (as the sync path; `WDL_DirScan` strips the slash), cancel → both empty → no-op. **Visible change:** the
  Load panel is a sheet on the plug-in window, the rest of the host stays usable.
- *iOS*: the Load button used to assert (iOS requires a handler); it now uses the handler. Not built or run.

**Tests — Linux, Docker (`iplug2-linux:24.04`, container `t032`):**
- Build: `DOCK_ENV="EXTRA_CMAKE_ARGS=-DCMAKE_CXX_FLAGS=-DIPLUG_TEST_DIALOGS" scripts/dock.sh -- scripts/build-cmake.sh .. build/t032-test IPlugControls-clap IPlugControls-vst3 IPlugControls-app` PASS.
- `scripts/gx.sh t032 scripts/dialog-test.sh evidence/t032/dialogs-{clap,vst3} "{CLAP,VST3}: IPlugControls" build/t032-test/out/IPlugControls.{clap,vst3}`:
  **PASS** both (all earlier dialog checks + the 4 preset checks + preset close-mid-dialog; every handler `main=1`).
  `evidence/t032/dialogs-*/checks.txt`, `sheet.png`; `dialogs-clap/sheet-preset.png` (dialog open; label
  "пресет А.fxp" and the handler line after loading).
- **Baseline** (the old header, same test build, `build/t032-base`): `evidence/t032/baseline-dialogs-clap` **FAIL (4)** —
  preset-load "no handler call", the list stays the start folder, close-mid-dialog "no dialog"; stderr has
  "PromptForFile without a completion handler cannot block on Linux". So the checks catch the old behaviour.
- P3b regression on a plain build (`build/t032-plain`): `run-gui-test.sh … scripts/steps/iplugcontrols-p3b.txt` CLAP
  **PASS**, VST3 **PASS** (`evidence/t032/p3b-controls-{clap,vst3}/RESULT.txt`, `sheet.png` looked at).
- VM not used.

**Tests — macOS (this Mac), scratch DerivedData/install paths, nothing in `~/Library`, main checkout untouched:**
- IPlugEffect CLAP (`scripts/coord/mac-xcodebuild.sh`) builds. IPlugControls APP with `EXTRA_DEBUG_DEFS=IPLUG_TEST_DIALOGS`
  builds only with the scratch workaround `OTHER_CPLUSPLUSFLAGS='$(OTHER_CFLAGS) -x objective-c++'
  GCC_PRECOMPILE_PREFIX_HEADER=NO`: the example APPs fail on plain `hvoya-linux` too (`IPlugAPP.cpp` imports Cocoa;
  CROSS_PLATFORM_FINDINGS X3). The `.rc_mac` files Xcode regenerates were restored.
- Synthetic clicks are dropped (the session's process is not accessibility-trusted), so a temporary, uncommitted probe
  (`evidence/t032/mac/probe.patch`, `T032_MAC_PROBE`) pressed the Load button in-process, then ended the sheet:
  - `probe-cancel.log`: `sheet=yes (NSOpenPanel) on window 'IPlugControls'`, ended with Cancel, APP alive;
  - `probe-ok.log`: ended with OK (the panel had no selection, so the handler got empty strings), APP alive;
  - `probe-remove-ok.log`: the control removed while the sheet was open, then OK; APP alive. Because the panel had no
    selection, this does **not** exercise the `weak_ptr` guard (the handler would have been a no-op anyway).
  - `mac/sheet.png`: the APP at start and with the Load sheet attached (the panel's file list is redacted: it showed the
    user's own folders).
- **Not verified on mac:** choosing a real file in the sheet (no event injection), a plug-in format in a host (sheet on a
  host's window), the guard against a control destroyed before the sheet ends.

**Cross-platform findings (report, not fixed):**
- **XP-1, low.** `IGraphicsMac_view.mm:1300` `onColorPicked:` calls `mColorPickerFunc` unconditionally; `PromptForColor`
  without a `func` on macOS stores an empty `std::function`, so picking a colour throws `std::bad_function_call` inside
  an AppKit callback (terminates). No iPlug2 caller passes no `func`.
- **XP-2, medium (for users of the option).** `IVPresetManagerControls.h:286-293` `LoadPresetAtCurrentIndex`: with
  `showFileExtensions = false` it strips the extension from the *path* it passes to `OnLoadFunc`, not only from the
  label, so the plug-in is asked to load `…/name` instead of `…/name.fxp`. All platforms. Repro: construct the control
  with `showFileExtensions=false` and an `onLoadFunc` that opens the path.
- **XP-3, trivial.** `Controls/Test/TestSVGControl.h:60`, `Test/Skia/ISkLottieControl.h:129`: `SetDirty(false)` is
  indented under `if (fileName.GetLength())` but runs unconditionally (no braces). Test controls only.
- **For CROSS_PLATFORM_FINDINGS (behaviour change of this task):** the stock `IVDiskPresetManagerControl` Load panel
  becomes a window-modal sheet on macOS (was app-modal); Windows unchanged; iOS no longer asserts.

**Incident:** the first mac screenshot attempt used a full-screen `screencapture`, which made macOS show the user a
"Claude is requesting to bypass the system private window picker" prompt. Not answered by the worker; the capture was
deleted. Window captures (`screencapture -l`) need no such permission and were used from then on.
