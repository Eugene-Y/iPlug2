# Test VM: real GNOME desktop (UTM, Ubuntu 24.04 arm64)

The Docker harness (`../../docker/README.md`) runs on Xvfb + openbox + llvmpipe. These scripts run the same harness
scripts on a real desktop: GNOME Shell (Mutter 46) on **X11**, Xorg, Mesa **virgl** over Apple Metal (OpenGL 2.1).

| | |
|---|---|
| VM | UTM on the Mac, Ubuntu 24.04.5 Desktop arm64, 4 CPUs, 8 GB, screen 1440×863, autologin on `:0`, Wayland disabled |
| Access | `ssh -i ~/.ssh/claude_linux_vm gene@192.168.64.2`, passwordless sudo (override with `VM_HOST`, `VM_KEY`) |
| Builds | in Docker on the Mac (24.04 image: same glibc as the VM), e.g. `dock.sh -- scripts/build-cmake.sh wt/x build/x` |

## Scripts (host side, run from anywhere; bash)

| script | what |
|---|---|
| `vm-provision.sh` | idempotent: apt packages (gdb, strace, x11-utils, xdotool, xclip, xsel, wmctrl, mesa-utils, imagemagick, scrot, sysstat, jackd2, gcc + libx11/libxfixes headers for the probes); REAPER `$REAPER_VER` (782) for the VM's arch from reaper.fm into `~/opt/REAPER`, with `/opt/REAPER -> ~/opt/REAPER` so the Docker scripts run unchanged; `~/.config/REAPER/reaper.ini` seeded with the harness keys (`splashscreen=0`, `linux_audio_mode=2`, existing values kept); `~/.clap ~/.vst3 ~/iplug-apps`; mirrors the harness |
| `vm-deploy.sh [-p prefix] <builddir> [Name...]` | `<Name>.clap` + `<Name>.resources` → `~/.clap`, built `.vst3` bundles → `~/.vst3`, APP binaries + resources → `~/iplug-apps`. `-p iplug-builds/x` stages a variant (e.g. a trace build) without replacing what REAPER scans |
| `vm-x.sh <cmd...>` | run a command on the VM in the GNOME session (DISPLAY, XAUTHORITY, session bus), cwd = the mirrored `linux-research/`, the sudo-gdb wrapper first on PATH |
| `vm-shot.sh <out.png> [window]` | screenshot of the VM's screen into a PNG on the Mac |
| `vm-run.sh <script> <out> [args...]` | run `scripts/<script> <out> args` on the VM, copy `<out>` back to the same path here (replacing it), write `<out>/run.log`. Closes the GNOME overview first; stops REAPER and jackd afterwards unless `VM_KEEP=1`. Args starting with `~/` are VM paths |
| `vm-reaper-test.sh <out> "<FMT: Name>" <plugin> [steps]` | `vm-run.sh reaper-plugin-test.sh ...` |
| `probe/*` | VM-only probes, run through `vm-run.sh vm/probe/<x>.sh <out> ...` unless noted. P3a: `cpu-fps.sh` (pidstat + Draw/s idle and while dragging), `wm-behaviour.sh` (focus, stacking, moves, minimize/restore, Draws while minimized), `swap-interval.sh` (interval in effect while drawing), `nvg-balance.sh` (NanoVG texture create/delete per open/close), `rss-open.sh`, `app-test.sh` (standalone APP with PipeWire audio, optional steps, X focus trail), `glx-churn.c` (GL-only RSS churn, no iPlug). P3b: `focus-edit.sh` (N edits + Escape from an unfocused FX window, focus trail, trace log), `focus-ts.sh` + `focus-ts-probe.c` (focus request timing vs. Mutter + a GDK-like proxy, no iPlug), `app-focus.sh` (APP: does Escape after a click reach the editor or SWELL), `gnome-clip.sh` (clipboard with zenity and gnome-text-editor; removes the editor's draft state it created), `clip-owner.c` (who owns CLIPBOARD), `menu-edges.sh` (menus with the editor dragged partly off screen), `ctx-menu.sh`/`ctx-close.sh` (REAPER's VST3 context menu: shows? blocks? host actions meanwhile), `fc-probe.cpp` (fonts by name; built in Docker, see notes/14) |

On the VM the harness lives in `~/iplug-harness/linux-research/` (`scripts/` is re-synced on every call; evidence is
written under it and pulled back). `scripts/vm/remote-bin/` holds the VM-side helpers: `vm-display-env` (sourced) and
a `gdb` wrapper.

