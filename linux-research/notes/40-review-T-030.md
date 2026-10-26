# Review of T-030 (multi-touch through XInput 2.2)

Independent reviewer, fresh context. Branch `linux/T-030`, range `hvoya-linux..linux/T-030` (108c7ccdc … a008851aa).

## Round 1

**Verdict: changes required** (one major finding, M1; the rest are minors and nits).

### What I checked

- The diff: `IGraphics/Platforms/IGraphicsLinuxTouch.h` (new), `IGraphicsLinux.{h,cpp}`, `Documentation/linux-development.md`,
  `notes/22-feature-parity.md`, the VM probes and the smoke script. Only Linux files change; no shared, mac or win
  code (`git diff --stat` over `IGraphics IPlug Scripts Examples Tests`). `check-product-names.sh`: clean.
- XI2 usage:
  - `XQueryExtension` for the opcode, then `XIQueryVersion(2.2)` with the reply checked against 2.2. This is on the
    editor's own connection, so the host's connection is untouched.
  - `XISelectEvents` selects Begin/Update/End together (the server demands all three) on `XIAllMasterDevices`, for the
    editor window only. It runs under `OpenWindow`'s error trap.
  - `XGetEventData`/`XFreeEventData` work on a copy of the const cookie, synchronously after `XNextEvent`. The motion
    coalescing only peeks at a GenericEvent and never dequeues one.
  - Touch ids are keyed by (sourceid, detail) and mapped to a non-zero counter. Deltas are per touch. A `TouchEnd`
    for an unknown sequence is ignored.
  - If libXi is missing, `GetXi()` returns null, `HasTouchscreen` is false, nothing is selected and nothing changes.
  - `w = Impl()` resets the touch state on close. IGraphics is recreated on every open
    (`IGEditorDelegate::CloseWindow` resets `mGraphics`), so `EnableMultiTouch` from the layout function always meets
    an open display.
- Mapping against `IGraphicsWin` `WM_TOUCH` and iOS `onTouchEvent`:
  - Down/drag/up carry `touchID`, with no double clicks, as on both.
  - Tablet input (no lock or warp) works as on Windows and mac.
  - Selection happens only when `MultiTouchEnabled()`, after `LayoutUI`, like `RegisterTouchWindow`.
  - Without multi-touch, touches stay the emulated mouse. That is the right fallback; Windows instead drops them,
    which the task's cross-platform finding reports.
- Evidence (I opened the PNGs myself):
  - `sheet-vm-app.png`, `igt-three-held.png` and `sheet-reaper-vst3.png` show what the Log claims: two tracks on the
    multi-touch control; a control and the Value slider moved at once (29 %, 88 %); knob values 70.24/55.24/21.60.
  - `params-*.txt` match the 5/5 PASS of both formats.
  - `t030-igt.log`: the three `press button` lines all come after the touch block (the mouse part of the run).
  - `t030-ctl2.log`: the emulated `MotionNotify` lands *before* each `TouchBegin`. The `HideTooltip()` in `Began`
    therefore cancels the tooltip it would have scheduled.
  - Docker Gneiss VST3 `FAIL (1)` is the same `g04b-context-menu` line as `merge-smoke/t038m-gneiss-vst3`, so it is
    pre-existing.

### Re-run by me

- Docker, `build/t030r-ex`: IPlugControls CLAP, VST3 and APP build PASS.
  - `readelf -d` in `iplug2-linux:24.04` shows no new NEEDED. CLAP and VST3 need only libX11/libm/libc/ld.
  - The `libXi.so.6` string is the dlopen name.
  - The APP's `libXi.so.6` NEEDED predates this branch (`Scripts/cmake/APP.cmake:174`, from 75fa19049).
- REAPER GUI regression, container `t030r` (removed afterwards), steps `iplugcontrols-p3b.txt`:
  - CLAP PASS (`evidence/t030/review-controls-clap/`) and VST3 PASS (`evidence/t030/review-controls-vst3/`).
  - Main-thread census is clean (all Draw/OnDisplayTimer/ProcessXEvents hits on tid 1).
  - I looked at `c07-textentry-committed.png` and `r03-two-instances.png`.
- VM, under the `vm` lock: a touch drag on IPlugControls' `IPLUG_TEST_DRAGOUT` square.
  - Build: APP with `-DIGRAPHICS_TRACE_LINUX_INPUT -DIPLUG_TEST_DRAGOUT` (`build/t030r-drag`), staged to
    `iplug-builds/t030r`.
  - Evidence: `evidence/t030/review-vm-dragout/` (`probe.sh`, `kbgrab.c`, `probe-output.txt`, `app.log`, shots).
  - `xdpyinfo` was OK before and after. The uinput device was removed (`xinput list` has no `t030`), and the VM's
    `/tmp/t030r` and `~/iplug-builds/t030r` were deleted. This is the source of M1.

### Findings

**M1 — major. A touch can start an external file drag that can never finish, and the editor then holds a keyboard grab
on the whole desktop.**

- Code: `IGraphics/Platforms/IGraphicsLinux.cpp:1912` (`InitiateExternalFileDragDrop`) →
  `IGraphicsLinuxDragSource.h:69-90` (`Start`).
