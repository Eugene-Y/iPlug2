# Existing iPlug2 Linux efforts (web research, 2026-10-10)

Raw notes from research agents + local git analysis. [V] = verified in source/API, [I] = inference.

## Upstream master
- No Linux. `IGraphics/Platforms/IGraphicsLinux.h` is a stub that does not compile (`virtual ~IGraphicsLinux);`).
- CMake (`Scripts/cmake/*.cmake`, upstream PR #1300, 2025-12) has `elseif(UNIX AND NOT APPLE)` placeholders: "Linux support - to be added later". `Deploy.cmake` already knows `~/.vst3`, `~/.clap`.
- README lists Windows, macOS, visionOS, iOS only.

## Upstream PR #1336 — RangerRick (Benjamin Reed) — main candidate
- https://github.com/iPlug2/iPlug2/pull/1336, branch https://github.com/RangerRick/iPlug2/tree/feature/linux-support
- Opened 2026-03-07, last commit 2026-06-09, last comment 2026-08-22. 25 commits, 48 files, +3561/−291. mergeable_state: dirty (conflicts). ~57 behind master. "Closes #105".
- Scope (PR description): `IGraphicsLinux` (~1600 lines): X11 child window, GLX GL3, NanoVG into FBO, **pthread timer ~60fps**, X11 clipboard, cursors, tooltips; GTK3 file/message dialogs via dlopen. APP (SWELL/GDK, RtAudio ALSA/JACK), VST3 (X11 embed), CLAP (posix-fd + timer). CMake presets `linux-ninja`, `linux-make`; fontconfig dlopen; static libstdc++. Not: Skia, native Wayland, WebView, LV2.
- Evidence: author tested Renoise (VST3), REAPER (VST3+CLAP) on Ubuntu 25.10. Oli Larkin 2026-08-05: "I had to go with this today on a Wayland machine and it's great!".
- Problems reported: bsvi (Debian 13/KDE/Xorg): GTK popups/file/color pickers hang; live-edit/FPS debug broken; IPlugInstrument keyboard resize. 2026-08-22 bsvi: "very buggy … convoluted", switched to DPF. TomTrnk: threaded Linux `IPlugTimer` breaks the "callback on calling thread" guarantee. AlexHarker asked about merge status / Skia — no answer.
- Fork issues: #65 VST3 should register X11 fd/timers with host `IRunLoop` (known gap); #64 state via env vars (objected); #66 LV2; #79 path hardening.

## stillwellaudio/iPlug2 — commercial, most active
- https://github.com/stillwellaudio/iPlug2, last push 2026-10-09. Scott Stillwell. Product branch ~503 behind master.
- Skia + VST3 proof-of-concept (Event Horizon), builds on older Debian for glibc. Own CMake/Python scripts.
- Branches: `stillwell/linux-vst3-current` (2026-08-12: "Implement Linux X11 plugin editors", "interruptible timers", Skia mutex), `stillwell/linux-clap-current`, `fix/linux-clap-timer-lifecycle-20260917`, `fix/linux-text-entry`, `fix/linux-context-menu-opt-out-20260917`.
- Fork PR #1 (merged 2026-09-17): drives Linux VST3 editors from the **host run loop**; releases pointer capture before host menus. https://github.com/stillwellaudio/iPlug2/pull/1

## TomTrnk/iPlug2bn `linux-tomtrnk`
- https://github.com/TomTrnk/iPlug2bn/commits/linux-tomtrnk — last push 2026-01-27, on top of Bindernews `linux`, 513 behind.
- Ships Deimos synth Linux **CLAP** (https://tom-trnk.itch.io/deimos). VST3 builds but hosts don't recognise it. Native popup/text-entry not there — use IGraphics AttachPopupMenuControl/AttachTextEntryControl. Tested Bitwig/Ubuntu 24.04. Dormant.

## Bindernews (Drew H / Andrew Heintz)
- https://github.com/Bindernews/iPlug2 — `linux` branch (2025-07), own Python/CMake (`Scripts/iplugpy`, `bn_*`) incompatible with upstream CMake; `linux-sdl3` (2025-12) SDL3 experiment.
- Upstream `linux-bindernews` (2025-12-14, Oli + Claude Opus 4.5 "Based on work by @BinderNews") [V local]: PlatformX11 (Xlib, SDL-style events), glad_GLX, fontconfig, zenity dialogs, CMake. **UI is driven by a private background thread** (`IPlugTaskThread`, 10 ms) — not the host thread. `IPlugVST3_View::isPlatformTypeSupported` and `IPlugCLAP::guiIsApiSupported` have no X11 → only APP/CLI work.

## Upstream `linux` branch (2019–2022) [V local]
- xcbt (Alexey Zhelezov/azslow3, XCB toolkit, zlib licence) with an "embed" abstraction: VST3 `IPlugVST3_RunLoop` (IRunLoop registerTimer/registerEventHandler), GLib embed (REAPER/SWELL), idle embed. LV2 wrapper. 1731 behind master. Forum: VST3/CLAP build, text entry/menus unfinished.

## Other
- BlueLab plugins (GPL, dead ~2022) shipped Linux iPlug2 builds; fork 404.
- Upstream `feature/cli-backend-linux*` (Oli, 2025-12): headless CLI target + screenshot renderer, Linux-capable; PR #1324 draft.
- iPlug3 (https://github.com/iPlug3, started 2026-01): new framework (MPLUG/MGFX, WebGPU/Skia Graphite/SDL3), roadmap lists Linux; code not public. Upstream's long-term Linux likely lands there [I].
