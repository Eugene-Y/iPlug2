# Linux beta: tester checklist

Thank you for testing Gneiss / Sand on Linux. This is a beta: the plug-ins have been tested in REAPER, Ardour and
Carla, but on few computers and graphics cards so far. That is exactly what we need your help with. A full pass
takes 30–60 minutes; even the first three checks help.

## What the beta runs on
- 64-bit Linux on a PC (x86_64) or an ARM computer (aarch64), with glibc 2.35 or newer: Ubuntu 22.04, Debian 12,
  Fedora 36 and later, or a distribution of the same age.
- An X11 desktop session. Wayland desktops run the plug-in windows through XWayland, which they normally include.
- Graphics with OpenGL 2.1 or newer, from Mesa or your graphics card maker's driver.
- A host that loads CLAP or VST3 plug-ins. There is also a standalone application.

## Install
1. Unpack the archive (`tar -xzf <archive name>.tar.gz`) and open a terminal in the folder it creates.
2. Run `./install.sh`. It installs for your user only and prints where everything went:
   CLAP to `~/.clap`, VST3 to `~/.vst3`, the standalone application to `~/.local/lib/<name>` with a menu entry.
   Options: `--no-clap`, `--no-vst3`, `--no-app`. For all users: `sudo ./install.sh --system`.
3. Rescan plug-ins in your host if it does not find the new plug-in by itself.

`README-Linux.txt` in the archive has the details. `./install.sh --uninstall` removes what it installed; your
presets and settings stay.

## Before you start
Please note: your Linux distribution and version, your desktop (GNOME, KDE, …) and whether it runs X11 or Wayland
(`echo $XDG_SESSION_TYPE` in a terminal), your display scaling, your graphics card, and your host and its version.
If we sent you `collect-linux-report.sh`, run `sh collect-linux-report.sh` instead: it writes all of this into one
text file. It only reads information, changes nothing and sends nothing; read the file before you send it to us.

Tip: start your host from a terminal and keep that window open. If something goes wrong, the text it prints is
very useful to us.

## Checks
For each one: did it work? If not, what did you see? A screenshot or a short screen recording helps a lot.

1. **Find and open.** The host lists the plug-in (CLAP and/or VST3). Add it to a track that plays audio and open
   its window. *Expected:* the window opens at a sensible size, completely drawn (text, knobs, logo) within a
   second, and the sound is processed.
2. **Use it.** Move the mouse over the controls, drag knobs, use the mouse wheel, right-click a knob, open menus.
   *Expected:* smooth drawing, values follow, menus open where you click and close when you click elsewhere.
3. **Resize.** Drag the handle at the bottom-right corner of the plug-in window, then close and reopen the window.
   *Expected:* the whole interface scales and keeps its size when reopened.
4. **Type a value** (Sand): click a value under a knob, type a number, press Enter (or Escape to cancel).
   *Expected:* the new value is set. On Linux only digits work for now: units such as "kHz" and input methods for
   non-Latin scripts are not supported yet.
5. **Tooltips.** Hover a control (tooltips can be switched on and off in the plug-in). *Expected:* a short help
   text appears near the pointer.
6. **Presets.** Step through the presets; save one to a file and load it again (Gneiss). *Expected:* the sound and
   the controls change together; the file window opens and the saved preset comes back.
7. **Automation.** Record a knob movement in the host and play it back, once with the plug-in window open and once
   with it closed. *Expected:* the sound and the knob follow the automation.
8. **Save and reopen.** Save the project, quit the host, open the project again. *Expected:* every setting is back.
9. **Oversampling** (Gneiss). Change the oversampling setting while playing. *Expected:* the sound stays in time
   with the other tracks (the host compensates the plug-in's delay). If your host shows plug-in latency, tell us the
   values before and after.
10. **Two at once.** Open two copies of the plug-in side by side and use both. *Expected:* both work on their own.
11. **Closing.** Open a menu in the plug-in and, while it is open, close the plug-in window. Remove the plug-in
    while the project plays. *Expected:* no crash, no menu left on screen.
12. **Standalone application.** Start it from your application menu (or run `~/.local/lib/<name>/<name>` in a
    terminal). Choose your audio driver and devices in File → Preferences, then play some sound through it. Use
    headphones if you use a microphone, to avoid feedback.
    *Expected:* sound passes through and is processed; quitting closes it completely.

## Known limitations on Linux
- Some desktops (Xfce, MATE, Cinnamon, KDE Plasma 5) move windows when you Alt-drag. There, Alt-drag on a knob
  moves the plug-in window instead. Change the window manager's move key to Super (Meta in KDE) in its settings.
- Input methods for non-Latin scripts do not work in the plug-ins' text fields.
- Units cannot be typed into value fields yet (see check 4).

## If something goes wrong
Send us:
- the report file (or the facts listed under "Before you start");
- which plug-in, which format (CLAP, VST3 or standalone) and the name of the archive you installed;
- what you did, step by step, and what happened; screenshots or a recording;
- the text your host printed in the terminal.

**If the host crashed:** tell us the time it happened. On Fedora, and on other systems where the command exists,
`coredumpctl list` shows recent crashes; `coredumpctl info <number in the PID column>` prints a report we can read.
Please send that output too.
