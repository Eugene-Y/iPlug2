# Review: T-036 alignment with Gneiss, T-011 R2-1 fix, linux-development §8 (round 1)

Independent reviewer, fresh context, 2026-10-22.

| Part | Subject | Verdict |
|---|---|---|
| A | T-036: Sand's dist scripts aligned with Gneiss', docs in the archives (Sand `92aeb3f`, `bd3e8dd`, `411bdb8`, `f7810b3`, `fc4b9e0`) | **approve** (no blocker or major; minors and nits optional) |
| B | T-011 coordinator fix `79138fa1d` (`oldprojectroot` lexical, review R2-1) | **approve** |
| C | `Documentation/linux-development.md` §8 "Known host issues" (`3053e64bd`) | **changes requested**: minor accuracy fixes only; a re-read of the diff is enough |

## A. T-036 — Sand's distribution scripts aligned with Gneiss'; docs in the archives

### What I checked and re-ran
- **Diffs read:** `92aeb3f` (docs), `bd3e8dd`/`f7810b3` (the merges), `411bdb8` (mac/win ship the docs) and `fc4b9e0`
  (`--doc MANUAL.md`). I compared the full current `makedist-mac.sh` and `makedist-win.ps1` against Gneiss'
  `scripts/makedist-{mac.sh,win.ps1}` (main checkout, read only) and `makedist-linux.sh` against
  `Gneiss-linux/scripts/makedist-linux.sh`.
  - The docs list and copy lines match Gneiss' (`DOCS=(…)`/`$Docs`, `cp`/`Copy-Item` to the folder root).
  - Linux passes `--doc MANUAL.md` exactly as Gneiss' line 77 does.
- **mac re-run:** `scripts/makedist-mac.sh --no-install --zip --out <scratch>/r30/distr` exited 0 after 26 s (incremental).
  - The zip has 76 entries. Its top level is `Sand.{clap,component,vst3}/`, `MANUAL.md` and `Installation instructions.txt`.
    It has no `.DS_Store`, `__MACOSX` or `._*` entries.
  - Both docs in the staged folder are byte-identical (`cmp`) to the repo's.
  - `codesign --verify --deep --strict` passes on all three bundles.
  - The `ls -laR ~/Library/Audio/Plug-Ins/{VST3,Components,CLAP}` output was identical before and after, and
    `find -newer <marker>` found nothing.
  - The listing of `distr/Sand` is unchanged.
  - Sand-dist `git status --ignored` shows only `!! build-mac/` and `!! plug_build_info.hpp`, and the IPlug2 main
    checkout is clean.
  - Scratch folder: `/private/tmp/claude-501/-Volumes-Storage-code-IPlug2-linux/f91df715-d8e2-4a5b-b1d6-6e68afefbe6a/scratchpad/r30/`.
- **Merges** (`git merge-tree --write-tree`):
  - master + dist-scripts → `979234d`, clean.
  - master + linux-experiment → `b813b73`, clean.
  - dist-scripts + linux-experiment → `68bc7d5`, clean.
  - (master+dist-scripts) + linux-experiment and the reverse order → the same tree `68bc7d5`, clean.
  - master `d2294da` is an ancestor of both branches. The merged tree holds `MANUAL.md`, `Installation instructions.txt`
    and all three makedist scripts.
- **Windows script**, read line by line for PowerShell pitfalls, with `Sand.sln`, `projects/Sand-vst3.vcxproj`,
  `config/Sand-win.props`, `scripts/postbuild-win.bat`, iPlug2's `common-win.props` and `Scripts/create_bundle.bat`.
  I found no defect. In detail:
  - **Script-scope variables:** `$noInstallDir` is defined before the first `Invoke-SolutionTarget` call, and
    dynamic scoping makes it visible inside the function under StrictMode.
  - **Exit codes:** `$LASTEXITCODE` after `& msbuild … | Set-Content` is msbuild's. After robocopy, the code is
    reset to 0 for exit codes below 8.
  - **Property queries:** `Get-ProjectProperties` uses the single-property `Trim()` path, or the JSON path for
    several properties.
  - **`Get-InstallDest`:** the multi-assignment from `switch` behaves as intended.
  - **`IPLUG2_ROOT`:** it is `$(ProjectDir)..\..\..\IPlug2`, which is absolute. `Combine` keeps it, and `GetFullPath`
    normalises it.
  - **Removing read-only bundles:** `create_bundle.bat` sets `attrib +r` on the bundle folder, and
    `Remove-Item -Recurse -Force` on such a folder is fine, because `-Force` clears ReadOnly/Hidden/System on
    directories too.
  - **Install redirection:** with `VST3_64_PATH` pointing at a folder that doesn't exist yet, the stock postbuild
    skips the VST3 install (`if exist`). It `mkdir`s the CLAP one inside `build-win`, which `.gitignore`'s `build-*`
    covers.
  - **Zip separators:** the `UseBackslash` switch name is right, and it is set before the first `ZipFile` call.
  - **Encoding:** the file is still pure ASCII.
  - Still not run or parsed anywhere (no Windows/`pwsh` here).
