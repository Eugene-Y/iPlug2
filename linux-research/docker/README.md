# iPlug2 Linux build & test harness (Docker)

Runs on the Mac through colima. Containers run natively as linux/arm64; add `-p linux/amd64` to
`dock.sh` to run under Rosetta. The repo is mounted at its host path, so worktrees, build dirs and
evidence look the same inside and outside the container.

## Images

```sh
docker build -t iplug2-linux:24.04 linux-research/docker                            # main dev/test image
docker build --build-arg UBUNTU=22.04 -t iplug2-linux:22.04 linux-research/docker   # old-glibc build check
docker build -t iplug2-linux:22.04-gcc13 linux-research/docker/gcc13                # 22.04 + GCC 13 (C++20 <format>, C++23)
```

Sand needs `<format>`, which 22.04's GCC 11 lacks: build it in `22.04-gcc13` with `DOCK_ENV="CC=gcc-13 CXX=g++-13"`.
iPlug2 links libstdc++ statically, so the floor stays 22.04's glibc 2.35 and no GLIBCXX is needed.

The 24.04 image has:
- Toolchain: gcc 13, clang 18, cmake 3.28, ninja.
- X11, GL, GTK3, ALSA, JACK and Pulse dev packages, plus the xcb packages the bindernews branch needs.
- Test display: Xvfb, openbox, xdotool, ImageMagick, dbus, zenity; xclip and xsel (P3b clipboard checks, own last layer).
- Debugging: gdb and strace.
- Audio: jackd2, so a dummy audio backend is available.
- Validators: `clap-validator` 0.4.1 (built with cargo) and the VST3 SDK v3.8.1 `validator` and `editorhost`.
- Host: REAPER 7.82 for the native arch, in `/opt/REAPER`.

Mesa uses software rendering (llvmpipe) because the image sets `LIBGL_ALWAYS_SOFTWARE=1`.

The 22.04 image is for building only. Its validators were built on 24.04, because vst3sdk 3.8 needs
CMake 3.25 or newer, so they need glibc 2.38 and will not run in the 22.04 image. Run tests in the
24.04 image.

### Release builds and clean-machine tests (T-021)

```sh
# x86_64 build images, run under Rosetta (tags must differ: one tag holds one architecture)
docker build --platform linux/amd64 --build-arg UBUNTU=22.04 -t iplug2-linux:22.04-amd64 linux-research/docker
docker build --platform linux/amd64 --build-arg BASE=iplug2-linux:22.04-amd64 -t iplug2-linux:22.04-gcc13-amd64 linux-research/docker/gcc13
# customer machines: runtime packages only (the build fails if a compiler or a -dev package slips in)
docker build --build-arg BASE=ubuntu:22.04 -t iplug2-rt:ubuntu22.04 linux-research/docker/runtime   # also ubuntu:24.04, debian:12, fedora:latest
docker build --platform linux/amd64 --build-arg BASE=ubuntu:22.04 -t iplug2-rt:ubuntu22.04-amd64 linux-research/docker/runtime
```

`Scripts/linux/package.sh` uses `iplug2-linux:22.04-gcc13` and `…-amd64`. `scripts/package-smoke.sh <archive> <outdir>
<image>` installs an archive in an `iplug2-rt` image and runs the validators, REAPER and the standalone; its
validators are built on 22.04, so they run on every image.

Under Rosetta (x86_64 on Apple silicon): a 22.04 build image takes ~6 min and Gneiss ~5 min per edition.
gdb attaches but its breakpoints never fire, so the smoke test skips the Draw-thread count there
(`package-smoke.sh` sets it up). Fedora's GNU tar fails under Rosetta ("Function not implemented"), so there is
no x86_64 Fedora image; Fedora is tested on aarch64.

## SDKs

```sh
linux-research/scripts/fetch-sdks.sh               # once: cache/sdks (VST3 v3.8.1, CLAP main, clap-helpers main)
linux-research/scripts/install-sdks.sh <worktree>  # copy them into <worktree>/Dependencies/IPlug
```

CLAP is pinned to `main`, not a release tag, because clap-helpers `main` already uses the
draft `param-hovered` extension. Every candidate's download script uses `main`.

## One-liners (run from the repo root on the host)

