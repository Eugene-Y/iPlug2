# Review of T-007 (P5 HiDPI / scaling), 2026-10-15

Independent fresh-context reviewer (AGENTS §4.4). Range `hvoya-linux..linux/T-007` (`11af162..b8582da`), task file
`tasks/T-007-hidpi.md`, results `notes/17-p5-hidpi.md`, evidence `evidence/t007/`. No VM. Reviewer outputs in
`build/t007-rev*` (gitignored); container `t007-rev` removed.

**Verdict: accept once findings 1 and 2 are fixed** (3–5 can go with them or into the Log as known issues). The
scale model is right, every shared-file change is `#ifdef OS_LINUX`-guarded (or `OS_WIN || OS_LINUX`, the same
code as before for Windows), and I reproduced the REAPER/mini-host results. The size reported before the open is
off by one logical pixel in one rounding case (finding 1), and the crops that back "crisp text" are not in git
(finding 2).

## What I re-ran (HEAD `b8582da`, fresh build `build/t007-rev`, all 4 targets PASS)

| Run | Result |
|---|---|
| `Tests/LinuxScale` (ASan+UBSan), `build/t007-rev-unit` | PASS, failures 0; the de_DE case SKIPs (no locale in the image) |
| `scale-probe.sh` CLAP IPlugEffect, `REAPER_UI_SCALE=1.5`, Xft.dpi 144 → `build/t007-rev-probe-clap1.5` | Update → QueryDesktopScale → host scale 1.5 → `IGraphics::SetScreenScale 1.5`; container = editor = 675×457, no resize call. Same as `evidence/t007/probe-clap-reaper1.5` |
| `reaper-plugin-test.sh` VST3 IPlugEffect, `REAPER_UI_SCALE=2 XACT_SCALE=2`, p3a steps → `build/t007-rev-effect-vst3-x2` | RESULT: PASS. Knob drag at 224,474 → lim thresh −60.0 (the right knob), corner resize → 1130×764, container = editor; 17 `Draw` / 1036 `ProcessXEvents`, all tid 1. Looked at the shots: crisp |
| `reaper-plugin-test.sh` CLAP IPlugControls, scale 1.5, full P3b steps → `build/t007-rev-controls-clap-x1.5` | RESULT: PASS, 14/14 `expect-param`, pointer back at 483,356; 1:1 crop crisp, `@2x` bitmaps |
| mini host built per its header, `probe/mini-scale.sh` → `build/t007-rev-mini/run` | Identical to `evidence/t007/mini-host/scale.txt` |
| mini host + REAPER at Xft.dpi 132 (finding 1) → `build/t007-rev-mini/drift.txt`, `build/t007-rev-probe-drift-dpi132` | Reproduces finding 1 |

Evidence I opened: `probe-clap-reaper1.5/zoom-text.png`, `open-crop.png`, `mini-host/xft-144.png`,
`vm-gnome-200/reaper-effect-clap/editor-1to1-tl.png`, `ctx-menu-vst3-x2/menu-crop.png`, `reaper-controls-clap-x2/sheet.jpg`.
All are crisp: glyph edges one antialiased pixel wide, no upscaling blur. The menu opens at the click point. I also
spot-checked the `calls.txt`/`sizes.txt`/`RESULT.txt` files (10× PASS), `validators.log`, `inspect.txt` and
`macos/mac-builds.txt`. The mac proof's commit `9708e246f` is off the branch after the history rewrite, but
`git diff 9708e246f HEAD -- IGraphics IPlug Tests` is empty.

## Findings

### 1. Medium: the closed editor's size drifts by one pixel when it is rescaled without a last size
`IGraphics/IGraphicsEditorDelegate.cpp:118-127` (`RescaleClosedEditorSize`, the `!hasLastSize` branch).
- **Problem.** The branch recovers the logical size as `lround(GetEditorWidth() / mEditorScreenScale)` from an
  already truncated physical size. That is not IGraphics' arithmetic, so the comment on line 118 is half wrong.
  With `P = floor(W·s)`, `round(P/s) ≠ W` whenever `frac(W·s) > s/2`. Examples: at s = 1.25, any W ≡ 3 (mod 4); at
  1.375 (W = 450, IPlugEffect), 450 → 618 → 449.