- **"Sand is better" rows**, each checked in the code:
  - `--out`/`-Out` on mac/win.
  - The dirty check covers the Sand and iPlug2 trees and fails closed when `git status` fails.
  - `--allow-dirty` needs `--out`.
  - Staging goes through `.partial` and a swap at the end.
  - `dst/` and `build-win\Sand.vst3` are emptied before the build.
  - The awk empty-field guard is in place.
  - `--out` without a value is caught on mac.
  - The `UseBackslash` switch is set.

  None of these is a regression against Gneiss. The only behavioural difference left is that install happens per
  format before the swap, which is what Gneiss does as well.
- **Placeholder manual link:** `https://hvoya.audio/installation/` is a generic installation page (Windows VST3/CLAP,
  macOS VST3/Components/CLAP + `xattr`), and it already uses Sand as its example. The worker's open question about
  the link is answered: it suits Sand.
- **`Installation instructions.txt` against what Sand ships:**
  - Windows: `Sand.vst3` (the bundle folder the script ships, or the bare DLL of the hand-made zip; "Copy … to" fits
    either) and `Sand.clap`. Paths `C:\Program Files\Common Files\{VST3,CLAP}` are the specs' system folders.
  - mac: VST3, Components and CLAP, with three `xattr` lines whose bundle names match the staged ones.
  - Linux archives don't carry it; they have `README-Linux.txt`, as Gneiss' do.
  - The text is accurate.

### Findings

**A1 (minor) `"${DOCS[@]}"` is no longer safe under bash 3.2 for an empty list**
- **Where:** `scripts/makedist-mac.sh:122`, on `dist-scripts`.
- **Problem:** `411bdb8` replaced `${DOCS[@]+"${DOCS[@]}"}` with Gneiss' `"${DOCS[@]}"`. macOS's `/bin/bash` 3.2 with
  `set -u` aborts on an empty array (`DOCS[@]: unbound variable`, verified with `/bin/bash` 3.2.57). The script works today because the list has two
  entries. The day someone empties `DOCS` (e.g. drops the placeholder manual), the run dies after every build and
  install, leaving `.partial`. This is the bash-3.2 safety the round-0 script had on purpose.
- **Fix:** keep the safe form: `for doc in ${DOCS[@]+"${DOCS[@]}"}; do`. Gneiss has the same latent issue; mention
  it there as a recommendation, not a T-036 change.

**A2 (minor) A run without `--zip` leaves the previous build's zip next to the new folder**
- **Where:** `scripts/makedist-mac.sh:126-132`; `scripts/makedist-win.ps1:194-207`.
- **Problem:** the folder is swapped, but `Sand_v<ver>_<os>.zip` is only touched with `--zip`. A plain re-run of the
  same version therefore leaves a zip whose content no longer matches `Sand_v<ver>_<os>/`, and the zip is what gets
  uploaded. Gneiss has the same behaviour, so this is not a regression, but Sand's whole `.partial` design is about
  never leaving a mismatched release behind.
- **Fix:** without `--zip`, delete a stale `"$DEST.zip"` after the swap with a one-line notice (mac
  `rm -f "$DEST.zip"`; win `Remove-Item "$dest.zip" -ErrorAction SilentlyContinue`). Alternatively, print a warning
  that names it.

**A3 (nit) mac instructions: which file goes where**
- **Where:** `Installation instructions.txt:16-19` (Sand repo, `92aeb3f`).
- **Problem:** "Copy the files to the correspondent locations:" is followed by three folders and three bundles, with
  no mapping. The `xattr` lines below imply it. "correspondent" should be "corresponding" (inherited from Gneiss).
- **Fix:** "Copy Sand.vst3 to /Library/Audio/Plug-Ins/VST3, Sand.component to …/Components and Sand.clap to …/CLAP",
  in the form of the Windows paragraph.

**A4 (nit) Curly apostrophes in a BOM-less UTF-8 `.txt` for Windows users**
- **Where:** `Installation instructions.txt:9, 28`.
- **Problem:** `DAW’s` and `doesn’t` render as `DAWâ€™s` in pre-1903 Notepad and in some unzip viewers. This is
  inherited from Gneiss.
- **Fix:** use ASCII `'` in this file (Gneiss' copy too, at the user's discretion).

