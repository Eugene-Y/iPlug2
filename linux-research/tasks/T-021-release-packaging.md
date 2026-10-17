# T-021 — Release packaging of Gneiss and Sand for Linux

- **Status:** review-done (review round 2 accepted 86d598736; open items below)
- **Owner:** T-021 worker (claimed 2026-10-17)
- **Branches/worktrees:**
  - iPlug2: `linux/T-021` from `hvoya-linux`, in `/Volumes/Storage/code/IPlug2-T-021` (scripts, docs).
  - Gneiss `linux-experiment` (`/Volumes/Storage/code/hvoya/Gneiss-linux`) and Sand `linux-experiment` (`/Volumes/Storage/code/hvoya/Sand-linux`, from T-012): only if a product needs a packaging hook. Local only, never push; main checkouts untouched.
- **Depends on:** T-003 (merged), T-012 (Sand).
- **Plan phase:** P7 bundles + P11 "one-command flows: package".
- **Conflicts:** none in IGraphics/IPlug; `linux-research/scripts/`, `Scripts/linux/`.

## Goal
One command produces what a customer downloads: for each product and architecture, Release CLAP + VST3 (+ standalone APP) with their resources, in an archive with install notes, built so they run on every mainstream distro from Ubuntu 22.04 on. Mac/win packaging untouched.

## Context
- Builds: `scripts/build-cmake.sh` in `scripts/dock.sh`; images `iplug2-linux:24.04` and `:22.04` (`docker/Dockerfile`, `--build-arg UBUNTU=22.04`). Release builds go on **22.04** (glibc floor): examples 2.34, Gneiss 2.35 (`hypot`, T-003 Log; `.symver` pinning optional — ask the coordinator before adding it).
- Layout (P0/P1, notes/08): VST3 `Name.vst3/Contents/<arch>-linux/Name.so` + `Contents/Resources`; CLAP `Name.clap` + `Name.resources/` beside it; APP binary + `Name.resources/`. Check that a CLAP copied without its resources folder fails gracefully (message, no crash) and document that both must be copied.
- Gneiss has variants: `-DHVOYA_IS_PAID=0/1` (T-003 Log). Ask the coordinator which variants ship; package both if unsure.
- Architectures: aarch64 natively in Docker on this Mac; **x86_64 matters most for customers** — try `--platform linux/amd64` (colima with Rosetta or QEMU; record speed). If x86_64 cannot be built here, document the exact steps for a PC/CI.
- Hygiene check per binary: `scripts/inspect-binary.sh` (exports, NEEDED, GLIBC/GLIBCXX versions), `strip` with split debug symbols (`objcopy --only-keep-debug` + `--add-gnu-debuglink`) kept out of the archive but archived for crash reports.
- Install locations: per user `~/.clap`, `~/.vst3`; system `/usr/lib/clap`, `/usr/lib/vst3` (CLAP and VST3 specs). APP: anywhere, plus an optional `.desktop` file.

## Steps
1. `Scripts/linux/package.sh <product-dir> [--arch …] [--variant …]` (or under `linux-research/scripts/` while experimental): configure + build Release in the 22.04 image, strip, split symbols, assemble `Name-<version>-linux-<arch>.tar.gz` with `README-Linux.txt` (install paths, dependencies: X11, an OpenGL driver; optional zenity/kdialog or xdg-desktop-portal for dialogs) and an `install.sh` (per-user by default, `--system` with sudo). Version from `config.h`/build info.
2. Package Gneiss (both variants if asked) and Sand; aarch64 + x86_64 if possible.
3. Smoke-test each archive in a **clean** container that has only runtime packages (no `-dev`): 22.04 and 24.04, plus Debian 12 and Fedora (latest) images if available — install via `install.sh`, run clap-validator and the VST3 validator, load in REAPER, open the editor (screenshot).
4. Document the flow in `Documentation/linux-development.md` (short section).

## Acceptance
- [ ] One command per product produces the archive(s); reproducible (same inputs → same file list).
- [ ] Each binary: minimal exports; CLAP/VST3 NEEDED = libX11 + libc/libm (+ ld-linux), APP's list recorded (it links SWELL/GDK and audio libs); GLIBC floor ≤ 2.35, no GLIBCXX; symbols split and kept.
- [ ] Clean-container installs pass validators and render in REAPER (22.04, 24.04, and whatever other distros were available); x86_64 built or its build path documented.
- [ ] No changes to mac/win packaging; product repos changed only on their local branches, if at all.
- [ ] Independent review (AGENTS §4); evidence in `evidence/t021/`; Log complete.

## Log
(append: date — what was done — commits — next)

**2026-10-17 — packaging, clean-distro smoke tests, findings.** Evidence: `evidence/t021/` (`dist/` inspect reports, file lists, build logs; `smoke-r1/` first full matrix; `smoke/` final cells; `findings/`). Trimmed to stay small: one REAPER screenshot per run (`r03`, two instances) + the standalone's, as 1000 px JPEG; no gdb thread dumps.