```sh
R=linux-research
# build an upstream-style CMake tree (PR #1336, linux-bindernews, a hvoya-based branch, ...)
$R/scripts/dock.sh -- scripts/build-cmake.sh wt/rangerrick build/rangerrick IPlugControls-vst3 IPlugControls-clap IPlugControls-app
#   aarch64 trees without -fsigned-char: DOCK_ENV="EXTRA_CMAKE_ARGS=-DCMAKE_CXX_FLAGS=-fsigned-char" $R/scripts/dock.sh -- ...
#   old glibc:                           IMAGE=iplug2-linux:22.04 $R/scripts/dock.sh -- ...
# stillwell (no Linux plugin build in the fork): Skia + out-of-tree CMake driver
$R/scripts/dock.sh -- scripts/build-skia-linux.sh            # ~25 min, cache/skia/<arch>
$R/scripts/dock.sh -- bash -c 'cmake -S cmake/stillwell -B build/stillwell -G Ninja -DIPLUG2_DIR=$PWD/wt/stillwell -DSKIA_DIR=$PWD/cache/skia/aarch64 && cmake --build build/stillwell'
# static checks + validators
$R/scripts/dock.sh -- scripts/inspect-binary.sh build/rangerrick/out/IPlugEffect.vst3 build/rangerrick/out/IPlugEffect.clap
$R/scripts/dock.sh -- scripts/validate.sh evidence/x/validators build/rangerrick/out/IPlugEffect.clap build/rangerrick/out/IPlugEffect.vst3
# GUI: one shot (starts the "gui" container if needed)
$R/scripts/run-gui-test.sh build/rangerrick/out/IPlugControls.vst3 evidence/x/reaper-vst3     # REAPER host
$R/scripts/run-gui-test.sh build/rangerrick/out/IPlugControls       evidence/x/app            # standalone
docker rm -f gui                                                                               # tear down
```

`dock.sh` and `gx.sh` use `linux-research/` as the working directory, so paths are relative to it.
`dock.sh` (and so `gui-up.sh`) mounts the tree its `linux-research/` belongs to, at the same path
(e.g. `/Volumes/Storage/code/IPlug2-linux`). When that tree is a linked worktree it also mounts the
git common dir, so `.git` links resolve. Nothing else is mounted: `/private/tmp` (the session scratchpad)
and `$HOME` are **not** visible in the containers, so anything a container must read
(gdb scripts, step files, patches) has to live under the repo, e.g. `linux-research/scripts/`.

### Building a hvoya-based tree

On `hvoya-linux` from 29c6bea6f on, nothing is needed: IPlugEffect has a working CMakeLists (hvoya
utils, generated `plug_build_info.hpp`) and all three examples build. The patch below does not apply there.

On plain `hvoya` and on trees from before 29c6bea6f (e.g. `linux-trial/rangerrick`),
`Examples/IPlugEffect/CMakeLists.txt` lists sources that no longer exist (`IPlugEffect.h`).
Configuring the top-level tree fails on that, even for IPlugControls targets. Apply the patch before the first configure:

```sh
git -C <worktree> apply $PWD/linux-research/evidence/hvoya-trial/patches/01-iplugeffect-cmake-sources.patch
```

The patch only touches that one file, so it still applies after `install-sdks.sh`.
IPlugEffect itself still won't build on those trees: they lack the CMake equivalents of the Xcode-generated
`plug_build_info.hpp` and of the `hvoya/utils` sources. Build IPlugControls / IPlugInstrument targets.

### REAPER test semantics (`reaper-plugin-test.sh`)

The test exits 1 and writes `RESULT: FAIL (n)` plus reasons to `<out>/RESULT.txt` when:
- the editor area is near-uniform (blank) on first open, after reopen, or for the 2nd instance;
- `c00` and `c02` are byte-identical, meaning nothing repainted after drag/click;
- the REAPER PID from `reaper-up.sh` dies, or a `reaper -nonewinst` helper turns into a fresh instance.
  After a death the later phases are skipped, so no screenshot of an empty fresh REAPER can pass as a result;
- `rpr.sh` returns a `LUA ERROR` or no output;
- gdb sees no `Draw`/`OnDisplayTimer` hits in 5 s.
- for builds with PR #1336's `Timer_impl` (detected with `nm`): fewer `Timer_impl::ThreadProc` threads than instances (2), which means an instance lost the start race and has no `OnIdle`.

The crash catcher (`gdb-catch.sh`) attaches **after** the first open by default. Attaching earlier
perturbs startup timing: it made PR #1336's timer-start race (thread checks `mRunning` before
the creator sets it) lose almost every time, which left a never-painted editor. Set `GDB_CATCH_EARLY=1`
to catch crashes that happen during open.

`scripts/rerun-reaper-matrix.sh [regex]` re-runs every REAPER test cited in `notes/03`.

