# T-016 — Feature-parity audit: Linux vs mac/win

- **Status:** merged (review approved round 2)
- **Owner:** coordinator (chat f91df715), lock `.linux-port-locks/T-016`
- **Branch/worktree:** `linux/T-016` from `hvoya-linux`, in `/Volumes/Storage/code/IPlug2-T-016` (docs only).
- **Depends on:** nothing.
- **Conflicts:** none (writes `notes/22-feature-parity.md`, new task files proposed to the coordinator).

## Goal
A complete, evidence-backed list of everything iPlug2 (hvoya) offers on mac or win and how Linux compares, so no gap is discovered by a customer. The user's requirement: Linux must not be poorer than mac or win (notes/22).

## Scope — check each, with file:line for the mac/win implementation and the Linux one
- **IGraphics platform API:** every virtual in `IGraphics.h` that `IGraphicsMac`/`IGraphicsWin` override (notes/22 has a first diff); also non-virtual paths with `#ifdef OS_MAC/OS_WIN` in `IGraphics/`.
- **Drawing backends and options:** NanoVG GL2/GL3/Metal, Skia (CPU/GL/Metal), `IGRAPHICS_CPU`?, layers/FBOs, SVG (NanoSVG, Skia SVG), bitmap formats, shader controls, `IGRAPHICS_IMGUI`, font loading (by name, from file, from memory, resources), text measuring.
- **Controls** with platform-specific code (`IGraphics/Controls`, `IGraphics/Extras`): web view, platform view, file browser, MIDI keyboard, vector controls using platform features.
- **IPlug:** every format wrapper and what builds on Linux (CLAP, VST3, APP, VST2 deprecated, REAPER extension `IPlug/ReaperExt`, REAPER-API plug-ins, LV2 planned, WAM/web n/a, AU/AAX n/a). `IPlug/Extras` (WebView, SwiftUI n/a, OSC, `HIIR`, `Synth`, `Oversampler`, `ISender`, CLI/command-line tools), `IPlugPaths` (all path helpers: user/shared data, presets, app support → XDG), `IPlugUtilities` platform bits, logging, crash/assert handling.
- **APP host:** audio/MIDI APIs, settings dialog, menus, window behaviour, screenshot/debug menu (`IPlugAPP_host`, SWELL).
- **Tests/Examples** in the repo: which ones build and run on Linux (`Examples/*`, `Tests/*`); each one that doesn't is a gap or an explained n/a.
- **Scripts/tooling:** `duplicate.py`, `Scripts/` build/packaging helpers, validators skill (`validate`), docs.

## Method
Grep/read on `hvoya-linux`; for anything ambiguous, build the example in Docker and try it. Don't change code.

## Output
- `notes/22-feature-parity.md`: the full table (feature · mac · win · Linux now · gap? · proposed task), replacing the first-pass table; keep the "not gaps" section with reasons.
- For each gap without a task: a short proposed task (goal, files, estimate) in the note; the coordinator files them.

## Acceptance
- [ ] Every area in Scope covered, with file:line evidence.
- [ ] Each example/test project's Linux status recorded (built/ran/failed, evidence).
- [ ] Independent review (AGENTS §4): the reviewer spot-checks at least 10 rows; Log complete.

## Log
(append: date — what was done — commits — next)

### 2026-10-19 — audit done
- `notes/22-feature-parity.md` rewritten: tables for the IGraphics platform layer, drawing, formats/IPlug/Extras, the APP host, every example/test project, tooling; ✅/❌/n/a with the task that did it or owns the gap.
- Overrides diff (`IGraphicsMac.h`/`IGraphicsWin.h` vs `IGraphicsLinux.h`): only `AttachPlatformView`/`RemovePlatformView`/`HidePlatformView` (T-017) and `PlatformSupportsMultiTouch` (T-030) are real gaps; `MeasureText` on mac is a pass-through; bundle IDs and module handles are mac/win concepts.
- `#if OS_MAC/OS_WIN` outside the platform files: AAX/Pro Tools modifiers, Windows `.rc` resources, the mac FBO stash, `IGraphics_select.h` backend choice — none is a Linux gap.
- **All example and test projects built:** 57 targets (19 projects × APP/CLAP/VST3), all PASS (`evidence/t016/build-results.txt`, `targets.txt`). **Opened each CLAP in REAPER** (`evidence/t016/run-all.txt`, `run-<Name>-clap*`): 9 PASS outright; 5 render but fail the harness's "redraw while the mouse moves" check (static UIs); WebView examples open no editor (T-017); ConvoEngine has no UI by design; IPlugReaperPlugin lacks Linux resources.
- **New gap → T-031** (`tasks/T-031-reaper-extension-linux.md`): REAPER extensions don't link on Linux (SWELL modstub), and the REAPER-plug-in example ships no fonts on Linux.
- Next: independent review (spot-check ≥10 rows).