Commits, iPlug2 `linux/T-021`:
- 5ed84b9e7 `Scripts/linux/package.sh`; `docker/gcc13` takes `BASE` (x86_64 image); `docker/runtime/Dockerfile` (customer machines: runtime packages only, the build fails on a compiler or a `-dev`/`-devel` package; validators built on 22.04 so they run everywhere; REAPER); `package-smoke.sh` (host) + `package-smoke-in.sh` (container); `gui-up.sh` passes `DOCK_PLATFORM`.
- 8135c13ba debug archive with `.build-id/` links; `install.sh` creates missing parents (a fresh user has no `~/.local/share`); **`IPlugLinuxPaths.cpp`: one stderr line per module when `<Name>.resources/` is missing** (Linux-only file); runtime image t64 names; `Documentation/linux-development.md` §7.
- 7be579516, 3997f5dbb, 1298e1bf6, 850f99fb0 harness: `GDB_COUNT_SECS` (split symbols take gdb ~7 s for Gneiss; 0 skips the count under Rosetta, where breakpoints never fire), `REAPER_OPEN_WAIT`, smoke stops leftover REAPERs and kills the standalone by path, `gdb-count.sh` stops gdb with repeated SIGINT and deletes its breakpoints before detaching (a SIGKILLed gdb left traps that killed REAPER), counts only complete `HIT … tid=N` lines; `probe/gdb-count-stress.sh`; docker README (release/runtime images, Rosetta notes).
- 74b1d8309 `package.sh --clean`, size in KiB.
- **ac4ca870f hvoya `HostInfoModel::buildTimeInfo`: fix of T21-2 below, by the user's request, as a separate commit** for cherry-picking to `hvoya`.

Commit, Gneiss `linux-experiment` (local, never pushed): 3b9767e `scripts/makedist-linux.sh` — both editions every run, like makedist-mac/win, through `package.sh`; every binary in each archive is checked for `hvoya::kEditionTag` (wrong edition → archive renamed `.REJECTED`, exit 1). Sand: no change (no editions; one command is `Scripts/linux/package.sh ../Sand`).

Flow (one command per product, from the Mac; Docker per arch):
- `Scripts/linux/package.sh /Volumes/Storage/code/hvoya/Sand-linux` and `IPLUG2_DIR=<this tree> Gneiss-linux/scripts/makedist-linux.sh` → `<project>/build-linux/dist/` (here `--out build/t021-dist`): `Name_v<ver>_linux_<arch>[_free|_paid].tar.gz` (`CLAP/`, `VST3/`, `Standalone/` + resources and the SVG icon, `install.sh`, `README-Linux.txt`, `MANUAL.md` for Gneiss), `…_debug-symbols.tar.gz`, `….inspect.txt`, `….files.txt`. Naming follows Gneiss' mac/win convention (`Gneiss_v1.3.1_mac_free`), not the task's `Name-<version>-linux-<arch>`.
- Release + `-g` (GCC's code does not change with -g), strip `--strip-unneeded`, `--only-keep-debug --compress-debug-sections`, `--add-gnu-debuglink`; tar sorted, owner 0, mtime = commit time, `gzip -n`, modes normalized (the build tree had `rwx------`, which broke `--system` installs).
- Checks that fail the package: GLIBC > `--glibc-max` 2.35, any GLIBCXX/CXXABI import, CLAP/VST3 exports beyond `clap_entry` / `GetPluginFactory ModuleEntry ModuleExit`, CLAP/VST3 NEEDED beyond libX11/libc/libm/ld-linux. Result for all 6 archives (`dist/*.inspect.txt`): exactly those exports, NEEDED libX11 libm libc ld-linux, GLIBC 2.35, no GLIBCXX. **APP NEEDED (recorded):** libgdk-3, gdk_pixbuf, cairo, gobject, glib, gio, freetype, asound, jack, pulse-simple, pulse, fontconfig, X11, Xi, GL — so the standalone needs GTK 3 + ALSA + Pulse + JACK client libs installed (README says so; `libjack.so.0` is a hard dependency, not dlopened).
- Times on this Mac (6 cores, colima): aarch64 Sand 2.4 min, Gneiss both editions 5 min; x86_64 under Rosetta Sand 4 min, Gneiss both editions 10 min (with image builds running alongside); incremental rebuilds of everything ~6 min. x86_64 build images: ~6 min to build.
- Reproducible: a `--clean` Sand rebuild into a second dir gave the same `files.txt`, the same `tar -tv` headers, and byte-identical CLAP/README/install.sh.
- Crash reports work: a stripped x86_64 release crash symbolized to source lines from the debug archive (`findings/T21-*`), via `addr2line` and via gdb's build-id lookup (the smoke test's Draw count uses it).
- A `.clap` copied without its `.resources/`: loads, editor opens without text/images, no crash (REAPER, 2 instances, reopen ×5), and stderr now says `iPlug2: resource folder …/Gneiss.resources/ not found; fonts and images will be missing.` (wording after review R10) README and §7 tell users to copy both.

