# Feature parity with mac/win (user requirement, 2026-10-16)

The user: the framework must be fully capable on Linux; Linux must not be poorer than mac or win. This replaces
PLAN §1's "out of scope for v1: Skia, WebView". Every gap below gets a task; none is dropped as "won't do"
without the user's say.

## Parity table (T-016 audit, 2026-10-19, hvoya-linux 238580eaa)

Scope of the runtime check: CLAP opened in REAPER; VST3 and APP only built. Screenshots are git-ignored (local in the T-016 worktree, `evidence/t016/run-*`).

Method: every `override` in `IGraphicsMac.h`/`IGraphicsWin.h` diffed against `IGraphicsLinux.h`; every `#if OS_MAC/OS_WIN` outside `IGraphics/Platforms`; formats, `IPlug/Extras`, the APP host; **every example and test project built** (57 targets, all PASS, `evidence/t016/build-results.txt`) **and opened in REAPER** as CLAP (`evidence/t016/run-all.txt`, per-project logs and screenshots). ✅ = works on Linux (task that did it), ❌ = gap (task), n/a = no Linux equivalent needed.

### IGraphics platform layer
| Feature | mac | win | Linux | Task |
|---|---|---|---|---|
| Editor window, embed, resize, scale (HiDPI) | ✓ | ✓ | ✅ X11 child, host scale, `Xft.dpi` (`IGraphics/Platforms/IGraphicsLinux.cpp:487` `OpenWindow`) | P3, T-007 |
| Mouse, keyboard, cursors, text entry, popup menus | native | native | ✅ IGraphics stand-ins for menus and text entry, no GTK in the plug-in (`IGraphics/Platforms/IGraphicsLinux.cpp:1709` cursor, `:1769` text entry, `:1787` menus) | P3, T-008 |
| Clipboard text / file path | ✓ / ✓ | ✓ / ✓ | ✅ / ✅ (`IGraphics/Platforms/IGraphicsLinux.cpp:1723`, `:1747`) | P3 / T-020 |
| Files dropped onto the editor / dragged out | ✓ / ✓ | ✓ / ✓ | ✅ XDND target (`IGraphicsLinuxDrop.h`) / ✅ XDND source (`IGraphics/Platforms/IGraphicsLinux.cpp:1732`, `IGraphicsLinuxDragSource.h`) | T-019 / T-020 |
| Tooltips | ✓ | ✓ | ✅ own override-redirect window (`IGraphics/Platforms/IGraphicsLinux.cpp:1924`) | T-019 |
| File and folder dialogs, message boxes (asynchronous, with a completion handler) | ✓ | ✓ | ✅ portal / zenity / kdialog (`IGraphics/Platforms/IGraphicsLinux.cpp:2194`, `:2225`, `:2178`) | T-002 |
| The same dialogs **called synchronously** (no completion handler: mac/win block and return the result) | ✓ | ✓ | ❌ return at once with nothing (`IGraphics/Platforms/IGraphicsLinux.cpp:2194`–`2205`; message boxes return `kNoResult`); e.g. the stock `IVDiskPresetManagerControl` load button does nothing (`IGraphics/Controls/IVPresetManagerControls.h:243`) | T-032 (user decision) |
| `OpenURL`, `RevealPathInExplorerOrFinder` | ✓ | ✓ | ✅ xdg-open (`IGraphics/Platforms/IGraphicsLinux.cpp:2270`) | P3 |
| Dark/light appearance (`GetUIAppearance`) | ✓ | — | ✅ xdg-desktop-portal (`IGraphics/Platforms/IGraphicsLinux.cpp:1742`, `IGraphicsLinuxAppearance.h`) | T-020 |
| Platform child views (`AttachPlatformView`…) | ✓ | — | ❌ | T-017 |
| Multi-touch (`PlatformSupportsMultiTouch`) | — | ✓ | ❌ | T-030 |
| `MeasureText` override | pass-through | — | not needed (same base implementation) | — |
| Bundle ID / App Group ID, `Get/SetWinModuleHandle` | ✓ | ✓ | n/a (mac/win concepts) | — |
| Resources | bundle | `.rc` in DLL (`kWinBinary`) | ✅ per-module `<Name>.resources/` | P1, T-021 |
| AAX modifier mapping, Pro Tools keys | ✓ | ✓ | n/a (no AAX on Linux) | — |

