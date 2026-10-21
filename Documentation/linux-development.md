# Developing iPlug2 plug-ins on Linux

Linux builds produce CLAP, VST3 and a standalone APP, with an IGraphics UI on X11 and NanoVG/OpenGL 2. This page
covers a native setup on Ubuntu 24.04 (x86_64 or arm64) with VS Code as the IDE. `Examples/IPlugEffect` and
`Examples/IPlugControls` are ready to open, and new projects duplicated from them inherit the setup.

Linux support in this fork is not upstream iPlug2's: it builds on Benjamin Reed's unmerged upstream pull request #1336 and
Stillwell Audio's Linux work, and was completed in this fork. Report Linux problems to this fork, not upstream.

The renderer is NanoVG GL2 by default (`IGRAPHICS_RENDERER=GL2`, as on Windows), which runs on any GL 2.1 driver.
`IGRAPHICS_RENDERER=GL3` asks for an OpenGL 3.2 core context; where the driver has none (old or virtual GPUs), the
editor does not open and the plug-in prints `no OpenGL 3.2 core context` on stderr.

## 1. Install

```sh
sudo apt install build-essential cmake ninja-build pkg-config git gdb clangd \
  libx11-dev libxi-dev libgl-dev libgtk-3-dev libfreetype-dev libfontconfig1-dev \
  libasound2-dev libjack-jackd2-dev libpulse-dev
```

- CMake 3.25 or newer is needed for the presets. Ubuntu 24.04 ships 3.28; on 22.04 use Kitware's apt repository
  or `pip install cmake`.
- Build with GCC (the default). clang does not define `__FLOAT_WORD_ORDER__`, which `WDL/wdlendian.h` requires.
- GTK is only linked into the standalone APP (SWELL). Plug-ins never load GTK into the host.

**VS Code**: install the `.deb` from https://code.visualstudio.com, then the extensions the workspace recommends:
CMake Tools (`ms-vscode.cmake-tools`), clangd (`llvm-vs-code-extensions.vscode-clangd`) and C/C++
(`ms-vscode.cpptools`, used only for its gdb debugger; its IntelliSense is switched off in favour of clangd).

**REAPER** (for "Debug in REAPER"): unpack the Linux tarball from https://www.reaper.fm and install it to
`/opt/REAPER` (its `install-reaper.sh` offers that). Installed elsewhere, symlink it there
(`sudo ln -s ~/opt/REAPER /opt/REAPER`), or change `iplug2.reaperPath` in the workspace file. Workspace settings
override user settings, so the setting only takes effect in that file.
In REAPER, *Preferences > Audio > Device*: pick **PulseAudio** on a PipeWire desktop (or JACK if you run it).
Without a working device REAPER never calls `ProcessBlock`.

**Plug-in SDKs**, once per checkout:

```sh
cd Dependencies/IPlug
./download-vst3-sdk.sh
./download-clap-sdks.sh
```

## 2. Open

```sh
code Examples/IPlugEffect/IPlugEffect-linux.code-workspace
```

On first open, pick a configure preset: **Linux Debug** (or Linux Release). VS Code remembers the choice for
that checkout, and configuring runs on its own from then on. clangd gets its compile database from
`build-linux/compile_commands.json`, which CMake Tools refreshes after each configure.

Projects built as C++23 that rely on its relaxed `constexpr` rules (P2448) need clangd 19 or newer; Ubuntu 24.04's
default is 18. Install `clangd-19` and set `"clangd.path": "clangd-19"` in your own VS Code user settings.

`<Name>.code-workspace` and `.vscode/c_cpp_properties.json` in the same folder belong to the macOS/Windows setup;
use the `-linux` workspace on Linux.

The example folders' `CMakePresets.json` holds only the Linux presets. On macOS/Windows, VS Code's CMake Tools
sees that file and switches to presets mode with an empty list; set `"cmake.useCMakePresets": "never"` there if
you build those examples with CMake Tools. Xcode and Visual Studio projects are unaffected.

## 3. Build and deploy

*Terminal > Run Build Task* (Ctrl+Shift+B) offers:

| Task | Result |
|---|---|
| Build APP | `build-linux/<preset>/out/<Name>` (+ `<Name>.resources/`) |
| Build and deploy CLAP | `out/<Name>.clap`, copied to `~/.clap/` with `<Name>.resources/` |
| Build and deploy VST3 | `out/<Name>.vst3`, copied to `~/.vst3/` |

From a shell, the same presets work without an IDE:

```sh
cd Examples/IPlugEffect
cmake --preset linux-debug
cmake --build --preset linux-debug --target IPlugEffect-clap
```

Leave out `--target` to build all three formats. Pass `-DIPLUG_DEPLOY_PLUGINS=OFF` at configure time to skip the
copy into `~/.clap` and `~/.vst3`.

## 4. Debug

*Run and Debug* (Ctrl+Shift+D), then pick a configuration:

| Configuration | What happens |
|---|---|
| Debug standalone APP | builds the APP and runs it under gdb |
| Debug CLAP in REAPER | builds and deploys the CLAP, starts a new REAPER instance (also when one is already open) under gdb with `Scripts/linux/reaper-open-fx.lua`, which puts the plug-in on a new track and opens its editor |
| Debug VST3 in REAPER | the same for VST3 |

