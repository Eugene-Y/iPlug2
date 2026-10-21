# Review: T-013 drops (case 8), T-020's VM checks (item 9), PC checklist (round 1)

Commit bf2490935 on `linux/T-013` (its own diff; `f9644cb91..bf2490935` holds no other commit). Fresh-context
reviewer, 2026-10-21. No framework code in the commit: 29 files under `linux-research/` (probes, evidence, task file).

**Verdict: changes requested.** One major finding (R1-1): the PC checklist cannot be run as written by its audience.
The evidence supports the case 8 and item 9 results, apart from the gaps in R1-4.

## What was checked

**The Files drag is a real XDND session.** Each `drops/<host>/RESULT.txt` has Files' `GDK_DEBUG=dnd` lines for a
real session:
- `Entering local Xdnd window` (Files' own window), then `XdndEnter`/`XdndPosition`/`XdndStatus` from Files' source
  `0x22001xx`, then `Sending enter … version N` to the host's window.
- Ardour: `XdndFinished` from `0x2e004dd`, which is the chain's top-level 48235741 (= 0x2e004dd, aware v5). The
  plug-in trace shows `enter from 0x2e00584, version 5 / drop … files 1 / finished 1`: Ardour re-sends the drag
  as its own source. I looked at `ardour/before.png` and `after.png`: the USER tab changes to the dropped layout.
  The PASS and "forwards" claims are confirmed.
- Carla 2.5.10 VST3: the chain's top-level 46137349 (= 0x2c00005) is aware **v3**. Files sends `version 3` to
  0x2c00005 and gets `XdndFinished` from it. No `iPlug2 drop` line comes from the Files run: the drop lines in
  `drop-trace.txt` all come from the scripted source (0x3000001). iPlug sets `XdndAware` only on its own window
  (`IGraphicsLinuxDrop.h:66`), so the v3 is Carla's. "Carla answers and finishes the drop itself" is confirmed.
- Carla main CLAP: the top-level is not aware, and Files enters no target. Confirmed.
- Renoise: the plug-in top-level 0x2c000b0 is not aware, and the release finishes nothing. Confirmed. See R1-6 for
  one confusing line.

**XdndAware versions:** REAPER 5 (cited from T-019, not re-run), Ardour 5, Carla 2.5.10 3, Carla main CLAP none,
Renoise plug-in window none (main window 5). These match the chains above.

**GL3 failure message:** `gl3-virgl.txt` has `Max core profile version: 0.0`, OpenGL 2.1 via virgl, the trapped
`GLXBadFBConfig`, and `no OpenGL 3.2 core context (this build renders with NanoVG GL3)`. That text is exactly
`IGraphicsLinux.cpp:630`. `01-gl3-clap-open.png` shows REAPER alive, showing its generic parameter UI.

**Other T-020 checks:**
- Clipboard: `TARGETS` lists `text/uri-list` and `x-special/gnome-copied-files` (`copy`, percent-encoded UTF-8),
  and `after:` lists the pasted `Preset #1 ünï.fxp`.
- Portal: `dark` / `light` / `dark`, all `main=1`, and `prefer-dark` restored.
- Drag out → Files: the file arrives, according to `after:`.
- Drag out → Renoise: Renoise's own log line shows the drop.
- Cursors: `default` → `not-allowed` / `pointer` → `default`, from XFixes samples along the drag.
- `dragout-reaper/after.png` shows two "drag me" tracks, matching the "twice by accident" note.

**Probes (safety):** none of the committed probes changes desktop settings, Renoise's OSC server or `~/.vst3`.
Those changes were made by hand (R1-3).
- `reaper-open.sh` goes through `reaper-up.sh`. That script kills any running REAPER and recreates
  `/tmp/rhome` (its own `HOME`), which is acceptable on the VM.
- `dragout-files.sh` moves the Files window and does not put it back. That is harmless.
- `clip-paste-files.sh` writes into `<dst>` by design.
- `carla-hold.py` closes the engine at the end.

**Hygiene:**
- `check-product-names.sh bf2490935`: clean.
- No secrets, keys or VM IP in the diff.
- `/home/gene/...` appears in 13 evidence lines (R1-8).

**Style:** the probe header comments say what each script does and why (Files must already be open; Tracker delays
mapping). No stale or restating comments, apart from the R1-7 nits.

## Findings

### R1-1 — major — `tasks/T-013-host-matrix.md` "PC checklist (Bitwig, x86_64)": a tester cannot run it as written

The acceptance item is ticked, but someone who does not know this port would get stuck or report nothing useful.

**Problems:**
- **Builds (step 1).**
  - `makedist-linux.sh` lives in the plug-in repos, not in iPlug2, and the checklist does not say where.
  - `-DIPLUG_TRACE_HOST_LOOP`, `-DIGRAPHICS_TRACE_LINUX_INPUT`, `-DIPLUG_TEST_DRAGOUT` and
    `-DIPLUG_TEST_APPEARANCE` are preprocessor defines, while `-DIGRAPHICS_RENDERER=GL3` is a CMake cache variable.
    The checklist gives no command that sets either kind.
- **The drag-out (step 4).** It needs `IPLUG_TEST_DRAG_FILE=<file>` in the host's environment
  (`Examples/IPlugControls/IPlugControls.cpp:21`). Without it the square starts nothing and prints
  `IPLUG_TEST_DRAGOUT start '' -> 0`.
- **Where the traces are (step 2).** Bitwig runs plug-ins in `BitwigPluginHost` processes. "In the terminal or in
  Bitwig's log" gives no log path, and the hosting-mode setting is not named as Bitwig's UI names it.
- **Missing expected results:**
  - step 2: what a good host-loop trace looks like (e.g. every `first … callback` line says `on main`);
  - step 4: the layout appears in the USER tab; and the three possible host behaviours from this Log (forwards,
    swallows, no target), so the tester can say which one Bitwig shows;
  - step 5: Enter sets the value, Escape restores it, a click into Bitwig commits;
  - step 7: what "right" looks like at 150 %/200 %.
- **Internal ids:** "case 5", "case 7", "T-010's open point", "(T-035)" mean nothing to a tester.
- **What to send back.** Step 9 only asks for `collect-linux-report.sh`, without its path
  (`linux-research/scripts/collect-linux-report.sh`). It should also ask for:
  - pass/fail per step;
  - the terminal output or Bitwig log with the `IPLUG_HOSTLOOP`, `iPlug2 input:`, `iPlug2 drop:` and
    `IPLUG_TEST_*` lines;
  - the `xprop XdndAware` output;
  - screenshots.

**Fix:**
- Give one copy-paste build command per variant. For example: Scripts/linux/package.sh's
  `--variant NAME:CMAKE_ARGS` with `-DCMAKE_CXX_FLAGS='-DIPLUG_TRACE_HOST_LOOP …'`, plus `-DIGRAPHICS_RENDERER=GL3`
  for the GL3 one. Name the plug-in repo's `scripts/makedist-linux.sh`.
- Add `IPLUG_TEST_DRAG_FILE=~/some.wav bitwig-studio` to step 4.
- Name Bitwig's log folder and hosting-mode option.
- Write one "Expected:" line per step, as `notes/25-tester-checklist.md` does.
- Replace the internal ids with a short reason.
- End with a "Send back" list. Untick the acceptance box until then.

### R1-2 — minor — PC checklist step 3: the `call-trace.sh` hint does not apply to Bitwig

`scripts/vm/probe/renoise/call-trace.sh` attaches to `pgrep -x renoise`, and its `.syms` lists are Renoise's and
iPlug's arm64 thunk names. Swapping `$x0`/`$x1` for `$rdi`/`$rsi` still traces nothing in Bitwig.

**Fix:** drop the sentence. Alternatively, give the probe a `-p <pid>` option and say the thunk offsets in
`latency.syms` must be re-read with `nm -C` on the x86_64 build. The click-alignment method in the same step is
the usable one.

### R1-3 — minor — the VM changes are not scripted, so neither the runs nor the restores can be reproduced

Three things were done by hand, with no committed script or command line:
- the portal toggle (`color-scheme` → `default` and back): `appearance-portal.txt`'s `original`/`restored` lines
  come from a script that is not in the commit;
- Renoise's OSC server on/off (the Log says it is off again);
- putting the GL2 test build into `~/.vst3` and the trace build back.

The Log says each was restored, which I cannot check without the VM. The next instance cannot repeat them safely.

**Fix:** commit the portal probe with the restore in a `trap … EXIT`, e.g. `appearance-toggle.sh`:
`orig=$(gsettings get org.gnome.desktop.interface color-scheme); trap 'gsettings set … "$orig"' EXIT; …`. Put the
`~/.vst3` swap and the OSC toggle as exact commands in the Log's VM notes. That OSC toggle may already be in the
Renoise session notes; then a pointer is enough.

### R1-4 — minor — the drag-out rows claim more than the committed text shows

- `t020/dragout-reaper/RESULT.txt` is the output of `dragout-files.sh`: a Files-window header, "over Files", and an
  empty `after:`. As committed it reads like a drag into Files that failed. The REAPER track exists only in the
  local `after.png`.
- Drag out → Ardour: the import, a region on a track, is visible only in the local `after-3s.png`, and only
  faintly.
- "Files logs XdndEnter, Positions, Drop" (drag out → Files): no committed file has Files' log for that run.
  `nautilus-dnd-all-runs.txt` covers the drops into hosts, where Files is the source.

**Fix:** add one line to `dragout-reaper/RESULT.txt` (or a `NOTE.txt`) saying it was the accidental drop onto
REAPER, plus REAPER's track count from `rpr.sh` (`CountTracks`). Add Ardour's import line from its log, or the
session's `interchange/` listing. Either add Files' log excerpt for the drag-out or drop that clause from the Log.

### R1-5 — minor — T-013 Log "Carla main VST3": a host crash after loading our plug-in, with no evidence and no decision

"The Python process … died with SIGSEGV right after loading Gneiss VST3 alone (apport, no stack). I did not
investigate." This is not the 2026-10-19 symptom (a core dump after a *second* editor opened), so "already has a
threading bug" does not cover it.

**Fix:**
- Keep the apport crash file's name, signal and the last trace lines as `drops/carla-main-vst3/crash.txt`.
- Say whether it reproduces.
- Either ticket it for the coordinator (is it our VST3 code or Carla main?) or mark it as deliberately deferred.

### R1-6 — nit — `drops/renoise/RESULT.txt` and `drops/nautilus-dnd-all-runs.txt`: easy to misread

- Renoise's RESULT shows `Entering Xdnd window 0x2c0009b` with `XdndStatus … action 461`. That is Renoise's
  *main* window (aware v5), crossed on the way to the plug-in window. A reader may take it as a target found at
  the plug-in.
- The all-runs file has no run separators, so the `async callback for #2c000b0` lines the Log quotes cannot be
  tied to a host without counting.

**Fix:** one Log clause ("the path crosses Renoise's main window, which accepts; the release over the plug-in
window finds no target"). Prefix each run in the all-runs file with its host.

### R1-7 — nit — probe scripts

- `dragout-files.sh`, `dragout-to.sh`: `/tmp/cursor-probe` must be built from `scripts/cursor-probe.c` first, and
  neither header says so.
- `drop-chain.sh`: the first id is printed in decimal (from `getmouselocation`) and its parents in hex, which is
  why the RESULTs mix both. Print it with `printf 0x%x`.
- `dragout-files.sh:9-12`: `wmctrl -i -a "$N"` and the sleep run before the `[ -n "$N" ]` check. Move the check up.

### R1-8 — nit — personal path in committed evidence

`/home/gene/t013-t020/src/...` appears in 13 committed lines (clipboard, drag-out) and in the Renoise evidence
before it. Nothing under `linux-research/` is shipped, and earlier evidence does the same, so this only matters if
the coordinator wants `~` in evidence from now on.

**Fix (if so):** run `sed 's#/home/[^/]*#~#'` over the text evidence before committing.

## Round 1 fixes

| Finding | Resolution | Commit |
|---|---|---|
| R1-1 major | Rewrote the PC checklist for a tester who does not know the port. It leads with the archives we send: release, trace, IPlugControls test, GL3. Advanced testers get copy-paste `cmake` commands with the defines quoted inside `CMAKE_CXX_FLAGS`, plus `-DIGRAPHICS_RENDERER=GL3` for the GL3 build; it names the repos, branches, SDK script and the plug-in repo's `scripts/makedist-linux.sh`. Bitwig's hosting mode is named (Settings > Plug-ins, "Individually"), and so is its log folder (`~/.BitwigStudio/log/`). Steps 1–9 each have an "Expected:" line, including the three host behaviours for drops and a good host-loop trace. `IPLUG_TEST_DRAG_FILE=$HOME/some.wav bitwig-studio` is in step 5. Internal ids are gone, and a "Send back" list with the report script's path ends it. The acceptance box is unticked until approval. | 3ea51c129 |
| R1-2 | The `call-trace.sh` sentence is dropped. Step 3 keeps the click-alignment method. | 3ea51c129 |
| R1-3 | New `scripts/vm/probe/t013/appearance-toggle.sh` puts `color-scheme` back on any exit (`trap … EXIT`). It was re-run on the VM, and `appearance-portal.txt` is its output. The Renoise OSC on/off commands and the `~/.vst3` swap (`vm-deploy.sh` both ways, plus Ardour's rescan) are in the Log's VM notes exactly as run. | 096191f2a, 0962c907f |
| R1-4 | REAPER: re-run with the new `dragout-reaper.sh` (one session; `rpr.sh` from a second session started another REAPER). `reaper-tracks.txt` shows tracks 1 → 2, track 2 "drag me" with the dragged file; the accidental-run RESULT is replaced. Files: `dragout-files/files-dnd.txt` has Files' own XdndEnter/Position/Drop for the drag-out. Ardour: `NOTE.txt` records the imported copy in `interchange/…/audiofiles/`, from the `find -newer` run before it was deleted. The Log says screenshots are local. | 096191f2a, 0962c907f |
| R1-5 | Reproduced under gdb (`drops/carla-main-vst3/crash.txt`). Carla main's engine thread `CarlaEngineRunner` calls the plug frame's `IRunLoop` timer (`first timer callback … on OFF-MAIN`). Gneiss' `OnIdle` → `rebuildMorphLayout` → `IGraphics::LoadFont` → `nvgCreateFontFaceMem` then segfaults, while the main thread is inside GLX opening the editor. The frames are iPlug's and Gneiss', but the cause is Carla's off-thread run-loop dispatch (the 2026-10-19 bug, which this time fires during the first open). Deferred in the Log with a minimal repro and **reported to the coordinator**; no fix made. A framework option is noted for the user: ignore run-loop callbacks from threads other than the attaching one. | 0962c907f |
| R1-6 | The Log's Renoise row says the drag crosses Renoise's main window, which accepts, and that over the plug-in window GTK finds no target. Each run in `nautilus-dnd-all-runs.txt` now starts with a `== run N: <host>` line. | 0962c907f |
| R1-7 | `dragout-files.sh` and `dragout-to.sh` give the `cursor-probe` build line. `drop-chain.sh` prints the first id in hex. `dragout-files.sh` checks for the Files window before using it. | 096191f2a |
| R1-8 | `/home/<user>` is replaced by `~` in this part's text evidence (`t020/`, `drops/`); older evidence is unchanged. | 0962c907f |

## Round 2

Range `a17b09c69..2938b5ac7` (096191f2a, 0962c907f, 3ea51c129, 2938b5ac7). Same reviewer, 2026-10-21. Still no
framework code; `check-product-names.sh HEAD` is clean.

**Verdict: approve.** One condition: R2-1, a wording-only change to the checklist, should land before the
acceptance box is ticked. It needs no further round. R2-2 to R2-4 are nits.

### R1-n status
- **R1-1:** resolved, apart from R2-1 to R2-3. The checklist now reads as a tester's document:
  - it starts with the archives we send;
  - it gives copy-paste builds, for those who build themselves;
  - every step has an "Expected:" line, including the three host behaviours for drops;
  - the drag-out step has `IPLUG_TEST_DRAG_FILE`;
  - no internal ids are left;
  - it ends with a "Send back" list.

  I checked its facts against the repos:
  - `Dependencies/IPlug/download-iplug-sdks.sh` exists.
  - `Documentation/linux-development.md` §1 is "Install".
  - The plug-in CMake takes `-DIPLUG2_DIR`.
  - `IPLUG_DEPLOY_PLUGINS` defaults to ON, so a build does install into `~/.clap`/`~/.vst3`.
  - The archive names match the plug-in repo's `makedist-linux.sh`.
  - The SVG slider in step 6 has a tooltip ("ISVGSlider H").
- **R1-2:** resolved: the sentence is gone.
- **R1-3:** resolved.
  - `appearance-toggle.sh` is reviewed below.
  - The Renoise OSC commands are written so that "off" runs only while Renoise is not running, so Renoise cannot
    write the setting back on quit.
  - The `~/.vst3` swap and Ardour's rescan are given as the exact commands run.
- **R1-4:** resolved.
  - `dragout-reaper/reaper-tracks.txt`: tracks 1 → 2, and track 2 is "drag me" with `~/t013-t020/src/drag me.wav`
    as its media. It was made by the new one-session `dragout-reaper.sh`.
  - `dragout-files/files-dnd.txt` has Files' own XdndEnter, Position and Drop for the drag-out.
  - Ardour's `NOTE.txt` gives the `find` command and its result: the imported copy in
    `interchange/…/audiofiles/`. This is a transcription, not raw output, which is acceptable for a deleted file.
- **R1-5:** the evidence supports the stated cause (details below).
- **R1-6:** resolved: the Renoise row explains the crossing, and the all-runs file has `== run N: <host>` labels.
- **R1-7:** resolved:
  - the build line for `cursor-probe` is in both headers;
  - `drop-chain.sh` prints hex ids, and its one comment is a WHY;
  - the Files check now comes first.

  RESULTs made before the change still show decimal ids, which is harmless.
- **R1-8:** resolved: there is no `/home/` left in `drops/` or `t020/`.

### R1-5: does `crash.txt` support the stated cause?

Yes, and the parts that are inferred are inferred reasonably.
- **The timer runs off the main thread.** After `VST3 view attached`, the trace switches to
  `backend fallback-thread -> vst3-irunloop(frame)`. The next line is
  `first timer callback id=1 … on OFF-MAIN`, on tid 70111. gdb names thread 70111 `CarlaEngineRunn`, and it is the
  thread that segfaults.
- **The crashing stack.** Thread 70111 runs `nvgCreateFontFaceMem` ← `IGraphicsNanoVG::LoadAPIFont` ←
  `IGraphics::LoadFont` ← the layout lambda ← `rebuildMorphLayout` ← `tryUpdateLayout` ← `Gneiss::OnIdle` ←
  `HostLoop::Dispatch` ← `VST3RunLoopHandler::onTimer` ← `libcarla_standalone2`. That is the claimed chain, frame
  for frame.
- **The main thread.** gdb shows `libGLX_mesa.so.0` for it, with no symbols above. "Opening the editor" is inferred
  from the order of the trace (the timer fires right after the view is attached), and is the only GLX work at
  that point. The trace cannot tell whether NanoVG's context is half-created or merely in use on another thread.
  "Racing" is a fair summary, but the Log could say "inferred" for the main-thread half.
- **The cause.** Either way, nothing would reach `LoadFont` concurrently with `OpenWindow` if the frame's
  `IRunLoop` were dispatched on the UI thread, as VST3 requires. Attributing it to Carla main's off-thread dispatch,
  the same bug as 2026-10-19, is correct.
- **Process.** The Log reports it to the coordinator, makes no iPlug change, and leaves the framework option (drop
  run-loop callbacks from a thread other than the attaching one) as a decision for the user. That is right.
  "Deferred" in the bullet's title could read as decided; "open, awaiting a decision" would match the text under it.

### `appearance-toggle.sh` (restore on exit)

- **Restore:** `trap '… gsettings set $KEY "$orig" …' EXIT` is installed before the first change. bash runs the EXIT
  trap on normal exit and on SIGINT, SIGTERM and SIGHUP, but not on SIGKILL, which nothing can catch.
- **Values:** `orig` keeps gsettings' GVariant quoting (`'prefer-dark'`), which `gsettings set` accepts as given.
  The unquoted `$KEY` is split into schema and key on purpose. The script restores twice ("set back", then the
  trap), which is harmless.
- **Evidence:** `appearance-portal.txt` shows `original 'prefer-dark'` → `set default` → `set back` → three
  `IPLUG_TEST_APPEARANCE … main=1` lines → `restored 'prefer-dark'`.
- **Gaps** (R2-4):
  - It has no shebang: the EXIT-on-signal behaviour needs bash, and under `sh` (dash) a Ctrl+C skips the restore.
  - If `gsettings get` fails (no session bus in the shell), `orig` is empty and the trap then tries to set an empty
    value.

### R2-1 — minor — PC checklist steps 1 and "Send back": Bitwig's setting names and log path are stated as fact, but were written from memory

- Step 1 says: Settings > Plug-ins, hosting mode "Individually" "(names as in Bitwig 5)".
- Steps 1 and "Send back" say the IPLUG_HOSTLOOP lines "are in Bitwig's logs under `~/.BitwigStudio/log/`" and ask
  the tester to send that folder.

These match my own recollection: the modes are Within Bitwig / Together / By manufacturer / By plug-in /
Individually, and the logs go under `~/.BitwigStudio/log/`. But nobody has checked them against a Bitwig 5 install,
and nobody knows whether the plug-in host processes' stderr reaches the terminal or that folder at all. A tester
who finds different names or an empty folder should know that this is expected and what to do.

**Fix:**
- Step 1: "the plug-in hosting mode that runs each plug-in in its own process ('Individually' in Bitwig 5 as far
  as we know; if your version names it differently, pick the closest and tell us its name)".
- Log path: "Bitwig's log folder, usually `~/.BitwigStudio/log/`".
- Add: "if the lines are in neither place, tell us; that is useful to know too".

### R2-2 — nit — step 2: the trace text does not quite match what the tester will see

- The CLAP backend prints as `backend=clap-timer-support`, not `timer-support`. The latter appears only in the
  host line `timer-support=1 posix-fd-support=1`.
- The `first … callback` lines end in `on main (init-tid=…)`, not in `on main`.

**Fix:** list the exact names, `clap-timer-support`, `vst3-irunloop(…)` and `fallback-thread`, and say "contains
`on main`".

### R2-3 — nit — steps 5 and the build section: two assumptions a tester cannot check

- **Step 5.** With "Individually" hosting, `IPLUG_TEST_DRAG_FILE` must survive Bitwig's spawning of its plug-in
  host process. That is likely, but unverified.
  **Fix:** "if the terminal shows `IPLUG_TEST_DRAGOUT start '' -> 0`, the variable did not reach the plug-in: tell
  us".
- **Build section.** It names "the iPlug2 fork" and "the plug-in repo" without saying where to get them.
  **Fix:** "we give you access to …".

### R2-4 — nit — `scripts/vm/probe/t013/appearance-toggle.sh:1,6-7`

**Fix:**
- Add `#!/usr/bin/env bash`, or run it as `bash …` in the usage line.
- Guard before installing the trap: `orig=$(gsettings get $KEY) || exit 1`.

## Round 2 fixes

R2-1 to R2-4 and the R1-5 wording are fixed in 24fdffe0f, and the PC-checklist acceptance box is ticked.