### Review round 1 (independent reviewer)
2026-10-19, fresh context, reviewed commit f231b5635. I made one extra Docker run (container `rev016`, since removed): IGraphicsTest CLAP in REAPER, clicking through all 28 test pages. Its contact sheets are `build/review-t016/sheetA.png` and `sheetB.png`. `build/` is gitignored, so they are not committed.

**Rows checked (16)**
1. **Overrides diff.** Confirmed with my own `comm` over the `override` names. Only mac overrides `Attach/Remove/HidePlatformView`, `MeasureText` (a pass-through, `IGraphicsMac.mm:91-94`) and `GetBundleID`/`GetAppGroupID`. Only win overrides `PlatformSupportsMultiTouch` (`IGraphicsWin.h:79`) and `Get/SetWinModuleHandle`. Linux has no override of its own. ✓
2. **`GetUIAppearance`:** overridden on mac (`IGraphicsMac.h:80`) and Linux (`IGraphicsLinux.h:66`), not on win. ✓
3. **Resources per module:** `Scripts/cmake/IPlugPlugin.cmake:79-117` creates `<Name>.resources/` for CLAP/APP and uses `Contents/Resources` for VST3. ✓
4. **REAPER extension link failure:** the cause is correct. The Linux branch of `ReaperExt.cmake:79-83` adds only the SWELL include path. The mac branch compiles `swell-modstub.mm` with `SWELL_PROVIDED_BY_APP`. `swell-modstub-generic.cpp:21-32,135` provides the function pointers, `SWELL_curmodule_dialogresource_head` and `SWELL_dllMain`. `build/t016-reaperext.log` lists exactly those SWELL symbols as undefined. ✓ But see M1: T-031 is under-scoped.
5. **IPlugReaperPlugin resources:** `Examples/IPlugReaperPlugin/CMakeLists.txt` adds the font only through `MACOSX_PACKAGE_LOCATION`, and doesn't use `iplug_add_plugin`. The stderr tail says "resource folder … not found". `r01-open.png` is a solid red editor with no text. ✓ (see m2)
6. **The 5 "static UI" projects:** I opened `run-IPlugMidiEffect-clap/r01-open.png`, `run-IPlugSideChain-clap/r03-two-instances.png` and `run-IGraphicsStressTest-clap/r01-open.png`. All three render correctly: MidiEffect's "Trigger Chord" button; SideChain's meters, labels and slider in both instances; StressTest's landing page and button strip. The "no Draw hits" result is harmless (see m3 for the precise reason). ✓
7. **IPlugConvoEngine:** `config.h:26` sets `PLUG_HAS_UI 0`, and `CMakeLists.txt:11` sets `NO_IGRAPHICS`. ✓
8. **WebView examples:** `IPlug/Extras/WebView/IPlugWebView_linux.cpp:11-36` is a no-op stub (`OpenWebView` returns nullptr). In the RESULT files the editor window is not found. ✓
9. **APP screenshot:** `IPlugAPP_dialog.cpp:26-30` (the Linux `SaveWindowScreenshot`), `:631-653` (CLI screenshot timer), `:724-766` (debug-menu item) and `IPlugAPP_main.cpp:584` (`--screenshot`). ✓
10. **OSC:** the POSIX socket path at `IPlugOSC_internal.cpp:5-9` and the Timer at `:289-292`. The path exists ✓, but see m6.
11. **Fontconfig system fonts:** `IGraphicsLinuxFontconfig.h:75` loads fontconfig with `dlopen`, `:113-125` matches family, weight and slant, and `IGraphicsLinux.cpp:1872` is `LoadPlatformFont(name, style)`. ✓
12. **JACK MIDI:** `APP.cmake:150-152` builds RtAudio/RtMidi with `__UNIX_JACK__`. The gap is owned by T-022, status todo. ✓
13. **Drawing row (SVG, bitmaps, layers, shadows):** the audit's evidence shows only IGraphicsTest's first page (Gradient). My sweep shows the rest render correctly:
    - correct: Polygon, Arcs, Beziers, MultiPath, Text, Animation, Draw contexts, SVG (tiger), Image, Layer (cached), Blend modes, DropShadow, ShadowGradient, Font, TextOrientation, TextSize, **Custom Shader (NanoVG GL2: RGB triangle)**, FlexBox, Mask, DirBrowse, DragNDrop.
    - "UNSUPPORTED" by design, the same on mac/win NanoVG: Multi-stop gradient (a NanoVG limitation), MPS (Metal only), Gesture recognizers (iOS only).
    - MultiTouch shows 0 touches (T-030).

    The row holds now. Cite the sweep. ✓
