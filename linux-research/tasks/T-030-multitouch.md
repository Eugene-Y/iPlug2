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
  `OnMouseDown/Drag/Up` with `L` set, like iOS. No `OnTouchCancelled`: without an `XI_TouchOwnership` selection the
  server replays a sequence to us only once we own it, so no sequence is taken away midway. Docker build of
  IPlugControls CLAP/VST3/APP passes; no new NEEDED (libXi is dlopen'ed).
