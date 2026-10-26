# Review of T-017 (Linux WebView, out-of-process WebKitGTK helper)

Range: `hvoya-linux..linux/T-017` (3e013012e … d3b625b0c, 14 commits). Reviewer: independent subagent, fresh context.

## Round 1

**Verdict: CHANGES REQUESTED** (1 major finding).

### What I checked and re-ran
- AGENTS §2:
  - Threads: everything on the host main thread through HostLoop.
  - No GTK/WebKit in the host process: `inspect-binary.txt`, the plug-in's NEEDED is libX11/libm/libc/ld-linux.
  - mac/win untouched: every shared-file hook is under `OS_LINUX` or `UNIX AND NOT APPLE`; `_iplug_linux_add_webview_helper` returns early off Linux.
  - Product names: `check-product-names.sh` is clean.
- Read all of the plug-in side, the helper, the IPC header, the delegate, the CMake and `package.sh`, and compared them with `IPlugWebView_mac.mm`, `IPlugWKWebView*.mm`, `IPlugWebView_win.cpp` and `IPlugWebViewEditorDelegate.cpp`.
  - Lifetime is sound: `CloseWebView` is idempotent, and `mAlive` guards re-entrant closes from owner callbacks. The handled frame is a copy. `MSG_NOSIGNAL` is used. Partial writes are queued with a write watch or the poll. The spawn goes through `sh -c '"$@" &'` with a waited launcher, so no zombie is left. `closefrom` runs past fd 3. Signal state is reset.
  - The IPC reader's bounds are correct.
- Fresh Release build (`build/t017r-rel`) from d3b625b0c. Tests in my own container `t017r`:
  - `webui-reaper-test.sh` CLAP, churn 10: **PASS** (`evidence/t017/review-r1-clap`). I opened the open, typed, killed and two-instances screenshots myself: page renders, typed text shows, failure message shows, second instance renders.
  - `iwebviewcontrol-test.sh` IPlugOSCEditor CLAP: **PASS** (`review-r1-iwvc`, rescaled screenshot checked).
  - `webui-app-test.sh` IPlugWebUI APP: **PASS** (`review-r1-app`).
  - IPlugEffect APP (IGraphics) after the `IPlugAPP_host.cpp` change: the editor sits below the menu bar (+0+17), screenshot `review-r1-effect-app/a01.png`.
  - `reaper-ext-test.sh` IPlugReaperExtensionWebUI at `REAPER_UI_SCALE=2`: **FAIL**, see R1-1 (`review-r1-ext-scale2`).
  - Page file chooser focus probe, see R1-2 (`review-r1-focus`).

### Findings

**R1-1 (major): the REAPER extension's web view is scaled down twice at ui_scale ≠ 1.**
- **Where:** `IPlug/ReaperExt/ReaperExtBase.cpp:479` and `:522`, together with `IPlug/Extras/WebView/IPlugWebViewEditorDelegate_linux.cpp:48`.
- **What happens:**
  - `ReaperExtBase` divides the client size by `GetScaleForHWND()` "for the WebView delegate" (the mac/win convention: UI units).
  - The Linux delegate's `OnParentWindowResize` takes pixels and divides by `mScreenScale` (which `SetScreenScale(scale)` just set) again.
  - At ui_scale 2 the web view fills a quarter of the client: 300×300 in a 600×600 client; the rest is black. `evidence/t017/review-r1-ext-scale2/e01-open.png`; 4 paint checks fail. The worker's runs covered the extension only at 1×.
- **Fix:** skip the division under `OS_LINUX` in both places (the Linux delegate already takes pixels everywhere else), or give the Linux delegate the UI-unit convention and change its other callers. Either way, add a scale-2 run of `reaper-ext-test.sh` to the evidence.

**R1-2 (minor): every button release in any of the helper's windows asks for the keyboard focus for the plug.**
- **Where:** `IPlug/Extras/WebView/IPlugWebViewLinuxHelper.c:402` (`OnGdkEvent`).
- **What happens:** `OnGdkEvent` sends `kIPlugWVFocusRequest` for releases in the page's GTK dialogs and in the Web Inspector too. The plug-in then sets X focus on the plug, with the later release time, so it wins over the WM.
- **Seen:** after a click into the page's "Select File" dialog, X focus went to the plug in the FX window and the dialog's title went inactive (`review-r1-focus/f2-typed.png`). Typing still reached the dialog only because GTK's modal grab redirects keys. The Web Inspector (devtools) is not modal, so typing into its console after a click would go to the page.
- **Fix:** send the request only for events whose toplevel is the plug's GdkWindow:

  ```c
  gdk_window_get_toplevel(pEvent->any.window) == gtk_widget_get_window(sHelper.plug)
  ```

