# T-034 — NanoVG GL: the main framebuffer's stencil is never cleared

- **Status:** fix verified on the VM; review and the user's OK pending
- **Owner:** coordinator (chat f91df715), lock `.linux-port-locks/T-034`
- **Branch/worktree:** `linux/T-034` from `hvoya-linux`, in `/Volumes/Storage/code/IPlug2-T-034`.
- **Found in:** T-013 (the user, 2026-10-20: a plug-in's SVG logo in Ardour on the VM drew light boxes behind each letter — "transparency of the SVG doesn't work").
- **Conflicts:** `IGraphics/Drawing/IGraphicsNanoVG.cpp` — **shared code** (every NanoVG GL build: Linux, Windows GL2/GL3). Merge only with the user's OK; also a candidate for `hvoya`.

## Symptom
On the VM (virgl → ANGLE → Metal), an editor's first frame sometimes draws concave fills (here: an SVG logo of
filled glyph outlines, `fill-rule=nonzero`) as their whole bounding box, light behind dark letters. It stays like
that until the area is redrawn. 2 of 3 editor opens in Ardour; never seen in Carla on the same VM or under Docker
(llvmpipe). Strokes, text and convex fills are unaffected.

## Cause
`IGraphicsNanoVG` draws every frame into `mMainFrameBuffer` (an FBO from `nvgCreateFramebuffer`: colour texture +
a separate `GL_STENCIL_INDEX8` renderbuffer). `BeginFrame` clears the **window's** colour/depth/stencil and then
binds the FBO; nothing ever clears the FBO's stencil. A new renderbuffer's contents are undefined. NanoVG's fill
draws the path into the stencil (INCR/DECR_WRAP), then covers its bounding quad where stencil ≠ 0 and zeroes it —
it assumes the stencil starts at 0. Garbage there shows up as the bounding box. Layer FBOs (`Bitmap` constructor)
already clear colour+depth+stencil after creating theirs; the main FBO was missed.

Evidence (`scripts/vm/probe/stencil-fbo.c`, on the VM): stencil, two-sided winding and fans with an offset all work
in the window and in FBOs; but a stencil renderbuffer created after another one was filled with 0x55 reads back
833 of 2048 pixels nonzero without a clear — the driver hands out stale memory.

## Fix
`IGraphicsNanoVG::DrawResize`: after creating `mMainFrameBuffer`, bind it and clear colour + stencil (GL builds only;
Metal clears through its load actions).

## Acceptance
- [x] The editor of a plug-in with a concave SVG opens correctly in Ardour on the VM, repeatedly.
- [ ] Docker smoke (IPlugControls, a product CLAP/VST3) unchanged.
- [ ] Independent review; the user's OK for the shared-code change.

## Log
(append: date — what was done — commits — next)

### 2026-10-20 — found, fixed, verified on the VM
- Found in T-013 (Ardour, Gneiss VST3; the user's report). Probe `scripts/vm/probe/stencil-fbo.c` (see Cause).
- Fix bbbf626f6. Gneiss VST3 built against this branch with the T-013 trace flags (`build/t034-trace.cmake`), swapped
  into `~/.vst3` on the VM (`~/iplug-builds/t013-unfixed`, `~/iplug-builds/t034-fixed` hold both).
- Result (`evidence/t034/`): the boxes appear on the **first editor open in a fresh Ardour process**; reopening the
  editor in the same process draws clean (the new FBO then gets memory the process's previous FBO left zeroed).
  | Build | First open after an Ardour start | Reopen in the same process |
  |---|---|---|
  | without the fix | 3 of 4 broken (`unfixed-first-open-x4.png`) | 0 of 6 broken |
  | with the fix | 0 of 5 broken (`fixed-first-open-x5.png`) | 0 of 6 broken |
- Windows GL builds run the same code; Windows drivers usually hand out zeroed memory, so it is unlikely to show
  there, but it is undefined behaviour on every GL driver. Metal (macOS/iOS) is not affected.
- Next: Docker smoke; independent review; the user's OK (shared code), then merge and port to `hvoya`.
