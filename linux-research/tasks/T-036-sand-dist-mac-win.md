# T-036 — Sand: makedist-mac.sh and makedist-win.ps1 like Gneiss'

- **Status:** review-done (round 2 approved; r2 nits fixed)
- **Owner:** T-036 worker (Claude), 2026-10-20
- **Branch/worktree:** Sand repo, branch `dist-scripts` from Sand `master`, worktree `/Volumes/Storage/code/hvoya/Sand-dist`.
  (Not an iPlug2 branch. The coordinator merges into Sand `master` only on the user's command.)
- **Conflicts:** none. Never touch `/Volumes/Storage/code/hvoya/Sand` (the user's main checkout) or `Sand-linux`.

## Why
Gneiss has `scripts/makedist-mac.sh` and `scripts/makedist-win.ps1` that build every shipped format and stage them,
with docs, into `/Volumes/Storage/code/hvoya/distr/Gneiss/Gneiss_v<version>_<os>_<edition>/` (+ optional zip).
Sand's mac/win releases are assembled by hand (`/Volumes/Storage/code/hvoya/distr/Sand/Sand_v1.1.10_mac.zip`: Sand.clap,
Sand.vst3, Sand.component; `…_win.zip`: Sand.clap, Sand.vst3). Sand now has `scripts/makedist-linux.sh` on
`linux-experiment` (aa3e46d) publishing to `distr/Sand/`.

## Deliverables
- `scripts/makedist-mac.sh`: Release VST3 + AUv2 + CLAP via `projects/Sand-macOS.xcodeproj`, staged into
  `distr/Sand/Sand_v<version>_mac/` (matching the existing hand-made naming), `--zip` makes `Sand_v<version>_mac.zip`,
  `--clean`, `--no-install`/install like Gneiss'. **Sand has no free/paid editions** — drop all edition logic.
  Packaging must never install into the machine's plug-in folders unless asked (Gneiss' pattern: own SYMROOT/OBJROOT/DSTROOT).
  No docs are shipped today; keep a `DOCS=()` hook but empty (ask in the hand-over whether to ship anything).
- `scripts/makedist-win.ps1`: same for Windows (VST3 + CLAP; AAX is not in the hand-made zips — leave it out, note it),
  modelled on Gneiss' script (`/Volumes/Storage/code/hvoya/Gneiss/scripts/makedist-win.ps1`, read-only reference).
  It cannot be run here: review it line by line against Gneiss' version and Sand's `.vcxproj` names; say so plainly.
- Follow Sand's own CLAUDE.md and comment rules; read Gneiss' scripts first and keep the structure parallel.

## Verification
- mac: run `scripts/makedist-mac.sh --no-install --zip --out <scratch>` (add an `--out` option like the Linux script's;
  default stays `distr/Sand`). Check the bundles are Release, load (`auval -v aufx <code> <mfr>` for the AU; clap-validator
  is not installed on the mac unless T-023 left one in `~/` — look; otherwise `pluginval` if present, else note it),
  and that nothing landed in `~/Library/Audio/Plug-Ins`. Never write into the real `distr/Sand` during testing.
- Build products and generated files must not dirty the worktree (`git status` clean after a run, apart from ignored
  build dirs) — Gneiss' mac build regenerates `resources/main.rc_mac_dlg` and `plug_build_info.hpp`; check Sand's.

## Acceptance
- [ ] Both scripts, mac one run end-to-end into scratch, archives' contents listed in the Log.
- [ ] Independent review (AGENTS §4).

## Log

### 2026-10-20 — worker
**Commits** (Sand repo, branch `dist-scripts` from master d2294da, worktree `/Volumes/Storage/code/hvoya/Sand-dist`):
- `54093bb` `scripts/makedist-mac.sh`: Release VST3 + AU + CLAP via `projects/Sand-macOS.xcodeproj` into
  `<distr>/Sand_v<version>_mac/`; `--zip`, `--clean`, `--no-install`, `--out DIR`, `--allow-dirty`. Gneiss' structure without
  editions; own SYMROOT/OBJROOT/DSTROOT under `build-mac/dist`; `DOCS=()` hook empty (bash 3.2-safe expansion); one
  `product_path` helper answers both "where was it built" (with overrides) and "where does it install" (without).
  Tree-dirty refusal mirrors `makedist-linux.sh`.
