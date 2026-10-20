# Real-hardware testing: a live USB on an x86_64 laptop, and what a tester sends back (T-035)

Internal note. The customer-facing version is `25-tester-checklist.md`; the report script is
`scripts/collect-linux-report.sh`. (Numbered 25 because 23 and 24 were taken when this was written.)

**Why:** Gneiss and Sand have only run on llvmpipe (Docker) and virgl (the UTM VM), both arm64 except Rosetta
Docker runs. Real x86_64 hardware with a real GPU driver (Intel, AMD, NVIDIA/nouveau) and Bitwig (x86_64 only) are
untested, and T-034 showed GL behaviour depends on the driver. The user has a Mac and an old ASUS laptop (x86_64,
model unknown), no Linux machine.

Package facts below come from `Scripts/linux/package.sh` and from real archives (Gneiss 1.3.1 paid x86_64 and
Sand 1.1.10 x86_64 test builds of 2026-10-20) unpacked and installed with their `install.sh` in Docker
(`iplug2-rt:ubuntu22.04-amd64`, a non-root user, then `--system` as root, then `--uninstall`). Third-party facts were
checked on the vendors' pages on 2026-10-20 (sources at the end). Anything marked *unverified* was not seen working
here; check it on the first real run and fix this note.

## 1. Which Ubuntu image

| Image (amd64 desktop ISO) | Size | Use it for |
|---|---|---|
| **24.04.5.1 LTS** `ubuntu-24.04.5.1-desktop-amd64.iso` (releases.ubuntu.com/noble) | 5.8 GB | **The main pass.** GNOME offers both an Xorg and a Wayland session; Bitwig 6 requires Ubuntu 24.04 or later. |
| 26.04.1 LTS `ubuntu-26.04.1-desktop-amd64.iso` (the current LTS download on ubuntu.com; 26.10 is offered beside it, also without Xorg) | 5.9 GB on ubuntu.com, 6.0G on releases.ubuntu.com | GNOME has **no Xorg session** any more: everything runs through XWayland. Newest Mesa. A second pass if time allows. |
| 22.04.5 LTS `ubuntu-22.04.5-desktop-amd64.iso` (releases.ubuntu.com/jammy) | 4.4 GB | The glibc 2.35 floor the packages are built for (the README promises "Ubuntu 22.04 … and later"). REAPER only: Bitwig 6 does not support 22.04. Docker already loads the packages on 22.04, so this pass is about the 22.04 Mesa/X stack, lowest priority. |

One stick of **8 GB or more** per image (the official tutorial asks for 8 GB). Check the download on the Mac:
`shasum -a 256 ubuntu-24.04.5.1-desktop-amd64.iso` against the line in `SHA256SUMS` from the same folder.

## 2. Writing the stick on macOS

