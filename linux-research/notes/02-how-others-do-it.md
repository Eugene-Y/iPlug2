# How other frameworks do Linux plugin UI (research, 2026-10-10)

[V] verified in source (clones in scratchpad `repos/`: JUCE, DPF, pugl, vstgui, visage, baseview, CPLUG, clap), [I] inference.

## Windowing
- CLAP: `CLAP_WINDOW_API_X11` (XEmbed, physical px). `CLAP_WINDOW_API_WAYLAND` exists but "embed not supported, use floating" [V].
- VST3: `kPlatformTypeX11EmbedWindowID`. **VST3 SDK 3.8 (MIT, 2025-10)** adds `kPlatformTypeWaylandSurfaceID` + `iwaylandframe.h` (`IWaylandHost::openWaylandConnection`, host acts as nested compositor; plugin makes wl_subsurface). Used by Studio One 7.2+; VSTGUI 4.15 preliminary [V].
- Hosts: Ardour 9.8 — VST3+LV2, **no CLAP**, X11/XWayland; REAPER — SWELL/GTK3 under XWayland; Bitwig/Carla — X11 assumed [I].
- → Ship X11 only; keep windowing abstract for a later VST3-Wayland backend.

## Event loop (hardest part)
- VST3 `Linux::IRunLoop` (from IPlugFrame): registerEventHandler(fd)/registerTimer(ms), all on host UI thread. CLAP `posix-fd-support` + `timer-support`, `[main-thread]`.
- **Rule: no plugin-owned GUI thread. All X work inside host callbacks.**
- DPF: simplest — no fd, just a **16 ms host timer** (VST3 + CLAP), each tick `puglUpdate()` drains X events + redraws [V].
- JUCE: background MessageThread until host IRunLoop appears, then switches to host thread and registers all fds [V].
- VSTGUI: XCB, registers xcb fd via its IRunLoop → host IRunLoop [V].
- Visage: X11Connection per window, posixFd() for wrapper; plus a std::thread that XSendEvents a ClientMessage per frame [V].
- baseview (nih-plug): own X11 thread + HostMainThreadCaller; X Present for frame pacing — deliberately breaks main-thread model [V].
- Pitfalls: libX11 ≥1.8 calls XInitThreads in ctor; plugins must not call it [V]. Always drain `while(XPending)` in fd handler and timer [I]. Timer-only fallback for hosts lacking posix-fd [I]. One Display per editor is simplest (hidden symbols mean different plugins never share anyway) [I].

## OpenGL / NanoVG
- pugl: GLX FBConfig + glXCreateContextAttribsARB fallback; swap interval only if hinted [V]. JUCE moved to **EGL** on X11 (`eglGetPlatformDisplay(EGL_PLATFORM_X11)`) [V].
- Prefer EGL (same path for future Wayland), GLX fallback [I]. **Swap interval 0 + host timer**; vsync blocks host main thread × N editors [I]. One context per editor, no sharing; make-current/restore around draw [I].

## Skia
- Skia `tools/window/unix/`: Raster+XPutImage, Ganesh GL (GLX), Vulkan [V]. Order: raster+MIT-SHM → Ganesh/EGL → Vulkan [I].

## Fonts / clipboard / cursor / keys / HiDPI
- NanoVG stb_truetype loads embedded TTF; fontconfig only for system family names [I].
- Clipboard = X selection protocol (XConvertSelection + SelectionNotify, INCR; owner serves SelectionRequest) [V pugl].
- Cursors: Xcursor theme names, blank pixmap to hide [V].
- Focus: _XEMBED handling (VSTGUI x11frame), `_XEMBED_INFO`, XSetInputFocus on click; hosts implement XEmbed partially [V/I].
- HiDPI: physical px on X11; CLAP gui.set_scale, VST3 IPlugViewContentScaleSupport; else Xft.dpi/96, GDK_SCALE [V].

## Dialogs — no GTK in-process
- GTK in plugin: ABI clash with host's GTK (GTK2+3 abort), Ardour crashes with GTK3 UIs [V].
- JUCE & VSTGUI: zenity/kdialog subprocess. DPF: xdg-desktop-portal FileChooser via libdbus, fallback self-drawn sofd [V].
- → portal (dlopen libdbus) → zenity/kdialog via posix_spawn with fd registered (never block host thread) → IGraphics-drawn [I].

## Packaging
- VST3: `Foo.vst3/Contents/x86_64-linux/Foo.so` (`aarch64-linux`); paths ~/.vst3, /usr/lib/vst3 … CLAP: single `.clap` .so; ~/.clap, /usr/lib/clap, $CLAP_PATH [V].
- `-fvisibility=hidden`, version script exporting only entry points, `-Wl,--exclude-libs,ALL -Wl,-Bsymbolic -Wl,--no-undefined`; build on old glibc (Ubuntu 22.04 / manylinux_2_28); `-static-libstdc++ -static-libgcc`; dlopen or dynamic libX11/GL [I].

## Standalone
- RtAudio 6: ALSA/JACK/Pulse/OSS (no native PipeWire; use pipewire-jack/pulse). RtMidi: ALSA/JACK. dlopen libjack [V/I].

## Validation
- clap-validator (free-audio), pluginval headless `--skip-gui-tests`, VST3 SDK validator + editorhost, `xvfb-run -a` with Mesa llvmpipe [V/I].

## LV2 — skip for v1 (Ardour loads VST3) [I].
