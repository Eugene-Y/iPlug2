# Developing iPlug2 plug-ins on Linux

Linux builds produce CLAP, VST3 and a standalone APP, with an IGraphics UI on X11 and NanoVG/OpenGL 2. This page
covers a native setup on Ubuntu 24.04 (x86_64 or arm64) with VS Code as the IDE. `Examples/IPlugEffect` and
`Examples/IPlugControls` are ready to open, and new projects duplicated from them inherit the setup.

The renderer is NanoVG GL2 (`IGRAPHICS_RENDERER=GL2`, as on Windows), the only one tested on Linux. The editor
creates a legacy GLX context, which old or virtual drivers may cap below 3.2, so `GL3` is not guaranteed to open.

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

Projects built as C++23 that use hvoya's `constexpr` lookup tables (Sand) need clangd 19 or newer; Ubuntu 24.04's
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

`Examples/duplicate.py` copies an example under a new name. The Linux files come along renamed: `CMakePresets.json`,
`<New>-linux.code-workspace` with `<New>-clap`/`-vst3`/`-app` targets, and `.clangd`. Builds live in
`build-linux/`, which `duplicate.py` does not copy. Duplicate inside `Examples/` for now. Duplicating to a path
outside the iPlug2 tree leaves `../..` paths that need fixing (T-011).
