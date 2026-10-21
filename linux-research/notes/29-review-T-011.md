# 29 — Review of T-011 (painless duplication), round 1

Reviewer: independent subagent, fresh context. Branch `linux/T-011` (6b35f425d, b245bec0d) against `hvoya-linux` 64312897b.

**Verdict: changes requested.** One major finding (R1-1): the new path rewrite does nothing on Windows, and the Log
and the cross-platform notes claim the opposite. The rest are minor or nits. On macOS and Linux the change does what
the Log says, and I re-ran the comparison myself: the Xcode/VS output is byte-identical apart from the fixed paths.

## What I re-checked myself

- **Before/after, all 20 examples.** I extracted `Examples/` and `Scripts/` of `hvoya-linux` and of `HEAD` with `git archive` into
  two scratchpad trees, both named `iPlug2`, so the paths compare. Then I ran `duplicate.py` with `random.seed(11)` (the
  worker's `dup-seeded.sh` method) five ways per example: in-tree (3 args), in-tree via a 4th argument `.`, a true
  sibling (`../..`, which gives `../iPlug2`), `../../sib`, a deep `deep/a/b/c`, and a parent with a space (`with space/x`).
  - In-tree, both ways: identical except `.gitignore` (+`plug_build_info.hpp`), for all 19 examples that had a complete
    "before". IPlugControls fails before the change and succeeds after it, and its `.knob` is byte-identical to the
    source.
  - Out-of-tree: the only files that differ are `.gitignore`, `CMakeLists.txt` (`IPLUG2_DIR`), `CMakePresets.json`,
    `<Name>-linux.code-workspace` and `.vscode/c_cpp_properties.json`, and only on `../..` → correct relative root
    lines. No xcconfig, pbxproj, vcxproj, props, plist or source file differs.
- **Could the rewrite hit a `../..` that is not the iPlug2 root?** I grepped every root-level and `.vscode/` file of all
  20 examples. Every `../..` there is the iPlug2 root. Runs like `../../..` and a lone `..` (the `cmake ... ..`
  comment in `CMakeLists.txt`) are left alone, as intended.
- **Binary extensions.** `find Examples -type f`: of the binary types, png, icns and ico are filtered, `ttf` (23 files)
  lives only in `resources/fonts/`, which `dirwalk()` never recurses into, and `.knob` (1 file, `scripts/`) is
  the only binary file under a recursed folder. So the extension list is the right place for the fix and nothing else
  is missing today.
- **`plug_build_info.hpp`.** `write_plug_build_info_to_file.sh "../"` (pbxproj) and the `.bat` (vcxproj) write it into
  the project root. IPlugEffect's own `.gitignore` already ignores it, and `duplicate.py` replaces that file, so the
  template line is right.
- **Docker** (`iplug2-linux:24.04`, container `r011`, `--rm`): an out-of-tree IPlugControls duplicate from this
  worktree into the scratchpad. `cmake --preset linux-debug` found iPlug2 at the rewritten relative path, and the build
  of APP, CLAP and VST3 passed. No container of mine is left.
- **Evidence.** I opened `vm/04`, `vm/06`, `vm/10`, `vm/21` and `docker/oot-MyCtl/reaper-MyCtl.png`. They show what the Log
  says (a `ProcessBlock` hit on `reaper/mediafx`, `IVKnobControl::OnMouseDown` on the main thread in CLAP and in the APP, a
  `Gneiss::ProcessBlock` hit, and the full IPlugControls editor). The `gdb-hits.txt` files and `preset-builds.log` match the
  Log too.
- `check-product-names.sh`: clean. Only `evidence/` names plug-ins. The Gneiss patch's include path
  (`../../IPlug2/...`) matches Gneiss's `CMakeLists.txt` layout.
- Style: the new functions follow the file's conventions (2-space indent, docstring-style first line, the
  `fileinput` in-place idiom). The one comment above `replaceprojectroot()` is a real WHY.

## Findings

### R1-1 (major): on Windows the project-root rewrite silently does nothing

`Examples/duplicate.py:260-261` (with `:89-103`).

**Problem.** On Windows `os.path.relpath()` returns backslashes. So `oldprojectroot` is `..\..`, and its `sWin` copy
is the same string. The files this pass edits (`CMakeLists.txt`, `CMakePresets.json`, the workspace,
`.vscode/*.json`) all spell the path `../..`, so `swap()` never matches.

**What follows.** An out-of-tree duplicate on Windows keeps `../..`, which is exactly the bug T-011 set out to fix. The
Log (task file, Cross-platform notes: "broken on every platform" … "In-tree output unchanged") presents it as fixed
everywhere.

**Repro.** Simulated with `ntpath` (cwd `C:\dev\iPlug2\Examples`, 4th argument `C:\dev`): `old='..\\..'`,
`new='..\\iPlug2'`. Both files come out unchanged.

**A trap in the obvious fix.** Normalising only `s` would write `..\iPlug2` into these files. In CMake strings and JSON,
`\i` is an invalid escape, so configure, or the presets/JSON parse, would fail. Every file this pass touches wants `/`
on every platform. The `sWin`/`rWin` branch is therefore dead today, and it would be harmful if it ever fired.

