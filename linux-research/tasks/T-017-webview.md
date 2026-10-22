# T-017 — WebView on Linux (out-of-process WebKitGTK helper)

- **Status:** ready (design decided by the user 2026-10-16: notes/22 option 1)
- **Owner:** —
- **Branch/worktree:** `linux/T-017` from `hvoya-linux`, in `/Volumes/Storage/code/IPlug2-T-017`.
- **Depends on:** nothing to start; the IGraphicsLinux platform-view hook (step 4) is coordinated with T-019/T-009/T-020 (same file).
- **Conflicts:** `IPlug/Extras/WebView/*` (Linux files only), `IGraphics/Platforms/IGraphicsLinux*` (step 4), CMake for the helper.

## Goal
`IWebViewEditorDelegate` (the whole UI is a web view, e.g. `Examples/IPlugWebUI` and the Svelte/React examples) and `IWebViewControl` (a web view inside an IGraphics UI) work on Linux in CLAP, VST3 and the APP, with the same API and behaviour as WKWebView (mac) and WebView2 (win). Today `IPlugWebView_linux.cpp` is a stub.

## Design (decided)
- **No GTK in the host process** (PLAN §1). A helper executable runs WebKitGTK (`webkit2gtk-4.1`, GTK3, X11 backend) and puts its window into our X11 window with XEmbed (`GtkPlug` on the helper side; our side is the embedder: `XReparentWindow`/XEmbed messages, like our own embedding in hosts).
- **IPC:** a Unix socket pair, created by the plug-in, fd passed to the helper. Plug-in side is pumped by `GetHostLoop()` (fd watch) on the host main thread; no threads. Messages: load URL/file/HTML, evaluate JS (+ result callback), JS→C++ messages (the `IPlugSendMsg`/`SAMFUI`/`SPVFUI` protocol the WebView delegate uses), resize, show/hide, focus, enable dev tools, custom URL scheme / local file serving as the mac/win implementations do, navigation policy, ready/crash.
- **Helper:** one per editor instance (simplest lifecycle; measure memory) or one per process — decide and record. Located in the plug-in's `<Name>.resources/` (or a shared location) — decide with the coordinator; must work from `~/.clap`, `~/.vst3`, system paths. If the helper or `libwebkit2gtk-4.1` is missing or crashes: the editor shows a clear message, the host keeps running; the plug-in can relaunch it on reopen.
- **Keyboard/focus:** XEmbed focus in/out, host shortcuts still work when the web view is not focused; text input works inside it.
- **HiDPI:** pass our scale (T-007) to WebKit (`GDK_SCALE`/zoom level), match mac/win.

## Steps
1. Read the mac/win implementations (`IPlugWebView.h/.cpp`, `_mac.mm`, `_win.cpp`, `IPlugWebViewEditorDelegate*`, `IWebViewControl`) and list every behaviour to match. Read how JUCE (`juce_linux_WebBrowserComponent`) and CHOC (in-process, for comparison) do it.
2. Prototype: helper + IPC + XEmbed in the APP, then CLAP in REAPER (Docker has no GPU: WebKit's software path; set `WEBKIT_DISABLE_COMPOSITING_MODE=1` if needed and record it).
3. Full `IWebViewEditorDelegate` on `Examples/IPlugWebUI` (and one framework example: Svelte or React); CLAP, VST3, APP.
4. `IWebViewControl`: needs `AttachPlatformView`/`RemovePlatformView`/`HidePlatformView` in IGraphicsLinux (a child X window at the control's bounds, scaled; follows resize). Coordinate the file with the coordinator.
5. Packaging: the helper built and installed by CMake next to the plug-in; `inspect-binary.sh` for both (the plug-in's NEEDED must not gain GTK/WebKit).
6. Tests: harness steps (load page, JS→C++ param change, C++→JS update, resize, reopen ×5, two instances, kill the helper → graceful), churn 50× in Docker (no leaked helpers/fds), main-thread census; VM: short look under GNOME (lock, light).

## Acceptance
- [ ] IPlugWebUI (+ one framework example) work in REAPER CLAP + VST3 and the APP: two-way messaging, resize, HiDPI, keyboard, reopen, two instances.
- [ ] `IWebViewControl` works inside an IGraphics UI.
- [ ] Plug-in binary NEEDED unchanged (no GTK/WebKit); helper missing/crashing handled gracefully.
- [ ] mac/win untouched; independent review (AGENTS §4); evidence in `evidence/t017/`; Log complete.

## Log
(append: date — what was done — commits — next)

### Note from T-031 (2026-10-21) — the REAPER WebUI extension
`IPlugReaperExtensionWebUI` now links and opens its window on Linux (T-031). For its web view:
- `ReaperExtBase` (Linux) hands `OpenWindow` the X window id of a `SWELL_CreateXBridgeWindow` child of the dialog, not
  an HWND; SWELL keeps that X window placed as the dialog moves, docks and resizes.
- Its `WM_DESTROY` calls `CloseWindow()` again after `DestroyMainWindow`/`ToggleDocking`/`WM_CLOSE` already did (the
  IGraphics path is guarded by `GetUI()`, the WebView path is not): the Linux `CloseWebView` must be idempotent.
- `WM_INITDIALOG`/`WM_SIZE` divide the client size by `GetScaleForHWND()` (SWELL's UI scale) for the WebView delegate.
- Release deploy copies `resources/web/` to `~/.config/REAPER/UserPlugins/IPlugReaperExtensionWebUI/`.
- Test: `scripts/reaper-ext-test.sh <out> reaper_IPlugReaperExtensionWebUI.so` (today it skips the paint checks for it).