**A5 (nit) Linux `--allow-dirty` can still replace the real release**
- **Where:** `scripts/makedist-linux.sh:9-10, 33, 39` (on `linux-experiment`).
- **Problem:** on mac/win, `--allow-dirty` now requires `--out` (round-1 n6). The Linux script still publishes a
  dirty build into `distr/Sand`. This is unchanged by `fc4b9e0` and the same as Gneiss' Linux script, so it does not
  block this round. It is the one place where Sand's three scripts disagree with each other.
- **Fix:** mirror the mac check (`--allow-dirty needs --out`) in both Linux scripts when they are next touched.

**A6 (nit) Task file header is stale**
- **Where:** `linux-research/tasks/T-036-sand-dist-mac-win.md:3` and the Acceptance list (lines 35-37).
- **Problem:** the Status says "review-done (round 2 approved; r2 nits fixed)", but the 2026-10-22 round awaited this
  review, and the boxes are unticked.
- **Fix:** record this round's verdict, set the Status accordingly, and tick the boxes that are done.

## B. T-011 — `oldprojectroot` computed lexically (R2-1)

### What I checked
- **The diff** (`Examples/duplicate.py:200-201, 258-259`):
  - `oldprojectroot = relpath(join(configpath, oldroot), inputprojectname)`: both operands are relative to the same
    unresolved cwd, so the result is the example's own layout as its files spell it (`../..`), independent of the
    file system.
  - The new-root side is unchanged and consistent. `outputbasepath = realpath(argv[4])` and
    `iplug2folder = realpath(join(configpath, oldroot))` are both resolved, and `newroot`/`newprojectroot` relate only
    those two.
- **ntpath simulation** (`<scratch>/r30/ntsim.py`, output `ntsim.txt`): `os.getcwd` is patched, and `realpath` is
  stubbed to resolve a `subst` drive, a mapped drive or a junction. Each row compares the new code with the previous
  realpath-based `oldprojectroot` ("prev").

| Setup | new `oldprojectroot` | `newprojectroot` | prev |
|---|---|---|---|
| plain `C:`, target `C:\work\out` / deep `C:\work\a\b\c\d` | `../..` | `../../iPlug2` / `../../../../../iPlug2` | `../..` |
| `subst S:=C:\work`, target `S:\out`, `..\..\out`, `C:\work\out`, `.` | `../..` | `../../iPlug2` (`.` → `../..`) | ValueError (mount C: vs S:) |
| mapped `Z:=\\srv\share`, target `Z:\out` | `../..` | `../../iPlug2` | ValueError |
| junction `C:\dev→C:\real`, target inside / outside it | `../..` | `../../iPlug2` / `../../../real/iPlug2` (correct physically) | `../../../../real/iPlug2`, never matches, so the rewrite was silently skipped |
| target on another drive than iPlug2 (`D:`) | ValueError | — | ValueError (pre-existing; see B1) |

- **mac runs** on a `git archive hvoya-linux Examples Scripts` copy in the scratchpad, with IPlugEffect at six
  placements:
  - in tree (3 args);
  - `.`;
  - sibling `../../sib`;
  - deep `<scratch>/deep/a/b/c`;
  - inside the tree, `sub/x`;
  - a target through a symlink (`<scratch>/link/tgt` → `real/tgt`).

  All exited 0. For each one, `cd config && cd $IPLUG2_ROOT` and the CMake `IPLUG2_DIR` both resolve
  (`pwd -P`) to the iPlug2 copy's root, and `CMakePresets.json`'s include exists. For the deep one,
  `Deep-linux.code-workspace` (`reaper-open-fx.lua`) and `.vscode/c_cpp_properties.json` also point at the root.
  The worktree stayed clean.

### Findings

**B1 (nit, pre-existing, separate ticket) A target on another drive or mount ends in a traceback after `copytree`**
- **Where:** `Examples/duplicate.py:239` (copytree) before `:255-260`.
- **Problem:** `relpath` raises `ValueError` when the target and iPlug2 are on different Windows drives. The copy has
  already happened by then, so a half-renamed project is left behind. Round 0's `abspath` code did the same; the fix
  neither causes nor cures it.
- **Fix:** compute `iplug2folder`/`newroot` before `copytree`, and turn the `ValueError` into
  `error: the output path must be on the same drive as iPlug2`. Low priority; it can go with the backslash ticket the
  coordinator already split off.

**B2 (nit) The new comment names only half of why it is lexical**
- **Where:** `Examples/duplicate.py:258`.
- **Problem:** "(on Windows: a subst or mapped drive)" leaves out the junction case. That case does not raise; it
  silently skips the rewrite, which is the more surprising failure.
- **Fix:** for example, `# The example's own layout as its files spell it: lexical, because realpath would also
  resolve a subst/mapped drive or a junction that the cwd (and inputprojectname) keep.`

## C. `Documentation/linux-development.md` §8 "Known host issues"

