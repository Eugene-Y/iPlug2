# Feature parity with mac/win (user requirement, 2026-10-16)

The user: the framework must be fully capable on Linux; Linux must not be poorer than mac or win. This replaces
PLAN §1's "out of scope for v1: Skia, WebView". Every gap below gets a task; none is dropped as "won't do"
without the user's say.

## Gaps found (first pass, by diffing platform overrides and stubs; to be completed by T-016)

| Gap | mac/win have | Linux now | Task |
|---|---|---|---|
| **WebView**: `IWebViewEditorDelegate` (whole UI in a web view) and `IWebViewControl` (web view inside IGraphics) | WKWebView / WebView2 | stub `IPlugWebView_linux.cpp` | T-017 |
| Platform child views (`AttachPlatformView` / `RemovePlatformView` / `HidePlatformView`), which `IWebViewControl` needs | mac | missing | T-017 |
| **Skia** backend | mac, win | NanoVG only | T-018 |
| NanoVG **GL3** | win (GL2/GL3) | legacy GLX context, GL2 only | T-020 |
| Files dropped **onto** the editor (`OnDrop`, `OnDropMultiple`) | mac, win | no XDND | T-019 |
| Drag files **out** (`InitiateExternalFileDragDrop`) | mac, win | missing | T-020 |
| `SetFilePathInClipboard` | mac, win | missing | T-020 |
| Tooltips (`UpdateTooltips`) | native on mac, win | no-op | T-019 |
| UI appearance (dark/light, `GetUIAppearance`) | mac | missing; Linux source: xdg-desktop-portal `org.freedesktop.appearance color-scheme` | T-020 |
| Multi-touch (`PlatformSupportsMultiTouch`) | win | missing; X11 source: XInput2 touch | T-020 |
| Standalone APP: devices, settings, MIDI, no-device start | mac, win | partial | T-004 |
| REAPER extension / REAPER-API plugins on Linux | mac, win | not checked | T-016 |

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