- **When it happens.** Before the first open there are always two rescales: the desktop's at `gui.create` /
  `createView`, then the host's at `set_scale`. Calling `RescaleClosedEditorSize` twice with the same fractional
  scale also drifts.
- **Reproduced.** Mini host, Xft.dpi 132:
  - `MINI_SET_SCALE=1`: get_size **449×305**, then `request_resize 450x305` on the first frame;
  - `MINI_SET_SCALE=1.5`: get_size **673×457**, then `request_resize 675x457`.
  - REAPER (ui_scale 1.5, Xft.dpi 132): `EditorResizeFromUI 675 x 457` → `clap EditorResize`, which the
    `probe-clap-reaper1.5` run never shows.
- **Realistic case.** KDE at 125 % (Xft.dpi 120) with REAPER at its default ui_scale 1, for any editor with a
  dimension ≡ 3 (mod 4).
- **Impact.** `TellHostTheOpenedSize` repairs it, so the user sees one frame in a container one pixel short. But it
  breaks the claim that get_size equals the opened window.
- **Fix.** Don't divide. Keep the logical size the closed editor was configured with, e.g.
  `mUnscaledEditorWidth/Height`, captured while `mEditorScreenScale == 1` (or at the first rescale), and use
  `int(W) * s` exactly like the `hasLastSize` branch. Then fix the line-118 comment. Add a `Tests/LinuxScale` case
  that sweeps W ∈ [400, 1100] × s ∈ {1.125, 1.25, 1.375, 1.5, 1.75} and checks that rescaling s1 → s2 → s2 equals
  `int(W·s2)`. The arithmetic can live in `IGraphicsLinuxScale.h` as a pure function so the test needs no
  delegate.

### 2. Medium: the 1:1 crops behind acceptance #2 are not committed
`linux-research/.gitignore:8` (`evidence/**/*.png`), `notes/17-p5-hidpi.md` acceptance row 2.
- **Problem.** Every file notes/17 cites for crispness is gitignored and exists only in this worktree:
  - `probe-clap-reaper1.5/zoom-text.png`, `*/open-crop.png`;
  - `mini-host/xft-*.png`;
  - `app-controls-xft144/view-*.png`;
  - `vm-gnome-200/reaper-effect-clap/editor-1to1*.png`;
  - `ctx-menu-vst3-x2/menu-crop.png`, `resize-effect-clap-x1.5/after-crop.png`, `reaper-controls-vst3-x2/c11-crop.png`.

  `git ls-files evidence/t007 | grep png` is empty. The committed `sheet.jpg` files are downscaled JPEGs and cannot
  show crispness. When the coordinator removes the worktree, acceptance #2 loses its evidence.
- **Fix.** Whitelist them the way P3a did, e.g. `!evidence/t007/**/*crop*.png`, `!evidence/t007/**/zoom-*.png`,
  `!evidence/t007/**/*1to1*.png`, `!evidence/t007/mini-host/xft-*.png` (about 15 small files), and commit.

### 3. Low: the first-frame size check also fires after a host resize, and answers the host with a counter-request
`IGraphics/Platforms/IGraphicsLinux.cpp:402, 544, 576-586`.
- **Problem.** The check compares `GetEditorWidth/Height` with `mWindowW/H` on the first timer tick, not at the end
  of the open. A host resize before that tick changes `mWindowW` (sync `set_size`/`onSize` → `OnParentWindowResize`
  → `Resize(..., false)` → `PlatformResize`, or a `ConfigureNotify` drained by `ProcessXEvents` just before). It
  does not change the editor size, because `EditorResizeFromUI(..., false)` does not call `SetEditorSize`. So the
  plug-in sends `request_resize`/`resizeView` back for the size the host just set. At a fractional scale that size
  is one pixel smaller (known issue 4).