**Fix.**
```python
    oldprojectroot = os.path.relpath(iplug2folder, inputprojectname).replace(os.sep, '/')
    newprojectroot = os.path.relpath(iplug2folder, outputpath).replace(os.sep, '/')
```
and in `replacepathruns()` drop `sWin`/`rWin`. Match `r'\.\.(?:/\.\.)*'` and compare against `s` only. Give the docstring
the reason in one clause, e.g. "CMake and JSON take '/' on every platform; a '\' is an escape there". I applied
exactly this to a scratchpad copy:
- mac output is unchanged, because `os.sep` is already `/`;
- the `ntpath` simulation now gives `${CMAKE_CURRENT_SOURCE_DIR}/../iPlug2` and `../iPlug2/Scripts/cmake/...`.

**Then update** the Log's cross-platform note to say Windows is covered, by simulation unless someone runs it on a PC.

### R1-2 (minor): a symlinked target or iPlug2 path produces wrong relative paths (pre-existing, now in more files)

`Examples/duplicate.py:205` (`os.path.abspath(sys.argv[4])`), `:258` (`os.path.abspath(...)` for `iplug2folder`).

**Problem.** `relpath` works on the logical path, but the OS resolves `..` physically. When the 4th argument goes
through a symlink to a folder at a different depth, every rewritten path points to the wrong place:
- the existing xcconfig `IPLUG2_ROOT` (pre-existing behaviour);
- now also `IPLUG2_DIR`, the presets include and the workspace path.

Real cases: macOS `/tmp` → `/private/tmp`, or a symlinked `~/dev`.

**Repro.** `ln -s <scratch>/real/a/b lnk; duplicate.py IPlugEffect LnkFx Mfr <scratch>/lnk`. Then
`ls lnk/LnkFx/../../iPlug2` and `ls lnk/LnkFx/config/../../../iPlug2` both say "No such file or directory".

**Fix.** Use `os.path.realpath` for `outputbasepath` (line 205) and `iplug2folder` (line 258). This also changes the
existing xcconfig rewrite, for the better, and falls under the user's "fix every rough edge" decision. If the
coordinator would rather not widen the shared change, record it under "Reported, not fixed" instead.

### R1-3 (minor): the Log cites evidence that does not contain the claimed line

`linux-research/tasks/T-011-duplicate-templates.md:53`.

**Problem.** The Log says "`Found iPlug2` at the rewritten path (`evidence/t011/mac-cmake-build.log`)", but that log holds
only the `ninja` build steps, `[0/2]`…`[35/36]`. There is no configure output and no `Found iPlug2` line. The build steps
do compile `/Volumes/Storage/code/IPlug2-T-011/IPlug/*.cpp` from the out-of-tree project, so the path did resolve.

**Fix.** Either append the configure output (`cmake -G Ninja ... | grep 'Found iPlug2'`) to the log, or reword the Log to cite
what the log does show.

### R1-4 (nit): `replacepathruns()` passes the path through `glob.glob`

`Examples/duplicate.py:102`.

**Problem.** A target path containing `[`, `]`, `*` or `?` makes `glob.glob(fullpath)` match nothing, so the file is
silently skipped. This is copied from `replacestrs()`, where the same thing makes the whole duplication a silent no-op:
`duplicate.py IPlugEffect BrFx Mfr '<scratch>/br[1]'` exits 0 with `IPlugEffect` still in `config.h` and `../..` in
`CMakeLists.txt`.

**Fix.** Pass `[filename]` in the new function. Report the pre-existing `replacestrs`/`replacestrsChop` case under
"Reported, not fixed" (low).

### R1-5 (nit): a Windows target on another drive dies with a traceback (pre-existing)

`Examples/duplicate.py:259-261`.

**Problem.** `os.path.relpath` raises `ValueError` ("path is on mount 'C:', start on mount 'D:'") at line 259, before the
new code is reached. Lines 260-261 would raise the same error.

**Fix.** Not a regression. Add it to "Reported, not fixed" (low). If it is ever fixed, catch `ValueError` and fall back to
the absolute path, with `/` for the CMake/JSON pass.

### R1-6 (nit): the docs example needs the parent folder to exist

`Documentation/linux-development.md:130`.

**Problem.** `./duplicate.py IPlugEffect MyPlugin MyCompany ~/dev` exits with "Output path does not exist" if `~/dev` is
missing.

**Fix.** Either `mkdir -p ~/dev` in the snippet, or "4th argument: an existing parent folder". After R1-1, the sentence at
`:134-136` ("points every iPlug2 path at this checkout") is accurate on all platforms. If R1-2 is not fixed, add
"(a real path, not through a symlink)".

### R1-7 (nit): the new pass rewrites and announces every root file, even without a match

`Examples/duplicate.py:112-118`.

**Problem.** `replaceprojectroot()` runs `fileinput` in place, and prints "Replacing iPlug2 root folder in file …", for
every file in the project root and `.vscode/`, including `.RPP`, `.sln` and `README.md`. The output is harmless (same
bytes on mac/Linux), and `dirwalk()` already does the same. `base` is unused.

**Fix.** Optional: skip files whose text does not contain `oldroot`, and drop `base`.

## For the coordinator

- Blocking: R1-1 only. Everything else can be fixed in the same round or moved to "Reported, not fixed".
- After the fix, re-run the before/after comparison for one example (mac output must stay identical) and the `ntpath`
  simulation. Both take seconds.