- `3d25440` `scripts/makedist-win.ps1`: Release|x64 `Sand-vst3` + `Sand-clap` via `Sand.sln` into
  `C:\code\Distributables\Sand\Sand_v<version>_win\` (Gneiss' Windows root; `-Out` overrides); `-Zip`, `-Clean`, `-NoInstall`,
  `-Out`, `-AllowDirty`. AAX left out (not in the hand-made zips). Pure ASCII (PS 5.1 reads BOM-less UTF-8 as ANSI).

**Generated files / git status.** The VST3/AU/CLAP targets run `write_plug_build_info_to_file.sh` (writes
`plug_build_info.hpp`, already in Sand's `.gitignore`) and `prepare_resources-mac.py` (rewrites `resources/Sand-*-Info.plist`;
content identical at 1.1.10, so no diff). `swell_resgen.sh` (→ `main.rc_mac_dlg`) runs only in the APP/AUv3App targets, which the
script does not build. After both runs: `git status --short --ignored` = `!! build-mac/`, `!! plug_build_info.hpp`; nothing to
restore, no `.gitignore` change. Note: after a version bump in `config.h` the first build rewrites the Info.plists — commit them
with the bump. The AU target also runs iPlug2's `clear_audiounit_caches.command` (deletes the AU caches in `~/Library/Caches`,
kills AudioComponentRegistrar) — a project build phase, not something the script adds.

**Ran (mac):**
1. `scripts/makedist-mac.sh --no-install --zip --out <scratch>/t036/distr` → 3× BUILD SUCCEEDED, staged Sand.vst3/.component/.clap, zip, ~50 s.
2. Same with `--clean` → same result (`evidence/t036/mac-makedist-run2.txt`).
3. Dirty tree (README.md touched) → refused, exit 1; bad option → exit 2. Install destinations the project reports:
   `~/Library/Audio/Plug-Ins/{VST3,Components,CLAP}/IPlug2Dev/Sand-macOS/Sand.*` (same as Gneiss' project layout).
- Release: every compile line in the logs has `-O3`, `-DNDEBUG`, `PLOG_DISABLE_LOGGING`; no `-DDEBUG`/`-D_DEBUG`.
  All three binaries universal (x86_64 arm64), ad-hoc signed.
- CLAP: clap-validator 0.4.1 → 44 run, 38 passed, 0 failed, 0 warnings, 6 skipped (`evidence/t036/mac-clap-validator-Sand.clap.txt`).
- VST3: VST3 SDK validator (binary another session built) → 47 passed, 0 failed (`evidence/t036/mac-vst3-validator-Sand.vst3.txt`).
- AU: **`auval` not run** — it only sees installed components and installing was off-limits. Instead an in-process smoke test
  (`evidence/t036/au_smoke.c`): load the staged bundle, `AudioComponentRegister` its `Sand_Factory` (aumf Sand HvyA), instantiate,
  2ch/48k, initialize, 28 parameters, Cocoa UI property present, 200×512 frames of a sine rendered, peak 0.427, no NaN/Inf,
  uninitialize/dispose OK (`evidence/t036/mac-au-smoke-Sand.component.txt`). The user should run `auval -v aumf Sand HvyA` after an install.
- Nothing landed in `~/Library/Audio/Plug-Ins` (full `ls -la` of VST3/Components/CLAP and their `IPlug2Dev/Sand-macOS` identical
  before/after; `find -newer` empty). Nothing written to `/Volumes/Storage/code/hvoya/distr`. `/Volumes/Storage/code/IPlug2` and
  the Sand main checkout unchanged.

**Archive** `Sand_v1.1.10_mac.zip` (8.9 MB, 74 entries, full listing `evidence/t036/mac-zip-listing.txt`; no `.DS_Store`/`__MACOSX`):
`Sand_v1.1.10_mac/{Sand.vst3,Sand.component,Sand.clap}/Contents/` each with `Info.plist`, `PkgInfo`, `MacOS/Sand`,
`_CodeSignature/CodeResources`, `Resources/` (Roboto-Regular.ttf, 8× RobotoMono-*.ttf, hvoya_logo_{circle_dark_bg,white_95,
white_circle_bg}.svg, sand_logo{,_filled,_versions}.svg; the component also `Sand.rsrc`).

**Windows script — not run** (no Windows machine, no `pwsh` here; not even parse-checked). Reviewed line by line against Gneiss'
script and Sand's files: solution `Sand.sln` has `Sand-vst3`/`Sand-clap` with `Release|x64`; `BUILD_DIR` = `$(SolutionDir)build-win`
(`config/Sand-win.props`), `TargetExt` `.vst3`/`.clap`, so the staged products are `build-win\Sand.vst3` (bundle made by the
postbuild via `create_bundle.bat`) and `build-win\Sand.clap`. One real difference from Gneiss: Gneiss rewrote its
`postbuild-win.bat` ("empty install path = don't install") and sets `VST3_64_PATH`/`CLAP_PATH` in its props; Sand still has iPlug2's
stock postbuild and defines neither (iPlug2's `common-win.props` no longer does since upstream c7ec2a78a). With a blank `CLAP_PATH` the
stock postbuild would `copy` the CLAP to `"\Sand.clap"` (drive root), so the script does not blank them like Gneiss: it passes
`-p:VST3_64_PATH=… -p:CLAP_PATH=<build-win>\makedist-no-install`. Install step: the project's value if a user props file sets one,
else `%LOCALAPPDATA%\Programs\Common\{VST3,CLAP}` (the spec per-user folders, as Gneiss-win.props uses). `msbuild -getProperty`
needs MSBuild ≥ 17.8 (the projects use toolset v145, so yes).

**Open questions for the user**
1. Docs: ship anything in the archive (e.g. an installation note like Gneiss')? `DOCS=()` / `$Docs = @()` are ready.
2. Windows VST3 format: the hand-made `Sand_v1.1.10_win.zip` has `Sand.vst3` as a single 1.6 MB file (the bare DLL); the script ships
   the bundle folder `Sand.vst3\Contents\x86_64-win\Sand.vst3` like Gneiss. Bundle OK?
3. Install by default (Gneiss' behaviour, kept) — mac installs into `…/IPlug2Dev/Sand-macOS/` as the Xcode project defines. OK, or
   should packaging default to `--no-install`?
4. Windows distribution root `C:\code\Distributables\Sand` copied from Gneiss' — right for Sand?

**Findings (not fixed, pre-existing)**
- **Version hex (medium):** `config.h` has `PLUG_VERSION_HEX 0x00010110` with `PLUG_VERSION_STR "1.1.10"`. iPlug2 reads the hex as
  0xMMMMmmbb, so the bundles report **1.1.16** (`CFBundleShortVersionString`, AU `version` 65808). The hand-made 1.1.10 release has the
  same 1.1.16. 1.1.10 would be `0x0001010A`, but that is a lower AU/VST3 version than what is already shipped — the user's decision.
- **Sand Windows postbuild (low, unverified — no Windows here):** `scripts/postbuild-win.bat` with an empty `CLAP_PATH` (neither Sand
  nor iPlug2 defines it; only a user props file on that machine could) runs `mkdir ""` and copies the CLAP to `"\Sand.clap"`; with an empty `VST3_64_PATH` the VST3 is simply not
  installed. Gneiss fixed the same in its own postbuild/props.
- **Stale VST3 bundle content (low):** the stock postbuild also stages Win32 builds into the same `build-win\Sand.vst3`
  (`Contents\x86-win`), so after a Win32 VST3 build that would be packaged too. Clear `build-win\Sand.vst3` before a release run if
  Win32 was ever built.

Lock released; worktree left in place.

### 2026-10-20 — Review round 1 fixes (worker)
Review: `notes/26-review-T-036.md` (approve with fixes). Commit `8ceca9d` on `dist-scripts` (on top of `3d25440`).
- **m1** mac: `rm -rf build-mac/dist/dst` before the build loop; Xcode recreates the bundles from the objects in `obj/`.
- **m2** mac `rm -rf "$DEST"`, win `Remove-Item $dest -Recurse -Force` before staging: the release folder starts empty.
- **m3** no code change — see hand-over: `config.h` is still 1.1.10, so a default run (no `--out`) replaces the hand-made
  `distr/Sand/Sand_v1.1.10_mac/` and, with `--zip`, `Sand_v1.1.10_mac.zip`. Bump the version or use `--out` first.
- **m4** dirty check covers the iPlug2 tree too, tracked files only. mac: `IPLUG2_ROOT` from `-showBuildSettings` (target VST3),
  resolved against `projects/` → `/Volumes/Storage/code/IPlug2`; win: `IPLUG2_ROOT` from `Get-ProjectProperties Sand-vst3`,
  `GetFullPath`. A failing `git status` now fails the check (it used to read as clean in bash).
- **m5** win: `Get-StagedProduct` moved before the build; `build-win\Sand.vst3` / `Sand.clap` deleted before each build.
  Relies on the postbuild running on every msbuild build (VC++ post-build events have no up-to-date check); if it ever didn't,
  the script stops with "build product missing" rather than shipping stale content. Unverified until a Windows run.
- **m6** win: `[AppContext]::SetSwitch('Switch.System.IO.Compression.ZipFile.UseBackslash', $false)` before `CreateFromDirectory`.
  Unverified until a Windows run (check `([IO.Compression.ZipFile]::OpenRead($zip)).Entries.FullName | Select -First 3`). The
  switch is cached per process, so run the script in a fresh `powershell.exe`, not after other ZipFile use in the same session.
- **n1** `--out` without a value → "--out needs a directory", exit 2.
- **n2** `product_path`'s awk exits 1 when DSTROOT or FULL_PRODUCT_NAME is missing; both call sites print a message and stop.
- **n3** header says the AU build phase still clears the AU caches and kills AudioComponentRegistrar.
- **n4** headers no longer promise docs; they say the folder is emptied first.
- **n5** comment: "stages the VST3 and the CLAP as …".
- **n6** chosen: `--allow-dirty` without `--out` (win: `-AllowDirty` without `-Out`) refuses, exit 2 / throw — a dirty test build
  can never replace a release.

**Tests (mac):**
- `--allow-dirty` alone → exit 2; `--out` without value → exit 2; Sand tree dirty (README.md touched, restored) → exit 1.
- iPlug2 resolution: `IPLUG2_ROOT=../../../IPlug2` → `/Volumes/Storage/code/IPlug2`. The dirty-iPlug2 branch itself was not
  exercised (may not modify that tree; a scratch fake-tree test was not permitted) — verified by reading only.
- Planted `build-mac/dist/dst/Sand.clap/Contents/Resources/STALE.txt` and `<scratch>/t036/distr2/Sand_v1.1.10_mac/PLANTED.txt`,
  then `scripts/makedist-mac.sh --no-install --zip --out <scratch>/t036/distr2` → exit 0, 3× BUILD SUCCEEDED
  (`evidence/t036/mac-makedist-r1-run.txt`). Neither file exists afterwards in `dst/` or the release folder, and the zip has no
  STALE/PLANTED/.DS_Store/__MACOSX entries: 74 entries, listing identical to round 0 (`evidence/t036/mac-zip-listing-r1.txt`).
- `codesign --verify --deep --strict` OK on all three; clap-validator 38/0/0 (6 skipped); VST3 validator 47/0; AU smoke OK (peak 0.427, no NaN).
- `~/Library/Audio/Plug-Ins` VST3/Components/CLAP listings identical to the original snapshot, `find -newer` empty; nothing new in
  `distr/`; `git status --ignored` = `!! build-mac/`, `!! plug_build_info.hpp`; IPlug2 and the Sand main checkout unchanged.
- Windows script still not run or parsed (no Windows/pwsh here); still pure ASCII.

Lock released; worktree left in place.

### 2026-10-20 — Round 2 nits (worker)
Round 2 approved (`notes/26-review-T-036.md`). Commit `bcb22ec` on `dist-scripts`:
- **r2-n1** mac and win stage into an emptied `Sand_v<ver>_<os>.partial/`, which replaces the release folder only after every
  format is staged (and docs copied); the per-bundle `rm -rf "$staged"` is gone. A failed run leaves the old release folder and
  zip alone and leaves the `.partial` for inspection; the next run removes it. Install still happens per format inside the loop.
- **r2-n2** win: `IPLUG2_ROOT` resolved with `[IO.Path]::Combine(<ProjectDir>\projects, value)` before `GetFullPath`, like the mac script.

Tests (mac): `--no-install --zip --out <scratch>/t036/distr3` → exit 0, zip listing identical to round 0
(`evidence/t036/mac-makedist-r2-run.txt`). Then a temporary local edit `TARGETS=(VST3 BOGUS CLAP)` with
`--allow-dirty --out <scratch>/t036/distr3` → VST3 staged, "BOGUS build failed", exit 1 (`evidence/t036/mac-makedist-r2-failing-run.txt`);
`Sand_v1.1.10_mac/` content checksum and the zip's md5 unchanged, `.partial` holds only Sand.vst3. Edit reverted (`git checkout`).
A follow-up normal run → exit 0, `.partial` gone. `~/Library/Audio/Plug-Ins` identical to the original snapshot; nothing new in
`distr/`; `git status --ignored` = `!! build-mac/`, `!! plug_build_info.hpp`; IPlug2 and the Sand main checkout unchanged.
Windows still not run (r2-n2 by reading only).

Final commits on `dist-scripts`: `54093bb`, `3d25440`, `8ceca9d`, `bcb22ec`. Lock released; worktree left in place.

### 2026-10-22 — aligned with Gneiss; docs in the archives
The user's decisions (2026-10-22): `dist-scripts` will be merged into Sand master by the coordinator after review; the
archives carry the docs the way Gneiss' do (placeholder manual for now); Sand's scripts work like Gneiss' unless there is a
Sand-specific reason; the Linux archive change goes on `linux-experiment`.

**What Gneiss ships and from where.** mac/win: `Installation instructions.txt` and `MANUAL.md`, both at the Gneiss repo root
(`DOCS=(…)` in `makedist-mac.sh`, `$Docs` in `makedist-win.ps1`), copied into the release folder's root. Linux: `MANUAL.md`
only, via `package.sh --doc MANUAL.md`; `package.sh` itself adds `README-Linux.txt` and `install.sh`. No license or readme file.

**Commits (Sand repo):**
- `92aeb3f` (parent: master `d2294da`, no branch of its own) — `MANUAL.md` (placeholder: title, "The Sand manual is coming
  soon.", the installation link Gneiss' manual has) and `Installation instructions.txt` (Gneiss' text adapted: Sand.vst3 +
  Sand.clap on Windows, VST3/Components/CLAP + three `xattr` lines on mac; Gneiss' VST2 `.dll` paragraph dropped).
- `dist-scripts` (Sand-dist): `bd3e8dd` merge of `92aeb3f`; `411bdb8` mac/win scripts ship both docs (same list and copy as Gneiss').
- `linux-experiment` (Sand-linux): `f7810b3` merge of `92aeb3f`; `fc4b9e0` `makedist-linux.sh` passes `--doc MANUAL.md`.
- One docs commit merged into both branches, so a later edit of the docs on master cannot meet an add/add conflict when
  `linux-experiment` comes in. `git merge-tree --write-tree dist-scripts linux-experiment` → clean.

**Sand vs Gneiss, option by option**

| Item | Gneiss | Sand | Verdict |
|---|---|---|---|
| Output folder / names | `distr/Gneiss/Gneiss_v<ver>_{mac,win}_<ed>/` (+`.zip`), win `C:\code\Distributables\Gneiss`; Linux `…_linux_<arch>_<ed>.tar.gz` | same without `_<ed>` | aligned (no editions in Sand) |
| Archive layout | bundles + 2 docs at the folder root; Linux: CLAP/ VST3/ Standalone/ MANUAL.md README-Linux.txt install.sh | same | aligned (this round) |
| Docs | `Installation instructions.txt`, `MANUAL.md` (Linux: MANUAL.md) | same | aligned (this round) |
| Editions, `--only`, edition stamp check | yes | none | kept: Sand has no editions |
| Debug symbols | mac/win: none kept; Linux: `debug-symbols/` | same | aligned |
| Install | by default into the project's install paths, `--no-install` / `-NoInstall` | same | aligned |
| Win install path when the project has none | Gneiss props set `VST3_64_PATH`/`CLAP_PATH`; build blanks them | stock postbuild: blank `CLAP_PATH` copies to the drive root → redirect into `build-win\makedist-no-install`; install falls back to `%LOCALAPPDATA%\Programs\Common\…` | kept: Sand-specific |
| `--out` / `-Out` | Linux only | mac, win, Linux | kept (Sand better); Gneiss mac/win could take it |
| Dirty check | mac/win none; Linux project tree only | mac/win: Sand + iPlug2 trees, git failure fails closed, `--allow-dirty` needs `--out`; Linux: project tree only (like Gneiss) | kept (Sand better); recommend for Gneiss mac/win and both Linux scripts |
| Staging | straight into the release folder, never emptied | empty `.partial`, swapped in at the end | kept (Sand better, m2/r2-n1); recommend for Gneiss |
| Stale bundle content | mac `dst/` and win `build-win\X.vst3` reused | both emptied before the build | kept (m1/m5); recommend for Gneiss |
| `-showBuildSettings` empty-field guard | none | awk exits 1 | kept (n2); recommend for Gneiss |
| `--out` without a value | Linux: `$2: unbound variable` | mac: exit 2; Linux: as Gneiss | kept on mac; trivial for both Linux scripts |
| Win zip separators | no switch | `UseBackslash=false` | kept (m6, unverified on Windows); recommend for Gneiss |
| Linux: all archives checked before any is published | yes (separate verify and publish loops) | yes | aligned; Sand's SIGPIPE fix is not needed in Gneiss (its `grep -x` reads all input) |

**Verification (2026-10-22):**
- mac: `scripts/makedist-mac.sh --no-install --zip --out <scratch>/t036a/distr` → exit 0, 3× BUILD SUCCEEDED, ~30 s. Zip: 76
  entries (round 2's 74 + the two docs), top level `Sand.{clap,vst3,component}/`, `MANUAL.md`, `Installation instructions.txt` —
  Gneiss' layout; no `.DS_Store`/`__MACOSX`/`._*`. Docs byte-identical to the repo's; `codesign --verify --deep --strict` OK on all
  three. `~/Library/Audio/Plug-Ins` listing identical, `find -newer` empty; `distr/Sand` unchanged; Sand-dist `git status --ignored`
  = `!! build-mac/`, `!! plug_build_info.hpp`; IPlug2 main checkout clean.
- Linux: `scripts/makedist-linux.sh --out <scratch>/t036a/distr-linux` (clean tree, Docker, both arches) → exit 0, 2 min 43 s.
  Both archives: `CLAP/ VST3/ Standalone/ MANUAL.md README-Linux.txt install.sh` — Gneiss' Linux layout; MANUAL.md identical to the
  repo's; `debug-symbols/` holds the symbols and reports.
- Windows: not run, no Windows and no `pwsh` here. Diff read: `$Docs` and the copy are now Gneiss' lines with `$stage` for
  `$dest`; the file is still pure ASCII.

Scratch: `/private/tmp/claude-501/-Volumes-Storage-code-IPlug2-linux/f91df715-d8e2-4a5b-b1d6-6e68afefbe6a/scratchpad/t036a/`.

**Open:** the `hvoya.audio/installation/` link in the placeholder manual is Gneiss'; check that it suits Sand. Gneiss'
`Installation instructions.txt` names a VST2 `.dll` it does not ship and does not mention CLAP (Gneiss not changed).
Still open from before: version hex (bundles report 1.1.16), Windows VST3 bundle vs bare DLL, install by default, Windows root,
first real Windows run, and a version bump before the first default (non-`--out`) run.