### Drawing
| Feature | mac | win | Linux | Task |
|---|---|---|---|---|
| NanoVG GL2 | ✓ | ✓ | ✅ (default) | P3 |
| NanoVG GL3 | ✓ | ✓ | ✅ 3.2 core context | T-020 |
| NanoVG Metal | ✓ | — | n/a | — |
| Skia (GL / CPU) | ✓ | ✓ | ❌ | T-018 |
| Fonts: resources, memory, system by name | ✓ | ✓ | ✅ (`IGraphics/Platforms/IGraphicsLinux.cpp:1864` `LoadPlatformFont`, fontconfig in `IGraphicsLinuxFontconfig.h`) | P3 |
| SVG, bitmaps @2x, layers/FBOs, shadows, masks, NanoVG GL shader | ✓ | ✓ | ✅ all 28 IGraphicsTest pages render in REAPER (the review's sweep); pages marked UNSUPPORTED are the same on mac/win (multi-stop gradients in NanoVG, Metal MPS, iOS gestures) | P3, this audit |
| `IShaderControl` | Skia | Skia | ❌ with Skia | T-018 |

### Formats and IPlug
| Feature | mac | win | Linux | Task |
|---|---|---|---|---|
| CLAP, VST3, standalone APP | ✓ | ✓ | ✅ all 19 example projects build | P0–P8 |
| VST2 (deprecated) | ✓ | ✓ | not built (no SDK here; deprecated) | — |
| REAPER-API plug-in (`IPlugReaperPlugin`) | ✓ | ✓ | ✅ builds and detects REAPER as host (red fill); API import and its text not shown yet: the example's CMake ships no fonts on Linux | T-031 |
| REAPER extension (`IPlugReaperExtension`, `…WebUI`) | ✓ | ✓ | ❌ link fails (SWELL modstub missing in `Scripts/cmake/ReaperExt.cmake`'s Linux branch); after that the editor would still not open: `OpenWindow` needs an `IPlugAPIBase` delegate (`IGraphics/Platforms/IGraphicsLinux.cpp:491`), the SWELL HWND is passed as an X window (`IPlug/ReaperExt/ReaperExtBase.cpp:434`), no HiDPI, UI resize mac-only | T-031 |
| LV2 | — | — | planned (Linux extra) | T-014 |
| AU, AUv3, AAX, WAM | ✓ | (AAX) | n/a | — |
| `IPlug/Extras`: OSC | ✓ | ✓ | ✅ POSIX branch (`IPlug/Extras/OSC/IPlugOSC_internal.cpp:5`); IPlugOSCEditor opens; OSC send/receive itself not exercised | — |
| `IPlug/Extras`: WebView editor / `IWebViewControl` | ✓ | ✓ | ❌ stub (IPlugWebUI, IPlugP5js, IPlugSvelteUI open no editor) | T-017 |
| `IPlug/Extras`: Cocoa, SwiftUI | ✓ | — | n/a (Apple UI kits) | — |
| `IPlug/Extras`: DSP (Synth, HIIR, Oversampler, SVF, …) | ✓ | ✓ | ✅ portable | — |

### Other IPlug services
| Feature | mac | win | Linux | Task |
|---|---|---|---|---|
| `IPlugPaths` (home, app support, INI, presets, resources) | ✓ | ✓ | ✅ XDG (`IPlug/Linux/IPlugLinuxPaths.cpp:120`, `:126`, `:157`) | P1, T-021 |
| Logging / `DBGMSG` / `Trace` (`IPlug/IPlugLogger.h`) | ✓ | ✓ | ✅ stderr, same code | — |
| Platform views (`IPlug/Extras/PlatformView`) | ✓ | — | ❌ | T-017 |
| ImGui (`IGRAPHICS_IMGUI`) | — | — | not in this tree | — |

### Standalone APP host
| Feature | mac | win | Linux | Task |
|---|---|---|---|---|
| Audio (RtAudio) | CoreAudio | ASIO/WASAPI | ✅ ALSA, JACK, PulseAudio; starts without a device | T-004 |
| MIDI (RtMidi) | CoreMIDI | WinMM | ✅ ALSA sequencer; ❌ JACK MIDI with the JACK driver | T-004 / T-022 |
| Settings dialog, menus, shortcuts | ✓ | ✓ | ✅ SWELL | T-004 |
| Debug menu incl. "Save Screenshot" | ✓ | ✓ | ✅ (`IPlugAPP_dialog.cpp` Linux branch) | T-004 |

### Example and test projects (REAPER, CLAP; `evidence/t016/run-*-clap*`)
| Project | Build (APP/CLAP/VST3) | Editor in REAPER | Note |
|---|---|---|---|
| IGraphicsTest, IPlugControls, IPlugDrumSynth, IPlugEffect, IPlugInstrument, IPlugOSCEditor, IPlugResponsiveUI, IPlugVisualizer, MetaParamTest | ✓ | ✓ PASS (render, reopen ×5, second instance) | |
| IGraphicsStressTest, IPlugChunks, IPlugMidiEffect, IPlugSideChain, IPlugSurroundEffect | ✓ | ✓ renders | the harness's "Draw hits while the mouse moves" fails: these never call `EnableMouseOver(true)` (off by default, shared code), so nothing redraws on hover — same on mac/win, not a defect. StressTest only seen on its first page |
| IPlugConvoEngine | ✓ | no editor | by design: `NO_IGRAPHICS`, `PLUG_HAS_UI 0` |
| IPlugWebUI, IPlugP5js, IPlugSvelteUI | ✓ | no editor | WebView stub | 
| IPlugReaperPlugin | ✓ | red fill, no text | missing Linux resources (fonts) | 
| IPlugReaperExtension, IPlugReaperExtensionWebUI | ✗ link | — | SWELL modstub missing |
| IPlugCocoaUI, IPlugSwiftUI | not configured | — | Apple only |

Tests `Tests/Linux*` (host loop, dialogs, input, paths, scale) are the port's own unit tests (P2–T-007).

### Tooling
| Item | mac | win | Linux | Task |
|---|---|---|---|---|
| CI (`.github/workflows/cmake-ci.yml`) | ✓ | ✓ | ❌ no native Linux build job | T-033 |
| Repo skills (`.claude/skills/`: `validate`, `screenshot`, `build-cmake`, `setup-deps`, …) | ✓ | ✓ | ❌ no Linux instructions | T-033 |
| `Examples/buildall-mac.sh`, `buildall-win.ps1` | ✓ | ✓ | — (CMake builds all examples; a `buildall-linux.sh` is not needed) | — |


`Examples/duplicate.py` out-of-tree paths and Linux IDE files: T-011. Packaging (`Scripts/linux/package.sh`, clean-distro smoke): ✅ T-021. Docs: `Documentation/linux-development.md` ✅; repo README last (T-015).

### New tasks from this audit
- **T-031** — REAPER extensions and the REAPER-plug-in example on Linux (link via SWELL modstub, then an editor that opens: HostLoop for a non-`IPlugAPIBase` delegate, SWELL HWND → XID, scale, resize; the example's resources). 2–3 days.
- **T-032** — synchronous dialogs: a decision for the user (nested wait as mac/win, or document + make iPlug2's own callers asynchronous).
- **T-033** — Linux in CI (`cmake-ci.yml`) and Linux instructions in the repo's skills.

Not gaps: AU/AUv3/AAX (no Linux hosts), native OS menus and text entry (IGraphics stand-ins, a deliberate choice:
no GTK in the plug-in process), native Wayland embedding (mac/win have no equivalent; Linux hosts embed X11 and
run under XWayland). LV2 is Linux-only extra (P12).

## WebView: the hard one — options (user chose 1, 2026-10-16)

The only system web engine on Linux is WebKitGTK, which needs GTK in its process. That collides with PLAN §1
"no GTK in the plug-in process": hosts built on GTK2 (Ardour's ytk) or another GTK major crash or misbehave when a
plug-in loads GTK3/4, and GTK wants its own main loop on the main thread.

1. **Out-of-process helper (recommended).** A small helper executable (shipped in `<Name>.resources/` or shared)
   runs WebKitGTK and embeds its window into our X11 child via XEmbed (`GtkPlug`). JS↔C++ messages and
   `LoadFile`/`LoadURL`/`EvaluateJavaScript` go over a socket pumped by the host loop. No GTK in the host; a web
   crash does not take down the DAW. JUCE's Linux web browser component uses this design. Cost: IPC layer,
   XEmbed focus/keyboard, resize sync; the user's system needs `libwebkit2gtk-4.1` (in every mainstream distro).
2. In-process WebKitGTK. Simplest, fine for the standalone APP (it already runs GDK via SWELL), unsafe in hosts
   for the reasons above.
3. CEF (Chromium). Self-contained but ~150 MB per install and multi-process anyway; no.