- How it happens:
  1. `Start` decides that "a button is held" from `XQueryPointer`. During a pointer-emulating touch, the server
     reports `Button1Mask` on the master pointer. So a touch drag on a control that calls
     `InitiateExternalFileDragDrop` starts the drag (`IPLUG_TEST_DRAGOUT start … -> 1` in `app.log`).
  2. `Start` then grabs the keyboard and waits for core `MotionNotify` and a `ButtonRelease` of that button.
  3. Neither arrives: the touch is delivered to us as XI2 touch events. There is no implicit pointer grab, and the
     emulated pointer events are not ours.
  4. The drag never moves (DnD by touch does not work at all), and the keyboard grab outlives the finger.
- Measured with a third client's `XGrabKeyboard` (`probe-output.txt`):
  - `AlreadyGrabbed` while the finger is held, after it lifts, and after the next mouse click (the first click after a
    touch is lost on this desktop).
  - Only a mouse release delivered to the editor, or Escape, ends it.
  - A user who clicks into another application instead keeps a dead keyboard everywhere.
- This is a regression of the change. Without the XI2 selection, the same touch was an emulated press with a real
  implicit grab.
- Trigger: any plug-in that enables multi-touch and offers drag-out, on a touchscreen. Neither product uses either API
  today, but the framework must not do this.
- Fix (one condition): refuse the drag when the press is a touch. Either `InitiateExternalFileDragDrop` returns false
  while `mTabletInput`/a touch is captured, or `Start` uses the editor's own core button state (`w.mButtonState`, set
  only by core presses) instead of `XQueryPointer`. Then re-run the probe.

**m1 — minor. Touches do not count as "a button held" in the focus and tooltip logic.**

- `HandleTouch` (`IGraphicsLinux.cpp:1238`) never sets `w.mButtonState`. Two places rely on it:
  - `UpdateEditingState` takes the keyboard focus *during* a held touch when the press opened a menu or a text entry
    (`:1545-1547`, the "Not during the press" rule).
  - `ShowDueTooltip` (`:2102`) can show a tooltip under a held finger if the real mouse moves meanwhile.
- The VM traces show focus taken only after `phase 2`, because the probes' touches opened nothing.
- Fix: keep a count of held touches (or OR a bit into the state these checks read) between `Began` and `Ended`.

**m2 — minor. Touch events from dependent touch devices (touchpads) are not filtered.**

- `IGraphicsLinuxTouch.h:121` explains that a dependent device's fingers belong to the pointer, and uses that to
  decide `HasTouchscreen`. But `Translate` (`:70`, `:140`) accepts touch events from *any* source device on the
  selected master.
- On a machine with a touchscreen *and* a touchpad driver that exposes `XITouchClass` (dependent mode), touchpad
  fingers would arrive as touches at the pointer position, next to the driver's own pointer motion and clicks. A tap
  would then be both a touch press and a mouse press.
- The common libinput/synaptics drivers do not expose touchpads this way, so this is unverified in practice.
- Fix: remember the direct-touch slave ids from `QueryTouchscreen` and drop events whose `sourceid` is not among them
  (or check the source's touch mode).

**n1 — nit. `Select` depends on `HasTouchscreen` having run on the same `Impl`.**

- `IGraphicsLinuxTouch.h:58-65` uses `mDisplay` and `GetXi()` without a check. Both are valid only because
  `PlatformSupportsMultiTouch()` was called on this `Impl` before.
- No path reaches `Select` otherwise today, because IGraphics is recreated per open.
- Gating with `MultiTouchEnabled() && PlatformSupportsMultiTouch()` at `IGraphicsLinux.cpp:743`, or passing the
  display to `Select`, makes the dependency explicit for free.

**n2 — nit (test tool). Problems with `scripts/vm/probe/vtouch.py` that I hit while re-running:**

- **The FIFO it creates is not writable by the user.** `os.mkfifo(fifo, 0o666)` at `:30` runs as root under the
  default umask, so the FIFO ends up `0644` root and a plain `echo … > fifo` gets *Permission denied*. Fix: `os.chmod`
  it after creating it, or document `mkfifo -m 666` before starting.
- **The device can outlive the run.** Started with `&` from a script, the process ignores SIGINT. With its FIFO gone,
  only SIGTERM removes the device (I had to do this once).
- **The x coordinates are scaled.** The touch landed at x × 0.95 because I passed the documented 1440 px width, but the
  screen is 1368 px wide now (the root screenshot is 1368 × 863). The script could read the size with `xdpyinfo`
  instead of taking it as arguments.

### What is good

- The design is clean:
  - It does lazy XI 2.2 negotiation on the editor's own connection.
  - libXi is dlopen'ed and never unloaded. Besides the reason in the comment (the host may use it too), this keeps the
    hooks valid that libXi registers in Xlib for each display.
  - The selection is gated like `RegisterTouchWindow`.
  - The default for plug-ins without multi-touch is the emulated mouse: better than Windows' drop.
- The tablet-input rule (a4674b8da) is right and minimal.
- Comments explain WHY only (the ownership rationale for having no `OnTouchCancelled`, counter ids, the tablet rule),
  and the style matches the surrounding input code.
- The evidence is honest, and the "not ours" desktop effects (GNOME's three-finger grab, the lost first click) are
  reproduced with `xev` and a baseline APP.
