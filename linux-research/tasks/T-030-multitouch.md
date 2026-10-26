# T-030 — Multi-touch on Linux (XInput2), split out of T-020 item 5

- **Status:** merged 381e38fbd (review approved in round 2, `notes/40-review-T-030.md`)
- **Owner:** T-030 worker (Claude), 2026-10-26
- **Branch/worktree:** `linux/T-030` from `hvoya-linux`.
- **Depends on:** a touch device to test with (none in Docker's Xvfb or on the VM): a `uinput` virtual touchscreen on the VM (python-evdev, sudo is passwordless), or a real touchscreen PC.
- **Conflicts:** `IGraphics/Platforms/IGraphicsLinux*`.

## Goal
`PlatformSupportsMultiTouch()` true on Linux when the X server has XInput2 touch, and touch events delivered as IGraphics touches (`OnTouchBegan/Moved/Ended/Cancelled`, `ITouchID`) the way `IGraphicsWin` maps `WM_POINTER`/`WM_TOUCH`.

## Steps
1. Read `IGraphicsWin.cpp`'s touch path and `IGraphics::OnTouch*`.
2. XInput 2.2: `XIQueryVersion`, `XISelectEvents` on the editor window for `XI_TouchBegin/Update/End` (libXi dlopen'ed, no new NEEDED; the X connection's extension opcode from `XQueryExtension("XInputExtension")`), `XGetEventData` in `ProcessXEvents`; touch ids from `detail`; coordinates `event_x/y` / scale. Pointer emulation: ignore emulated pointer events for touches IGraphics handles (`XIPointerEmulated` flag).
3. Test on the VM with a uinput virtual multi-touch device (python-evdev) driving two fingers on IPlugControls' XY pad / multi-slider; `xinput test-xi2` to check the device first. Main-thread census as in T-019.
4. Independent review.

## Log
- 2026-10-26: lock taken, worktree `/Volumes/Storage/code/IPlug2-T-030` on `linux/T-030` from `hvoya-linux` 2aeb81213.
- Code: `IGraphicsLinuxTouch.h` (new): `LinuxTouch` asks for XInput 2.2 and a direct-touch slave device lazily, from
  `PlatformSupportsMultiTouch()`, so plug-ins that never enable multi-touch never load libXi or announce XI 2.2.
  `OpenWindow` selects `XI_TouchBegin/Update/End` on the editor window after `LayoutUI` only if `MultiTouchEnabled()`
  (as Windows registers its touch window); otherwise touches stay emulated mouse events as before. Touches get ITouchIDs
  from a counter (X touch ids are unique per device only, and 0 is the mouse); `HandleTouch` maps them to
  `OnMouseDown/Drag/Up` with `L` set, like iOS. No `OnTouchCancelled`: without an `XI_TouchOwnership` selection, XI2proto
  ties delivery to ownership ("Clients may opt for touch events to be delivered before they become the owner"), and
  an owner's sequence is not taken away; a `TouchEnd` for a sequence we never began (another listener accepted it) is
  ignored. Docker build
  of IPlugControls CLAP/VST3/APP passes; no new NEEDED (libXi is dlopen'ed). Commit 108c7ccdc.
- f097efae7: `IGRAPHICS_TRACE_LINUX_INPUT` also logs button presses, touches and the touch selection;
  `scripts/vm/probe/vtouch.py`, a uinput multi-touch screen (python3-evdev, installed on the VM with apt) driven
  through a FIFO.
- **VM (GNOME 46 X11, Xorg 21.1.11, XI 2.4), 2026-10-26.** `xinput list` shows the virtual device as a slave pointer
  with `XITouchClass`, mode direct, 10 touches. Evidence `evidence/t030/vm-app/` (APP, trace builds; logs are the
  APPs' stderr) and `evidence/t030/vm-reaper-controls-{clap,vst3}/` (REAPER, Release builds, `vm/probe/touch-reaper.sh`).
  - IGraphicsTest APP: "touches selected" at open; a one-finger tap selects the MultiTouch page (`igt3.png`);
    two fingers on the multi-touch test control draw two independent tracks (`igt-held2.png`); one finger on that
    control and one on the Value slider at once move both (`igt-two-controls-held.png`: 1 touch on the control, the
    slider at 29 %); a third finger 0.6 s after the first two works too (`igt-three-held.png`). The trace shows only
    `touch` lines for all of these, no `press button` (`t030-igt.log`): the emulated mouse never reaches the editor.
  - IPlugControls APP: IVKnobControl and the XY pad dragged by two fingers at once (`ctl-two-held.png`: gain 70.24 %,
    pad moved); one finger on ISVGKnobControl (`ctl-single-knob.png`: 55.24 %); the mouse drags the horizontal slider
    (`ctl-mouse.png` and its crop: 21.60 %). A temporary motion trace (`t030-ctl2.log`) shows the only core events
    during touches: one MotionNotify without buttons where a finger lands (the server moves the pointer there, so
    hover follows the first finger), and the warp back after a knob's cursor lock. No motion with Button1 state.
  - REAPER CLAP and VST3 (IPlugControls Release): two fingers change gain and Freq 1/2 at once, one finger gain,
    then the mouse gain: 5/5 PASS each, by the parameter values REAPER reports (`params-*.txt`).
  - **Not ours, seen on this desktop:** (1) three fingers landing together never reach any client: GNOME Shell's
    three-finger gestures take them through Mutter's touch grab on the root window; staggered they work.
    (2) The first mouse press after any touch is lost, in xev too (`t030-xev.log`) and with an APP that did not
    select touches (`t030-igt-b.log`, started before the device existed, so touches came as emulated mouse events):
    Mutter/XTEST, not the editor. The probes click once on the background first.
  - The uinput device was removed afterwards (`xinput list` has no `t030` entry).
- a4674b8da: a touch neither locks nor warps the pointer, as on mac and Windows (`SetTabletInput(true)` on touches,
  `false` on mouse buttons and motion; `HideMouseCursor` locks only `!mTabletInput`, `MoveMouseCursor` returns early
  for it, like `IGraphicsWin`/`IGraphicsMac`). Before, a knob that hides and locks the cursor warped the pointer back to
  where the finger landed on release. VM check (trace APP, `vm-app/t030-ctl3.log`, `ctl-tablet-input.png`): after a
  finger drags ISVGKnobControl the pointer stays where the finger left (239,392), and a mouse drag on IVKnobControl
  still locks and returns the pointer to its press point (494,333).
- **Docker regression** (`scripts/coord/smoke-t030.sh`, at a4674b8da + docs; Xvfb has no touchscreen, so this is the
  path without XInput touches): IPlugControls P3b CLAP/VST3 PASS, Sand CLAP/VST3 PASS, Gneiss CLAP PASS, Gneiss VST3
  FAIL (1) on the known REAPER VST3 context-menu step (`g04b-context-menu`, a synthetic click; the same failure in the
  T-009/T-032/T-038 merge smokes). Evidence `evidence/t030/docker-*`. Gneiss-linux and Sand-linux git status clean.
- **Review round 1** (`notes/40-review-T-030.md`): changes required, 1 major (a touch could start a file drag-out
  that kept the keyboard grabbed), 2 minor, 2 nits. All fixed in 87e8d2fef and checked on the VM (see the notes).
- **Review round 2: approve**, no major or minor finding left (4773140ce). Open, optional: classify a touch device
  the first time it sends a touch instead of at open (r2-n1).
- **Hand-over.** Code: 108c7ccdc, f097efae7, a4674b8da, 87e8d2fef (the rest are evidence, docs and review). VM state:
  `~/.clap` and `~/.vst3` hold Release builds of Gneiss, Sand and IPlugControls from `linux/T-030` (code as at
  87e8d2fef; built at 1d42e5d4a); `~/iplug-apps` untouched; python3-evdev installed with apt; no uinput device, no
  process or `/tmp` file of this task left; staging dirs `~/iplug-builds/t030*` removed. No containers left.

## Cross-platform findings
- **Windows, unverified (read from the code, no Windows touchscreen here), medium for touchscreen users:**
  `IGraphicsWin.cpp:286`/`:309`/`:384` drop every mouse message the system synthesises from a touch
  (`IsTouchEvent()`: WM_LBUTTONDOWN, WM_MOUSEMOVE, double clicks), but the window registers for WM_TOUCH only when the
  plug-in enabled multi-touch (`:1077`). A plug-in that does not (most) then gets no presses or drags from a touchscreen
  at all, only the releases (`WM_LBUTTONUP` is not filtered). Repro: a plug-in without `EnableMultiTouch(true)` on a
  Windows touchscreen; tap a button. Linux does the opposite on purpose: without multi-touch, touches stay the emulated
  mouse.
- `Tests/IGraphicsTest/IGraphicsTest.cpp:102` labels the page "MultiTouch (iOS/Win/Web only)"; it works on Linux now.
  Left alone (shared test file).
