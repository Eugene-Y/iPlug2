# T-021 — Release packaging of Gneiss and Sand for Linux

- **Status:** in progress
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
- A `.clap` copied without its `.resources/`: loads, editor opens without text/images, no crash (REAPER, 2 instances, reopen ×5), and stderr now says `iPlug2: resource folder …/Gneiss.resources/ not found; copy it next to the plug-in.` README and §7 tell users to copy both.

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

Everything else in the x86_64 cells passed (install, ldd, clap-validator, REAPER render in CLAP and VST3, standalone, missing-resources message, system install, uninstall). clap-validator: 4 failures everywhere, the known ones (Sand: `process-audio-double-{in,out}-of-place` F5, `process-varying-sample-rates` F3, `param-conversions` F4; Gneiss: the same families); `state-reproducibility-*` pass since T-006.
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