**balenaEtcher** (what Ubuntu's tutorial uses on macOS): "Flash from file" → the ISO → "Select target" → the stick
→ "Flash!", admin password, it verifies afterwards. Ubuntu's docs note that Etcher shares some usage data with
Balena. If macOS then says the disk is not readable, choose **Eject**, not Initialise.

**dd** (no extra app). **Everything on the stick is erased**, and a wrong disk number erases that disk instead
(a Time Machine drive or a second external disk is also "external physical"). So the number is set once, by hand,
after looking twice; while `N` is empty, `${N:?…}` stops every command below before it runs:

No `#` comments on command lines: the Mac's zsh does not treat `#` as a comment when typing or pasting.

1. Before inserting the stick, note what is listed (maybe nothing):
   ```sh
   diskutil list external physical
   ```
2. Insert the stick and run it again; the one new entry is the stick:
   ```sh
   diskutil list external physical
   ```
3. Type its number yourself (e.g. `N=4` for `/dev/disk4`), then check it:
   ```sh
   N=
   diskutil info /dev/disk${N:?set N first} | grep -E 'Device / Media Name|Disk Size|Removable Media|Protocol'
   ```

**Stop** unless that shows the stick's brand, its size, and `Removable Media` / USB. Then:

```sh
diskutil unmountDisk /dev/disk${N:?set N first}
sudo dd if=ubuntu-24.04.5.1-desktop-amd64.iso of=/dev/rdisk${N:?set N first} bs=4m status=progress
sync
diskutil eject /dev/disk${N:?set N first}
```

- `rdisk` is the raw device node: the same disk, written much faster.
- `bs=4m` and `status=progress` are accepted by the macOS 26 `dd` (checked on the user's Mac); if an older `dd`
  rejects `status=progress`, drop it and press Ctrl+T for progress.
- Never use `/dev/disk0` or `/dev/disk1`–`3` on an Apple Silicon Mac: they are internal or synthesized APFS disks.
  `external physical` hides them, so if the stick does not show up there, stop and look again rather than guess.

## 3. Booting the ASUS laptop

- **Boot menu:** power off fully, hold **Esc**, press the power button, release Esc when the boot menu appears, pick
  the USB stick (ASUS FAQ 1013017). Ubuntu's tutorial also lists F12, F2 and F10 as common keys on other machines.
- **BIOS setup:** power off, hold **F2**, press power, release F2 when the setup screen shows (ASUS FAQ 1008829). If
  Windows is installed and boots too fast (Fast Boot): Windows → Change advanced startup options → Restart now →
  Troubleshoot → Advanced options → UEFI Firmware Settings.
- **Before changing any BIOS setting: BitLocker.** If the laptop's Windows uses BitLocker or Device Encryption
  (the default on many Windows 10/11 laptops with a TPM), changing a firmware security setting such as Secure Boot
  makes Windows ask for the **BitLocker recovery key** at its next start; without the key, Windows and its data are
  locked. In Windows, check Settings → Privacy & security → Device encryption (or Control Panel → BitLocker Drive
  Encryption). If it is on, save the recovery key first ("Back up your recovery key", or find it at
  https://aka.ms/myrecoverykey). Booting the stick from the Esc menu changes nothing and does not trigger this.
- **Fast Boot:** if the stick does not appear in the Esc menu, set Boot → Fast Boot to Disabled in the BIOS (F7 for
  Advanced Mode), save with F10 (ASUS FAQ 1013017). The BitLocker point above applies.
- **Secure Boot:** Ubuntu's signed boot loader normally boots with Secure Boot on; leave it on. Only if the stick
  is refused, set Secure Boot Control to Disabled in the BIOS (Security/Boot → Secure Boot, save with F10) and
  re-enable it afterwards. A very old ASUS without UEFI boots the stick in legacy mode (*unverified* for these
  ISOs).
- **GRUB menu:** choose "Try or Install Ubuntu". Do **not** use the "safe graphics" entry unless the screen stays
  black: it disables the GPU driver (`nomodeset`), so GL falls back to llvmpipe and the run says nothing about the
  real driver. If that is the only way to get a picture, note it in the results.
- **Installer:** language → accessibility → keyboard → network (connect now: apt and the downloads need it) →
  **"Try Ubuntu"**. Nothing is written to the laptop's disk unless you install: **never click "Install Ubuntu"**
  (the live desktop keeps a launcher for it, and the installer can reopen from there). Old laptops with Broadcom Wi-Fi often have no Wi-Fi in a
  live session: use Ethernet or USB tethering from a phone.
- **Persistence:** none. A live session keeps everything in RAM and loses it at shutdown (installed packages,
  REAPER, our plug-ins, reports). Keep the downloads (REAPER tarball, Bitwig installer, our archives, the report
  script) on a **second USB stick** (exFAT or FAT32, readable on the Mac), copy them into `~` at the start of each
  session, and save results back to that stick before shutting down. Persistent live sticks need tools the Mac does
  not have (mkusb on Linux, Rufus on Windows); not worth it for a few sessions.
- RAM: everything installed in the live session lives in RAM. With 4 GB, Bitwig plus REAPER may not fit; check
  `free -h` and run one host per boot if needed.

## 4. What the live session shows, and what it cannot

- **GPU:** `lspci -nnk | grep -A3 -E 'VGA|3D'` names the GPU and the kernel driver; `glxinfo -B` names the GL
  renderer (`OpenGL renderer string`) and whether it is accelerated. Intel and AMD use the same Mesa drivers a
  customer gets.
- **Hybrid graphics** (Intel + NVIDIA/AMD, common in ASUS laptops): the Intel GPU renders by default. Start a host
  with `DRI_PRIME=1` to render on the second GPU (`DRI_PRIME=1 glxinfo -B` shows which one); test both if present.
- **NVIDIA:** the live session has only **nouveau** (open source; Ubuntu's docs: "the preview uses only open-source
  drivers"). The **proprietary NVIDIA driver cannot be tested live**: it needs a full install (kernel module,
  reboot). Most NVIDIA customers run the proprietary driver, so a nouveau pass is a weak proxy; old NVIDIA GPUs on
  nouveau can also be very slow. Say so in the results.
- **X11 vs Wayland:** 24.04 starts the live session on Wayland (our editors then run through XWayland). For an X11
  pass: log out (top-right menu → Power → Log Out), click the live user, choose **"Ubuntu on Xorg"** from the gear
  icon, log in (*unverified* in a live session; the live user has no password, leave it empty if asked). Check with
  `echo $XDG_SESSION_TYPE`. On 26.04 there is only Wayland.
- **Not shown at all:** the proprietary NVIDIA driver, KDE/Xfce window managers (Alt-drag, focus rules), other
  distros' library versions, pro audio interfaces unless one is plugged in, long sessions.

## 5. Setting up a session

```sh
sudo add-apt-repository -y universe      # mesa-utils and systemd-coredump are in universe
sudo apt update
sudo apt install -y mesa-utils gdb systemd-coredump
cat /proc/sys/kernel/core_pattern        # should now pipe to systemd-coredump
glxinfo -B; echo "$XDG_SESSION_TYPE"
```

`sudo` in the live session needs no password. Installing systemd-coredump replaces apport's core-dump handler
(apport itself stays); in a live session that is harmless and gives `coredumpctl`.

### REAPER (portable, no install)
- reaper.fm/download.php → "Linux x86-64": `reaper782_linux_x86_64.tar.xz` (7.82 on 2026-10-20; 60-day evaluation,
  no registration).
- `tar -xf reaper782_linux_x86_64.tar.xz` gives `reaper_linux_x86_64/REAPER/` (the layout our Docker image uses).
  Run it from a terminal so its output can be saved:
  `~/reaper_linux_x86_64/REAPER/reaper 2>&1 | tee ~/reaper-log.txt`. `install-reaper.sh` is not needed; it only
  adds a desktop entry and a system- or user-wide copy.
- Settings go to `~/.config/REAPER` (an empty `reaper.ini` next to the `reaper` binary makes it keep them in its own
  folder instead, according to REAPER's Linux readme; *unverified* here).
- Audio: Options → Preferences → Audio → Device → Audio system **PulseAudio** (Ubuntu's PipeWire provides it).
  Without a working device REAPER never processes audio.
- REAPER scans `~/.clap` and `~/.vst3` by default (our Docker harness relies on that).

### Bitwig Studio
- **Check AVX2 first:** `grep -qw avx2 /proc/cpuinfo && echo yes` (the report script prints it). Bitwig 6 requires
  "a 64-bit x86 CPU with AVX2 support" and Ubuntu 24.04 or later. A CPU without AVX2 cannot run it (older Intel
  Core, and Pentium/Celeron/Atom chips until about 2020; check, don't go by age): then this pass is REAPER only.
- bitwig.com/download → Linux → the **Ubuntu** installer (6.1.4 on 2026-10-20), then
  `sudo apt install ./bitwig-studio-*.deb` (the package file name is *unverified*; apt pulls in its dependencies).
  The Flatpak is the other option; its sandbox may not see `~/.clap` and `~/.vst3` (*unverified*), so prefer the
  Ubuntu installer.
- The 30-day trial needs a Bitwig account: the user signs in himself.
- Bitwig hosts plug-ins in separate processes; a crashed plug-in shows a message in Bitwig rather than taking it
  down. Its plug-in hosting mode (Settings → Plug-ins) decides which plug-ins share a process.

### Our packages
- Archive names: `Gneiss_v<version>_linux_x86_64_<free|paid>.tar.gz`, `Sand_v<version>_linux_x86_64.tar.gz` (from
  `hvoya/distr/<Plugin>/`). Each unpacks into a folder of the same name with `CLAP/`, `VST3/`, `Standalone/`,
  `install.sh`, `README-Linux.txt` (Gneiss also `MANUAL.md`). The README's last line, `Build: <commit>`, identifies
  the build; a `-dirty` suffix means a test build from uncommitted sources.
- Install for the live user:
  ```sh
  tar -xzf Gneiss_v1.3.1_linux_x86_64_paid.tar.gz
  cd Gneiss_v1.3.1_linux_x86_64_paid && ./install.sh
  ```
  It prints (seen in Docker):
  ```
  CLAP -> /home/ubuntu/.clap/Gneiss.clap
  VST3 -> /home/ubuntu/.vst3/Gneiss.vst3
  Standalone -> /home/ubuntu/.local/lib/Gneiss/Gneiss (link /home/ubuntu/.local/bin/Gneiss, menu entry /home/ubuntu/.local/share/applications/gneiss.desktop)
  Done. Rescan plug-ins in your host if it does not pick Gneiss up.
  ```
  Options: `--no-clap`, `--no-vst3`, `--no-app`, `--uninstall`; `sudo ./install.sh --system` installs into
  `/usr/lib/clap`, `/usr/lib/vst3`, `/opt/<Name>` (+ `/usr/local/bin`, `/usr/share/applications`). Run as root
  without `--system`, it refuses ("run without sudo for a per-user install, or pass --system").
- **The standalone:** start it from the app grid (the menu entry, with the product's icon) or by its full path
  `~/.local/lib/Gneiss/Gneiss`. Typing just `Gneiss` may not work in the live session: Ubuntu adds `~/.local/bin` to
  `PATH` only at login, and only if it existed then. Its settings: `~/.config/<Name>/settings.ini`; the products'
  own user data: `~/.local/share/<Name>` (the installer never touches either).
- Requirements as the README states them: glibc 2.35+, X11 or XWayland, libX11 and an OpenGL 2.1 `libGL.so.1`;
  optional libfontconfig, libXcursor, zenity/kdialog/xdg-desktop-portal for file dialogs; the standalone also GTK 3,
  ALSA, PulseAudio and JACK client libraries. Ubuntu desktop has all of them (the report script lists them).

### Once per session
`sh collect-linux-report.sh` (from the data stick) writes `linux-report-<date>.txt`: distro, kernel, glibc, CPU and
AVX2, RAM, session type, desktop, scale settings (GDK_SCALE, QT_SCALE_FACTOR, Xft.dpi, GNOME scaling), screens,
GPU and kernel driver, `glxinfo -B`, audio server, the libraries above (for the machine's own architecture),
installed Gneiss/Sand files with their build-ids and, for an installed standalone, any library `ldd` cannot find,
recent crashes from `coredumpctl`/`/var/crash`. It only reads, uses no network and writes the home folder
as `~`. Tested in Docker on Ubuntu 22.04 (plain and runtime images, arm64 and amd64), 24.04 and Fedora 44, with and
without the packages installed and with Xvfb; the `lspci`, `pactl`, `coredumpctl` and GNOME `gsettings` lines were
not exercised there (the images lack those tools or a GNOME session).

## 6. The test pass (per host: REAPER, then Bitwig; ~30–60 min each)

Before starting: note host name + version, session (X11/Wayland), display scale, GPU (`glxinfo -B` renderer).
Keep the host's terminal output (`… 2>&1 | tee`). Lines starting with `iPlug2:` are ours (e.g. `iPlug2: no usable
libGL.so.1 …, the editor cannot open`, `iPlug2: the host provides no event loop; timers (OnIdle) run on a plug-in
thread`, `iPlug2: resource folder … not found`); any of them is worth reporting. Screenshots: GNOME's
PrtScn key saves to `~/Pictures/Screenshots`; a screen recording (Ctrl+Shift+Alt+R) for anything that moves.

| # | Step | Expected | Capture |
|---|---|---|---|
| 1 | Scan: start the host (REAPER: Preferences → Plug-ins → CLAP / VST → Re-scan if needed). | Gneiss and Sand appear as CLAP and VST3. | Host output; the plug-in list if one is missing. |
| 2 | Open each editor (Gneiss CLAP, Gneiss VST3, Sand CLAP, Sand VST3) on a track playing audio. | Window at the right size, fully drawn within a second: fonts, logo, knobs; no black/white boxes, no garbage. Audio processes. | Screenshot of each first frame; `glxinfo -B` renderer. |
| 3 | Move the mouse over everything, drag knobs, mouse wheel, right-click a knob. | Smooth redraw; values follow; menus open where clicked and close on a click outside. | Recording if anything lags or flickers. |
| 4 | Resize: drag the corner handle at the bottom right of the plug-in UI (both products scale the whole UI); close and reopen. | The UI scales smoothly, the host window follows; the size is kept on reopen. | Screenshots small / large. |
| 5 | Text entry (Sand): click a value readout (e.g. the filter frequency on the FLT tab), type a number, Enter; again with Escape. | The field opens with the value selected; typing replaces it; Enter sets the value, Escape cancels. Plain numbers work (digits, `.`, `+`, `-`: "1.5", "-6"); letters and units ("kHz") are **not** accepted on Linux (`ITextEntryControl` filters them; T-029 is a proposal), IME input is not supported. | Screenshot of the open field; the value afterwards. |
| 6 | Tooltips: Sand (toggle "show/hide tooltips"), Gneiss (on by default, "?" on the right edge). Hover controls. | A tooltip appears near the pointer after a short delay and goes away on move. | Screenshot. |
| 7 | Presets: Gneiss — step with ‹ ›, open the list by clicking the preset name, save a preset to a file and load it back (file dialog). Sand has no presets of its own (one default program, `kNumPresets = 1`): check only that the host's own preset save/restore brings its settings back. | Sound and UI change together; the file dialog opens, the saved file loads; Sand's settings come back from the host preset. | Which dialog appeared (GNOME portal/zenity); host output. |
| 8 | Automation: write a knob move, play it back with the editor open and with it closed. | The knob follows playback; the host's lane shows the right parameter names and value texts. | Screenshot of the lane. |
| 9 | Save the project with the editors open, close the host, reopen the project; then once more with the editors closed. | Every setting comes back; editors reopen at their size. | Project file if not. |
| 10 | Latency (Gneiss): change the oversampling (OS) factor, first in the editor, then with the editor closed through the host's generic parameter view (REAPER: the FX window's "UI" button). | The host's delay compensation follows the change. REAPER shows a plug-in's latency in samples at the bottom of its FX window ("… spls", from memory: confirm). For VST3 in Bitwig with the editor closed, this is T-010's open question: note whether the delay changes, and whether the terminal shows `iPlug2: the host provides no event loop`. | The latency readout before/after each change; host output. |
| 11 | Two instances of each product open side by side; move knobs in both. | Independent, both redraw, no slowdown. | Recording if not. |
| 12 | Open a menu (right-click a knob, or the preset list) and, while it is open, close the plug-in window; again, remove the plug-in while playing. | No crash, no stuck menu window left on screen. | Host output; crash data (§7). |
| 13 | X11 vs Wayland: repeat 2–4 in the other session type (§4); if the display allows, at 200 % scale (Settings → Displays). | Same results. Under Wayland a scaled XWayland window may look blurry: note, not a failure. | Screenshots at each scale. |
| 14 | Hybrid graphics: repeat 2–3 with the host started with `DRI_PRIME=1`. | Same results on the second GPU. | `DRI_PRIME=1 glxinfo -B`. |
| 15 | Standalone (both): start it from the app grid; File → Preferences: driver PulseAudio, input/output devices; play audio in (the laptop's microphone works, **use headphones** to avoid feedback); MIDI if a controller is at hand. Quit with File → Quit. | Sound passes through and is processed; the meters/knobs react; Preferences changes apply without restart; quitting leaves no process (`pgrep -l Gneiss`). If no audio device works, a message over the UI says "Audio is off …". | Screenshot of Preferences; terminal output (start it from a terminal for this). |

Known Linux limitations, not failures: Alt-drag/Alt-click on a knob is taken by some window managers (Xfce, MATE,
Cinnamon, KDE Plasma 5; Gneiss's manual says so; GNOME is not affected); no IME in text fields; units cannot be
typed (step 5).

## 7. Reporting a failure

**Always:** the report script's file; host name and version; plug-in, format (CLAP/VST3/standalone) and the
archive name; X11 or Wayland and the scale; the steps; screenshots/recording; the host's terminal output.

**A crash:**
1. With systemd-coredump installed (§5; Fedora has it by default): `coredumpctl list` shows it; `coredumpctl info
   <PID>` prints the stack of the crashing thread with frames like `#0 0x… n/a (Gneiss.clap + 0x1a0d58)` and, for
   each module, its build-id (format from systemd's documentation; *unverified* with our binaries). Send that
   output. `coredumpctl dump <PID> -o core.<PID>` saves the core if we need more.
2. Without it, run the host under gdb (Bitwig: attach to its plug-in host process instead,
   `sudo gdb -p $(pgrep -f -i pluginhost | head -1)`; the process name is *unverified*):
   ```sh
   gdb -q -ex 'handle SIGPIPE nostop noprint' -ex run --args ~/reaper_linux_x86_64/REAPER/reaper
   # after the crash, at the (gdb) prompt:
   thread apply all bt
   info proc mappings
   ```
   The backtrace shows absolute addresses in our module; `info proc mappings` gives the module's load base (the
   first line of `Gneiss.clap` with offset 0x0). Offset = address − base.
3. A hang: the same gdb, Ctrl+C, `thread apply all bt`.

**From an offset to a source line** (checked in Docker with the 1.3.1 paid x86_64 archive): our binaries are
stripped; their symbols are in `hvoya/distr/<Plugin>/debug-symbols/<archive>_debug-symbols.tar.gz` (kept with us,
never shipped). Each package has **three different binaries, each with its own build-id and its own `.debug` file**: the CLAP
`Gneiss.clap` → `Gneiss.clap.debug`, the VST3 `Gneiss.vst3/Contents/x86_64-linux/Gneiss.so` → `Gneiss.so.debug`,
the standalone `Gneiss` → `Gneiss.debug`. The same offset means a different function in each, so always pick the
file by the crashing module's build-id (the report script and `coredumpctl info` print it; the archive's
`.inspect.txt` lists it per binary); the `.build-id/<first 2 hex>/<rest>.debug` links in the symbols archive point
to the right file. For a frame `(Gneiss.clap + 0x1a0d58)` from the 1.3.1 paid x86_64 CLAP (build-id
`0d35b4b8…`):

```sh
tar -xzf Gneiss_v1.3.1_linux_x86_64_paid_debug-symbols.tar.gz
ls -l Gneiss_v1.3.1_linux_x86_64_paid_debug-symbols/.build-id/0d/
#   35b4b8….debug -> ../../Gneiss.clap.debug
addr2line -f -C -i -e Gneiss_v1.3.1_linux_x86_64_paid_debug-symbols/Gneiss.clap.debug 0x1a0d58
#   iplug::IPlugProcessor::ProcessBlock(double**, double**, int)
#   /Volumes/Storage/code/IPlug2-linux/IPlug/IPlugProcessor.cpp:78 (discriminator 1)
```

(Checked in Docker. The same offset in `Gneiss.so.debug` gives an unrelated VST3 view thunk, which is why the file
must match.) The offsets are file addresses (each binary's first LOAD segment is at 0). gdb can use the stripped
binary as shipped and find the symbols by build-id itself:
`gdb -batch -ex 'set debug-file-directory <unpacked debug-symbols dir>' -ex 'file Gneiss.clap' -ex 'info line *0x1a0d58'`
(same line 78). The binaries are Linux ELF: run these in the Linux Docker image (amd64 image for x86_64 binaries),
not with the Mac's tools.

**The primary route is the `coredumpctl info` stack plus `addr2line` per frame of ours** (above). Opening the core
itself in gdb is for later and needs more than our symbols: an x86_64 host binary of the tester's exact version (our
Docker image has REAPER for arm64 only), the stripped plug-in from the same archive where gdb can find it
(`set solib-search-path <unpacked archive>/CLAP:<unpacked archive>/VST3/Gneiss.vst3/Contents/x86_64-linux`, because
the core records the tester's `/home/<user>/.clap/Gneiss.clap`), and `set debug-file-directory` as above; then
`gdb <host binary> core.<PID>`, `bt`. Frames in system libraries stay unresolved without the tester's distro
libraries.

## 8. Customers as testers

The checklist in `25-tester-checklist.md` is the plain-language subset (install, twelve checks, known limitations, what to send). It
does not mention internal paths or tools. Whether to ship `collect-linux-report.sh` with the beta (next to the
archives, or pasted into the e-mail) is the user's decision; the checklist asks for its output but also lists the
facts by hand for testers who do not want to run a script.

## Sources (checked 2026-10-20)
- Ubuntu downloads: https://ubuntu.com/download/desktop (26.04.1 LTS current; 6 GB RAM, 25 GB disk for an install),
  https://releases.ubuntu.com/ (22.04.5, 24.04.5.1, 26.04.1; `SHA256SUMS`), https://releases.ubuntu.com/noble/,
  https://releases.ubuntu.com/jammy/, https://releases.ubuntu.com/resolute/
- Try Ubuntu (8 GB stick, Etcher on macOS, F12/Esc/F2/F10, "Try Ubuntu", open-source drivers only, no permanent
  changes): https://ubuntu.com/desktop/docs/en/latest/tutorial/try-ubuntu-desktop (was
  documentation.ubuntu.com/desktop/…); older macOS tutorial: https://ubuntu.com/tutorials/create-a-usb-stick-on-macos
- GNOME without an Xorg session in 26.04: https://www.omgubuntu.co.uk/2026/04/ubuntu-26-04-lts-changes-since-24-04/amp,
  https://linuxiac.com/ubuntu-26-04-lts-resolute-raccoon-released/
- ASUS boot menu (Esc) and Secure Boot: https://www.asus.com/support/faq/1013017/ ; BIOS (F2, Windows Advanced
  startup): https://www.asus.com/global/support/FAQ/1008829/
- REAPER: https://www.reaper.fm/download.php (7.82, `reaper782_linux_x86_64.tar.xz`, 60-day evaluation); running
  from the extracted folder and `reaper.ini`: REAPER's Linux readme as quoted by https://ProRec.com/?p=6117
- Bitwig: https://www.bitwig.com/download/ (6.1.4; Ubuntu installer and Flatpak; Ubuntu 24.04+, AVX2),
  https://www.bitwig.com/try/ (30-day trial, sign-up)
- Packages: https://packages.ubuntu.com/noble/mesa-utils , https://packages.ubuntu.com/noble/systemd-coredump
  (both universe); apport vs systemd-coredump:
  https://discourse.ubuntu.com/t/apport-2-28-0-gained-systemd-coredump-integration/44910
