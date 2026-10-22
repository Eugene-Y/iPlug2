# Developing iPlug2 plug-ins on Linux

Linux builds produce CLAP, VST3 and a standalone APP, with an IGraphics UI on X11, drawn with NanoVG/OpenGL 2 by
default or with Skia. This page
covers a native setup on Ubuntu 24.04 (x86_64 or arm64) with VS Code as the IDE. `Examples/IPlugEffect` and
`Examples/IPlugControls` are ready to open, and new projects duplicated from them inherit the setup.

Linux support in this fork is not upstream iPlug2's: it builds on Benjamin Reed's unmerged upstream pull request #1336 and
Stillwell Audio's Linux work, and was completed in this fork. Report Linux problems to this fork, not upstream.

The renderer is NanoVG GL2 by default (`IGRAPHICS_RENDERER=GL2`, as on Windows), which runs on any GL 2.1 driver.
`IGRAPHICS_RENDERER=GL3` asks for an OpenGL 3.2 core context; where the driver has none (old or virtual GPUs, e.g. a
VM's virgl driver), the editor does not open and the plug-in prints `no OpenGL 3.2 core context` on stderr.

Skia (`-DIGRAPHICS_BACKEND=SKIA`) renders with `GL3`, its default on Linux, or `CPU`; as on macOS, it has no GL2
renderer. The presets set `IGRAPHICS_RENDERER=GL2`, so pass `-DIGRAPHICS_RENDERER=GL3` or `CPU` with them. `CPU`
needs no GL driver: frames are drawn in memory and copied into the window, which suits drivers without GL 3.2.
iPlug2 has no prebuilt Skia for Linux; build it once per architecture (needs `clang`, `ninja-build`, `python3`, `git`):

```sh
cd Dependencies/IGraphics
./build-skia-linux.sh
```

It clones Skia's `chrome/m130` branch, the one the macOS and Windows libraries come from, into
`Dependencies/Build/src/skia`, and writes the libraries to `Dependencies/Build/linux/<arch>/lib`. That takes a few
minutes natively and about 20 under Rosetta. Like the plug-ins (§7), build it on the oldest distribution they must run
on. Skia adds no runtime dependency: it gets GL functions and fonts from iPlug2, which loads libGL and fontconfig at
run time. Skia text uses only the fonts the plug-in loads, as NanoVG does: no fallback to other system fonts for
missing glyphs, also in `DrawMultiLineText`.

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
mkdir -p ~/dev
cd Examples
./duplicate.py IPlugEffect MyPlugin MyCompany ~/dev   # 4th argument: an existing parent folder, e.g. next to iPlug2
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

## 8. Known host issues

Host behaviour seen while testing on Linux; iPlug2 does not work around it.
- **Carla, development version (main, tested at 97a9e07), VST3:** runs the plug frame's `IRunLoop` handlers (timers and
  the X connection's fd) on its engine thread instead of the UI thread, so the editor's `OnIdle` and drawing run off
  the main thread and can crash while the editor opens. CLAP in Carla main and VST3 in Carla 2.5.10 were fine.
- **Renoise 3.5.4:** hosts VST3 (no CLAP on Linux). While an editor has the keyboard focus, Renoise delivers keys to it
  as synthetic events. iPlug2 handles them (text entry works) but does not pass unhandled ones back (that loops in
  REAPER), and Renoise ignores keys passed to the parent window, so host shortcuts (Space) don't work until the focus
  leaves the editor.
- **Qtractor (1.5.3, and the same code in 1.6.4), CLAP:** one shared timer drives every CLAP plug-in's timers, and
  closing the last CLAP editor stops it. From then on no plug-in gets timer callbacks, so `OnIdle` (and anything a
  plug-in reports from it, such as a latency change) waits until a CLAP editor opens or a plug-in registers a new
  timer.
- **Qtractor 1.5.3, VST3:** gives `IRunLoop` only through the plug frame, so with the editor closed `OnIdle` runs on
  iPlug2's own thread. A latency change made then reaches the host's `restartComponent` from that thread, which VST3
  does not allow (an iPlug2 limitation); Qtractor answers it with a state round trip (`getState`/`setState`) on that
  thread. It reads the latency when playback starts. Qtractor 1.6.4's source also offers `IRunLoop` on the host
  context, but drives it from the same kind of shared timer, stopped when the last VST3 editor closes, so with every
  VST3 editor closed `OnIdle` would not run at all (read from the source; untested).
- **Files dropped from a GTK file manager (GNOME Files):** it drops on the top-level window under the pointer, so a file
  reaches the editor only if the host forwards XDND to its plug-in window: Ardour and REAPER do; Carla 2.5.10 takes the
  drop itself; Qtractor's plug-in window is drop-aware and refuses it; Carla main (CLAP) and Renoise don't. A source
  that targets the deepest drop-aware window, as Qt's do (tested with a scripted source), reached the editor in every
  host tried. Dragging files *out* of the editor works in every host where it was tried (REAPER, Ardour, Renoise, and
  GNOME Files as the target).

## 9. Plug-in code: differences from macOS and Windows

- **Dialogs are asynchronous only.** `PromptForFile`, `PromptForDirectory`, `PromptForColor` and `ShowMessageBox`
  return at once on Linux, as on iOS, and the answer arrives later in the completion handler, on the host's main
  thread. A plug-in cannot block there: a nested event loop would pump only the plug-in's own window and freeze the
  host's. Without a handler the file, folder and colour prompts show nothing, return an empty name or `false` and print
  one warning on stderr; `ShowMessageBox` shows the message and returns `kNoResult`. Pass a handler everywhere and do
  the work that needs the answer inside it:

  ```cpp
  WDL_String fileName, path;
  GetUI()->PromptForFile(fileName, path, EFileAction::Open, "fxp",
    [this](const WDL_String& chosenFile, const WDL_String& chosenFolder) {
      if (chosenFile.GetLength())
        LoadPresetFromFXP(chosenFile.Get());
    });
  ```

  The same code works on macOS and Windows: Windows still shows a modal dialog and calls the handler before
  `PromptForFile` returns; macOS shows the file and folder panels as sheets on the plug-in's window. A Linux dialog closes
  without calling its handler when the editor closes; on macOS a sheet can outlive the editor, so a handler that
  captures a control should check that the control still exists, e.g. through a `std::weak_ptr` to a token the control
  owns. iPlug2's own controls use the handler form (`IVDiskPresetManagerControl` does both).