Breakpoints in the plug-in can be set before launch. gdb binds them when REAPER loads the plug-in. For example,
`ProcessBlock` stops on one of REAPER's FX processing threads (`reaper/mediafx`) even with the transport
stopped, and a control's `OnMouseDown`
stops on the main thread.

If REAPER says it does not know the plug-in, the build was not deployed, or REAPER needs a rescan:
*Options > Preferences > Plug-ins > CLAP/VST > Re-scan*.

Attaching to an already running REAPER needs ptrace rights: Ubuntu's Yama setting (`ptrace_scope=1`) only allows
a debugger to trace its own children. Launching from the IDE, as above, needs nothing extra.

## 5. Other IDEs

Both read the same `CMakePresets.json`, so build settings need no extra files.

- **Qt Creator** (free, GPL; `sudo apt install qtcreator` or the qt.io installer): *File > Open File or Project*,
  pick the project's `CMakeLists.txt`, keep the "Linux Debug/Release (CMake preset)" kits. To debug in REAPER, add a
  *Custom Executable* run configuration once, in *Projects > Run*:
  - executable `/opt/REAPER/reaper`;
  - arguments `-newinst -new <iPlug2>/Scripts/linux/reaper-open-fx.lua` (`-newinst`: without it a REAPER that
    is already running takes the arguments and the new process exits);
  - environment `IPLUG2_REAPER_FX=CLAP: <Name>`.

  On machines with 8 GB of RAM it turns clangd off at first; accept "Enable Anyway".
- **CLion**: opens the presets natively; add the same custom run configuration. Its free licence does not cover
  developing products that are sold, so commercial work needs a paid licence.

## 6. New projects

Duplicate an example, then open the new project's `-linux` workspace:

```sh
cd Examples
./duplicate.py IPlugEffect MyPlugin MyCompany ~/dev   # 4th argument: parent folder, e.g. next to iPlug2
code ~/dev/MyPlugin/MyPlugin-linux.code-workspace
```

Without the 4th argument the project lands in `Examples/`. Out of the tree, `duplicate.py` points every iPlug2 path
at this checkout, as a relative path: the Xcode/VS configs, `CMakeLists.txt` (`IPLUG2_DIR`), the `CMakePresets.json`
include, the workspace's REAPER script and `.vscode/c_cpp_properties.json`. Moving the project or iPlug2 later
breaks them; `cmake -DIPLUG2_DIR=<path>` overrides the CMake one.

The Linux files come along renamed: `CMakePresets.json`, `<New>-linux.code-workspace` with
`<New>-clap`/`-vst3`/`-app` targets, and `.clangd` (outside the iPlug2 tree, iPlug2's own `.clangd` does not reach
the project). Builds go to `build-linux/`, which `duplicate.py` does not copy and the new project's `.gitignore`
ignores. Then continue with §2: pick the preset, build, debug.

## 7. Release packages

```sh
Scripts/linux/package.sh <project-dir> [--arch aarch64|x86_64] [--variant NAME:CMAKE_ARGS] [--doc FILE] [--icon FILE]
```

builds Release CLAP, VST3 and APP and writes to `<project>/build-linux/dist/`:

| File | For |
|---|---|
| `<Name>_v<version>_linux_<arch>[_<variant>].tar.gz` | customers: `CLAP/`, `VST3/`, `Standalone/` (each with its resources), `install.sh`, `README-Linux.txt`, the `--doc` files |
| `…_debug-symbols.tar.gz` | you: the stripped binaries' symbols, for crash reports (below) |
| `….inspect.txt`, `….files.txt` | the binary checks and the archive's file list |

- The version is `PLUG_VERSION_STR` from `config.h`. Each `--variant` is a separate build with its own CMake
  arguments, e.g. `--variant lite:-DMY_PLUGIN_LITE=1`.
- **glibc**: binaries need the build machine's glibc or newer. Build on Ubuntu 22.04 (glibc 2.35: Ubuntu 22.04,
  Debian 12, Fedora 36 and later). The script refuses binaries above `--glibc-max` (2.35), libstdc++ imports,
  extra exports, and plug-in dependencies beyond libX11 and libc. 22.04's GCC 11 lacks C++20 `<format>`; use
  `gcc-13` from the `ubuntu-toolchain-r/test` PPA (the script picks `gcc-13` when it is installed).
- **On macOS** (or for the other architecture) the script runs itself in Docker, by default
  `iplug2-linux:22.04-gcc13` and `…-amd64` for x86_64 (`linux-research/docker/README.md`; colima with Rosetta
  builds x86_64 on Apple silicon). Without `--arch` it builds both there, and the host's architecture on Linux.
- `install.sh` installs for the current user (`~/.clap`, `~/.vst3`, the APP in `~/.local/share/<Name>` with a menu
  entry), or for everyone with `sudo ./install.sh --system` (`/usr/lib/clap`, `/usr/lib/vst3`, `/opt/<Name>`);
  `--uninstall` removes either.
- A `.clap` needs its `<Name>.resources/` folder beside it; copied alone it loads, but without its fonts and
  images. The README tells customers.

Crash reports: unpack the matching `_debug-symbols` archive and point gdb at it; it matches the binary by build
id (`readelf -n <binary>` shows it; `….inspect.txt` lists them):

```sh
tar -xzf Name_v1.0.0_linux_x86_64_debug-symbols.tar.gz
gdb -iex "set debug-file-directory $PWD/Name_v1.0.0_linux_x86_64_debug-symbols" /opt/REAPER/reaper core
```
