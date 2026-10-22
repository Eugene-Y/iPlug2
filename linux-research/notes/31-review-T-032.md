# Review of T-032 (synchronous dialogs → completion handlers)

Reviewer: independent subagent, fresh context. Branch `linux/T-032`, range `refs/heads/hvoya-linux..refs/heads/linux/T-032`
(098ebce5a, 2d8b0e302, 272bd4eff).

## Round 1

### What was checked

- **Caller table.** Grepped `IGraphics/ IPlug/ Examples/ Tests/` for `PromptForFile|PromptForDirectory|PromptForColor|ShowMessageBox`
  and for `MessageBox|GetOpenFileName|BrowseFor*`. The table is complete. The only synchronous IGraphics calls are
  the converted `IVDiskPresetManagerControl` Load button and the deliberate `t-sync` test button. `IPlug/` has no IGraphics
  dialog calls. The other `MessageBox` hits (`IPlug/APP/*`, `Examples/IPlugReaperExtension*`) are SWELL/REAPER, not the
  IGraphics API. I see no weighty reason for the Load button to stay synchronous. It is a button's animation-end
  action on the UI thread, and all of its post-call work can move into the handler.
- **Equivalence**, read from `IGraphicsWin.cpp:1481-1587`, `IGraphicsMac.mm:510-593`, `IGraphicsLinux.cpp:2233-2262` and
  `IGraphicsIOS.mm:214-246`. The Log's reasoning holds:
  - *Windows:* the handler receives the same `fileName`/`path` locals, which are still in scope, before `PromptForFile`
    returns. The only reorder is `ReleaseMouseCapture` running after the handler.
  - *Cancel:* gives an empty file and an untouched (empty) path on Windows, and both strings empty on mac and Linux.
  - *OK:* gives the full path plus the folder with a trailing separator on all three.
  - *macOS:* with a handler the panel becomes a sheet on `[mView window]`. The block captures only `pPanel` and a copy
    of the `std::function`, never `IGraphicsMac`, so after the editor is gone the only `this` it can reach is the
    control's. That is why the guard is the right one.
  - *iOS:* the Load button no longer asserts.
- **Lifetime.**
  - **Guard:** the `weak_ptr` guard is correct, cheap and readable: it expires with the control, and there is no
    cross-thread use, so `expired()` then call is fine. On Linux and Windows it is redundant, and on macOS it is
    plausible but unproven (the Log says so).
  - **Captures:** `loadPresetFunc`'s `[&]` captures only `this`, not locals of `OnAttached`. The handler uses neither
    `pCaller` nor the `fileName`/`path` locals.
  - **Checked, no issue:** in `IPlugControls.cpp:194` the `[&]` refers to the button closure's own `pGraphics`
    member, which lives in the control's stored action function. Linux drops pending handlers when the editor closes,
    so nothing dangles there.
- **Shared-code rule.**
  - **macOS:** the header is compiled on mac by IPlugEffect, whose `IPlugEffectUI.cpp` includes `IControls.h`, which
    includes `IVPresetManagerControls.h`. The control is instantiated in the IPlugControls APP probe build.
  - **Windows / other:** nothing in the change is Windows-specific apart from the reasoning above. There is no other
    mac/win behaviour change beyond the documented macOS sheet.
- **Product names:** `scripts/coord/check-product-names.sh refs/heads/linux/T-032`: clean.
- **Evidence (opened myself):**
  - `dialogs-clap/d09` shows the GTK "Open File" dialog with the `*.fxp` filter over the editor;
  - `d10` and `sheet-preset.png` show the label "пресет А.fxp" and the handler line;
  - `checks.txt` for CLAP and VST3 has all 39 ok, including the 4 preset checks and close-mid-dialog preset-load;
  - `baseline-dialogs-clap/RESULT.txt` is FAIL (4) with exactly the claimed checks;
  - the P3b sheets look normal;
  - `mac/sheet.png` shows the NSOpenPanel attached as a sheet to the IPlugControls window, and the probe logs match
    the Log. As the Log itself says, the mac `remove-ok` probe does not exercise the guard.
- **Re-run:**
  - **Build:** rebuilt `IPlugControls-clap` with `-DIPLUG_TEST_DIALOGS` from the branch head
    (`linux-research/build/t032-review`), with no warnings in the changed files.
  - **Test:** `scripts/gui-up.sh t032r && scripts/gx.sh t032r scripts/dialog-test.sh evidence/t032-review/dialogs-clap
    "CLAP: IPlugControls" build/t032-review/out/IPlugControls.clap` gave **RESULT: PASS**, 39/39 ok. My own `d10`
    shows the loaded label and the handler line. Container `t032r` removed; `evidence/t032-review` deleted.

### Findings

