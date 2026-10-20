# T-035 — Real-hardware test kit: live USB on the user's laptop, tester checklist

- **Status:** ready
- **Owner:** —
- **Branch/worktree:** `linux/T-035` from `hvoya-linux`, in `/Volumes/Storage/code/IPlug2-T-035`.
- **Depends on:** packages from `makedist-linux.sh` (Gneiss, Sand; T-034/T-023 merged).
- **Conflicts:** none (documents and, if useful, a small collection script). No VM, no Docker GUI needed.

## Why
Gneiss and Sand on Linux have only run on llvmpipe (Docker) and virgl (the UTM VM). No real GPU driver (Intel, AMD,
NVIDIA), no Bitwig (x86_64 only), no x86_64 hardware at all. T-034 showed GL behaviour depends on the driver. The user
has no Linux machine; options: an old ASUS laptop booted from a **live USB** (no install), and **asking customers** to
test a beta.

## Deliverables
1. `linux-research/notes/23-real-hardware-testing.md` (internal, English):
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
   `linux-research/notes/23-tester-checklist.md` — the user will adapt it for the website/e-mail. It must not promise
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
- [ ] Both notes written, every package step verified against a real archive.
- [ ] Optional script tested in Docker (22.04 and 24.04 images) or explicitly dropped.
- [ ] Independent review (AGENTS §4).

## Log