- **Scope.** Host resize enabled only; neither the examples nor Gneiss enable it, so I found this by reading, not
  by a run. It is one-shot, so there is no loop.
- **Fix.** Decide at the end of `OpenWindow` (window size vs `GetEditorWidth/Height`) and only defer the
  call. Or clear `mHostSizeUnconfirmed` whenever the host sizes the editor (`HandleConfigure`, the
  `OnParentWindowResize` path).

### 4. Low: the knob-value claim in notes/17 does not match the logs
`notes/17-p5-hidpi.md:53` says every `expect-param` reads the same value as at 1×, "knob 22.77". The step
regex accepts any value from 0 to 99, and the logs show:

| Run | Knob value |
|---|---|
| CLAP 1.5 | 12.30 % |
| CLAP 2 | 30.95 % |
| VST3 1.5 | 0.00 |
| VST3 2 | 22.77 |
| My CLAP 1.5 | 9.52 % |
| Every 1× run (`p3b-review/*`) | 0.00 |

So the same logical drag moves the locked-cursor knob less at scale > 1. That is probably motion coalescing:
llvmpipe draws a 2.25–4× framebuffer, fewer motion events arrive between warps, and the drag steps are absolute
positions. The number boxes and captions match exactly.
- **Fix.** Correct the sentence: the knob changes and the pointer returns, but the amount is timing-dependent.
  Optionally confirm with a gdb sum of `dY` in `IVKnobControl::OnMouseDrag` at 1 and 2.

### 5. Low: an ignored `plug_build_info.hpp` from the mac proof shadows the Linux build info
`Examples/IPlugEffect/plug_build_info.hpp` is ignored and untracked. Xcode's pre-build step wrote it at 21:39.
- **Problem.** The source directory comes before CMake's `generated/` on the include path. So every Linux build
  from this worktree embeds `9708e24` / `2026.10.14`, including my fresh build of `b8582da`, whose
  `generated/plug_build_info.hpp` says `b8582da`. The version text in the evidence screenshots therefore does not
  identify the build: early shots show `11af162`, later ones `9708e24`.
- **Fix.** Delete the file after the mac build. Add it to the AGENTS §2 note next to the `.rc_mac` restore, or put
  CMake's `generated/` first on the include path.

### 6. Low: harness side effects
- `scripts/xft-dpi.sh:3,5-6`:
  - setting overwrites the whole `RESOURCE_MANAGER`, and `-` deletes it;
  - the header says it also runs on the VM, where it would wipe GNOME's `Xft.*`/`Xcursor.*` resources until
    gsd rewrites them.

  Fix: edit only the `Xft.dpi` line (read, `sed`, write back), or drop "the VM" from the header.
- `scripts/scale-probe.sh:10` leaves the container at the given Xft.dpi. A later APP run in the same container then
  silently opens scaled. Fix: restore the previous value on exit, or say so in the header.
- `scripts/restore-size-probe.sh:20` hard-codes IPlugEffect's corner coordinates although `<FX>` is a parameter.
  Fix: name it in the usage line.
- `xact.sh` scaling: I checked every command. `click`'s button argument is not scaled, `drag-check` passes the
  scale to its nested step once, and integer steps are unchanged at 1. No existing user breaks.
  `gui-up.sh XVFB_SCREEN` and `reaper-up.sh REAPER_UI_SCALE` (a false `[ -n ]` under `set -e` in an `&&` list is
  not fatal) are fine.

### 7. Low (not P5's code, but P5's model): host-resize constraints mix units at scale ≠ 1
`IPlugCLAP::guiAdjustSize` (`IPlug/CLAP/IPlugCLAP.cpp:1014`) and `IPlugVST3View::checkSizeConstraint` pass the
host's physical sizes to `ConstrainEditorResize`, whose min/max are logical (`PLUG_MIN/MAX_*`). At scale 2, a max of
4 × logical caps the editor at 2 × its design size. It is the same on Windows, and host resize is off everywhere
today. Fix: add it to notes/17's known issues next to issue 4, for whoever enables host resize.

