# T-037 — `duplicate.py` on Windows writes backslash paths into mac/iOS/web project files

- **Status:** in-progress (low; ticket from the T-011 review, user 2026-10-22)
- **Owner:** worker subagent (lock `.linux-port-locks/T-037`)
- **Branch/worktree:** `linux/T-037` from `hvoya-linux`.
- **Depends on:** T-011 (merged 839e1b51d).
- **Conflicts:** `Examples/duplicate.py` (shared with mac/win).

## Problem (pre-existing upstream code, 2018; details in `notes/29-review-T-011.md`, round 2)
The `config/`/`projects/` rewrite takes `newroot` from `os.path.relpath`, which gives `..\..` on Windows, and writes it
into every file that held `../../..`: mac/iOS xcconfig and pbxproj, the web/wasm `.mk` files, `makedist-*.sh`,
`prepare_resources-*.py`. When iPlug2 is not a direct child of the target folder, the second replacement matches its
own output and the path doubles (`IPLUG2_ROOT = ..\..\..\iPlug2\iPlug2`). Windows builds are fine (`.props`,
`.vcxproj`); a project duplicated on Windows does not build on mac, iOS or web.

## Also (review notes/30 B1, pre-existing)
A target on a different drive from iPlug2 raises `ValueError` in `relpath` after `copytree`, leaving a half-renamed
project. Compute the roots before the copy and print a clear error.

## Fix
Normalise `newroot` to `/` before `dirwalk()` (as T-011 does for the project-folder roots); check that `.props` and
`.vcxproj` still get the form VS expects; check the doubling case.

## Acceptance
- [x] `ntpath` simulation (as in `evidence/t011/duplicate-windows-ntpath-sim.txt`): every rewritten mac/iOS/web file
  gets `/` paths; no doubling with iPlug2 not a direct child of the target.