- **R1-1 (minor) — `IGraphics/IGraphics.h:903-908`.** The new `PromptForColor` summary says the colour arrives
  through `func` on macOS/iOS/Linux, but the untouched `@param`/`@return` lines below it still contradict it (and the
  user's rule is to fix a stale comment you touch):
  - `@param color` says the referenced IColor "will be updated", but only Windows writes it back. macOS, iOS and Linux
    leave it unchanged.
  - `@param str`'s example says "(Windows only)", but Linux uses `str` as the dialog title (`IGraphicsLinux.cpp:2300`).
  - `@return` says "true if prompt completed successfully", but Linux and iOS always return `false`, and macOS returns
    `true` once the panel shows.

  Fix: e.g. `@param color The initial colour; Windows also writes the chosen colour back into it`, `@param str The
  dialog title (Windows, Linux)`, and `@return true if color now holds a chosen colour (Windows only; elsewhere the
  colour arrives only through func)`. For macOS, either "true when the panel opened" or leave it out.
- **R1-2 (nit) — `IGraphics/IGraphics.h:877-878, 886-889, 903-904`.** The comments now list each platform's behaviour
  but leave out the web build. There, `ShowMessageBox` blocks via `alert`/`confirm` for OK/OK-Cancel/Yes-No,
  `PromptForFile` and `PromptForDirectory` are empty TODOs (`IGraphicsWeb.cpp:1193-1215`), and `PromptForColor`
  returns at once. Fix: either add a short "Web: file and folder prompts are not implemented", or word the colour line
  as "Only Windows blocks; elsewhere it returns at once …" so it does not read as a complete list.
- **R1-3 (nit) — `Documentation/linux-development.md:209-220`.**
  - The example calls `LoadPresetFile(...)`, which is not an iPlug2 API, so a reader may look for it. Use the real
    `IPluginBase` member: `LoadPresetFromFXP(chosenFile.Get());`.
  - "a handler that captures a control should check that the control still exists" does not say how. Add the
    pattern in a few words, e.g. "(as `IVDiskPresetManagerControl` does: the control owns a `shared_ptr` token and
    the handler holds a `weak_ptr` to it)".
- **R1-4 (nit) — `IGraphics/Controls/IVPresetManagerControls.h:308`.** The doc comment on `OnPresetFileChosen`
  restates the body ("Lists the chosen file's folder and loads the file"). Its second half keys on "an empty file
  name", while the code tests the folder and the file separately. Fix: delete it (the name and the two `if`s read
  themselves), or keep only the non-obvious fact, e.g. `// A cancelled dialog passes two empty strings on every
  platform.`
- **R1-5 (nit) — `IGraphics/Controls/IVPresetManagerControls.h:328`.** There is an unrelated whitespace-only change in
  `GetSubControlBounds` (trailing spaces removed). It is harmless, but it is noise in a shared upstream file. Revert it
  to keep the diff minimal.
- **R1-6 (nit) — `linux-research/scripts/dialog-test.sh:113-114`.** `preset-cancel-kept-folder` presses `>` and
  expects А, but that does not tell "cancel left the list alone" from "cancel re-listed the folder". Re-listing would
  make `SetupMenu` reset the index to -1, and `>` would also give А. It only catches "reverted to the start folder".
  Pressing `<` instead tells them apart: if the list was kept, index 1 → 0 = А; if it was re-listed, -1 → wrap = Б.
  Fix: add `[preset-prev]="719 598"` (50 px left of `preset-next`), click it after the cancel, and expect
  `пресет А\.fxp`.
- **R1-7 (nit) — `linux-research/tasks/T-032-sync-dialogs.md:9, 46`.**
  - The Problem's line references are stale: `IGraphicsLinux.cpp:2178–2260` is now `2210–2312`, and
    `IVPresetManagerControls.h:245` is now 248.
  - The caller-table note says only that the APP's dialogs are SWELL. Also mention that the
    `Examples/IPlugReaperExtension*` `MessageBox` calls are SWELL/REAPER, not IGraphics.

### Residual risk (not a finding; for the coordinator)
The macOS change is a visible one: the Load panel is now a window-modal sheet instead of an app-modal panel. It was
verified only in the standalone APP, through an in-process probe. A plug-in format in a mac host, and the `weak_ptr`
guard with a real file chosen after the control is gone, were not exercised. Windows was reasoned from the code, not
run. All of this is stated in the Log and the acceptance list. The CROSS_PLATFORM_FINDINGS entries XP-1, XP-2 and XP-3
were checked against the code and are accurate.

Verdict round 1: approve

### Worker response to round 1
- R1-1 fixed: `@param color`, `@param str`, `@param func` (was the malformed `@param IColorPickerHandlerFunc func`) and
  `@return` per platform.
- R1-2 fixed: `PromptForFile` notes that the web build does not implement it; the colour summary lists the web too.
  The web `ShowMessageBox` is left out of the per-platform sentence on purpose: its Yes-No and OK-Cancel case falls
  through into `default` and returns `kNoResult` without calling the handler (`IGraphicsWeb.cpp:1176-1185`), so no
  short sentence describes it correctly; reported as XP-4 in the task Log.
- R1-3 fixed: `LoadPresetFromFXP`; the `weak_ptr` token pattern named.
- R1-4 fixed: the comment is now only "A cancelled dialog passes two empty strings".
- R1-5 fixed: whitespace restored; the header diff against `hvoya-linux` has no whitespace-only hunk.
- R1-6 fixed: `preset-prev` (719 598) after the cancel, expecting А.
- R1-7 fixed: the Problem names functions instead of line numbers; the table note names the SWELL/REAPER `MessageBox` calls.
- Re-run after the fixes (rebuilt `build/t032-test`): `dialog-test.sh` CLAP **PASS**, VST3 **PASS**, sheets regenerated.