### 8. Nits
- `IGraphicsLinuxScale.h:69`: `@param gdkScale @param qtScaleFactor` on one line. Doxygen needs one
  `@param` per line.
- `Tests/LinuxScale/CMakeLists.txt:17`: a framework test points into `linux-research/notes/17`, which will not
  exist upstream. Drop the reference and keep "the X lookup is covered by the REAPER/APP runs".
- `IGraphicsEditorDelegate.h:92`: a whitespace-only change on a shared line. Harmless, but it is a diff line outside
  `OS_LINUX`.
- `evidence/t007/macos/mac-builds.txt`: name a commit on the branch (`e0ff5b42b`, code identical) instead of the
  rewritten-away `9708e246f`.
- `probe-vst3-dpi144/calls.gdb` predates the delegate `SetScreenScale` breakpoint, so that run does not show REAPER
  passing VST3 scale 1. `vm-gnome-text150/probe-vst3` does, so the claim stands.
- The unit test's decimal-comma case never runs in the image. The parser uses no locale functions, so this is
  fine, but say "not exercised" rather than PASS.

## Checked and fine
- **Shared-code rule (strict reading).** Each shared hunk is inside `#ifdef OS_LINUX`, or is `OS_WIN || OS_LINUX`,
  which for Windows is the old `#ifdef OS_WIN`:
  - `IGraphics.cpp` context menu;
  - `IGraphicsEditorDelegate.{h,cpp}`: include, `SetScreenScale` branch, new members and functions;
  - `IPlugEditorDelegate.h` virtual;
  - `IPlugCLAP.cpp guiCreate`;
  - `IPlugVST3_View.h`: constructor, `setContentScaleFactor`;
  - `IPlugAPP_dialog.cpp`: the call sits in the existing `#ifdef OS_LINUX` branch.

  Mac and Windows preprocess to the old code apart from the whitespace nit.
- **Priority.** Host scale is sticky once given, then Xft.dpi via `XGetDefault` (Xft's own lookup), then
  `GDK_SCALE`, then `QT_SCALE_FACTOR`, then 1. Values are quantized to eighths and clamped to [1, 4]; the parser
  ignores the locale.
- **Restored-size path.** `int(float(mLastWidth)·mLastScale)` then `int(·s)` matches `WindowWidth()` ·
  `GetPlatformWindowScale()` and `IGraphicsLinux::OpenWindow`. Reopen, set_scale before or after create, never,
  and while open are all consistent (mini host, my re-run).
- **`SetScreenScale` in the `IGraphicsLinux` constructor.**
  - `PlatformResize` returns (no window).
  - `DrawResize`'s `ScopedGLContext` returns before touching the null `mGLX`/`mGLContext`, and `mGLDepth` stays
    balanced.
  - With no `mVG` there is no FBO; there are no controls yet.
  - `mInOpenWindow` keeps the host from being told.
  - Bitmaps then load at the right scale in `OpenWindow`. IGraphicsMac does the same in its constructor.
- **`TellHostTheOpenedSize`.**
  - One-shot, flag cleared first.
  - Uses a local `shared_ptr` copy of the open flag and touches nothing after a host close.
  - Main thread (host timer).
  - The restore-size probe shows exactly one `EditorResizeFromUI` and REAPER's container following.
  - Apart from finding 3, it does not fire wrongly.
- **Threading.** `QueryDesktopScale` opens and closes its own X connection on the host's main thread (`gui.create`
  / `createView` / APP). There is no `XInitThreads` and no GTK.
- **Comments.** They state the WHY (`mInOpenWindow` reasoning, Xft lookup, eighths); no restating comments. The
  stale one is line 118 (finding 1).