```sh
R=linux-research
$R/scripts/vm/vm-provision.sh
$R/scripts/dock.sh -- scripts/build-cmake.sh wt/vm-p3a build/vm-p3a
$R/scripts/vm/vm-deploy.sh $R/build/vm-p3a
$R/scripts/vm/vm-reaper-test.sh evidence/vm-p3a/reaper-controls-clap "CLAP: IPlugControls" "~/.clap/IPlugControls.clap" scripts/steps/iplugcontrols-p3a.txt
$R/scripts/vm/vm-run.sh editor-churn-test.sh evidence/vm-p3a/churn-controls-vst3 "VST3: IPlugControls" "~/.vst3/IPlugControls.vst3" 50
$R/scripts/vm/vm-run.sh host-loop-test.sh evidence/vm-p3a/host-loop-trace-controls-clap "CLAP: IPlugControls" "~/iplug-builds/vm-p3a-trace/.clap/IPlugControls.clap"
$R/scripts/vm/vm-run.sh vm/probe/app-test.sh evidence/vm-p3a/app-controls "~/iplug-apps/IPlugControls" scripts/steps/iplugcontrols-p3a.txt
$R/scripts/vm/vm-shot.sh /tmp/vm.png
```

The REAPER tests use the harness's throwaway profile in `/tmp/rhome` (as in Docker), not `~/.config/REAPER`, so runs
are reproducible and the seeded profile stays for manual use (`~/opt/REAPER/reaper`).

## Gotchas on a real desktop

- **Never `xdotool windowclose` a window on GNOME/X11.** It is `XDestroyWindow` on another client's window; it made
  `mutter-x11-frames` die with an X error, and from then on every new window stayed `Iconic` (unmapped, invisible),
  REAPER's included. `reaper-up.sh` now closes the evaluation nag with `wmctrl -c` (a polite `_NET_CLOSE_WINDOW`) and
  retries until its countdown allows it. To recover a broken session: Alt+F2, `r`, Enter restarts GNOME Shell (X11 only)
  and respawns `mutter-x11-frames`.
- **The Activities overview hides every window** (hot corner, Super). Something in an earlier run opened it, and the
  next run measured thumbnails. `vm-run.sh` closes it first through `org.gnome.Shell.OverviewActive`.
- **Mutter's frame windows carry the client's title.** `xdotool search --name` returns the frame as well as REAPER's
  window; filter by `WM_CLASS` (`"REAPER"`) when the id matters. `reaper-plugin-test.sh`'s `tail -1` happens to pick the client.
- **`xwininfo -id ""` waits for a click on the screen.** A probe whose window lookup failed would sit there until
  somebody clicks the VM; the probes guard every lookup.
- **ptrace.** Ubuntu's Yama `ptrace_scope=1` forbids attaching to REAPER as the same user. Instead of changing it the
  harness runs gdb through `sudo -n` (`remote-bin/gdb`). `pkill` of a root gdb fails ("Operation not permitted"); signal
  its `sudo` parent (`pkill -f '^sudo -n /usr/bin/gdb ...'`), which forwards the signal.
- **Screen height.** 863 px minus the top bar: a 1024×768 editor plus REAPER's FX header and the title bar does not
  quite fit. Mutter constrains the FX window, so REAPER's container is 1024×766 and the editor's bottom 2 px are clipped.
- `import -window root` shows the composited screen (top bar and dock included) but no mouse cursor.
- `linux_audio_mode=2` is REAPER's dummy audio; the harness's `jackd` dummy is started but unused. The APP opens ALSA
  `default`, which is PipeWire here.
- `vm-env.sh` must be sourced from bash (it refuses otherwise): under zsh `BASH_SOURCE` is empty and the paths go wrong.
- **`xdotool click` presses and releases in the same millisecond.** That ties X focus-request timestamps (a WM's
  WM_TAKE_FOCUS then wins or loses by arrival order) and makes GTK menus close on the release. Probes that model a
  person hold the button: `mousedown 1; sleep 0.08; mouseup 1`.
- **`xclip` (xact's `set-clipboard`) stays behind as the selection owner with the caller's stdout open.** A pipe or an
  ssh session then waits until something else takes the clipboard. `vm-x.sh` ends the session on a status line
  (`__VMX_RC=`) printed after the command, so leftover daemons (xclip, jackd, REAPER) no longer hold it; scripts on
  the VM should write xact's output to a file, not into a pipe.
- `scripts/probe/focuswatch.c` lacks `<stdlib.h>` for `atof`: build it with `gcc -include stdlib.h`, or its run time
  is garbage.
- gnome-text-editor asks to save even an emptied draft and keeps drafts in `~/.local/share/org.gnome.TextEditor`;
  `gnome-clip.sh` cancels, quits it and removes that state if the run created it.
- macOS `rsync` is openrsync, which ignores `--delete` for these pulls; `vm_pull` removes the local `<out>` first.
