# T-030 — Multi-touch on Linux (XInput2), split out of T-020 item 5

- **Status:** in progress
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