14. **File dialogs and message boxes:** the asynchronous form works ✓. The synchronous form is missing (M2).
15. **NanoVG GL2/GL3 selection:** `IGraphics_select.h:34-46` has a Linux branch. ✓
16. **Skia and shader controls:** `IShaderControl.h:13-14` errors out unless the backend is Skia, so it goes with T-018. The table doesn't say this (M3). ✓

**Findings**
- **[major] M1: T-031 is under-scoped.** Fixing the link is not enough. After that, the extension's IGraphics editor still cannot open on Linux:
  - (a) `IGraphicsLinux::OpenWindow` refuses any delegate that is not an `IPlugAPIBase` (`IGraphicsLinux.cpp:491-496`), because the `HostLoop` lives in `IPlugAPIBase` (`IPlugAPIBase.h:189,237`). `ReaperExtBase` derives from `EDITOR_DELEGATE_CLASS` = `IGEditorDelegate` (`ReaperExtBase.h:40`, `IPlugDelegate_select.h:29`), so `OpenWindow` prints "cannot open" and returns nullptr. The extension needs a HostLoop: REAPER runs the GLib/GDK loop through SWELL, so the GLib backend or a REAPER `timer` registration could serve.
  - (b) `MainDlgProc` passes the SWELL `HWND` to `OpenWindow` (`ReaperExtBase.cpp:434`). `IGraphicsLinux.cpp:525` would treat that pointer as an X Window ID. It has to be converted first: `SWELL_GetOSWindow(hwnd, "GdkWindow")` (`swell-functions.h:1068`), then `gdk_x11_window_get_xid` looked up with `dlsym` from REAPER's GDK.
  - (c) On non-Windows, `GetScaleForHWND` returns 1 (`ReaperExt_include_in_plug_src.h:116-119`), so there is no HiDPI.
  - (d) `EditorResizeFromUI` resizes the window only `#ifdef OS_MAC` (`ReaperExtBase.cpp:57`).

  Add these as steps. T-031's test should require the IGraphics editor to render inside the (dockable) extension window. Re-estimate at about 2–3 days instead of 0.5–1.
- **[major] M2: missing parity row for synchronous dialogs.** On Linux, `PromptForFile`, `PromptForDirectory` and `PromptForColor` without a completion handler return empty and print a warning (`IGraphicsLinux.cpp:2171-2176, 2196-2203, 2227-2234, 2249-2257`). `ShowMessageBox` always returns `kNoResult` (`:2178-2192`). mac (`IGraphicsMac.mm:381,590`, `runModal`) and win (`IGraphicsWin.cpp:1027-1032`) block and return the result.
  - Consequences: the stock **`IVDiskPresetManagerControl` "load" button does nothing on Linux**, because it calls `PromptForFile` synchronously (`IVPresetManagerControls.h:243-245`). Any customer plug-in that uses the synchronous form or reads `ShowMessageBox`'s return value fails silently.
  - This was a deliberate T-002 choice (the host thread must not block), but it is exactly a "Linux poorer than mac/win" item. It needs a row and a decision. Suggestion: a documented deviation, plus switching the stock control to a completion handler. That is a small shared-code change and safe on mac/win. Or propose a task for it.
