# T-016 — Feature-parity audit: Linux vs mac/win

- **Status:** review next
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