Smoke test per archive (runtime-only image, `install.sh` as user `customer`, then `--system` as root, then uninstall): ldd all resolved; clap-validator + VST3 validator `-e` on the installed copies; REAPER CLAP + VST3 (open, reopen ×5, 2 instances, render check, Draw/X on REAPER's main thread via the split symbols); standalone `--no-io` window; CLAP without resources (message, no crash); `--system` copies world-readable and loadable by `customer`; nothing left after `--uninstall`. I looked at the screenshots (e.g. `smoke-r1/gneiss-aarch64-paid-ubuntu24.04/reaper-vst3/r03-two-instances.jpg`, `…/app.jpg`, `smoke/gneiss-x86-64-paid-ubuntu22.04-amd64/reaper-vst3/r03-two-instances.jpg`): full UIs, fonts, icons, logo.

Round 1, archives before ac4ca870f (`smoke-r1/matrix-*.txt`):

| | Ubuntu 22.04 | Ubuntu 24.04 | Debian 12 | Fedora (44) |
|---|---|---|---|---|
| Sand aarch64 | PASS | PASS | FAIL: REAPER gone after the gdb count (harness, see below) | PASS |
| Gneiss paid aarch64 | FAIL: harness (SIGKILLed gdb, fixed) | PASS | PASS | PASS |
| Gneiss free aarch64 | PASS | — | — | — |
| Sand x86_64 | VST3 validator SEGV = T21-1 | T21-1 | PASS | no image (*) |
| Gneiss paid x86_64 | VST3 validator SIGFPE = T21-2 | T21-2 | T21-2 | no image (*) |
| Gneiss free x86_64 | T21-2 | — | — | — |

Everything else in the x86_64 cells passed (install, ldd, clap-validator, REAPER render in CLAP and VST3, standalone, missing-resources message, system install, uninstall). clap-validator: 4 failures everywhere, the known ones (Sand: `process-audio-double-{in,out}-of-place` F5, `process-varying-sample-rates` F3, `param-conversions` F4; Gneiss: `process-audio-double-{in,out}-of-place` C3, `process-sleep-process-status` and `param-conversions` C5 — lists in `scripts/known-clap-failures/`); `state-reproducibility-*` pass since T-006.
(*) Fedora's GNU tar fails under Rosetta ("Function not implemented"), so no x86_64 Fedora image; Fedora is covered on aarch64 with the same build.
Harness deaths: REAPER died right after the gdb Draw count twice, both in loaded runs (two matrices + builds in parallel). The plug-in's Draw/X hits were all on tid 1 before it. Not reproduced in 25 + 25 stress cycles with the old and new `gdb-count.sh`; the new one deletes its breakpoints before detaching.

Cross-platform findings (NOT Linux; both found only because x86_64 was tested — Apple silicon hides them):
- **T21-1 — Sand, High (memory corruption), fixed elsewhere.** `DmtGrind::setSampleRate` (`dmt_grind.hpp:159`) only `assert`s `sr <= maxSR` (192 kHz × 16); in Release, `assignStorageAndLengths` → `DelayLineWithFeedback::setBuffer` → `clear()` writes past `_storage` when host rate × oversampling exceeds it. The VST3 validator leaves Sand at up to 8x OS from its random-parameter tests, then sets 1 234 567.8 Hz: x86_64 SEGV (6/6 on 22.04, `findings/T21-1-…`), aarch64 silent overrun. Real sessions: host above 384 kHz with 8x (or above 768 kHz with 4x). **Per the user (2026-10-17): fixed by another chat in a parallel worktree**, together with the actual cause of F3: `HilbertEnvelopeDetector` (shared hvoya) needs more than `kMaxCoefs = 14` coefficients above 384 kHz and overwrites neighbouring memory — NaN already at 768 kHz with 1x, and on current master at 88.2/96 kHz with 8x and 192 kHz with 4x with default parameters. So F3 is the Hilbert bug, not T21-1. Not changed here.
- **T21-2 — hvoya `HostInfoModel`, High on x86 (host crash), fixed in ac4ca870f.** `buildTimeInfo` did `size_t(...) % den` with the host's time-signature denominator; a VST3 `process` without a `ProcessContext` (allowed; the validator does it) leaves iPlug's saved context zeroed → den 0 → SIGFPE on x86_64 (kills the host), silent 0 on arm64. Every hvoya plug-in's Windows x64 / Intel mac build. Fix: without num, den and tempo > 0 there is no musical position (beat 1, as arm64 showed). Checked: x86_64 Gneiss VST3 validator SIGFPE 3/3 → 537/537 4/4; macOS Apple clang 21 test TU: identical results to the old header for 4/4, 7/8 and the zero case (`findings/T21-2-…`). Not built in Xcode (the Xcode project builds against the main checkout, which this task does not touch). **Needs cherry-picking to `hvoya`** for mac/win releases.
- T21-3 — iPlug VST3, Low. `IPlugVST3ProcessorBase::PrepareProcessContext` copies `timeSigNumerator/Denominator` (and tempo) without checking `kTimeSigValid`/`kTempoValid`, and its saved context starts zeroed, so plug-ins see 0/0 time signatures. Upstream iPlug2 behaviour; T21-2 guards hvoya against it. Not changed.


Final round, archives with ac4ca870f (iPlug2 850f99fb0 tree, Gneiss 3b9767e, Sand cec9c1d; `smoke/log-final.txt`):

| | Ubuntu 22.04 | Ubuntu 24.04 | Debian 12 |
|---|---|---|---|
| Gneiss paid aarch64 | PASS | (r1 PASS) | (r1 PASS) |
| Gneiss free aarch64 | PASS | | |
| Gneiss paid x86_64 | **PASS** (T21-2 fixed) | **PASS** | **PASS** |
| Gneiss free x86_64 | **PASS** | | |
| Sand aarch64 | REAPER gone after the gdb count (T21-4); everything else passes | (r1 PASS) | VST3 validator SEGV (Sand F1/T21-1, intermittent on arm) |
| Sand x86_64 | VST3 validator SEGV (T21-1) | | |

- T21-4 — REAPER dies after the harness's gdb Draw count, **only with Sand, only on 22.04 / Debian 12 images** (older gdb). Repro: `scripts/probe/hover-stress.sh` with `COUNT_SECS=20` in `iplug2-rt:ubuntu22.04`: dies in round 1–3 (3/3 runs). SIGSEGV inside REAPER's own code (`strcasecmp(NULL)` from REAPER's main loop; once a bad atexit pointer), never in the Sand module (`findings/T21-4-*`, via `segv-preload.c`). Controls: Sand with hover but no gdb 15/15 alive; Gneiss with gdb + hover 8/8; Sand ASan build (24.04) 8/8 with no ASan report; Sand release in the 24.04 image 12/12. Customers do not attach a debugger, so this does not block the release; most likely a gdb-detach interaction, but heap damage from Sand's F1 (0-frame blocks after the stall) is not excluded. Re-run the probe after T-024 fixes F1.
- Sand's remaining cells are its own known bugs (F1, T21-1); the Sand fixes made in the other chat are not on Sand `linux-experiment`, so these archives still have them. Rebuild Sand's archives after they land there.

Acceptance:
- [x] One command per product (`Scripts/linux/package.sh ../Sand`, `Gneiss/scripts/makedist-linux.sh`); reproducible file list (and here byte-identical).
- [x] Exports, NEEDED, GLIBC 2.35, no GLIBCXX, APP NEEDED recorded, symbols split and kept (`dist/*.inspect.txt`).
- [x] Clean-container installs: validators + REAPER render on 22.04, 24.04, Debian 12, Fedora (aarch64) and 22.04/24.04/Debian 12 (x86_64); x86_64 built here under Rosetta. Remaining failures are Sand's own bugs (F1, T21-1) and T21-4.
- [x] No change to mac/win packaging; Gneiss changed only on local `linux-experiment` (3b9767e), Sand not at all. Shared iPlug2 code changed: `IPlugLinuxPaths.cpp` (Linux-only) and, on request, hvoya `host_info_model.hpp` (ac4ca870f; checked with Apple clang, not with Xcode).
- [ ] Independent review.

Next: independent review; then hand-over. For the coordinator: cherry-pick ac4ca870f to `hvoya`; T21-3 decision; rebuild Sand archives once its fixes are on `linux-experiment`.

**Review (round 1, 2026-10-17) — verdict: changes requested** (1 blocker, 2 major). Reviewed `hvoya-linux..linux/T-021` at f4fae7d71 and Gneiss 3b9767e. Most of the work holds up: the archives are well formed, the binary checks are real, the edition split is correct, and the hvoya fix is correct. But `install.sh` deletes Gneiss users' presets, and T21-4 is a bug in our harness, not in REAPER or Sand.

What I re-ran myself:
- Unpacked `Gneiss_v1.3.1_linux_x86_64_free.tar.gz` on the Mac. Layout, modes, owner 0, README and `.build-id` debug layout are as the Log says; `bash -n` / `sh -n` pass on `install.sh`, `package.sh`, `makedist-linux.sh` and all changed harness scripts.
- Edition check: `strings` of all 12 Gneiss binaries (CLAP, VST3 .so and APP × 2 arches × 2 editions). Free ones contain only `edition: free`, paid ones only `edition: paid`.
- `readelf` on x86_64 paid CLAP/VST3. The exports are exactly `clap_entry` / `GetPluginFactory ModuleEntry ModuleExit`, with 0 defined WEAK/UNIQUE symbols and no GLIBCXX/CXXABI. Inspect reports for all 6 archives: NEEDED libX11/libm/libc/ld-linux, GLIBC 2.35, APP list recorded.
- Smoke cell `package-smoke.sh …aarch64_paid.tar.gz ../build/t021r-smoke/gneiss-u24 iplug2-rt:ubuntu24.04`: **RESULT: PASS**. I looked at the screenshots:
  - CLAP and VST3 `r03`: two full UIs with fonts, icons and logo.
  - `app.png`: the full standalone.
  - `nores/r03`: the UI without text or images, no crash.
  - The message is printed, and the Draw/X hits are all on tid=1.
- `Scripts/linux/package.sh /Volumes/Storage/code/hvoya/Sand-linux --arch aarch64 --formats clap --out build/t021r-pkg` works end to end (incremental, 11 s). `Sand.clap`, `install.sh` and `README-Linux.txt` are byte-identical to those in the shipped `Sand_v1.1.10_linux_aarch64.tar.gz`.
- The same with a relative path fails (R5).
- I checked the evidence against the Log: the RESULT lines of `smoke/` and `smoke-r1/` match both tables, and clap-validator fails the same 4 tests in all 25 cells. I looked at the nores screenshots of all 9 final cells (contact sheet) and the x86_64 / Fedora REAPER screenshots: real UIs. Findings T21-1/T21-2 stacks are symbolized to source lines as claimed.
- ac4ca870f: I compiled the old and the new `host_info_model.hpp` against a mock plug-in (Apple clang). On 22 680 valid inputs (num 1–13, den 1–32, tempo 40–999, ppq 0–123456.5, three sample positions, two rates) the output is **byte-identical**. The fix is correct and behaviour-neutral on valid input, and its comment is a WHY. This confirms the Log's claim, which had no artifact in `findings/`.
- Probe for T21-4 (scratchpad copies of `gdb-count.sh`/`hover-stress.sh`, `iplug2-rt:ubuntu22.04`, Sand aarch64 with its debug archive in `/usr/lib/debug`), see R2.

Findings:
- **R1 — blocker. `install.sh` wipes Gneiss' user presets on every reinstall/upgrade and on `--uninstall`.**
  - Where: `Scripts/linux/package.sh:311` (`APP_DIR=${XDG_DATA_HOME:-$HOME/.local/share}/$NAME`), `:340` (`replace` = `rm -rf "$APP_DIR"`), `:319` (uninstall `rm -rf "$APP_DIR"`).
  - Why: on Linux `AppSupportPath()` is `$XDG_DATA_HOME` (`IPlugLinuxPaths.cpp:126`). Gneiss keeps its user presets in `AppSupportPath()/Gneiss/Presets` (`Gneiss.cpp:193-196`), which is the same folder.
  - Reproduced in `iplug2-rt:ubuntu24.04` as user `customer`: after `./install.sh`, I created `~/.local/share/Gneiss/Presets/My.preset`; a second `./install.sh` removed it, and so did `--uninstall`.
  - Fix:
    - Put the per-user standalone where no plug-in keeps data, e.g. `~/.local/lib/<Name>/`; `/opt/<Name>` is fine.
    - Never `rm -rf` a folder the product also writes to.
    - Update README, §7 and `package-smoke-in.sh` (`APP=`, the `left=` check).
    - Add a smoke step: a file in `~/.local/share/<Name>/Presets` must survive reinstall and uninstall.
    - Rebuild the archives, since `install.sh` is inside them.
- **R2 — major. T21-4 is a harness bug in `gdb-count.sh`, not REAPER or Sand.**
  - Where: `linux-research/scripts/gdb-count.sh:11` (`printf "HIT %s tid=%d\n", "<sym>", $_thread`).
  - Cause: gdb 12 (22.04) has to place the string-literal argument in the inferior, so it **calls `malloc` inside REAPER on every hit**. When the stop SIGINT arrives during such a call, gdb prints "The program received a signal in another thread while making a function call from GDB … (malloc) will be abandoned". The sourced script then aborts before `delete`/`detach`, gdb exits, and REAPER resumes from a broken dummy frame, which gives SEGV at garbage PCs. This matches both `findings/T21-4-*` traces.
  - Reproduced: with the original line, REAPER died in round 1 and in round 4, both times right after that gdb error. With the symbol baked into the format string (`printf "HIT <sym> tid=%d\n", $_thread`, no inferior call), it stayed alive 8/8 rounds (~3 300 hits each).
  - Side effect: thousands of inferior `malloc` calls per count also disturb the host under test.
  - Fix:
    - Apply that one-line change; `set unwindonsignal on` is a cheap extra guard.
    - Re-run the Sand aarch64 22.04 / Debian 12 cells.
    - Rewrite T21-4 in the Log: drop "SIGSEGV inside REAPER's own code" and "heap damage from Sand's F1 not excluded". The r1 "harness deaths" were most likely the same thing.
- **R3 — major. The ldd check can never fail the smoke result.**
  - Where: `package-smoke-in.sh:30-35`. The `for … done 2>&1 | tee "$OUT/ldd.txt"` loop runs in a subshell, so `fail` appends to a `FAILS` copy that is thrown away.
  - Demonstrated with the same shape: "FAIL: ldd a" is printed, but the `FAILS` count is 0 afterwards.
  - The evidence is still fine: every `ldd.txt` is empty.
  - Fix: run the loop into the file, then `grep -q 'not found' "$OUT/ldd.txt" && fail …` in the main shell.
- **R4 — minor. The smoke test still does not stop the standalone under Rosetta, although 850f99fb0 says it does.**
  - Where: `package-smoke-in.sh:80` (`pkill -f "^$APP --no-io"` does not match the emulated process).
  - Seen: the Gneiss standalone window is still on screen in the `nores/r03` screenshots of `gneiss-x86-64-paid-ubuntu22.04-amd64`, `-paid-debian12-amd64` and `-free-ubuntu22.04-amd64`. On aarch64 it is gone.
  - Fix: `kill "$(cat /tmp/app.pid)"`, since `$!` in the customer's shell is the app itself, then check that it is gone.
- **R5 — minor. A relative project dir breaks the Docker path.**
  - Where: `package.sh:96-104`. `ORIG_ARGS` passes the relative path into a container whose cwd is `$PROJECT_DIR`.
  - Reproduced: `Scripts/linux/package.sh ../hvoya/Sand-linux …` fails with `cd: ../hvoya/Sand-linux: No such file or directory`. The Log's own command `Scripts/linux/package.sh ../Sand` (lines 48, 97) would hit this.
  - Fix: build the container's argument list from the parsed absolute values (`$PROJECT_DIR`, `--doc`/`--icon` as given relative to it, variants, formats …) instead of forwarding `ORIG_ARGS`. Also correct those Log lines.
- **R6 — minor. The "minimal exports" check counts only GLOBAL binding.**
  - Where: `package.sh:138`. WEAK/UNIQUE exports (vague-linkage templates, `operator new` overrides) would pass.
  - None exist today.
  - Fix: `$5 != "LOCAL"`.
- **R7 — minor. The README names the wrong JACK package.**
  - Where: README text, `package.sh:390`. `libjack0` is JACK1 on Debian/Ubuntu and conflicts with the default `libjack-jackd2-0`, so apt would remove jackd2.
  - Fix: name `libjack-jackd2-0` (what `docker/runtime/Dockerfile` installs) "or PipeWire's JACK libraries".
- **R8 — minor. clap-validator results are not checked.**
  - Where: `package-smoke-in.sh:40-44`. Exit 1 is accepted and only "tests run" is required, so a new failing test would not be noticed.
  - Fix: compare the failed test names with a per-product allowlist.
  - Gneiss fails `param-conversions`, `process-audio-double-{in,out-of}-place` and `process-sleep-process-status`; Sand fails the first three plus `process-varying-sample-rates`.
  - The Log (line 72, "Gneiss: the same families") should name `process-sleep-process-status`.
- **R9 — nit. README and `install.sh` assume all three formats and a resources folder.**
  - The README's Contents/Install text is fixed and ignores `--formats` (`package.sh:367-371`). In my `--formats clap` archive it lists `VST3/` and `Standalone/`, which are not there.
  - `install.sh` `replace "$HERE/CLAP/$NAME.resources"` (`package.sh:332`) fails under `set -e` for a plug-in without a resources folder, which `package.sh` allows (it copies the folder only if it exists).
  - Fix: generate those README lines from `FORMATS`, and guard the copy with `[ -d ]`.
- **R10 — nit. The missing-resources message is wrong for VST3 and the standalone.**
  - Where: `IPlugLinuxPaths.cpp:36`, "copy it next to the plug-in". For VST3 the folder is inside the bundle (`Contents/Resources`).
  - Fix: a format-neutral text, e.g. "…not found; fonts and images will be missing".
- **R11 — nit. Two small `package.sh` gaps.**
  - `--variant`, `--arch` and the other options with a value die with "unbound variable" when the value is missing (`package.sh:39-46`). Fix: `${2:?--variant needs a value}`.
  - On a Linux host newer than 22.04, a native build fails the GLIBC check. Fix: say so in `--help`/§7, or offer a way to force Docker.
- **R12 — nit (process).**
  - Gneiss `CLAUDE.md` says "Never `git commit` … unless explicitly told", and AGENTS §2 says to follow the product's commit rules. The coordinator should confirm that 3b9767e on `linux-experiment` was authorized.
  - `makedist-linux.sh:15` defaults to `../../IPlug2` (main checkout, branch `hvoya`), which has no `package.sh` until the port lands there. The error message could say that.
- **R13 — nit. The evidence could be smaller.** The full per-cell `vst3-validator*.txt` logs are 4.4 MB of the 12 MB; summaries plus failures would do.

New cross-platform finding (report, don't fix):
- **X1 — Low, display only. hvoya `HostInfoModel::buildTimeInfo` computes the beat correctly only at 120 BPM.**
  - Where: `host_info_model.hpp:187`. The beat formula is `ppq*60/tempo*(den/8.)*(4./num)+1`; the bar position should be `ppq*den/(4.*num)+1`.
  - Measured with the mock: 4/4 at ppq = 1 shows beat 4 at 40 BPM, 3 at 60, 2 at 120 and 1 at ≥ 133.7.
  - It affects only the "host time" view in the Gneiss and Sand UIs. The bug is older than ac4ca870f, which leaves this path untouched.

Not issues: no Xcode/VS/`config.h` changes. The shared code touched is `IPlugLinuxPaths.cpp` (Linux-only) and `host_info_model.hpp` (requested by the user, verified above); the comments are WHYs. My containers (`t021-smoke-iplug2-rt-ubuntu24-04`, `t021r-inst`, `t021r-elf`, `t021r-probe`) are removed. Round 2 should check R1–R5 and a re-run of one Gneiss and one Sand cell on 22.04.

**2026-10-17 — review round 1 fixes** (9a05996ac; the reviewer's findings are in its section above):
- R1 (blocker) fixed: per-user standalone in `~/.local/lib/<Name>`; `install.sh` removes only a folder carrying its marker file; the smoke test writes `~/.local/share/<Name>/Presets/smoke-test.preset` before installing twice and checks it after `--uninstall`.
- **R2: T21-4 was the harness, not REAPER or Sand.** `gdb-count.sh` passed the symbol name to gdb's `printf` as a string argument, so gdb 12 called `malloc` in REAPER on every hit; a stop during that call ("will be abandoned") left REAPER crashing after the detach — hence a crash in REAPER's own code and never in the module. With the name in the format string: `probe/hover-stress.sh` Sand, 22.04, `COUNT_SECS=20`: 10/10 rounds alive (before: died in round 1–3, 3/3). My earlier reading ("possibly Sand's F1") was wrong; T21-4 is withdrawn as a finding.
- R3–R11, R13 fixed as listed in the commit. R12: Gneiss `CLAUDE.md` says to commit only when told; 3b9767e was made under this task's brief ("product repos changed only on their local branches"), as T-003/T-012 did — **for the user to confirm**. `makedist-linux.sh` defaults to `../../IPlug2` like the product's CMakeLists, which has no `Scripts/linux/package.sh` until `hvoya-linux` reaches `hvoya`; until then it needs `IPLUG2_DIR` (its error says so).
- T21-5 (reviewer, cross-platform, Low, display only, not changed; was mislabelled X1): hvoya `host_info_model.hpp:187`'s beat is right only at 120 BPM (4/4, one quarter in: beat 4 at 40 BPM, 3 at 60, 2 at 120, 1 from ~134). Predates ac4ca870f.
- R6 follow-up 6ad4c3d6b: `$5 != "LOCAL"` let readelf's header line through ("Name" counted as an export) and failed every package; the check now reads only symbol rows. The round-2 script now stops when packaging fails (a first round-2 run had smoke-tested the previous archives; moved out, not evidence).

Round 2 cells, archives from 6ad4c3d6b (Gneiss 3b9767e, Sand cec9c1d), `smoke-r2/` (`log-round2.txt`):

| Cell | Result |
|---|---|
| Gneiss paid aarch64, Ubuntu 22.04 | PASS |
| Gneiss free x86_64, Ubuntu 22.04 | PASS |
| Gneiss paid x86_64, Ubuntu 22.04 | everything passes except one VST3 validator SEGV in "Silence Processing" (below) |
| Sand aarch64, Ubuntu 22.04 | VST3 validator SEGV (Sand F1/T21-1) — everything else passes, REAPER alive through the gdb count |
| Sand aarch64, Debian 12 | same |

In every cell: install twice + uninstall keep `~/.local/share/<Name>/Presets/smoke-test.preset`; the standalone runs from `~/.local/lib/<Name>`; ldd clean; clap-validator failures exactly the known lists.

- Intermittent Gneiss x86_64 VST3 validator SEGV, twice in 7 smoke runs, both in "Silence Processing" right after "Process function running in another thread" — the ProcessTest family where T-003's ASan caught the **X1** race (`SetLatency` from the fallback-thread `OnIdle` resizes the latency delay line while `process` runs: null in `NChanDelayLine::ProcessBlock` via `PassThroughBuffers`). Not reproduced in isolation: 30 runs with the crash handler, 24 as 4 parallel validators, 12 as user `customer` without the handler — 0 crashes (`findings/X1-*`). Both crashes came while the other smoke stream rendered REAPER, so it needs load; consistent with X1, which awaits the user's decision (STATUS "Waiting on the user" 2, T-010). Not a packaging defect; validator-only (REAPER drives `OnIdle` from its main thread).

**Review (round 2, 2026-10-17) — verdict: accepted, with one open item for the coordinator (R2-1).** Reviewed 9a05996ac, 6ad4c3d6b and 06b9b71ba. R1–R11 and R13 are fixed; R12 is waiting on the user's confirmation.

What I re-checked:
- **R1 (blocker): fixed.**
  - In `iplug2-rt:ubuntu24.04`, as user `customer`, with the current Gneiss aarch64 archive: `~/.local/share/Gneiss/Presets/My.preset` survives two installs and `--uninstall`. The standalone is in `~/.local/lib/Gneiss` with its marker file.
  - A foreign `~/.local/lib/Gneiss` without the marker is left alone: exit 1, message, contents kept.
  - The README's per-user line says `~/.local/lib/Gneiss`.
  - The smoke test now checks that the preset survives, and that check passes in all of the worker's round-2 cells and in both of mine.
- **R2: fixed.**
  - `gdb-count.sh` now puts the symbol name in the printf format string, and the comment explains why (a WHY comment).
  - My Sand aarch64 cell on 22.04 kept REAPER alive through both Draw counts. Every hit was on tid=1: 13 Draw calls per format, 2 000 / 3 980 OnDisplayTimer hits.
- **R3: fixed.** `fail` is now called in the main shell; the output goes to `ldd.txt`, which is empty in all cells.
- **R4: fixed.**
  - The standalone is now stopped by PID.
  - In the round-2 `nores/r03` screenshots of both x86_64 Gneiss cells, no standalone is left on screen. In round 1 it was still there.
- **R5: fixed.**
  - `Scripts/linux/package.sh ../hvoya/Sand-linux --arch aarch64 --formats clap` now gets through configure in the container.
  - It then fails to compile, but for an unrelated reason (R2-3).
  - `--arch` without a value gives "package.sh: --arch needs a value".
- **R6 (+ follow-up): fixed.**
  - `$1 ~ /^[0-9]+:$/ && $5 != "LOCAL"` is right.
  - Current inspect reports: CLAP/VST3 exports are exactly the entry points in all 6 archives, GLIBC floor is 2.35 everywhere.
  - The APP export counts rose to ~4 600 because WEAK symbols are now counted; they are reported only.
- **R7–R11, R13: fixed as described.** I checked:
  - the README text (libjack-jackd2-0, Contents and Install lines driven by `--formats`);
  - the `[ -d ]` guard;
  - the new message;
  - `needs_value`;
  - the clap allowlists;
  - the trimmed VST3 logs.
- **Round-2 screenshots:** contact sheets of all 5 `smoke-r2` cells (CLAP/VST3 `r03`, `app`, `nores`). Full UIs and standalones are shown; the nores runs show the UI without text or images.
- **Smoke cells I re-ran on 22.04** against `build/t021-dist` (6ad4c3d6b):
  - **Sand aarch64:** RESULT FAIL (1), the VST3 validator exit 139 (Sand F1/T21-1, known). Everything else passes: install ×2, preset kept, ldd, the known clap-validator failures only, REAPER CLAP/VST3, standalone, nores, system install, uninstall.
  - **Gneiss x86_64 paid:** RESULT FAIL (1), the VST3 validator exit 139 again. Everything else passes.

Findings:
- **R2-1 — major, for the coordinator and user, not a packaging defect: the intermittent Gneiss x86_64 VST3 validator crash is more frequent than the Log says, and the evidence that it "needs load" is not valid.** I reproduced it in an idle `iplug2-rt:ubuntu22.04-amd64` container, with nothing else running:
  - The smoke script's own install + validator steps, run 8 times: 3 crashes.
  - The validator alone, as `customer`: 2/10 crashes with `timeout`, 2/10 without it, 1/10 on the unpacked `/tmp` copy.
  - With `LD_PRELOAD=build/t021-segv/segv.so`: 0/10 here, 0/20 in another container, 0/25 more in two loops. **The crash-handler preload hides the crash.** So the "30 runs with the crash handler" in `findings/X1-*` say nothing.
  - The crash point varies. 4 of 5 crashes stopped in the 64-bit "Process function running in another thread" test, 1 in the "Silence Processing" test right after it; the validator's output is flushed per line.
  - Attribution to X1 (`SetLatency` from `OnIdle` on the fallback thread while `process` runs, as T-003's ASan caught it): **plausible but not proven**. There is no stack yet. gdb cannot run programs under Rosetta ("Couldn't get registers"), and `dock.sh` sets `core=0`.
  - Fix:
    - Correct the Log: about 1 in 5 validator runs crash on idle x86_64; drop "needs load" and "0 of 66 isolated".
    - Mark `findings/X1-*` as taken with the preload, so not a valid isolation test.
    - To confirm X1, either run the same loop on a build with the latency change deferred to the audio thread (T-010) and show 0/40, or get a stack on real x86_64 hardware or a VM.
  - Impact beyond the validator: `IPlugLinuxHostLoopVST3.h` says the fallback thread also runs the timers in a real host whose only IRunLoop comes through the view's IPlugFrame, while the editor is closed. And X1 is cross-platform anyway (`OnIdle` runs off the audio thread everywhere). So "validator-only" overstates it.
  - Until X1 is decided, the shipped x86_64 Gneiss VST3 fails the Steinberg validator about 1 run in 5. That belongs in the release notes or the decision list.
- **R2-2 — nit: `install.sh` can stop halfway.** It installs CLAP and VST3 before it finds a foreign `APP_DIR`, then exits 1 with a partial install. Check `APP_DIR` before copying anything.
  - The per-user uninstall still runs `rm -f ~/.local/bin/<Name>` without checking that it is our symlink. A `[ -L ] && readlink` check would make it safe.
- **R2-3 — note for the coordinator: Sand `linux-experiment` can no longer be packaged against this tree.** It is now at a027e6d (T-024 F1–F8 merged). It does not compile against `linux/T-021`'s hvoya: `makeSegmentedParser` is missing and there is an `initializer_list` mismatch in `Sand.cpp:164-210`. Rebuild the Sand archives once the matching hvoya changes are on the integration branch. Until then the Sand archives in `build/t021-dist` remain from cec9c1d, with F1/T21-1.
- **R2-4 — nit (my error in round 1): two different findings share the ID X1.** I named the round-1 beat-formula finding "X1", but X1 is already the `SetLatency` race in `CROSS_PLATFORM_FINDINGS.md`, and the Log now uses X1 for both. Rename the beat finding to **T21-5** (`host_info_model.hpp:187`, Low, display only).

The worker's round-2 table matches the evidence (RESULT lines, the `clap-validator-failed.txt` files, the draw-thread counts all on tid=1). My containers (`t021-smoke-iplug2-rt-ubuntu22-04`, `t021-smoke-iplug2-rt-ubuntu22-04-amd64`, `t021r-inst2`, `t021r-x1`, `t021r-x1b`, `t021r-x1c`) are removed. Nothing more is needed from T-021 for the packaging itself; R2-1 goes to X1/T-010 and the release decision.

**2026-10-17 — after review round 2** (verdict: accepted; R2-1 open for the user):
- **R2-1 accepted, my earlier reading corrected.** The Gneiss x86_64 VST3 validator crash does not need load: the reviewer saw it in an idle container in 3/8 smoke-step runs and 5/40 validator runs, and my `segv.so` preload hides it (0/55 with it loaded), so my 30 + 24 handler runs prove nothing and "needs load" was wrong; my 0/12 without the handler was luck at a ~1-in-5–8 rate. It sits in or right after the 64-bit "Process function running in another thread" test, which fits X1, but there is no stack (gdb cannot run programs under Rosetta; the containers have core dumps off). Not validator-only either: a host that gives its IRunLoop only through the editor leaves `OnIdle` on the fallback thread while the editor is closed, and X1 exists on mac/win anyway. **Until X1 is decided, the x86_64 Gneiss VST3 fails the Steinberg validator about 1 run in 5** — for the release notes / the user's decision on X1. Proof options: a T-010 build that changes latency only on the audio thread (expect 0/40), or a stack on real x86_64 hardware.
- R2-2 fixed (this commit): `install.sh` checks, before touching anything, that `~/.local/lib/<Name>` carries its marker and that `~/.local/bin/<Name>` is its own link; uninstall then removes them. Checked on an IPlugEffect archive made by `package.sh` from this tree (`build/t021-example`): a foreign folder and a foreign file each stop the install with nothing copied; install ×2 + uninstall leave `~/.local/share/<Name>` intact. IPlugEffect also packs clean: `clap_entry` / `GetPluginFactory ModuleEntry ModuleExit`, NEEDED libX11/libm/libc, GLIBC 2.35.
- **R2-3: both products have moved past this tree.** Sand `linux-experiment` is at a027e6d (T-024 fixes) and Gneiss at 8a0f05b (master 4dc02eb "OnIdle pumps the MIDI CC mediator", needs hvoya 083bc87c5); neither compiles against `linux/T-021`'s hvoya (`makeSegmentedParser`, `CCMediator::OnIdle` missing). So `build/t021-dist` holds Sand cec9c1d and Gneiss 3b9767e archives from 6ad4c3d6b, i.e. **without R2-2's install.sh and, for Sand, with F1/T21-1**. Rebuild both (one command each) once `hvoya-linux` has the new hvoya.
- R2-4 done: the beat finding is T21-5.

Hand-over:
- Commits `hvoya-linux..linux/T-021` (this file lists them; the last is this one). Gneiss: 3b9767e on local `linux-experiment` (R12: the user decides whether it stays). Sand: none.
- Evidence `evidence/t021/` (dist, smoke-r1, smoke, smoke-r2, findings); full screenshots and logs only in the session scratchpad.
- For the coordinator/user: cherry-pick ac4ca870f (T21-2) to `hvoya`; decide X1 (now also R2-1), T21-3, T21-5; rebuild the archives after the hvoya update; R12.
- Containers removed, lock released, worktree left in place. Docker images added: `iplug2-linux:22.04-amd64`, `iplug2-linux:22.04-gcc13-amd64`, `iplug2-rt:{ubuntu22.04,ubuntu24.04,debian12,fedora}` and `…-amd64` (no Fedora amd64).

**2026-10-17 — user decisions after hand-over, carried out:**
- **X1 must be fixed** → new task **T-025** (`tasks/T-025-setlatency-race.md`, board row, priority "important"; 8ef86da9c). T-010 had explicitly excluded X1, so nothing covered it; T-010 now points to T-025.
- **T21-2 cherry-picked to `hvoya`** (main checkout, user-approved): `59c33feec` (`-x` from ac4ca870f), clean apply, not pushed. Checked on macOS: Gneiss `master` 4dc02eb CLAP Release builds against that `hvoya` with xcodebuild (temporary detached worktree `hvoya/Gneiss-t021mac`, removed; SYMROOT/OBJROOT/DSTROOT in the session scratchpad; nothing installed in `~/Library`).
- **R12: keep** Gneiss 3b9767e.
- **T21-3 fixed** (kept because it makes hosts' missing context safe): ca34a2e69 — the VST3 wrapper takes tempo and time signature only with `kTempoValid`/`kTimeSigValid`, as the CLAP wrapper does with its flags. macOS IPlugEffect VST3 Debug builds (xcodebuild, scratch paths; the `plug_build_info.hpp` it writes into `Examples/IPlugEffect` was moved out, hvoya.cmake's guard caught it). Linux IPlugEffect VST3 validator: 536/537, the same single failure as `hvoya-linux` without the change — "Parameter 001 (id=0): has no title" (IPlugEffect example on `hvoya-linux`; not looked into, for whoever owns the examples). **Cherry-picked to `hvoya` with the user's OK: `228f1ea8e`** (`-x` from ca34a2e69, clean apply, not pushed); Gneiss `master` VST3 Release builds against it on macOS (xcodebuild, temporary worktree removed, nothing in `~/Library`).