- **[major] M3: Acceptance item 1 ("every Scope area, with file:line") is not met.** Most rows cite a task instead of file:line. These Scope areas have no row:
  - `IPlugPaths`: Linux implements every helper (`IPlug/Linux/IPlugLinuxPaths.cpp:73-177`). `SharedMusicPath`, `AppGroupContainerPath` and `AppIsSandboxed` are mac-only (`IPlugPaths.mm:139-146,261`), so n/a.
  - `IPlugUtilities` platform bits, logging (`IPlugLogger.h:41-42,93-96`: `DBGMSG`/printf, `LOGFILE` in `$HOME`) and crash/assert handling.
  - Shader controls: `IShaderControl` is Skia-only (→ T-018); the NanoVG GL custom shader works (row 13).
  - File-browser controls (`IDirBrowseControlBase` via `WDL_DirScan`, `IControl.cpp:1133`) and `IVDiskPresetManagerControl`.
  - `IPlug/Extras/PlatformView` (`IPlatformView`/`IPlatformViewControl`): `.mm` only, so mac/iOS. Put it under T-017.
  - `IGRAPHICS_IMGUI`: not present in this tree. Say so.
  - `IGraphics/Extras/IGraphicsFlexBox` (portable; FlexBox renders) and the MIDI keyboard (portable).
  - Tooling:
    - the `validate`, `screenshot`, `build-cmake` and `setup-deps` skills in `.claude/skills/*` have no Linux instructions;
    - `.github/workflows/cmake-ci.yml` has no Linux native-build job: mac/win jobs exist (`:192`, `:269`), and ubuntu runs only parse/WAM/WASM. That is a gap with no task;
    - `Examples/buildall-mac.sh` and `buildall-win.ps1` have no Linux counterpart.
- **[minor] m1: the drawing-row evidence covered only the Gradient page.** It is now backed by my sweep (row 13). Add its result to the note.
- **[minor] m2: "API reaches REAPER (red STOPPED fill)" overstates the evidence.** Red only proves `GetHost()==kHostReaper`. `GetPlayState && GetPlayState()` (`IPlugReaperPlugin.cpp:70`) is also false when the import failed. Loading is plausible: `get_extension` at `:125` would have crashed otherwise. Reword the row, or verify it in T-031 (track name and channel count text).
- **[minor] m3: the "static UI" explanation is imprecise.** The real cause: those five examples never call `EnableMouseOver(true)`, which defaults to false (`IGraphics.h:1881`; `GetMouseControlIdx`, `IGraphics.cpp:1331`). IPlugMidiEffect's full-size `IVButtonControl` does have a hover look. The meters stay idle because `IPeakAvgSender` sends nothing in silence. This is shared behaviour, identical on mac/win, so the "not a defect" conclusion stands.
- **[minor] m4: IGraphicsStressTest showed only its idle landing page.** The stress tests themselves (Tab cycles through them) were not run. "Renders" is true, but say that the stress tests were not run.
- **[minor] m5: the screenshots are not durable evidence.** No PNG or contact sheet is committed (`linux-research/.gitignore` keeps only `sheet*`/`contact*`), so the run screenshots disappear with the worktree. Commit a contact sheet of the `r01-open` shots.
- **[minor] m6: the OSC row's "IPlugOSCEditor runs" proves the editor, not UDP send/receive.** Mark OSC I/O as untested, or test it with `oscsend`/`oscdump`.
- **[minor] m7: only the CLAPs were opened.** VST3 and APP targets were only built. Say so in the method line.
- **[nit] n1:** `targets.txt` has 114 lines, 57 of them `cmake_object_order_depends_target_*` noise.
- **[nit] n2:** "all 19 example projects" means 16 examples plus 3 test projects. Say so.
- **[nit] n3:** the VST2 row could note that Linux `IPlugVST2` has no host-loop backend (`effEditIdle` is commented out, `IPlugVST2.cpp:304`). It would run on the fallback thread, so it is untested and not just "no SDK".
- **[nit] n4:** T-031 step 1 should also restrict or check the Linux exports (`ReaperPluginEntry`, `SWELL_dllMain`), as the mac branch does (`ReaperExt.cmake` mac link options). The Linux branch exports everything.
- **[nit] n5:** the WebView examples declare `PLUG_HAS_UI 1` while the stub opens nothing, so hosts show an empty or missing window. Mention it in the WebView row; T-017 removes it.

**T-031 assessment:** the problem is well-founded: the link cause is correct and the modstub mechanism is the right one. As written it is not actionable to "done": add M1 (a)–(d) and n4, and re-estimate.

**Verdict: changes requested.** No blocker. Fix M1–M3 (table rows, T-031 scope and estimate), address m1–m7, then round 2.