## Acceptance

| Item | Verdict |
|---|---|
| Host scale when offered, else Xft.dpi; crisp text at 1.5 and 2 | **Pass** (re-run; images checked), but the evidence is not committed (finding 2), and get_size is off in the finding 1 case |
| Mouse hit-testing at 1.5 / 2 | **Pass** (re-run IPlugControls CLAP 1.5 full P3b, IPlugEffect VST3 2); fix the notes wording (finding 4) |
| Resize at scale ≠ 1, no loop with REAPER | **Pass** (re-run at 2: 1130×764; evidence at 1.5); finding 3 for host-resize plug-ins |
| Main thread, validators, mac proof | **Pass** (tid 1 in my runs; validators per log; mac code identical to the proven commit) |
| Self-review, evidence, Log | **Open**: findings 1–2, then record this verdict in the task Log |

## Round 2 (fixes in `3ae56b76e`), 2026-10-15

I rebuilt IPlugEffect CLAP + VST3 at `3ae56b7` into `build/t007-rev`. The embedded build info now reads `3ae56b7`,
so the stray `plug_build_info.hpp` is gone. Container `t007-rev` re-created and removed.

| Finding | Status | How verified |
|---|---|---|
| 1 drift | **Fixed** | Mini host at Xft.dpi 132, host scale 1 / 1.25 / 1.5 / 2: get_size = parent = editor (450×305, 562×381, 675×457, 900×610) with no `request_resize`. Also checked with no host scale (618×419), rescale to 2 while open (one `request_resize 900x610`) and reopen at 1.25. Output: `build/t007-rev-mini/drift-r2.txt`. In REAPER at ui_scale 1.5 and Xft.dpi 132, the extra `EditorResizeFromUI` from round 1 is gone (`build/t007-rev2-probe-dpi132`) |
| 2 crops | **Fixed** | Whitelisted in `.gitignore` and committed (15 PNGs) |
| 3 first-frame check | **Fixed** | Decided at the end of `OpenWindow`; the first frame skips it if the window was resized meanwhile (code read). Restore-size probe at 1.5 still sends exactly one `EditorResizeFromUI 847 x 573`, and REAPER's container follows (`build/t007-rev2-restore`) |
| 4 notes wording | **Fixed** | notes/17 row 3 |
| 5 build-info header | **Fixed** | File deleted; the rebuild embeds the right hash |
| 6 harness | **Fixed** | `xft-dpi.sh` sets, overrides, removes, removes again when already absent, and refuses a display that carries other resources |
| 7 constraint units | **Recorded** | notes/17 known issue 8 |
| 8 nits | **Fixed** | The shared header's diff against `hvoya-linux` is now only the two `#ifdef OS_LINUX` blocks |

Regression check:
- REAPER CLAP IPlugEffect at scale 2 with p3a steps: RESULT PASS (`build/t007-rev2-effect-clap-x2`). The knob goes
  to −60.0, the corner resize reaches 1130×764 with container = editor, Draw and ProcessXEvents run on tid 1, and
  the shot is crisp.
- Shared-code rule still holds: the new members and logic are inside `OS_LINUX`.

Left as nits, not blocking:
- No unit test sweeps the rescale arithmetic. The member-based code no longer divides, so the risk is low.
- notes/17 row 3 has a comma splice: "…cuts the drag differently, the locked-cursor drag returns…".

### Final acceptance

| Item | Verdict |
|---|---|
| Host scale, else Xft.dpi; crisp text at 1.5 / 2 | **Pass** |
| Mouse hit-testing at 1.5 / 2 | **Pass** |
| Resize at scale ≠ 1, no loop with REAPER | **Pass** |
| Main thread, validators, mac proof | **Pass** (mac rebuild per implementer; changes are `OS_LINUX`-only) |
| Self-review, evidence, Log | **Pass** once this verdict is recorded in the task Log |

**Verdict: accept T-007.**
