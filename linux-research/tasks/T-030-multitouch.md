# T-030 — Multi-touch on Linux (XInput2), split out of T-020 item 5

- **Status:** todo (low priority: Windows-only today; mac has no multi-touch in IGraphics)
- **Owner:** —
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
