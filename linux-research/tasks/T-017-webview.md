# T-017 — WebView on Linux (out-of-process WebKitGTK helper)

- **Status:** in progress (design decided by the user 2026-10-16: notes/22 option 1)
- **Owner:** Claude worker (T-017 lock, 2026-10-26)
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

### Log

- 2026-10-26 — Lock taken, worktree `/Volumes/Storage/code/IPlug2-T-017` on `linux/T-017` from `hvoya-linux` 2aeb81213.
  Docker: `iplug2-linux:24.04` extended in place with `libwebkit2gtk-4.1-dev` (WebKitGTK 2.52.6) by
  `docker/webview/Dockerfile` (a full rebuild of the main Dockerfile was uncached, ~1 h, and would have changed every
  package); the old image is tagged `iplug2-linux:24.04-pre-t017`. The main Dockerfile has the same layer for fresh
  builds. 22.04 has WebKitGTK 2.50 (`libwebkit2gtk-4.1-0` in jammy-updates), so the helper's floor (2.40) holds there.
- First working version: helper + IPC + XEmbed; IPlugWebUI CLAP renders in REAPER (Docker).
- Decisions (recorded here, as the task asks):
  - **Helper lifetime: one helper per open web view** (per editor, and per `IWebViewControl`). Simplest lifecycle:
    it starts at `OpenWebView`, ends when its socket closes (editor closed, host quit or crashed: the kernel closes
    the host's end). A per-process helper would save only the helper itself (~150 MB RSS); WebKit runs a web
    process per view anyway (~300 MB RSS under llvmpipe) plus a network process (~50 MB), so the per-editor cost
    is ~0.5 GB RSS either way (`evidence/t017/reaper-*/memory-*.txt`).
  - **Location: the module's resource folder**, `iplug2-webview-helper` beside `web/`: `<Name>.resources/` next to a
    `.clap` or the APP, `Contents/Resources/` in a `.vst3`, `<stem>.resources/` next to a REAPER extension. Found from
    the plug-in module via `dladdr` (`BundleResourcePath`), so it works from `~/.clap`, `~/.vst3`, `/usr/lib/...` and
    the APP folder; each plug-in has its own copy from its own iPlug2 (protocol version checked at start).
    `IPLUG2_WEBVIEW_HELPER` overrides it.
  - **Helper in C** (links only WebKitGTK/GTK/GLib, no C++ runtime of its own); started detached through `/bin/sh -c
    '"$@" &'` (as the dialog helpers), so init reaps it; socket fd 3, the host's other fds closed (`closefrom`).
  - **No IGraphicsLinux change**: like Windows, the web view's container is a child X window of whatever window it
    is given (the IGraphics window for `IWebViewControl`), placed by `SetWebViewBounds`; `AttachPlatformView` stays the
    no-op base (step 4 of the plan is not needed).
  - **Focus**: a click gives the page the keyboard (X focus on the plug, XEmbed WINDOW_ACTIVATE/FOCUS_IN), asked on
    the button release with its time and asked again during a 1 s episode if the WM/host took it back (as
    IGraphicsLinux text entry). Host shortcuts work while the page was not clicked.
  - **HiDPI**: the editor delegate's size is in pixels (host scale, else Xft.dpi), the page zoomed by the scale;
    the helper pins GTK's font DPI to 96 so WebKit does not scale text by Xft.dpi on top.
  - `EvaluateJavaScript` results: JSON (as WebView2). No "drop while loading" guard (mac has one, win not).
- Tests (Docker, REAPER 7.82, Release unless noted; `scripts/t017/`):
  - `webui-reaper-test.sh`, IPlugWebUI CLAP and VST3: render; knob drag -> host parameter 0 -> 0.3; host sets 0.75 ->
    knob moves; page buttons -> C++ -> Resize (1024x768, 512x335); typing into a text field (added to the installed
    copy of the page) shows the text, and the typed space does not reach REAPER; with the page unfocused, space
    starts REAPER's transport; 20x close/reopen: 0 helpers and 0 WebKit processes after each close; helper SIGKILLed
    while open: REAPER alive, the editor shows "helper program stopped", a reopen recovers; two instances; thread
    census (gdb): `OnSocketReadable`, `ProcessXEvents`, `HandleMessage`, `Flush`, `SendParameterValueFromUI` all on
    tid 1. PASS both (`evidence/t017/reaper-{clap,vst3}-rel`). Debug CLAP: same PASS plus "Data Size 4 bytes" from the
    Binary Test button (`reaper-clap-dbg`). REAPER's VST3 run loop does not take our fds, so VST3 polls them from a
    15 ms timer (main thread).
  - `webui-app-test.sh`: the APP renders, typing, Large GUI grows the window, helper kill survived, quit leaves no
    process (`app-rel`). PASS.
  - `iwebviewcontrol-test.sh`, IPlugOSCEditor CLAP (`IWebViewControl` in IGraphics): hidden at start, shown by its
    button, the OSC log arrives via `LoadHTML`, corner-resizer rescale follows (position and zoom), hide, close (0
    helpers), reopen. PASS (`iwebviewcontrol-clap`).
  - Svelte (custom scheme `iplug2:`, module scripts, hashed assets) and p5.js (WebGL) render; the Svelte knob reaches the
    host (`frameworks/`).
  - REAPER extension WebUI (T-031 hand-off): `reaper-ext-test.sh` no longer skips its paint checks: open, resized,
    docked, undocked, reopened all paint the web UI; PASS (`reaper-ext-webui`). The helper sits in
    `reaper_<Name>.resources/`; `reaper-up.sh` installs it.
  - HiDPI: REAPER `ui_scale` 2, CLAP and VST3: helper at 1200x1200, zoom 2, page laid out as at 1x, Large GUI -> 2048
    wide; APP at Xft.dpi 192 (`hidpi/`). Found and fixed (Linux-only APP code): the APP put the editor at -299,-270
    at 2x (GetWindowRect lags a just-resized window); now both values from SWELL's record.
  - Helper missing: the editor shows the message with the expected path, REAPER fine (`helper-missing/`). Plug-in
    NEEDED unchanged: libX11, libm, libc, ld-linux (`inspect-binary.txt`).
  - Packaging: `package.sh Examples/IPlugWebUI` (24.04 image, `--glibc-max 2.39`): the helper in all three resource
    folders, executable, symbols split, README lists WebKitGTK; IPlugEffect (22.04-gcc13) packs as before; IPlugWebUI in
    22.04-gcc13 (no WebKitGTK there) is refused with the install hint.
  - macOS: `xcodebuild` IPlugWebUI macOS-VST3 and CMake IPlugWebUI-clap build; no files changed.
- VM (GNOME/X11, virgl; the user had just restarted it; WebKitGTK 2.52.6 was already installed there), Release builds,
  short batches with the VM lock and `xdpyinfo` before/after:
  - batch 1, CLAP (`vm-reaper-clap` first run): render, knob drag -> host, host -> knob, typing into the page's field,
    space typed into the page not reaching REAPER, space with the page unfocused starting REAPER, Large GUI, 3x
    churn (0 helpers/WebKit after each close), helper kill survived, two instances, thread census on the main thread.
    One failure: the test clicked "Small GUI" at the old origin after Mutter had moved the grown window, which hit
    "Open iPlug2 Website"; GIO (no https handler in the harness's session) opened the page in gnome-text-editor
    (started by the test, closed again; it left no draft state). Fixed: the helper opens links through xdg-open as
    IGraphics' OpenURL does; the test re-reads the origin.
  - batch 2, VST3: PASS (all of the above, `vm-reaper-vst3`).
  - batch 3: APP PASS (`vm-app`); then the CLAP rerun **hung in virgl**: output stops after the churn ("after the last
    close: helpers 0, WebKit 0"), right before the helper-kill step; `xdpyinfo` timed out afterwards and dmesg shows
    `kworker blocked for more than 614 seconds ... drm_atomic_helper_wait_for_fences / virtio_gpu_queue_fenced_ctrl_buffer`
    (the known VM hang, STATUS "Environment gotchas"). Stopped there as instructed. **The VM needs Force Stop + Start
    in UTM.** WebKitGTK composites with GL (virgl here) in every helper, so web view runs add GPU load of their own;
    this was the 3rd batch within ~25 minutes, with ~25 editor opens in total.
  - Afterwards (files only, no X): `~/.clap` and `~/.vst3` restored to Release builds from this branch at fa9b3a63d:
    IPlugControls (iPlug2 fa9b3a63d), Gneiss (`linux-experiment` 6246fb7), Sand (`linux-experiment` 95be3b0), all built
    in Docker against `/Volumes/Storage/code/IPlug2-T-017`; IPlugEffect left as it was (T-038); IPlugWebUI removed from
    `~/.clap`, `~/.vst3`, `~/iplug-apps`. No REAPER, helper or WebKit process of ours left on the VM.
- Review round 1: CHANGES REQUESTED (R1-1 major: the REAPER extension's web view at UI scale 2; minors R1-2..R1-6,
  nits). All fixed or answered in `notes/39-review-T-017.md`. Cross-platform finding X-1 (low): mac's custom scheme
  handler (`IPlugWKWebViewScriptMessageHandler.mm:74-118`) serves any absolute path to the page.
- Review round 2: APPROVED, with R2-1 (minor, security): a sandboxed iframe of remote content (origin "null") could
  read any local file through the custom scheme. Fixed (coordinator's choice, option 1): the scheme serves only files
  under the folders of the pages the plug-in loaded with it, else 403. CORS probe now fails in the iframe too; Svelte,
  IPlugWebUI CLAP and APP pass again (`notes/39`).
- Remaining mac behaviour (X-1, cross-platform, low; not changed): mac's custom scheme handler
  (`IPlugWKWebViewScriptMessageHandler.mm:74-118`) serves any absolute path to the page that asks. It sends no
  Access-Control-Allow-Origin header, so other origins cannot read the responses, but the scheme's own pages can read
  any file.
- Next: review round 3 (R2-1 fix).