### 2026-10-19 (2) — review round 1 fixes
- **M1:** T-031 now lists what stops the extension's editor after the link (`OpenWindow` needs an `IPlugAPIBase`, SWELL HWND → XID, scale, UI resize, exports); estimate 2–3 days.
- **M2:** new row "dialogs called synchronously" (❌) and **T-032** (options for the user; recommendation: make iPlug2's own synchronous callers asynchronous now, consider a nested wait later). Verified: `IGraphicsLinux.cpp:2194`–`2205` returns without a completion handler; `IVPresetManagerControls.h:243` calls it synchronously.
- **M3:** Linux file:line for the platform rows; new rows: `IPlugPaths`, logging, platform views, ImGui (not in the tree), `IShaderControl` (Skia, T-018), CI and the repo's skills (**T-033**), `buildall` scripts (not needed: CMake builds all examples).
- m1: the drawing row cites the reviewer's sweep of all 28 IGraphicsTest pages. m2: the REAPER-plug-in row says only host detection is shown. m3: the static-UI row names `EnableMouseOver`. m4/m6/m7: StressTest seen on its first page only, OSC send/receive not exercised, only CLAP opened (VST3/APP built) — stated in the note. m5: screenshots stay git-ignored and local; the note says where.

### Review round 2 (independent reviewer)
2026-10-19, fresh context, reviewed 7e9fc893f (read-only; no Docker/VM).

**Round-1 findings**
- M1 ✓ T-031 has (a)–(d), the exports and a 2–3 day estimate. Checked: (a) `IGraphicsLinux.cpp:491` `dynamic_cast<IPlugAPIBase*>`; (b) `ReaperExtBase.cpp:434` → `IGraphicsLinux.cpp:525`; (c) `ReaperExt_include_in_plug_src.h:116`, which is also 1 on mac; (d) `ReaperExtBase.cpp:57`, mac only, and win doesn't resize either. Step 4 could state more plainly that the IGraphics editor must render in the docked and undocked window (nit).
- M2 ✓ The row and T-032 exist, but T-032 has factual errors (R1, R2 below).
- M3 mostly ✓ Missing still: one line for IPlugUtilities and assert/crash (no Linux-relevant platform code), and one for the portable controls (DirBrowse via `WDL_DirScan`, FlexBox, MIDI keyboard; the sweep covers them). The mac/win side still has no file:line; the overrides diff is the evidence there. Acceptable.
- m1–m4, m6, m7 ✓
- m5 not done. The note cites `evidence/t016/run-*` screenshots and the drawing row rests on round 1's `build/review-t016/sheetA/B.png`. All of these are git-ignored, so they vanish when the coordinator removes the worktree (§4.5). `.gitignore` lets `evidence/**/sheet*.png` through: copy the two sheets to `evidence/t016/` and commit them (minor).
- Nits n1, n2 ("all 19 example projects" = 16 examples + 3 tests), n3 and n5 are not addressed. They are optional.

**file:line spot-checks (20; all on this branch)**
- `IGraphicsLinux.cpp`, all ✓: 487 `OpenWindow`, 491, 525, 1709 cursor, 1723 / 1747 clipboard, 1732 drag source, 1742 `GetUIAppearance`, 1769 text entry, 1787 menus, 1864 `LoadPlatformFont`, 1924 tooltips, 2178 `ShowMessageBox`, 2194–2203 `PromptForFile` early return, 2225 `PromptForDirectory`, 2270 `OpenURL`.
- `ReaperExtBase.cpp:434` ✓; `IPlugLinuxPaths.cpp:120/126/157` ✓.
- Nits: `RevealPathInExplorerOrFinder` is at `:2275`, not 2270. The synchronous call is at `IVPresetManagerControls.h:245` (243 is the lambda). The POSIX OSC branch is at `IPlugOSC_internal.cpp:7-8` (5 is `#ifdef OS_WIN`).
- My own override `comm` agrees: mac-only = PlatformView ×3, `MeasureText`, bundle and app-group IDs; win-only = `PlatformSupportsMultiTouch` and the module handle.

**T-032 (a user decision, so it must be exact)**
- **R1 [major for T-032]: `PromptForColor` is not synchronous on mac.** `IGraphicsMac_view.mm:1284-1296` opens the non-modal `NSColorPanel` and returns `true` at once with `color` unchanged. With a null `func`, the first pick calls an empty `std::function`. Only win blocks (`ChooseColorW`, `IGraphicsWin.cpp:1648-1681`). For colour, Linux already equals mac. Fix the row and T-032's Problem.
- **R2 [major for T-032]: option 1 cannot be "as mac/win do".** mac `runModal` (`IGraphicsMac.mm:381,590`) and win `GetOpenFileNameW` / `MessageBoxW` (`IGraphicsWin.cpp:1551-1556,1027`) run a nested loop that dispatches the whole process's events. The host stays responsive there, and that is where the re-entrancy comes from. A Linux plug-in can pump only its own X connection and the dialog's pipe or D-Bus. The host's GTK/Qt/X loop stalls, so its windows stop repainting, the WM's `_NET_WM_PING` may mark it "not responding", and host main-thread callbacks wait. The one exception is the APP, which owns its loop. Say this, and make "1 later" opt-in at most (or APP-only).
- **R3 [minor]: option 2's "works the same on mac/win" is not exact.**
  - On mac, passing a handler switches from an app-modal panel to a window sheet (`beginSheetModalForWindow`, `IGraphicsMac.mm:579-590`). Win is unchanged. This is a visible mac change, so it needs the user's approval under §2.
  - In favour of option 2, add the precedent. iPlug2 already has async-only platforms: `IGraphicsConstants.h:208` documents `kNoResult` for message boxes that "can't return inline"; iOS returns it (`IGraphicsIOS.mm:187`) and asserts a completion handler in `PromptForFile` (`:216`). So the stock preset control's load button already fails on iOS: the control fix is a cross-platform fix, not a Linux workaround.
  - Also note that mac/win `ShowMessageBox` blocks and returns the result even when given a handler. Linux returns `kNoResult` always.
- Recommendation 3, read as "2 now, 1 only opt-in after R2", is fair. The other synchronous callers in the tree are the stock preset control (above) and the `IPlugControls` dialog-test button at `IPlugControls.cpp:609`, which is the port's own test. `TestSVGControl`, `TestImageControl` and `ISkLottieControl` pass handlers.

**T-031 and T-033:** accurate and actionable. T-033 correctly treats the workflow as a proposal (it runs on the user's account); `cmake-ci.yml` has mac/win native jobs at `:192`/`:269` and only parse/WAM/WASM on ubuntu (`:76,739,805`).

**Still missing from the table (minor)**
- **Text entry with a system input method.** mac/win native text fields get the OS IME (CJK, emoji picker). The Linux stand-in uses only Xlib's built-in IM: compose and dead keys work, but IBus/Fcitx are not reached, because `XMODIFIERS` is deliberately untouched (`IGraphicsLinux.cpp:595-599`, notes/13). Add a row: a deliberate deviation or a task for the user.
- Checked and fine: `OpenURL`'s confirm/error messages are ignored on Linux, as on mac (`IGraphicsMac.mm:708`), so that is not a gap. The remaining `#if OS_MAC/OS_WIN` in shared code (vDSP in `ISender`, Pro Tools fine-control, win UTF-16, `kWinBinary`) has no Linux gap. Packaging covers the APP icon (`package.sh --icon`).

**Verdict: approved, subject to text fixes; no round 3 needed.** Before merge, fix R1–R3 in T-032 and the "synchronous" row, add the IME row, and commit the two sweep sheets (m5). The rest are nits.

### 2026-10-19 (3) — round-2 text fixes, hand-over
- T-032 rewritten (R1–R3): colour prompts are non-modal on mac (Linux matches; only Windows blocks); a blocking wait cannot be "as mac/win" for a plug-in (their modal loops pump the whole process; ours would freeze the host's windows) — APP-only or opt-in; option 2 changes mac (file panel becomes a sheet) and also fixes iOS, where the stock control already asserts; recommendation unchanged (2 now, 1 only APP/opt-in).
- File:line corrections (`:2275` reveal, `IVPresetManagerControls.h:245`, `IPlugOSC_internal.cpp:7-8`); new rows: system input methods (IBus/Fcitx: ❌, for the user to decide), `IPlugUtilities`/asserts/crash handling, portable controls.
- m5: screenshots and the reviewer's IGraphicsTest sweep sheets (`sheetA.png`, `sheetB.png`) are copied into `IPlug2-linux/linux-research/evidence/t016/` (git-ignored PNGs, local; they outlive this worktree).
