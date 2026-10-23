# Review: T-036 (Sand makedist-mac.sh / makedist-win.ps1)

Independent reviewer, fresh context, 2026-10-20. Sand repo, branch `dist-scripts`, worktree
`/Volumes/Storage/code/hvoya/Sand-dist`, commits `54093bb` (mac) and `3d25440` (win), on master `d2294da`.

**Verdict: approve with fixes.** No blockers or majors. The minors below are cheap and worth doing before the
first real release run. The nits are optional.

## What I re-ran and checked myself

- `scripts/makedist-mac.sh --no-install --zip --out <scratch>/t036r/distr` exited 0 after about 40 s (incremental on
  the worker's `build-mac/dist`). There were 3× BUILD SUCCEEDED and three bundles were staged.
  - The zip has 74 entries and its listing is identical to the worker's `mac-zip-listing.txt`. It has no
    `.DS_Store`, `__MACOSX` or `._*` entries.
  - Layout `Sand_v1.1.10_mac/{Sand.vst3,Sand.component,Sand.clap}` matches the hand-made `distr/Sand/Sand_v1.1.10_mac.zip`,
    minus that zip's `.DS_Store` and `__MACOSX/` junk.
  - The binaries are universal (x86_64 arm64) and ad-hoc signed, like the hand-made ones. AU codes are aumf/Sand/HvyA.
- **Release:** compile flags live in Xcode's `*-common-args.resp` response files, not on the logged command lines. All
  20 response files (AU 8, CLAP 6, VST3 6) carry `-O3 '-DNDEBUG=1'`, and none has a DEBUG define; the CLAP one I opened
  also has `-DPLOG_DISABLE_LOGGING`.
- **No install:** `ls -laR ~/Library/Audio/Plug-Ins/{VST3,Components,CLAP}` was identical before and after all three of
  my runs, and `find ~/Library/Audio/Plug-Ins -newer <marker>` was empty.
  - `xcodebuild -showBuildSettings` confirms the project's Release `DSTROOT` is `~/Library/Audio/Plug-Ins/<fmt>/IPlug2Dev/Sand-macOS`
    (`DEPLOYMENT_LOCATION = YES`). The overrides move it to `build-mac/dist/dst`.
  - `product_path` returns `~/Library/.../IPlug2Dev/Sand-macOS/Sand.{vst3,component,clap}` without overrides and
    `build-mac/dist/dst/Sand.*` with them.
- `git status --porcelain --ignored` after every run shows only `!! build-mac/` and `!! plug_build_info.hpp`. The
  listing of `distr/Sand` is unchanged.
- **Option parsing:** `--bogus` exits 2 with usage. `--out` with no argument gives `line 29: $2: unbound variable`,
  exit 1 (see n1). `bash -n` passes, and the shell is bash 3.2.
- **Validators** (`<scratchpad>/tools/clap-validator` 0.4.1, `<scratchpad>/tools/vst3-validator`):
  - CLAP: 44 run, 38 passed, 0 failed, 0 warnings, 6 skipped.
  - VST3: 47 passed, 0 failed.
  - I rebuilt the worker's `au_smoke.c` and ran it on my staged component: identical output, render peak 0.427 with
    no non-finite samples.
- **Windows:** `pwsh` is not installed here, so the script is not run and not even parsed. `makedist-win.ps1` is pure
  ASCII (`file`) with LF line endings. I reviewed it line by line against Gneiss' script, `Sand.sln`,
  `projects/Sand-{vst3,clap}.vcxproj`, `config/Sand-win.props`, `scripts/postbuild-win.bat` and iPlug2's `common-win.props`.
- **Worker's pre-existing claims:** all three are confirmed (see the end of this file).

## Findings

### m1 (minor) Incremental builds ship stale bundle content (mac)
- **Where:** `scripts/makedist-mac.sh:51-52, 60-62, 72-77`.
- **Problem:** without `--clean`, the product is whatever has accumulated in the persistent `build-mac/dist/dst/Sand.*`.
  Xcode never removes files from a bundle that the project no longer produces, for example a resource removed from the
  project or renamed.
- **Repro:** I put `STALE.txt` into `build-mac/dist/dst/Sand.clap/Contents/Resources/` and ran the script without
  `--clean`.
  - The file was staged into `Sand_v1.1.10_mac/Sand.clap`.
  - `codesign --verify --deep --strict` still passes, because the build re-signed the bundle with the file inside the seal.
  - Nothing would ever flag it.
- **Note:** `xcodebuild clean` with these overrides removes the whole `dst/`, so `--clean` is safe. Gneiss has the same
  gap.
- **Fix:** `rm -rf "$build_root/dst"` once before the loop. It costs only the copy/sign phases; objects in `obj/`
  stay. Alternatively, `rm -rf "$(product_path "$target" "${overrides[@]}")"` before each build.

### m2 (minor) Leftovers in the release folder are zipped (mac and win)
- **Where:** `makedist-mac.sh:54-55, 96-97`; `makedist-win.ps1:129-132, 177-182`.
- **Problem:** `DEST` is created but never emptied. Only the three or two bundle names are replaced.
  - Anything else already in `Sand_v<ver>_<os>/` is zipped into the release: a doc dropped from `DOCS`, a hand-placed
    file, or on Windows an AAX or a `Sand.vst3` from a hand build that the bundle copy replaces but neighbours do not.
  - Concretely, the default `DISTR_ROOT` already contains the hand-made `Sand_v1.1.10_mac/`, with a `.DS_Store` the
    script deletes.
- **Fix:** start from an empty `DEST`. In bash: `rm -rf "$DEST"` after the version is known, guarded by
  `[ -n "$VERSION" ]`, which is already checked. In PowerShell: `Remove-Item -Recurse -Force` when the folder exists.
  This is the mac/win analogue of `makedist-linux.sh:49-53` ("Only this run's archives may be published").

### m3 (minor) A plain run now overwrites the hand-made 1.1.10 release (operational)
- **Where:** `makedist-mac.sh:15, 54, 97`.
- **Problem:** `config.h` is still at 1.1.10. A default run without `--out` replaces the bundles in
  `distr/Sand/Sand_v1.1.10_mac/` and, with `--zip`, the shipped `Sand_v1.1.10_mac.zip`. The new zip carries a
  different build of the same version, and the old hand-made artifact is lost.
- **Context:** this is by design ("its folder would replace the same version's release"), but the user has never run
  the script.
- **Fix:** no code change is needed. Add one sentence to the hand-over: bump the version, or use `--out`, before the
  first real run. Optionally, refuse an existing `$DEST.zip` without `--force`.

### m4 (minor) The dirty check does not cover the iPlug2 tree, which holds `hvoya/utils`
- **Where:** `makedist-mac.sh:35-37`; `makedist-win.ps1:56-62`.
- **Problem:** the shipped binary compiles `/Volumes/Storage/code/IPlug2` (`../../../IPlug2`), including
  `IPlug2/hvoya/hvoya/utils/*`, which Sand's DSP and UI use directly. That tree is the user's active working checkout
  for all hvoya plug-ins. Uncommitted edits there go into a "clean" release unnoticed. (It was clean during this review.)
- **Fix:** run the same `git status --porcelain --untracked-files=no` on the iPlug2 root.
  - mac: read `IPLUG2_ROOT` from `-showBuildSettings`, or use `$PROJECT_DIR/../../IPlug2`.
  - win: use `IPLUG2_ROOT` from `Get-ProjectProperties`.
  - Name the tree in the message. Or, at minimum, say in the header that only Sand's tree is checked.

### m5 (minor) Windows: the VST3 bundle in build-win is not reset, and -Clean does not reset it either
- **Where:** `makedist-win.ps1:139-153`; `scripts/postbuild-win.bat:87-93, 157-163`.
- **Problem:** the worker's third pre-existing finding is confirmed.
  - The Win32 VST3 postbuild copies into the same `%BUILD_DIR%\Sand.vst3\Contents\x86-win`.
  - The bundle folder is a postbuild artifact, not an MSBuild output, so `Sand-vst3:Clean` leaves it.
  - Any former Win32 build, or any file left there, is staged by `robocopy /MIR` and shipped.
  - `Sand.sln` still offers Win32 configurations.
- **Fix:** the script can make this a non-issue cheaply instead of documenting it: `Remove-Item -Recurse -Force $product`
  (when it exists) before `Invoke-SolutionTarget $target`, so the postbuild recreates the bundle from scratch. The
  product path is known before the build; move `Get-StagedProduct` above the build.

### m6 (minor, unverified: needs a Windows run) PS 5.1 zips may get backslash entry names
- **Where:** `makedist-win.ps1:177-182`.
- **Problem:** under Windows PowerShell 5.1 (.NET Framework, where powershell.exe does not target 4.6.1+),
  `ZipFile.CreateFromDirectory` is reported to write `Sand_v1.1.10_win\Sand.vst3\...` with backslashes. Explorer copes;
  macOS/Linux `unzip` and some web unpackers produce flat files with backslashes in their names.
- **Context:** Gneiss has the same code. The Gneiss win zip in `distr/` was not made by it: it has directory entries for
  non-empty folders, which `CreateFromDirectory` never writes. So there is no evidence either way.
- **Fix:** add `[AppContext]::SetSwitch('Switch.System.IO.Compression.ZipFile.UseBackslash', $false)` before the first
  ZipFile use. Then verify on the first real run with
  `([IO.Compression.ZipFile]::OpenRead($zipPath)).Entries.FullName | Select -First 3`.

### n1 (nit) `--out` without a value
- **Where:** `makedist-mac.sh:29`.
- **Problem:** `--out` at the end of the command line dies with `$2: unbound variable` (exit 1) instead of the usage
  line (exit 2). `makedist-linux.sh` has the same pattern.
- **Fix:** `--out) [ $# -ge 2 ] || { echo "--out needs a directory" >&2; exit 2; }; DISTR_ROOT="$2"; shift ;;`.

### n2 (nit) `product_path` trusts xcodebuild's exit code; an empty field turns into a directory path
- **Where:** `makedist-mac.sh:44-48, 72-77, 83-86`.
- **Problem:** `xcodebuild -showBuildSettings` exits 0 even when it fails half-way. I reproduced it with an unwritable
  `OBJROOT`: it prints `DSTROOT`, then `*** error: Could not get build settings`, and exits 0. `pipefail` doesn't
  catch it, and `product_path` returns `<DSTROOT>/`.
  - In the staging step, `[ -d ]` passes and `basename` stages `dst/`.
  - In the install step, `rm -rf "$dest"` would hit `.../IPlug2Dev/Sand-macOS/`.
  - It is hard to reach after a successful build. Gneiss has the same awk.
- **Fix:** `END{ if (d == "" || n == "") exit 1; print d"/"n }` (the non-zero awk status then trips `pipefail`), plus a
  message at the call site.

### n3 (nit) Packaging still clears the AU caches and kills AudioComponentRegistrar
- **Where:** an AU target build phase (`clear_audiounit_caches.command`).
- **Problem:** it ran in my `--no-install` run (`build-mac/makedist-AU.log`). The worker already noted it in the Log.
  It is harmless, but it is a machine-side effect of a "never touches the machine" run, and it kills a registrar that
  open hosts are using.
- **Fix:** one line in the header so nobody is surprised.

### n4 (nit) The headers promise docs that don't ship
- **Where:** `makedist-mac.sh:2-3`; `makedist-win.ps1:3-4`.
- **Problem:** "together with the user-facing docs" while `DOCS=()` / `$Docs = @()`, and line 16 / line 34 say
  "nothing user-facing ships yet". The header is wrong today.
- **Fix:** drop the phrase until a doc is added.

### n5 (nit) The comment over `Get-StagedProduct` overstates
- **Where:** `makedist-win.ps1:77-78`.
- **Problem:** "The postbuild event stages every format as `$(BUILD_DIR)\$(BINARY_NAME)$(TargetExt)`" is not true for
  the app and VST2. Sand's postbuild writes `%NAME%_%PLATFORM%.exe` / `.dll`. It is true for VST3, CLAP and AAX.
- **Fix:** "stages the VST3 and CLAP as …".

### n6 (nit) `--allow-dirty` still targets the real release folder by default
- **Where:** `makedist-mac.sh:8-9, 35`; `makedist-win.ps1:12-13, 123`.
- **Problem:** a "test build" of a dirty tree replaces the real `Sand_v<ver>_mac/` (and zip) unless `--out` is also
  given.
- **Fix (optional):** make `--allow-dirty` require `--out`, or suffix the folder `_dirty`.

## Windows script: line-by-line notes (no findings beyond m2, m4-m6, n5)

- **Targets and configurations:**
  - `Sand.sln` has projects `Sand-vst3` and `Sand-clap`, and a `Release|x64` solution configuration mapped to
    `Release|x64` for both.
  - The vcxprojs have `TargetExt` `.vst3` / `.clap` and import `$(SolutionDir)\config\Sand-win.props`.
  - `BUILD_DIR = $(SolutionDir)build-win` and `BINARY_NAME = Sand`.
  - So `Get-StagedProduct` yields `build-win\Sand.vst3` (the bundle from `create_bundle.bat` + `copy` into
    `Contents\x86_64-win`) and `build-win\Sand.clap` (`postbuild-win.bat:185`).
  - `-t:Sand-vst3` and `-t:Sand-vst3:Clean` are valid solution metaproj target names.
- **Install-path redirect (worker's claim verified in `postbuild-win.bat`):**
  - CLAP: `set CLAP_PATH=%~5` (line 23).
    - Empty value: `if not exist "" mkdir ""`, then `copy … "\Sand.clap"` (lines 190-191), which copies to the root of
      the current drive. On C: that fails without elevation and fails the build through the errorlevel. On a
      user-writable drive it silently drops `X:\Sand.clap`.
    - Redirected into `build-win\makedist-no-install`: the folder is created and the copy stays inside the ignored
      `build-win`.
  - VST3: `set VST3_64_PATH=%9` keeps the quotes. `if exist %VST3_64_PATH%` (line 164) is false for `""`, so blanking
    would have been fine for the VST3. The redirect is still correct and harmless:
    - On the first run the VST3 builds before the CLAP has created the folder, so it skips.
    - On later runs it xcopies into the same ignored folder.
  - iPlug2's `common-win.props` (hvoya branch) defines neither property. `VST3_64_PATH` was removed in `c7ec2a78a`
    (an ancestor of hvoya), and there is no `<CLAP_PATH>`. Gneiss sets both in its own props and rewrote its postbuild.
- **Error handling:**
  - `Set-StrictMode -Version Latest` and `$ErrorActionPreference = 'Stop'` are set.
  - `$LASTEXITCODE` is checked after every msbuild call: through the returned status in `Invoke-SolutionTarget` (the
    pipeline into `Set-Content` keeps msbuild's exit code), and directly in `Get-ProjectProperties`.
  - It is checked after git in `Assert-CleanTree`, and robocopy's `>= 8` rule is handled with the reset of
    `$global:LASTEXITCODE`.
  - Native stderr is not redirected, so PS 5.1 does not turn git/msbuild stderr into terminating errors.
- **Dirty check:** it runs before anything is built. The git failure is checked, unlike the bash `[ -n "$(git …)" ]`,
  where a failing git reads as "clean". That mac/Linux nuance is not worth a finding: a failing git prints its own error.
- **PS 5.1:**
  - `${ProjectDir}:` / `${installed}:` / `"${target}:Clean"` avoid the drive-qualified-variable trap.
  - `$prop, $folder = switch …` multi-assignment is fine.
  - Splatting `@getters` into a native command is fine.
  - `GetUnresolvedProviderPathFromPSPath` resolves a relative `-Out` against the PS location, which is better than
    `[IO.Path]::GetFullPath`, which uses the process CWD.
  - `-Out` does not clash with the `-OutVariable`/`-OutBuffer` common parameters: an exact name wins over prefix matching.
  - `-getProperty` needs MSBuild 17.8+, and toolset v145 implies VS 2026, so that holds.
  - The file is pure ASCII.
- **Default output folder:** `C:\code\Distributables\Sand` mirrors Gneiss' `C:\code\Distributables\Gneiss`. That is
  fine; the worker already put it to the user as open question 4.
- **Install fallback:** `%LOCALAPPDATA%\Programs\Common\{VST3,CLAP}` are the per-user locations from the VST3 and CLAP
  specs and match what `Gneiss-win.props` sets. The project's own value wins when a user props file defines one.

## Structure and comment rules

- Both scripts mirror Gneiss' closely: same option style, build, stage, install-the-staged-artifact order, logs and
  summaries.
- The edition logic is cleanly gone. Merging Gneiss' two settings queries into one `product_path` (mac) and one
  `Get-ProjectProperties` (win) is an improvement.
- Comments are WHY-only: own build roots, installing the staged artifact, the SolutionDir/backslash quoting trap, the
  `<project>:<verb>` naming, the postbuild's empty-path behaviour, robocopy vs Copy-Item.
- Exceptions: n4 and n5. Sand's CLAUDE.md rules are respected (nothing in `~/Library`; no regenerated tracked file left
  dirty).
- The default install (`INSTALL=1`, Gneiss' behaviour) is a product decision already raised as open question 3.

## Pre-existing problems: verification of the worker's claims

1. **Version hex: confirmed.**
   - `config.h:3-4` has `PLUG_VERSION_HEX 0x00010110` with `PLUG_VERSION_STR "1.1.10"`.
   - iPlug2 decodes 0xMMMMmmbb, so this is 1.1.16. My staged bundles report `CFBundleShortVersionString` 1.1.16
     (all three) and AU `version` 65808 (0x10110). The AU smoke test prints factory version 0x00010110.
   - The hand-made `distr/Sand/Sand_v1.1.10_mac` bundles also report 1.1.16 and 65808.
   - Folder and zip names come from the string (1.1.10), so they match the hand-made naming.
   - Lowering the hex to 0x0001010A would decrease the version hosts see, so this is correctly left to the user.
2. **Windows CLAP_PATH / postbuild: confirmed by reading.**
   - `postbuild-win.bat:23, 189-191`: an empty `CLAP_PATH` leads to `mkdir ""` and a copy to `"\Sand.clap"`.
   - Neither Sand's props nor iPlug2's `common-win.props` define `CLAP_PATH` or `VST3_64_PATH`.
   - An IDE build on the Windows machine has the same problem unless a user props file sets them.
   - It affects plain IDE builds, not only this script, which works around it.
3. **Win32/x64 shared VST3 bundle folder: confirmed** (`postbuild-win.bat:89-90` vs `159-160`, both
   `%BUILD_DIR%\%NAME%.vst3`). See m5 for a cheap fix inside the script.

## Side effects of this review

- Build products went into Sand-dist's ignored `build-mac/`. My `xcodebuild clean` test wiped `build-mac/dist/dst`;
  the next script run rebuilt it, and no `STALE.txt` is left.
- Scratch output is in `/private/tmp/claude-501/-Volumes-Storage-code-IPlug2-linux/f91df715-d8e2-4a5b-b1d6-6e68afefbe6a/scratchpad/t036r/`.
- Nothing was written to `distr/`, `~/Library/Audio/Plug-Ins`, the Sand main checkout or `/Volumes/Storage/code/IPlug2`.
- No git writes.

---

# Round 2 (fix commit `8ceca9d`)

Same reviewer and the same rules, 2026-10-20. I read the diff `3d25440..8ceca9d` and the current scripts in full.
`makedist-win.ps1` is still pure ASCII. `pwsh` is still not installed here, so the Windows changes were checked by reading.

## What I re-ran

- **m1/m2:** I planted two kinds of leftover files and re-ran the script:
  - `STALE.txt` in `build-mac/dist/dst/Sand.clap/Contents/Resources/`;
  - in the scratch release folder, `OLD.txt` and `Sand.vst3/Contents/Resources/OLD2.txt`, with the bundle otherwise
    empty.

  Then I ran `scripts/makedist-mac.sh --no-install --zip --out <scratch>/t036r/distr2`:
  - It exited 0 with 3× BUILD SUCCEEDED in about 23 s.
  - None of the three planted files exists afterwards, either in `dst/` or in the release folder.
  - The zip has 74 entries and no STALE/OLD/`.DS_Store`/`__MACOSX` entries.
  - `codesign --verify --deep --strict` passes on all three bundles.
  - clap-validator: 38 passed, 0 failed, 6 skipped. VST3 validator: 47/47. AU smoke test OK (peak 0.427).
  - `ls -laR ~/Library/Audio/Plug-Ins/{VST3,Components,CLAP}` is identical to before this run and to my round-1
    snapshot, and `find -newer` is empty. The `distr/Sand` listing is unchanged.
  - `git status --ignored` shows only `!! build-mac/` and `!! plug_build_info.hpp`.
- **m4 refusal, tested without touching any repo:** I built a scratch layout `m4/a/IPlug2/` and `m4/a/b/fake/`, made both
  scratch git repos, and committed in them.
  - `fake` holds copies of `projects/Sand-macOS.xcodeproj`, `config/Sand-mac.xcconfig`, `config.h` and the new script.
  - `a/IPlug2` holds a copy of `common-mac.xcconfig`. This layout makes `IPLUG2_ROOT = ../../../IPlug2` resolve to the
    scratch tree.
  - **Dirty scratch iPlug2:** the script printed `uncommitted changes in …/m4/a/IPlug2: commit them, or pass --allow-dirty --out DIR …`
    and exited 1. Nothing was built and no `build-mac/` or out folder was created.
  - **Dirty `fake/config.h`:** the same refusal naming `fake`, exit 1.
  - **Not a git repo** (my first, failed setup): `git status failed in …`, exit 1. So a failing git now fails closed.
  - `--allow-dirty` alone: `--allow-dirty needs --out …`, exit 2. `--out` with no value: `--out needs a directory`, exit 2.
- **n2:** I sourced the new `product_path` and called it with an unwritable `OBJROOT`. xcodebuild still exits 0, but the
  awk now exits 1, so `product="$(…)" || {…}` fires with an empty value. Normal calls still return
  `~/Library/…/IPlug2Dev/Sand-macOS/Sand.clap` (without overrides) and `build-mac/dist/dst/Sand.component` (with them).

## Verdict per fixed finding

| Finding | Verdict | Notes |
|---|---|---|
| m1 stale `dst` content | **fixed** | `rm -rf "$build_root/dst"` before the loop (mac:79). Verified by the planted file. |
| m2 leftovers in the release folder | **fixed** | mac:82, win:141. Verified on mac by the planted files; win by reading (guarded by `Test-Path`). See r2-n1. |
| m3 hand-made 1.1.10 overwritten | **closed** | Hand-over note in the Log, as agreed. n6's refusal does not cover it (a clean tree with the default `--out` still replaces it), so the note must reach the user. |
| m4 iPlug2 tree not checked | **fixed (mac verified, win by reading)** | mac: `IPLUG2_ROOT` resolved against `projects/`, as Xcode does; refusal tested above. Win: see r2-n2. |
| m5 Windows VST3 bundle never reset | **fixed (by reading)** | `Get-StagedProduct` only evaluates properties, so it is valid before the build. `Remove-Item` sits behind `Test-Path`, so a missing path is never an error. `-Recurse -Force` removes the hidden/system/read-only bundle attributes. If the postbuild did not run, line 164 stops with "build product missing", which fails closed. |
| m6 backslash zip entries | **fixed (by reading; unverified on Windows)** | `[AppContext]::SetSwitch` exists in .NET 4.6+, which Windows 10/11 ships with PS 5.1. It is set before the first ZipFile call in the process, which is when ZipFile reads and caches the switch. If the default is already "forward slashes", setting `false` is a no-op, so the change cannot make things worse. The worker's Log note "run in a fresh powershell.exe" is the right caveat. |
| n1 `--out` without value | **fixed** | exit 2, tested. |
| n2 empty settings field | **fixed** | Tested above. |
| n3 AU cache side effect | **fixed** | Header lines 8-9. |
| n4 docs promised | **fixed** | Both headers. |
| n5 "every format" | **fixed** | win:77. |
| n6 `--allow-dirty` into the real release | **fixed** | mac exit 2, tested. Win throws before anything happens (read). |

## New in round 2 (all nits; none blocks)

### r2-n1 (nit) The release folder is deleted before the build, not after it succeeds
- **Where:** `makedist-mac.sh:82`; `makedist-win.ps1:141`.
- **Problem:** a failed or interrupted run leaves `Sand_v<ver>_<os>/` empty or partial next to the previous run's zip.
  With the default `--out` today, that would be the hand-made `distr/Sand/Sand_v1.1.10_mac/`. Its zip survives, so
  nothing is unrecoverable, and m3's hand-over note already says not to run the default destination before a version
  bump.
- **Fix (optional):** stage into `"$DEST.partial"` (win: `"$dest.partial"`), emptied first. After the loop, swap it in
  with `rm -rf "$DEST" && mv "$DEST.partial" "$DEST"`.
- **Related:** `rm -rf "$staged"` (mac:105) is now always a no-op and could go.

### r2-n2 (nit, unverifiable here) The Windows `IPLUG2_ROOT` resolution assumes the value is absolute
- **Where:** `makedist-win.ps1:131`.
- **Problem:** `Sand-win.props` defines `IPLUG2_ROOT` as `$(ProjectDir)..\..\..\IPlug2`. If `ProjectDir` were still
  undefined when the property sheet is evaluated, the value would be relative.
  - Real builds would not reveal this. Relative include paths resolve from the project dir, and the
    `$(IPLUG2_ROOT)\common-win.props` import resolves from `config/`, which is also one level below Sand.
  - In that case `[IO.Path]::GetFullPath` would resolve it against the process CWD, not `projects\`.
  - It mostly fails closed: a nonexistent dir makes git fail, and the script throws. But it would check the wrong repo
    if the CWD-relative path happened to be one.
- **Fix:** resolve it the way the mac script does:
  `[IO.Path]::GetFullPath([IO.Path]::Combine((Join-Path $ProjectDir 'projects'), $root))`. `Combine` returns `$root`
  unchanged when it is absolute.
- **Verify on the first Windows run:** the dirty check should name `…\code\IPlug2`.

## Round 2 verdict: **approve**

Every round-1 finding is fixed or closed. The mac fixes are verified by my own runs, including a real refusal from a
dirty iPlug2 tree in a scratch layout. The Windows fixes are correct by reading. r2-n1 and r2-n2 are optional
hardening.

Still open, for the user and the first Windows run, not for this review:
- the four open questions in the Log (docs, VST3 bundle vs bare DLL on Windows, install by default, the Windows
  distribution root);
- the version-hex decision (bundles report 1.1.16);
- the m3 hand-over note;
- the first real Windows run: check the zip entry separators, that the dirty check names `…\code\IPlug2`, and that
  `build-win\Sand.vst3` holds only `x86_64-win`.

Side effects of round 2:
- Scratch files went to `<scratchpad>/t036r/{distr2,m4}`. The scratch git repos live only there.
- The planted `STALE.txt` was removed by the script's own `dst` wipe.
- No writes to Sand-dist apart from its ignored `build-mac/`. Nothing was written to `distr/`, `~/Library` or
  `/Volumes/Storage/code/IPlug2`.
