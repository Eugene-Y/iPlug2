# 32 — Review of T-037 (duplicate.py Windows backslashes), round 1

Reviewer: independent subagent, fresh context. Range `hvoya-linux..linux/T-037`: a15efea90 (fix), a73b8a285
(evidence), 5ef135011 (task log).

## Verdict

**Approve**, with one minor comment fix (R1-1) to make before the merge. No major finding. The behaviour is right: the
mac/iOS/web files get `/` roots on Windows, there is no doubling, and the VS output does not change. The other-drive
case now fails before anything is copied. macOS output is byte-identical. R1-1 changes a comment only, so a quick
look at that one edit is enough; the evidence does not need re-running.

## What I re-checked myself

- **Diff** (`Examples/duplicate.py` 245-266). The root block moved above `copytree`. `newroot` gets
  `.replace(os.sep, '/')`. The three `relpath` calls sit in a `try` with a `ValueError` → `error: …` + exit 1. The dead
  `else: newroot = ""` is gone. That is all; nothing else in the script changed.
- **`compare.sh` + `analyse.sh`**, re-run into my scratchpad (`compare.sh <wt> hvoya-linux a15efea90 <out>`, 1 min
  19 s). My `analyse.sh` output is **identical line for line** to the committed `compare-summary.txt`:
  480/480 runs exit 0. mac before = after (files and console logs). Emulated-Windows after = mac after. The emulated
  Windows before→after diff touches only xcconfig/pbxproj/`.mk`/`makedist-*.sh`/`prepare_resources-*.py`, never a
  VS file. `\` paths in mac/iOS/web files: 935 → 0. Files with a doubled root: 374 → 0. Roots resolve in 190/190.
  For the other drive I checked the target myself: `win/after/driveD/out` is empty, while `win/before` left
  `XEffectDrive/config/IPlugEffect-*` behind.
- **`python3 ntsim.py`**: the output is identical to the committed `ntsim.txt`.
- **Which files hold the root.** I grepped all 20 examples. `../../..` occurs only in mac/iOS xcconfig and pbxproj,
  `prepare_resources-{mac,ios,win}.py`, `-web.mk`/`-wasm.mk` and `makedist-{web,wasm}.sh`. `..\..\..` occurs only in
  `.props`, `.vcxproj` and `.vcxproj.filters`. No file holds both, so the two `dirwalk()` passes cannot interfere
  once `newroot` is `/`, and the VS pass still derives exactly the old `\` string. The Log's claim holds.
- **The Log's cross-platform notes**, checked in my run's output:
  - the `@executable_path/../../iPlug2/../Frameworks` and `../../iPlug2/../WDL/jnetlib/netinc.h` mangling;
  - `fileinput`'s text-mode writes;
  - `git check-attr` on the example files.

## Is the emulation faithful enough?

- **`dup-emul.py`** patches `posixpath.relpath`, which is `os.path.relpath`, to return `\`, and sets `os.sep = '\\'`
  on the shared `os` module that `duplicate.py` imports. It therefore exercises the real script end to end, both
  `dirwalk()` passes and `replaceprojectroot()`, with the exact strings Windows' `relpath` produces. It does not model
  ntpath `join`/`realpath`/drive handling, case-insensitivity, or `fileinput`'s CRLF writes. Setting `os.sep` globally
  could in principle disturb `glob`/`fileinput`/`shutil`, but mac-after == win-after shows it did not.
- **`ntsim.py`** covers the gap. Its `roots()` mirrors `main()` 248-263 statement for statement, using real `ntpath`,
  and its `dirwalk_root_passes()` is exactly lines 171-172. `os.getcwd` is stubbed and `ntpath.abspath`'s fallback
  uses it, so `.` and `..\..\out` resolve against the simulated cwd, as the output shows. The `realpath` stub maps
  subst to the underlying path and a mapped drive to UNC, which is what `GetFinalPathNameByHandleW`-based
  `realpath` does on Python ≥ 3.8. On older Pythons `realpath` is `abspath`, the lexical case, and the plain-`C:`
  rows already cover it.
- **Together** they are a fair model of the separator and drive rules. The one real gap is the one the Log names
  itself: there has been no run on a real Windows Python and file system.

## Windows edge cases considered (no finding)

- **3 args (in-tree):** the block is skipped and nothing changes.
- **`.` as the target:** `newroot == oldroot` after the fix, so the replace is a no-op. Before the fix it was
  `..\..\..`, written into the mac files.
- **Drive-relative (`D:out`), forward-slash (`C:/x`) and `\\?\` targets:** `realpath` normalises all three before
  `relpath`, same as before.
- **UNC vs drive, two different shares, or iPlug2 behind a junction that points to another drive:** each is a
  `ValueError` from `ntpath.relpath`, now caught before the copy. The message prints the *resolved* `iplug2folder`,
  which is the useful path when a junction or mapped drive is the cause.
- **Drive-letter case (`c:` vs `C:`):** `ntpath.relpath` compares with `normcase`, so there is no spurious `..\C:`.
- **Root computation now runs before `copytree`.** Only `parse_xcconfig()` of the **input** project can now fail
  earlier, on a missing `-mac.xcconfig` or a missing `IPLUG2_ROOT` key. That is the same file the old code read
  right after the copy, so the failure is the same traceback, just without the half-copied project left behind.
  This is an improvement, not a behaviour change on any working input. `oldprojectroot`'s `relpath` takes two
  cwd-relative paths and cannot raise. On POSIX, `relpath` raises `ValueError` only for an empty path, which cannot
  happen here.
- **Python 2** (the header still claims it): `try/except ValueError` works there, and Python 2's `ntpath.relpath`
  raises `ValueError` for drive or UNC mismatches too.

## Findings

### R1-1 — minor — `Examples/duplicate.py:254-258` — the `newroot` comment cites the wrong files

**Problem.** The comment says `'/'` is needed because "CMake and JSON read '\' as an escape". The `newroot` replace
never reaches a CMake or JSON file: `dirwalk()`'s `../../..` occurs only in xcconfig, pbxproj, `.mk`, `makedist-*.sh`
and `prepare_resources-*.py` (grep of all 20 examples). The CMake/JSON reason belongs to
`oldprojectroot`/`newprojectroot`. The comment that used to say so ("These land in CMake and JSON files…") was
dropped in this diff, so the `.replace(os.sep, '/')` on those two lines lost its WHY. The second comment
("Lexical, …") explains only the lexical choice. By the user's rule a comment that does not match its code is a
defect.

**Fix.** Give each reason to its own line, and keep both comments short, e.g.:

```python
      # '/' on every platform: the mac/iOS/web files take nothing else, and dirwalk() derives the VS files' '\' form in a
      # later pass, which would match a '\' newroot again and double the path.
      newroot = ...
      # These land in CMake and JSON files, which read '\' as an escape. Lexical, like inputprojectname, which is
      # relative to the unresolved cwd (on Windows a subst or mapped drive or a junction would make a resolved path
      # disagree with it).
      oldprojectroot = ...