**R1-3 (minor): every Linux project builds the helper in `all`, also without a web view.**
- **Where:** `Scripts/cmake/Linux/WebViewHelper.cmake:28`, included from `IPlug.cmake:309` for every project, because the `IWebViewControl` interface target is always defined.
- **What happens:** with webkit2gtk installed, configuring a project without a web view runs pkg-config for WebKit, enables C and adds `iplug2-webview-helper` to `all`. In `build/t017-Gneiss/build.ninja`: `build all: phony webview-helper/iplug2-webview-helper …`.
  - A VS Code "Build" (target all) of an IGraphics-only plug-in compiles and links a WebKitGTK program.
  - A broken webkit dev install would fail that build.
- **Fix:** `EXCLUDE_FROM_ALL` on the helper (the existing `add_dependencies` still builds it for targets that bundle it). Optionally, include `WebViewHelper.cmake` lazily from `_iplug_linux_add_webview_helper`.

**R1-4 (minor): the custom scheme serves any file on disk, with `Access-Control-Allow-Origin: *`.**
- **Where:** `IPlug/Extras/WebView/IPlugWebViewLinuxHelper.c:479-504`; the scheme is also registered as secure and CORS-enabled at `:547-549`.
- **What happens:** `<scheme>:/any/absolute/path` returns that file to any requester. With ACAO `*` and the scheme CORS-enabled, remote content in the view (a plug-in that loads a remote URL, an iframe, an XSS) can `fetch()` the user's files (`~/.ssh/...`). mac's handler also serves any path (see the cross-platform section), but without ACAO a cross-origin read is blocked there, so Linux is more open than mac.
- **Fix:** serve only paths under the web root, i.e. the folder of the page `LoadFile()` loaded (`mWebRoot`, passed to the helper in a message) or the resource folder. Answer 404 otherwise.

**R1-5 (minor): the helper spawn duplicates `LinuxBackgroundProcess::Start`, without its child environment.**
- **Where:** `IPlug/Extras/WebView/IPlugWebView_linux.cpp:333-411`, the same steps as `IPlug/Linux/IPlugLinuxSubprocess.cpp:96-153`: the sh launcher, `/dev/null` stdin, closefrom, pgroup, sig reset, the waited launcher.
- **Differences:**
  - It passes the host's raw `environ` (`:393`); the dialog helpers use `MakeChildEnvironment` (a UTF-8 `LC_CTYPE` for a non-UTF-8 host locale).
  - It does not carry the dialog helpers' stderr choice.
- **Fix:** factor out one "spawn detached through sh with these fd actions" helper in `IPlug/Linux/` and use it in both. At least use `MakeChildEnvironment`, so GTK/WebKit in the helper get a UTF-8 locale under a host started with `LANG=C`.

**R1-6 (minor): the Log does not mention that an evidence result failed.**
- **Where:** `evidence/t017/hidpi/app-xft192/RESULT.txt`.
- **What happens:** the result says `FAIL (1) - app did not quit`, while the Log lists "APP at Xft.dpi 192 (`hidpi/`)" among the passing HiDPI results.
- **Fix:** say why it failed (harness or app), re-run it, or note it in the Log.

**R1-7 (nit): GDK warns in the host at every helper kill, unexplained.**
- **Where:** the REAPER and APP logs.
- **What happens:** `Gdk-WARNING: GdkWindow 0x4000xx unexpectedly destroyed` comes from the host's own GDK (`reaper:PID`, `IPlugWebUI:PID`) whenever the helper is killed. It is in every run, mine included (`review-r1-clap/reaper.log`).
- **Fix:** worth a short look: which GdkWindow of the host process is destroyed? It can be an XID collision (the helper got a recycled client base), or a host window the X server really destroyed. If it is harmless, note why in the Log.

**R1-8 (nit): the plug-in reader ignores a failed input buffer.**
- **Where:** `IPlug/Extras/WebView/IPlugWebView_linux.cpp:610-645`.
- **What happens:** `OnSocketReadable` never checks `mIn.failed`. After a failed `realloc` the frames silently stop (the helper side does check `h->in.failed`).
- **Fix:** treat `mIn.failed` like `mOut.failed` (`OnHelperGone`).

