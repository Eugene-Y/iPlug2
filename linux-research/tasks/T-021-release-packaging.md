# T-021 — Release packaging of Gneiss and Sand for Linux

- **Status:** ready for Gneiss; Sand part after T-012 is merged.
- **Owner:** —
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