```

### R1-2 — nit — task file Log, "Cross-platform notes", CRLF item — refine before it goes into CROSS_PLATFORM_FINDINGS

**Problem.** The substance is right. `fileinput` with `inplace=1` reopens each file with `open(name, 'w')` (text mode),
so on Windows every file `replacestrs()` visits is written with CRLF, and `replaceprojectroot()`'s `newline=''` is
unaffected. "Unless git's autocrlf converts them back" is incomplete, though:

- Inside an iPlug2 checkout, the root `.gitattributes` normalises on commit regardless of `core.autocrlf`:
  `*.sh`, `*.xcconfig` and `*.pbxproj` are `text eol=lf`, and `*.py` is `text=auto`.
- Its `.mk` line lacks the `*`, so `git check-attr` reports `text: unset` for `IPlugEffect-web.mk` (under `* -text`).
  CRLF there survives a commit. This is mostly cosmetic, since GNU make drops a CR before LF.
- A project in its own repo (no `.gitattributes`) depends on `core.autocrlf`. A copy made without git (zip, shared
  drive) keeps CRLF, and that is where bash's `$'\r'` error bites.

**Fix.** Reword the note along these lines, and mention the `.mk` pattern typo in `.gitattributes` as a separate
pre-existing low item. Report only; do not fix it in this branch.

### R1-3 — nit — task file header — status still `in-progress`

The Log says done, but the **Status** line still reads `in-progress`. Per AGENTS §4.5, set it to `review-done` and
record this verdict once R1-1 is in.

## The Log's other cross-platform note — confirmed

The substring-replace mangling is real and pre-existing, and identical in mac/before, mac/after and win/after. In
`XConvoEngineParent`'s macOS pbxproj, 3 lines read `"@executable_path/../../iPlug2/../Frameworks"`. The source reads
`../../../../Frameworks`. `XOSCEditorParent`'s iOS pbxproj has `path = ../../iPlug2/../WDL/jnetlib/netinc.h`. The
description, the repro and the suggested `replacepathruns()` reuse are accurate.