**R1-9 (nit): frames that arrive together with EOF are dropped.**
- **Where:** `IPlug/Extras/WebView/IPlugWebView_linux.cpp:610-645`.
- **What happens:** frames read in the same pass as EOF are discarded, because `OnHelperGone` frees `mIn` before they are handled.
- **Fix:** handle the complete frames first, then report the end.

**R1-10 (nit): queued X events can wait for the next X traffic (by reading, not observed).**
- **Where:** `IPlug/Extras/WebView/IPlugWebView_linux.cpp:891-946` (`CheckFocus`/`TopLevelOf`).
- **What happens:** round trips outside `ProcessXEvents` (`XGetInputFocus`, `XQueryTree`, `XSync` in `OpenWebView`) can read events into Xlib's queue. The fd watch does not see those, so they wait for the next X traffic.
- **Fix:** call `ProcessXEvents()` at the end of the focus timer callback, or check `XEventsQueued(QueuedAlready)` there.

### Cross-platform findings (report, not fix)

**X-1 (low): mac's custom URL scheme handler serves any absolute path.**
- **Where:** `IPlug/Extras/WebView/IPlugWKWebViewScriptMessageHandler.mm:74-118` (`startURLSchemeTask`).
- **What happens:** it serves any absolute path the URL names, not only files under the web root. It is less exposed than R1-4 because it sends no ACAO header. It is the same class of issue as R1-4.

### Notes (no change requested)
- **Destruction order:** if a host destroyed the plug-in while the web view is open, `~WebViewEditorDelegate` → `CloseWebView` → `Unwatch` would use `IPlugAPIBase::mHostLoop` after it was destroyed. IGraphicsLinux has the same exposure, and hosts close editors first, so this is not a T-017 regression.
- **Focus episode:** within the 1 s after a click, a focus change inside the same toplevel (an IGraphics text entry next to an `IWebViewControl`, or REAPER's FX-chain combo) is taken back after 100 ms. The design accepts this; it may deserve a line in the docs.

## Round 1 — fixes (worker)

- R1-1 (major): `ReaperExtBase.cpp` WM_INITDIALOG/WM_SIZE divide by the scale only off Linux (`#if defined
  WEBVIEW_EDITOR_DELEGATE && !defined OS_LINUX`): the Linux delegate takes pixels and divides itself. Extension test at
  `REAPER_UI_SCALE=2`: PASS, the page fills the client at zoom 2 (`evidence/t017/reaper-ext-webui-scale2`).
- R1-2: the helper asks for the focus only for releases in the plug's own top-level window. Checked: a click into the
  page's file dialog leaves the X focus on the dialog (`evidence/t017/focus-dialog/result.txt`).
- R1-3: the helper target is `EXCLUDE_FROM_ALL`; the WebView targets still build it (add_dependencies).
- R1-4: the custom scheme answers with `Access-Control-Allow-Origin: null` (only the scheme's own, opaque-origin pages),
  not `*`. Module scripts still load (helper probe, Svelte renders). Serving only the web root would break a plain
  `LoadURL("<scheme>:/path")`, which mac serves; kept as on mac. X-1 goes to the cross-platform findings.
- R1-5: `MakeChildEnvironment` is now public in `IPlug/Linux/IPlugLinuxSubprocess.h` and the helper spawn uses it
  (UTF-8 LC_CTYPE under a `LANG=C` host). The spawn itself stays separate: `LinuxBackgroundProcess` reads a status
  line from stdout and kills a process group, the helper needs a socket on fd 3 and neither.
- R1-6: that FAIL was the first 2x run, before the APP fix, and its quit used `xdotool windowclose` (the test now
  closes with wmctrl or SIGTERM). The app test takes `APP_SCALE`; re-run at Xft.dpi 192: PASS, typing at 2x, Large GUI
  2048 wide (`evidence/t017/hidpi/app-xft192`).
- R1-7: not ours: the same GDK warning is in REAPER's stderr of IGraphics runs from earlier tasks
  (`evidence/p2/reaper-clap-controls/reaper-stderr.txt`, `p4/dialogs-*`, `t028/...`).
- R1-8: a failed input buffer ends the web view with "ran out of memory", like the output side.
- R1-9: frames read together with the end of the socket are handled before the helper is reported gone.
- R1-10: `CheckFocus()` and `OpenWebView()` drain Xlib's queue after their round trips.
- Note on the focus episode: one line in `Documentation/linux-development.md` §10.
- Regression after the fixes (Docker): IPlugWebUI CLAP 20x churn PASS (fds 6 before/after), helper probe, Svelte.