### What I checked
- I checked each bullet against `tasks/T-013-host-matrix.md`'s Log and its evidence:
  - `evidence/t013/drops/carla-main-vst3/crash.txt`: `first timer callback … on OFF-MAIN`, SIGSEGV on
    "CarlaEngineRunn" in `nvgCreateFontFaceMem` ← `IGraphics::LoadFont`;
  - `evidence/t013/t020/gl3-virgl.txt`: `Max core profile version: 0.0`, `OpenGL version string: 2.1`, the
    `no OpenGL 3.2 core context` line;
  - the Carla, drops and Renoise tables.
- `linux-research/scripts/coord/check-product-names.sh`: clean (HEAD).
- No product names appear in §8. Host names are fine.

### Findings

**C1 (minor) The drop bullet contradicts T-013's conclusion**
- **Where:** `Documentation/linux-development.md:189-190`.
- **Problem:** the bullet says files from "a file manager" reach the editor "only if the host forwards XDND". T-013
  found that this holds for a GTK source (GNOME Files). A source that targets the deepest XdndAware window, as Qt
  does, reached the editor in every host tried (Carla 2.5.10, Carla main CLAP, Renoise: "PASS: layout loaded"). That
  was tested with the scripted `xdnd-source.c`, not a real Qt file manager. Also, Carla 2.5.10 does not merely fail to
  forward: it is XdndAware itself and keeps the drop.
- **Fix:** "**Files dropped from a GTK file manager (GNOME Files):** reach the editor only if the host forwards XDND
  to its plug-in window (REAPER and Ardour do; Carla keeps the drop itself; Renoise and Carla main's CLAP window are no
  drop target). A source that addresses the deepest XdndAware window, as Qt file managers do, reached the editor in
  every host tried. Dragging files *out* of the editor worked in REAPER, Ardour, Renoise and GNOME Files."

**C2 (minor) The Renoise bullet hides iPlug2's half of the behaviour**
- **Where:** `Documentation/linux-development.md:187-188`.
- **Problem:** "Renoise sends every key to the plug-in and ignores the ones it hands back" reads as if iPlug2 hands
  the keys back. Per T-013 finding 2:
  - with Renoise's plug-in window focused, the keys arrive as **synthetic** events, and iPlug2 drops unhandled
    synthetic keys on purpose (forwarding them would loop in REAPER);
  - only the keys that come straight to the editor (pointer over it) are forwarded to the parent, and Renoise ignores
    those.

  The section's lead says "iPlug2 does not work around it", and a reader who wants to change that needs to know that
  iPlug2's drop rule is part of the picture.
- **Fix:** "While an editor has the keyboard focus, Renoise forwards every key to it as a synthetic event; iPlug2
  drops unhandled synthetic keys (forwarding them loops in REAPER), and Renoise ignores the keys iPlug2 does pass to
  its parent window. Host shortcuts (Space) work again once Renoise's own window has the focus." Name the tested
  version, 3.5.4.

**C3 (minor) The GL3 bullet repeats the page's intro and is not a host issue**
- **Where:** `Documentation/linux-development.md:191`; already said at `:10-12`.
- **Problem:** the intro already says that GL3 needs a 3.2 core context, that virtual GPUs lack one, that the editor
  does not open, and which message is printed. §8 repeats it under "host behaviour", but virgl is a driver. This
  breaks the "say it once" rule in the user's CLAUDE.md.
- **Fix:** delete the bullet. If the VM case is worth naming, add "(e.g. a VM's virgl driver, GL 2.1 only)" to the
  intro's "old or virtual GPUs".

**C4 (minor) The Carla bullet understates what runs off-thread and overgeneralises the fine versions**
- **Where:** `Documentation/linux-development.md:184-186`.
- **Problem:**
  - Per the T-013 Carla table and `CarlaPluginVST3.cpp` analysis, Carla main dispatches the frame `IRunLoop`'s timers
    **and its fd handlers** (the X connection) from its engine thread, so X event handling runs off the main thread
    too, not only `OnIdle`/drawing.
  - "Carla 2.5.x releases" generalises from the one release tested, 2.5.10, and only its VST3 (2.5.10 has no CLAP).
  - "Carla main" moves; without the tested commit, a reader can't tell whether their build has the bug.
- **Fix:** "**Carla main (tested at 97a9e07), VST3:** calls the plug frame's `IRunLoop` timer and fd handlers from its
  engine thread instead of the UI thread, so the editor's idle, drawing and X event handling run off the main thread
  and can crash while an editor opens. Carla 2.5.10 (VST3) and Carla main's CLAP are fine."

**C5 (nit) Formatting differs from §1-7**
- **Where:** `Documentation/linux-development.md:182-183, 192`.
- **Problem:** every other `## N.` heading is followed by a blank line; §8's is not. The file now ends with an extra
  blank line (`\n\n`).
- **Fix:** add the blank line after the heading and drop the trailing one.