**One GUI container per person/agent.** Inside a container the scripts use fixed paths
(`/tmp/rhome`, `/tmp/reaper.pid`) and one X display (:99). Two people sharing a container will
measure each other's REAPER. Pass `GUI=gui-<name>` to `run-gui-test.sh` and `rerun-reaper-matrix.sh`
(or the 4th argument of `run-gui-test.sh`).

The steps file `steps/iplugcontrols.txt` is tied to IPlugControls' 1024x768 layout. For other
plugins, pass your own steps file, or `/dev/null` to only open, reopen and add a second instance.

All containers run with `--ulimit core=0`, so a crashing REAPER no longer leaves a ~580 MB `core`
in `linux-research/`. `core` is also git-ignored.

## Scripts (`linux-research/scripts/`)

| script | where | what |
|---|---|---|
| `dock.sh` | host | `docker run --rm --init` with the repo mounted, ptrace allowed (gdb) |
| `gui-up.sh [name]` | host | detached container: Xvfb :99 1600x1000 + openbox + dbus |
| `gx.sh [-d] <name> cmd` | host | `docker exec` into the GUI container with DISPLAY/dbus set |
| `build-cmake.sh <wt> <build> [targets]` | container | configure + build, one `=== RESULT <target> PASS/FAIL` per target |
| `build-skia-linux.sh [branch]` | container | Skia chrome/m130 static, Ganesh GL/GLX + fontconfig |
| `inspect-binary.sh <bundle/so>...` | container | layout, exported symbols, NEEDED, ldd, GLIBC/GLIBCXX floor |
| `validate.sh <out> <plugins>...` | container | clap-validator + VST3 validator |
| `reaper-up.sh <plugins>...` | GUI container | fresh REAPER profile in /tmp/rhome, plugins in ~/.clap ~/.vst3 (+resources), JACK dummy |
| `rpr.sh '<lua>'` | GUI container | run ReaScript in the running REAPER (`reaper -nonewinst`), `out()` prints |
| `reaper-plugin-test.sh <out> "<FMT: Name>" <plugin> [steps]` | GUI container | full editor test in REAPER, crash catch via gdb |
| `xact.sh` | GUI container | xdotool step DSL (click/dclick/drag/key/type/shot/alive/windows) |
| `steps/iplugcontrols.txt` | – | IPlugControls interaction script (knob, XY pad, keyboard, popups, text entry, OS menu/text-entry toggles, colour picker, font dialog, about box) |
| `threads.sh <proc>` | GUI container | thread names + all-thread backtraces |
| `gdb-count.sh <proc> <secs> <sym>...` | GUI container | count hits per gdb thread id (`tid=1` = main thread) |
| `gdb-catch.sh <pid> <out>` | GUI container | attach and dump backtraces on crash |
| `gdb-hits.sh <pid> <secs> <sym>...` | GUI container | every hit of member functions with the caller's LWP (main = pid) and `this` |
| `host-loop-test.sh <out> "<FMT: Name>" <plugin> [Class]` | GUI container | P2: `OnTimer`/`OnIdle` thread per phase (never opened, open, closed, 2 instances, 50× churn), latency via REAPER `pdc`, thread/fd/registration leaks. `EXPECT_CLOSED=worker` for frame-only test builds |
| `app-host-loop-test.sh <out> <app>` | GUI container | P2: the APP's `OnTimer`/`OnIdle` on its main thread (`--no-io`) |
| `trial/resolve.py` | host | resolve conflict blocks ours/theirs/both (hvoya trial) |
| `run-gui-test.sh <plugin-or-app> <out>` | host | one-shot REAPER or standalone test (starts the GUI container) |
| `rerun-reaper-matrix.sh [regex]` | host | re-run all REAPER tests behind notes/03, prints one RESULT per row |

## Gotchas found while building this harness

- The APP needs `--no-io` in the container. With no ALSA device, PR #1336's APP segfaults in `IPlugAPPHost::GetAudioDeviceID`.
- In the standalone APP, Escape goes to the SWELL dialog and closes the app (IDCANCEL). The step script avoids Escape for that reason.
- REAPER: if the main instance dies, the next `reaper -nonewinst` quietly becomes a new instance. `reaper-plugin-test.sh` therefore tracks the PID from `/tmp/reaper.pid`.
- Flat `.clap` files need their resources installed next to them: `resources/` for PR #1336, `<Name>.resources/` for stillwell. Without them the editor is blank (font fails to load) or the host crashes (null SVG in `ISVGSliderControl::OnResize`).
