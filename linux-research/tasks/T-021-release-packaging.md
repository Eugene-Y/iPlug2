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