- [x] mac before/after for all examples in- and out-of-tree: no change.
- [ ] Ideally a real Windows run (the user's PC).
- [ ] Independent review; CROSS_PLATFORM_FINDINGS T11-4 updated.

## Log

### 2026-10-22 — done (worker subagent)
- Lock `.linux-port-locks/T-037`; worktree `/Volumes/Storage/code/IPlug2-T-037`, branch `linux/T-037` from
  `hvoya-linux` 8f7b9f613.
- **Fix** (a15efea90, `Examples/duplicate.py`, approved by the user for this ticket):
  - `newroot` is `/`-separated (`.replace(os.sep, '/')`), like the project-folder roots. On POSIX that is a no-op.
  - The roots are computed **before** `copytree`. A `ValueError` from `relpath` (target on another Windows drive than
    iPlug2) prints `error: output path must be on the same drive as iPlug2 (<root>)` and exits 1 before anything is
    copied (B1). The old code left a half-renamed project behind a traceback.
  - The dead `else: newroot = ""` and a trailing-space line went with the moved block.
- **What the VS files need.** `.props`, `.vcxproj` and `.vcxproj.filters` only ever hold `..\..\..`; the
  mac/iOS/web files only hold `../../..` (no file of the 20 examples holds both). `dirwalk()`'s second pass already
  derives `newroot.replace('/', '\\')` for the VS files. So with a `/` newroot the VS output is the same `\` string
  as before, on Windows and mac alike. VS accepts `/` too, but nothing there needed changing: VS output is
  byte-identical to today's everywhere (checked below).
- **Verification** (`evidence/t037/`):
  - `compare.sh` + `analyse.sh` → `compare-summary.txt`. Before (`hvoya-linux`) and after (a15efea90), each in its own
    `git archive` copy named `iPlug2` with the same layout around it, all 20 examples × 6 placements (in-tree 3 args,
    `.`, `../..`, `../../sib`, deep `deep/a/b/c`, `with space/x`; T-011's set plus `../..`), `random.seed(11)`; once
    natively (mac) and once under `dup-emul.py`'s Windows-separator emulation (`relpath` returns `\`, `os.sep` is `\`;
    file I/O stays POSIX). Results:
    - all 480 runs exit 0;
    - **mac before vs after: identical**, every output file and every console log (paths normalised);
    - **emulated Windows after vs mac after: identical** (every file of every duplicate);
    - emulated Windows before vs after: only the `/` kinds differ (xcconfig, pbxproj, `.mk`, `makedist-*.sh`,
      `prepare_resources-*.py`), never a `.props`/`.vcxproj`/`.filters`;
    - mac/iOS/web files holding a `\` path: 935 before, 0 after; files with a doubled root: 374 before, 0 after.
      Before, a deep target gave `..\..\..\..\..\..\iPlug2\..\..\..\..\..\..\iPlug2\iPlug2`, and `with space/x` gave
      `..\..\..\..\iPlug2\..\iPlug2`;
    - every out-of-tree duplicate's xcconfig and props root resolves to the iPlug2 copy (190/190);
    - other drive (`SIM_OTHER_DRIVE`): before, a traceback after the copy, with `XEffectDrive/` left with unrenamed
      `IPlugEffect-*` files (`drive-before.log`); after, the error line, exit 1, nothing in the target
      (`drive-after.log`).
  - `ntsim.py` → `ntsim.txt`: real `ntpath`, root computation plus `dirwalk()`'s two passes on an xcconfig line and a
    props line, before vs after, for notes/30 B's setups (plain `C:`: direct child, sibling, deep, `.`; `subst S:`
    with targets on `S:`, relative, on `C:`, `.`; mapped `Z:`; a junction with the target inside and outside it;
    another drive). After: `/` lines everywhere, no doubling; the VS line is identical before and after in every
    row; mapped-`Z:`-to-`D:` and `C:`-to-`D:` give the new error before the copy.
  - No mac build is needed: the macOS output is byte-identical. `check-product-names.sh`: clean.
- **Not covered:** a real Windows run (the user's PC); the emulation and the replay model the path separator and
  drive rules, not the Windows file system.

### Cross-platform notes (for CROSS_PLATFORM_FINDINGS.md; the coordinator writes it)
- **T11-4: fixed** (a15efea90): duplicates made on Windows get `/` roots in the mac/iOS/web files, and no doubling.
  VS output is unchanged; macOS/Linux output is byte-identical. Also **B1 / the T11-5 "other drive" item: fixed**: a
  clear error before anything is copied.
- **Reported, not fixed** (pre-existing upstream, all platforms):
  - (low) `dirwalk()`'s `replacestrs(oldroot, newroot)` is a plain substring replace, so `../../..` also matches inside
    longer runs that are not the iPlug2 root. In the macOS pbxproj of IPlugConvoEngine, IPlugWebUI and
    IPlugOSCEditor, `LD_RUNPATH_SEARCH_PATHS` `"@executable_path/../../../../Frameworks"` becomes
    `"@executable_path/../../iPlug2/../Frameworks"` in an out-of-tree duplicate (`../..` target). IPlugOSCEditor's iOS
    pbxproj path `../../../../WDL/jnetlib/netinc.h` gets the same treatment. Repro: `duplicate.py IPlugConvoEngine X
    Mfr ../..`, then grep `executable_path` in the macOS pbxproj. A fix would reuse T-011's `replacepathruns()`.
  - (low, Windows, not run on a PC) `replacestrs()` rewrites each file through `fileinput` in text mode, so on
    Windows every file it touches is written with CRLF line endings, `makedist-*.sh` included. T-011's
    `replaceprojectroot()` uses `newline=''` and is not affected. What reaches a mac depends on how the project
    travels: inside an iPlug2 checkout the root `.gitattributes` normalises `*.sh`/`*.xcconfig`/`*.pbxproj` (`eol=lf`)
    and `*.py` (`text=auto`) on commit; a project in its own repo depends on `core.autocrlf`; a copy made without git
    (zip, shared drive) keeps CRLF, and bash then fails with `$'\r': command not found` (review R1-2).
  - (low, all platforms) `.gitattributes:34` says `.mk` without the `*`, so it matches no file: `git check-attr`
    reports `text: unset` for `Examples/IPlugEffect/config/IPlugEffect-web.mk` (under `* -text`), and CRLF there survives a
    commit. Mostly cosmetic, since GNU make drops a CR before LF (review R1-2).

### Review round 1 (2026-10-22)
- `notes/32-review-T-037.md`: approve; R1-1 (minor) the `newroot` comment cited CMake/JSON, which that pass never
  reaches, and the CMake/JSON reason for the project-folder roots had been dropped: each reason now sits on its own
  line (comment-only). R1-2 (nit): the CRLF note above refined, `.gitattributes`'s `.mk` line reported. R1-3: status.

### Open
- Re-check of R1-1 by the reviewer.
- User: a real Windows run, if wanted.
