# T-035 — Real-hardware test kit: live USB on the user's laptop, tester checklist

- **Status:** work-done (awaiting independent review, round 2)
- **Owner:** worker subagent (chat f91df715), lock `.linux-port-locks/T-035` (released)
- **Branch/worktree:** `linux/T-035` from `hvoya-linux`, in `/Volumes/Storage/code/IPlug2-T-035`.
- **Depends on:** packages from `makedist-linux.sh` (Gneiss, Sand; T-034/T-023 merged).
- **Conflicts:** none (documents and, if useful, a small collection script). No VM, no Docker GUI needed.

## Why
Gneiss and Sand on Linux have only run on llvmpipe (Docker) and virgl (the UTM VM). No real GPU driver (Intel, AMD,
NVIDIA), no Bitwig (x86_64 only), no x86_64 hardware at all. T-034 showed GL behaviour depends on the driver. The user
has no Linux machine; options: an old ASUS laptop booted from a **live USB** (no install), and **asking customers** to
test a beta.

## Deliverables
1. `linux-research/notes/25-real-hardware-testing.md` (internal, English):
   - Live USB on an old x86_64 laptop from macOS: which image (Ubuntu 24.04 LTS desktop; note 22.04 for the glibc
     baseline the packages target), how to write it on a Mac (balenaEtcher or `dd` — exact, safe commands with
     `diskutil list` checks), BIOS/boot-menu keys typical for ASUS (Esc/F2), "Try Ubuntu", Secure Boot notes, NVIDIA
     (nouveau vs proprietary) caveat, persistence optional. Say what a live session cannot show (proprietary NVIDIA
     driver unless installed).
   - What to install in the live session without a full install: REAPER's Linux tarball (portable, `install-reaper.sh`
     not needed), Bitwig demo (.deb or Flatpak), our tarballs and their `install.sh` (read `Scripts/linux/package.sh`
     and the README-Linux.txt it generates to describe the real steps), `glxinfo -B`.
   - A test pass of ~30–60 min per host (REAPER, Bitwig): scan, open editors, resize, text entry (Sand), tooltips,
     presets, automation, save/reload project, latency change (Gneiss OS factor), two instances, close while a menu is
     open, standalone app with audio. Each step with the expected result and what to capture.
   - How to report a failure: which logs and environment facts (distro, kernel, `glxinfo -B`, desktop session X11 or
     Wayland, scale factor, host version), how a crash is captured (core/gdb, or the host's crash log), and how our
     `_debug-symbols.tar.gz` (kept in `hvoya/distr/<Plugin>/debug-symbols/`) turns an address into a line.
2. A short **tester-facing** checklist (customer beta testers; plain language, no internal paths, no repo names), as
   `linux-research/notes/25-tester-checklist.md` — the user will adapt it for the website/e-mail. It must not promise
   anything the packages don't do. No product-internal jargon.
3. Optional: `linux-research/scripts/collect-linux-report.sh` — a POSIX script a tester can run that prints the
   environment facts above into one text file (no network, nothing sent anywhere, no personal data beyond what is
   listed; it says so at the top). Only if it stays small (< 80 lines) and runs on Ubuntu 22.04/24.04 and Fedora;
   test it in the Docker image (`scripts/dock.sh`).

## Rules
- Facts about our packages come from the code (`Scripts/linux/package.sh`, the generated README-Linux.txt/install.sh in
  a real archive under `/Volumes/Storage/code/hvoya/distr/` or a fresh `makedist-linux.sh --out <scratch>` run) — not
  from memory. Third-party steps (Ubuntu, REAPER, Bitwig, Etcher) must be current; check their official pages.
- Product names are fine here (linux-research/ and tester docs are not iPlug2's shipped text).

## Acceptance
- [x] Both notes written, every package step verified against a real archive.
- [x] Optional script tested in Docker (22.04 and 24.04 images) or explicitly dropped.
- [ ] Independent review (AGENTS §4).

## Log

### 2026-10-20 — work done (worker)
**Files** (numbered **25**, not 23: `notes/23-review-T-004.md` and `notes/24-review-2026-10-20.md` already exist):
- `notes/25-real-hardware-testing.md` — internal: image choice (24.04.5.1 main pass; 26.04.1 is Wayland-only GNOME;
  22.04.5 for the glibc floor, REAPER only), Etcher and `dd` with `diskutil list external physical` checks, ASUS
  Esc/F2, Secure Boot, safe-graphics and NVIDIA/nouveau/hybrid caveats, live-session limits, setup (universe,
  mesa-utils, gdb, systemd-coredump), REAPER portable, Bitwig (AVX2 + Ubuntu 24.04 requirement, trial needs an account),
  our packages and `install.sh` with its real output, a 15-step pass with expected results and captures, crash
  capture (coredumpctl / gdb + `info proc mappings`) and offset → source line with our debug symbols; sources.
- `notes/25-tester-checklist.md` — customer-facing: requirements as README-Linux.txt states them, install, 12 checks,
  known limitations (Alt-drag, IME, no units in text fields), what to send.
- `scripts/collect-linux-report.sh` — 69 lines, POSIX sh, read-only, no network, writes `$HOME` as `~`.

**Commits:** e508e6a29 (script), ddcae5d62 (notes), and this Log commit.

**Verified, and how**
- Archives: Gneiss 1.3.1 paid x86_64 and Sand 1.1.10 x86_64 (test builds in the coordinator's scratchpad
  `distr-test/`, Gneiss marked `2cab58b-dirty`) unpacked; README-Linux.txt and install.sh read; install.sh run in
  `iplug2-rt:ubuntu22.04-amd64` as a user (`./install.sh`, `--no-app`), as root without `--system` (refused),
  `--system`, `--system --uninstall`, `--uninstall`: output quoted in the note matches.
- Symbols: `addr2line` on `Gneiss.so.debug` and gdb with `set debug-file-directory` on the stripped `Gneiss.so`
  (build-id lookup) both resolve 0x1e13a8 → `IPlugProcessor.cpp:78` (`iplug2-linux:22.04-gcc13-amd64`). First LOAD
  segment at vaddr 0, so module offsets are file addresses.
- Report script in Docker: plain `ubuntu:22.04` (no tools, all libraries MISSING), `iplug2-rt:ubuntu22.04-amd64`
  (packages installed: build-ids match the `.inspect.txt`), `iplug2-rt:ubuntu24.04` (+amd64) with Xvfb `-noreset`
  and an Xft.dpi resource (read back as 144; xrandr lines), `iplug2-linux:22.04` and `:24.04` with Xvfb (glxinfo -B,
  gsettings), `iplug2-rt:fedora` (Fedora 44, Gneiss aarch64 with its `.resources` removed: flagged). Containers
  `t035-*`, all `--rm`. Outputs: scratchpad `t035/out/`.
- macOS `dd` accepts `bs=4m status=progress` (macOS 26.6.2, /dev/zero → /dev/null).
- Third-party facts from the vendors' pages on 2026-10-20 (list at the end of the note).

**Not verified** (marked *unverified* in the note): booting a live session at all (no hardware here); "Ubuntu on
Xorg" from a live session's login screen; legacy-BIOS boot of these ISOs; REAPER's `reaper.ini` portable switch
(from a third-party quote of its readme); the Bitwig .deb file name and the Flatpak's view of `~/.clap`/`~/.vst3`;
Bitwig's plug-in host process name; `coredumpctl info` output with our binaries; REAPER's "spls" latency readout
on Linux. The script's `lspci`, `pactl`, `coredumpctl`, `/var/crash` and GNOME `gsettings` lines were not
exercised (no such tools/session in the images). No shellcheck available (not on the Mac or in the images).

**Open questions for the coordinator/user**
1. Ship `collect-linux-report.sh` to beta testers (beside the archives or pasted into the e-mail)? The checklist
   mentions it conditionally ("If we sent you …").
2. Sand text entry: the checklist says units cannot be typed on Linux; update it if T-029 is accepted and merged.
3. The note's first real run on the ASUS should replace each *unverified* mark with what was seen.

### Review round 1 (`notes/25-review-T-035.md`): approve with fixes — fixes
- **M1** crash example: the frame is now `(Gneiss.clap + 0x1a0d58)` resolved with `Gneiss.clap.debug`; the note maps
  each binary to its `.debug` (`.clap` → `Gneiss.clap.debug`, VST3 `.so` → `Gneiss.so.debug`, standalone →
  `Gneiss.debug`) and says to choose by build-id. Re-run in `iplug2-linux:22.04-gcc13-amd64` (`t035-sym`, `--rm`):
  `Gneiss.clap.debug` @0x1a0d58 → `IPlugProcessor::ProcessBlock`, IPlugProcessor.cpp:78; gdb on the stripped
  `Gneiss.clap` via `debug-file-directory` → same line; `Gneiss.so.debug` at the same offset → an unrelated VST3 view
  thunk (said in the note); `Gneiss.clap.debug` @0x1e13a8 → `fonsTextBounds` (the reviewer's finding confirmed).
- **M2** BitLocker/Device Encryption warning and saving the recovery key before any BIOS security change.
- **M3** dd block: `N=` set by hand, every command uses `${N:?set N first}` (checked in zsh and bash: the command is
  not run while N is empty), "everything on the stick is erased", stop unless `diskutil info` shows the stick.
- **m1** Sand has no presets: note step 7 and checklist check 6 (Gneiss-only, Sand via the host's preset).
- **m2** script: `build-id unknown (needs readelf or file)` instead of an empty field.
- **m3** script: `$HOME` substitution only when HOME is set and not `/`; "Could not write <file>" and exit 1 when the
  file is empty or not created.
- **m4** checklist: `coredumpctl info` privacy sentence.
- **m5** text entry: plain numbers with `.`, `+`, `-` (note and checklist); "yet" dropped from the checklist.
- **m6** note §8: twelve checks.
- **m7** note §7: `coredumpctl info` + `addr2line` is the primary route; a core in gdb needs an x86_64 host binary of
  the same version, `solib-search-path` to the unpacked archive, and leaves system-library frames unresolved.
- **m8** never click "Install Ubuntu"; ASUS Fast Boot off (F7, F10) if the stick is missing from the Esc menu.
- **Nits done:** n1 (apport's handler only), n2 (26.04.1 5.9/6.0 GB, 26.10 mentioned), n3 (library check matches the
  machine's architecture tag), n4 (`ldd … | grep 'not found'` for an installed standalone), n5 (AVX2: check, don't
  go by age), n6 (Deliverables paths → `25-…`), n8 (Alt-drag line names Gneiss's cutoff snapping).
- **Nit left:** n7 (`/var/crash` mask with `_` in a user name, `$HOME` containing `|`) — acceptable for testers, as
  the reviewer says; fixing it would cost lines in a script near its 80-line budget (now 77).
- **Script re-tests** (all `--rm`, `t035-*`, none left; outputs in scratchpad `t035/out2/`): plain `ubuntu:22.04`
  (dash) as a user with Gneiss aarch64 installed (build-ids "unknown (needs readelf or file)", the standalone's
  missing libraries listed by `ldd`), unwritable path ("Could not write", exit 1), HOME unset (38-line report);
  `iplug2-rt:ubuntu22.04-amd64` with Gneiss + Sand x86_64 installed (all six build-ids, libraries found with
  `libc6,x86-64`); `iplug2-linux:24.04` with Xvfb and `iplug2-rt:fedora` (libraries found with `libc6,AArch64`).

