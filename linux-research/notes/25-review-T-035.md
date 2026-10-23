# Review of T-035 (real-hardware kit): round 1

Reviewer: independent subagent, fresh context, 2026-10-20. Scope: `hvoya-linux..linux/T-035`
(e508e6a29 script, ddcae5d62 notes, cb94e415e task file). The diff touches only `linux-research/` (4 files), so no
shared code and no mac/win impact. All three commits carry the trailer.

## What I re-checked myself

- **Archives** (coordinator scratchpad `distr-test/`), unpacked into my own scratch `t035r/`: Gneiss 1.3.1 paid
  x86_64 (`Build: 2cab58b-dirty`), Gneiss 1.3.1 free aarch64, Sand 1.1.10 x86_64 (`Build: 87ccdbf`), and the Gneiss
  x86_64 paid debug-symbols archive. I read `README-Linux.txt` and `install.sh` in both products (the same apart from
  names) and compared them with `Scripts/linux/package.sh`, which is identical in the T-035 worktree and in
  `hvoya-linux`. Archive names, folder layout (`CLAP/ VST3/ Standalone/ install.sh README-Linux.txt`, plus Gneiss's
  `MANUAL.md`), the options, `--system` targets, the root refusal message, the printed install lines and the
  requirements list all match the note and the checklist. The `iPlug2: …` stderr lines quoted in note §6 exist
  verbatim (`IGraphicsLinuxGLX.h:68`, `IPlugLinuxHostLoop.cpp:274`, `IPlugLinuxPaths.cpp:36`), and so does "Audio is
  off" (`IPlugAPP_host.cpp:591`). Debug symbols go to `hvoya/distr/<Plugin>/debug-symbols/` (`makedist-linux.sh:5,100`).
- **Symbolising** in `iplug2-linux:22.04-gcc13-amd64` (container `t035r-sym`, `--rm`): `addr2line -f -C -i -e
  …/Gneiss.so.debug 0x1e13a8` → `iplug::IPlugProcessor::ProcessBlock(double**, double**, int)`,
  `IPlugProcessor.cpp:78 (discriminator 1)`, exactly as in the note. `gdb -batch -ex 'set debug-file-directory …'
  -ex 'file …/Gneiss.so' -ex 'info line *0x1e13a8'` on the stripped VST3 binary finds the symbols by build-id: same
  line. The first LOAD segment is at vaddr 0 (`readelf -lW`). But see **M1**: the same offset in `Gneiss.clap` is a
  different function.
- **Report script**: `sh -n` (macOS) and `dash -n` pass; 70 lines. Runs (all `--rm`, names `t035r-*`, none left):
  - plain `ubuntu:22.04` (arm64, dash): as a user with the Gneiss aarch64 package installed; as root without packages;
    with `HOME` unset; with an unwritable output path;
  - `iplug2-rt:ubuntu22.04-amd64`: Gneiss x86_64 (user, with the app) + Sand x86_64 (user `--no-app`, then
    `--system` with `/usr/lib/clap/Sand.resources` removed). Every installed file is found with the build-ids of
    the archive's `.inspect.txt` (Gneiss `0d35b4b8…`, `467039e6…`, `f1013d1e…`), and the missing resources folder is
    flagged. The `file` fallback (PATH without `readelf`) gives the same build-ids;
  - `iplug2-linux:24.04` with Xvfb + `GDK_SCALE=2`: `glxinfo -B` and the session lines are right;
  - `iplug2-rt:fedora` (Fedora 44): libraries and build-ids found.

  No network use (no curl/wget/ping; glxinfo, pactl and gsettings talk to local servers). No hostname or user name
  in any output; `$HOME` is written as `~`. The only comment is the header the task asks for; no comment restates
  code.
- **Third-party facts**, checked today on the vendors' pages:
  - releases.ubuntu.com lists `ubuntu-24.04.5.1-desktop-amd64.iso` (5.8G) and `ubuntu-22.04.5-desktop-amd64.iso`
    (4.4G);
  - ubuntu.com/download/desktop offers 26.04.1 LTS and **26.10** (both 5.9 GB amd64);
  - Ubuntu's try-ubuntu tutorial: 8 GB stick, Etcher on macOS, F12/Esc/F2/F10, "only uses open-source drivers";
  - 26.04 has no GNOME-on-Xorg session (linuxiac, debugpoint, OMG! Ubuntu);
  - Bitwig: 6.1.4, "Ubuntu 24.04 or later", "64-bit x86 CPU with AVX2 support", Ubuntu installer + Flatpak, a
    30-day trial with sign-up;
  - REAPER 7.82, `reaper782_linux_x86_64.tar.xz`, 60-day evaluation;
  - ASUS FAQ 1013017: Esc boot menu; Secure Boot Control under Security, F10 to save. The FAQ also disables Fast Boot;
  - ASUS FAQ 1008829: F2 and the Windows Advanced startup route, with the menu names as in the note;
  - packages.ubuntu.com: `systemd-coredump` is in universe in noble.
- **Product claims**:
  - Alt-drag text matches Gneiss `MANUAL.md:284`;
  - the Gneiss tooltip "?" on the right edge and the ‹ › preset buttons match `MANUAL.md:11`;
  - Sand has a "show/hide tooltips" toggle (`SandUI.cpp:258`) and an FLT tab (`SandUI.cpp:322`);
  - both products attach a scaling corner resizer;
  - Linux has no IME (no XIM in `IGraphicsLinux*.cpp`);
  - Ardour and Carla runs exist (T-013 board line), so the checklist's "tested in REAPER, Ardour and Carla" holds.

## Findings

### M1 (major): the crash example pairs a CLAP frame with the VST3 binary's symbols
`notes/25-real-hardware-testing.md:208-209` gives the `coredumpctl info` frame as `(Gneiss.clap + 0x1e13a8)`. Then
`:229-233` resolves 0x1e13a8 with `Gneiss.so.debug`, which belongs to the **VST3** binary. I checked both in Docker:
`Gneiss.so.debug` @0x1e13a8 = `IPlugProcessor::ProcessBlock` (IPlugProcessor.cpp:78), but `Gneiss.clap.debug`
@0x1e13a8 = `fonsTextBounds` (fontstash.h:1493). The CLAP, the VST3 `.so` and the app are three different binaries
with three build-ids, each with its own `.debug` file. Someone who follows the example as written symbolises a CLAP
crash against the wrong file, and gets a plausible but wrong function.
**Fix:** make the frame `(Gneiss.so + 0x1e13a8)`, or keep `.clap` and use `Gneiss.clap.debug`. Add one line that
maps each binary to its debug file: `Gneiss.clap` → `Gneiss.clap.debug`, VST3 `Gneiss.so` → `Gneiss.so.debug`,
standalone `Gneiss` → `Gneiss.debug`. Always choose by build-id, which the `.build-id/` links already do.

### M2 (major): Secure Boot advice says nothing about BitLocker
`notes/25-real-hardware-testing.md:60-63` says to set Secure Boot Control to Disabled if the stick is refused. On a
laptop whose Windows uses BitLocker / Device Encryption (the default on many Windows 10/11 laptops with a TPM), a
change to the Secure Boot setting makes Windows ask for the **BitLocker recovery key** at the next start. Without the
key, the user is locked out of Windows and its data. Booting the stick from the Esc menu does not trigger this;
changing firmware security settings does.
**Fix:** add a line before the Secure Boot step. In Windows, check Settings → Privacy & security → Device
encryption (or "Manage BitLocker"). If it is on, save the recovery key first (aka.ms/myrecoverykey, or "Back up your
recovery key") before changing any BIOS security setting. Keep "leave Secure Boot on unless the stick is refused".

### M3 (major): the `dd` block has a ready-to-paste literal disk number
`notes/25-real-hardware-testing.md:38-45`: the commands use `/dev/disk4` and `/dev/rdisk4` literally. The "e.g." sits
in a comment two lines above. The before/after `diskutil list external physical` check is good. But a block that can
be pasted as a whole will wipe whatever disk4 is on that day: a Time Machine drive or a second external disk is also
"external physical". The task asked for exact, defensive commands.
**Fix:** use a placeholder that fails if it is not edited, and set it once:
```sh
N=     # the stick's number from the second `diskutil list external physical`, e.g. N=4
diskutil info /dev/disk$N | grep -E 'Device / Media Name|Disk Size|Removable Media|Protocol'
diskutil unmountDisk /dev/disk$N
sudo dd if=ubuntu-24.04.5.1-desktop-amd64.iso of=/dev/rdisk$N bs=4m status=progress
```
With `N` empty, `/dev/disk` / `/dev/rdisk` do not exist and every command fails harmlessly. Also say: "everything on
the stick is erased", and "if `diskutil info` does not show the stick's brand and size, stop".

### m1 (minor): Sand has no factory presets
`notes/25-real-hardware-testing.md:187` says "Sand — its factory presets from the host's preset menu". Sand declares
`kNumPresets = 1` (`Sand.hpp:25`) and has no `MakePreset` calls and no preset UI. The checklist's check 6
(`notes/25-tester-checklist.md:48-49`) puts "(Gneiss)" only after the save/load half, so a Sand tester will look for
presets to step through.
**Fix:** in the note, "Sand: none (one default program); check only that the host's own preset save/restore works".
In the checklist, make check 6 Gneiss-only ("**Presets** (Gneiss). Step through …"), or add a Sand line about the
host's preset save/load.

### m2 (minor): the build-id is silently empty without `readelf` and `file`
`scripts/collect-linux-report.sh:11-14,58`: on plain `ubuntu:22.04` the report printed `~/.clap/Gneiss.clap
build-id ` with nothing after it. `file` is usually present on desktops, `binutils` often not. An empty field looks
like a broken binary.
**Fix:** `id=$(build_id "$f"); echo "$f  build-id ${id:-unknown (needs readelf or file)}"`. Alternatively, add
`cksum "$f"` (coreutils, always present) and keep checksums in `.inspect.txt`.

### m3 (minor): "Written to" is printed even when nothing was written
`scripts/collect-linux-report.sh:69-70`:
- with an unwritable path, dash prints `cannot create /nonexist/x.txt` and then "Written to /nonexist/x.txt";
- with `HOME` unset, `sed "s||~|g"` fails ("no previous regular expression"), the file is **empty**, and the
  message still says "Written to".

**Fix:** guard the substitution (`[ -n "$HOME" ] && [ "$HOME" != / ]`; otherwise `cat`), and end with
`[ -s "$OUT" ] && echo "Written to $OUT. …" || { echo "Could not write $OUT" >&2; exit 1; }`.

### m4 (minor): the checklist asks for `coredumpctl info` output without a privacy note
`notes/25-tester-checklist.md:77-79`: `coredumpctl info` prints the Hostname, Machine ID, Boot ID, the command line
and paths under the user's home. The report file gets "read it before you send it"; this output does not.
**Fix:** "It contains your computer's name and folder names; remove anything you prefer not to share."

### m5 (minor): "only digits work" is not accurate
`notes/25-tester-checklist.md:44-45` (and `notes/25-real-hardware-testing.md:185`, "digits replace it"). The Linux
field (`ITextEntryControl.cpp:279-297`) also accepts `+ -` for integers and `+ - .` for decimals, so "1.5" and "-6"
work. A tester told "only digits" will not try decimals, or will report them as a surprise.
**Fix:** "On Linux you can type plain numbers (digits, a decimal point, a minus sign); units such as "kHz" cannot be
typed, and input methods for non-Latin scripts are not supported." Drop "yet" or keep it, as the user prefers. It
implies a promise; T-029 is still a proposal.

### m6 (minor): note §8 is out of date
`notes/25-real-hardware-testing.md:244` says the checklist has "install, five checks, what to send". It has 12
checks (`25-tester-checklist.md:36-62`), as the Log says.
**Fix:** "install, twelve checks, known limitations, what to send", or don't count them.

### m7 (minor): symbolising a core needs more than the note says
`notes/25-real-hardware-testing.md:238-240`: `gdb … <host binary> core.<PID>` also needs:
- the stripped plug-in at a path gdb can find (`set solib-search-path <unpacked archive>/CLAP:<…>/VST3/Gneiss.vst3/Contents/x86_64-linux`,
  or `set sysroot`); the core records the tester's `/home/<user>/.clap/Gneiss.clap`;
- an x86_64 REAPER 7.82 binary in the amd64 image (our Docker image has the arm64 REAPER).

Frames in system libraries stay unresolved without the tester's distro libraries.
**Fix:** add these points. Or say plainly that the `coredumpctl info` stack plus `addr2line` per frame (verified
here) is the primary route, and a core in gdb is for later.

### m8 (minor): live-session wording that can still cost data or time
`notes/25-real-hardware-testing.md:55-59,67-69`:
- The live desktop keeps an "Install Ubuntu" launcher (24.04's installer reopens from it). Add: "never click Install;
  nothing touches the laptop's disk unless you do."
- ASUS FAQ 1013017 also disables **Fast Boot** in the BIOS. Add: "if the stick does not appear in the Esc menu, set
  Boot → Fast Boot to Disabled (F7 for Advanced Mode), F10 to save".

### n1 (nit): apport is not removed
`notes/25-real-hardware-testing.md:102,106`: in noble, `apport` Depends on `apport-core-dump-handler | systemd-coredump`
(packages.ubuntu.com). Installing systemd-coredump swaps out the core-dump handler; apport itself stays.
**Fix:** "(replaces apport's core-dump handler)".

### n2 (nit): image table
`notes/25-real-hardware-testing.md:22`:
- ubuntu.com now gives 5.9 GB for 26.04.1;
- 26.10 was released and is offered beside it (also no Xorg session).

**Fix:** correct the size; optionally say "26.04.1 LTS (or 26.10)".

### n3 (nit): multiarch false positive in `lib()`
`scripts/collect-linux-report.sh:15`: on a system with i386 libraries (Steam/Wine users), the i386 copy of
`libGL.so.1` alone gives "found".
**Fix:** match the architecture tag, e.g. `grep -E "^[[:space:]]*$1 \(libc6,(x86-64|AArch64)"`.

### n4 (nit): check the standalone's real dependencies instead of a list
`scripts/collect-linux-report.sh:47-48`: the app NEEDs `libgdk-3`, `libXi`, `libpulse-simple`, `libcairo`,
`libfreetype` … (`.inspect.txt`), more than the ten names checked.
**Fix:** for each installed standalone, `ldd "$f" 2>/dev/null | grep 'not found'` (local, read-only) is exact and
shorter.

### n5 (nit): the AVX2 age rule
`notes/25-real-hardware-testing.md:124-125`: "before ~2013". Pentium/Celeron/Atom chips lacked AVX2 until about 2020,
and AMD before about 2015. The `grep` is the real test; say "check it, don't go by age".

### n6 (nit): the task file still names `23-*.md` under Deliverables
`tasks/T-035-real-hardware-kit.md:16,32`. The Log explains the renumbering; also update the Deliverables paths to
`25-…`.

### n7 (nit): masking edge cases
`scripts/collect-linux-report.sh:66,69`:
- the `/var/crash` mask `_home_[^_]*_` leaves the tail of a user name that contains `_`;
- `$HOME` is used as a sed regex (a `|` breaks it).

Acceptable for the tester population; mention only if touching those lines anyway.

### n8 (nit): the Alt-drag limitation in a shared checklist
`notes/25-tester-checklist.md:65-66`: the limitation matters for Gneiss's cutoff snapping. Sand testers may wonder
what Alt does. Say "(Gneiss: Alt-drag on a cutoff snaps it to notes)".

## Checked and OK
- **Package facts:**
  - archive naming;
  - `install.sh` behaviour: user and `--system` paths, the root refusal, `--uninstall` keeps settings and user data;
  - the `~/.local/bin` PATH caveat;
  - the README's requirement list, quoted faithfully in both documents.
- **macOS commands:**
  - `diskutil list external physical`, the `diskutil info` field names, `unmountDisk` before `dd`, `rdisk`, `eject`;
  - the "Eject, not Initialise" advice;
  - Ctrl+T as the progress fallback;
  - on this Mac today there is no external physical disk; `/Volumes/Storage` is on the internal `disk3`.
- **The Ubuntu, Bitwig, REAPER and ASUS facts listed above;** the X11/Wayland and XWayland-blur wording; the NVIDIA
  live-session caveat (Ubuntu's own wording).
- **Checklist:** plain language, no internal paths, repo names or task ids. It promises only what the README states,
  apart from m1 and m5.
- **Script:** POSIX/dash-clean; read-only, no network, privacy as stated (home masked, no host or user name).
  Product names in it are allowed in `linux-research/`. It would need renaming if it ever moves into `Scripts/`.

## Verdict
**Approve with fixes.** Fix M1–M3 before the user uses the note on the laptop. M1 gives a wrong function name for a
CLAP crash; M2 and M3 are data-loss risks that each need a few added lines. m1–m8 should go in the same pass (each is
a line or two). The nits are optional.

## Round 2 (2026-10-20, same reviewer)

Scope: e4fe7ef72 on `linux/T-035`, which touches the two notes, the script, the task file, and adds this review.
Re-run myself:
- the symbolisation in `iplug2-linux:22.04-gcc13-amd64`;
- the `dd` block in interactive zsh and bash, with `diskutil`, `sudo` and `sync` replaced by echo stubs (nothing was
  run against a disk);
- the script in dash on plain `ubuntu:22.04` and on `iplug2-rt:ubuntu22.04-amd64` with both packages installed.

All containers were `--rm`, named `t035r-*`; none are left. Outputs: scratchpad `t035r/out2r/`.

### Per finding
- **M1: fixed.** `Gneiss.clap.debug` @0x1a0d58 → `iplug::IPlugProcessor::ProcessBlock`, `IPlugProcessor.cpp:78
  (discriminator 1)`, exactly as the note prints it. gdb with `set debug-file-directory` on the stripped
  `CLAP/Gneiss.clap`: same line. `.build-id/0d/35b4b8….debug -> ../../Gneiss.clap.debug` as the note shows.
  `Gneiss.so.debug` at the same offset → `non-virtual thunk to IPlugVST3View<…>::onKeyUp`, so the note's "unrelated
  VST3 view thunk" is right. The first LOAD segment is at 0 in all three binaries (CLAP, VST3, standalone), so "file
  address = module offset" holds for each. The binary-to-`.debug` map and "choose by build-id" are clear.
- **M2: fixed.** The BitLocker / Device Encryption warning comes before any BIOS change. It names the consequence, the
  check, and where to save the key (aka.ms/myrecoverykey). It says the Esc boot menu alone does not trigger it, and the
  Fast Boot bullet points back to it. Nit: the Settings path is Windows 11's; on Windows 10 it is Settings → Update &
  Security → Device encryption. Control Panel → BitLocker covers both, so this is fine as is.
- **M3: fixed for the danger it was about.** With `N` unset or empty, every `${N:?set N first}` command stops before
  it runs:
  - bash 3.2 `-i`: `bash: N: set N first`, for `diskutil info`, `unmountDisk`, `sudo dd` and `eject`;
  - zsh `-f -i`: `zsh: N: set N first`, for the same four. The stubs show `diskutil`/`sudo` never ran with a disk
    argument; only `sync` ran, which is harmless.

  The expansion happens in the user's shell before `sudo`, so `dd` never starts. The text says the stick is erased,
  and to stop unless `diskutil info` shows the stick. A new issue in the same block is **R2-1** below.
- **m1: fixed.** Note step 7 and checklist check 6 are Gneiss-only; Sand is checked through the host's preset save
  and load.
- **m2: fixed.** Plain `ubuntu:22.04` (no readelf/file) prints `build-id unknown (needs readelf or file)`. The amd64
  image prints all six build-ids, matching `.inspect.txt` (Gneiss `0d35b4b8…`/`467039e6…`/`f1013d1e…`, Sand
  `3590b44a…`/`ec090980…`/`1235aa40…`).
- **m3: fixed for the reported cases:**
  - an unwritable path gives "Could not write /nonexist/x.txt", exit 1;
  - `HOME` unset gives a 38-line report, no sed error;
  - `HOME=/` is not substituted (no `~` in place of every `/`).

  One case remains: a **stale, read-only file of the same name**. The shell cannot overwrite it, `[ -s ]` sees the
  old content, and the script prints "Written to r.txt" with exit 0 (reproduced). Rare (same explicit name, or the
  same minute, on a read-only medium), so this is a nit (**R2-2**).
- **m4: fixed.** The checklist now says the crash report contains the computer's name and folder names.
- **m5: fixed.** "plain numbers (digits, a decimal point, a minus sign)" matches `ITextEntryControl.cpp:279-297`.
  "yet" is gone.
- **m6: fixed.** "twelve checks, known limitations".
- **m7: fixed.** `coredumpctl info` + `addr2line` is the primary route. The core-in-gdb needs (x86_64 host binary of
  the same version, `solib-search-path`, unresolved system frames) are stated.
- **m8: fixed.** "never click Install Ubuntu"; the ASUS Fast Boot step (F7, F10).
- **n1, n2, n3, n4, n5, n6, n8: fixed.**
  - n3: the arch tag `libc6,x86-64` / `libc6,AArch64` is matched as a fixed-string prefix. An i386-only
    `(libc6)` line now reads MISSING; an `(libc6,x86-64, OS ABI: …)` line reads found.
  - n4: `ldd … | grep 'not found'` listed all 15 missing standalone libraries on plain 22.04, and printed nothing on
    the runtime image.
- **n7: left by decision.** Accepted.

The script is 77 lines, and `sh -n` and `dash -n` both pass. There are still no comments beyond the header, and no
network use. The home folder is masked (no `/home` in either output).

### New findings
- **R2-1 (minor): trailing `#` comments break the Mac commands in the user's zsh.**
  `notes/25-real-hardware-testing.md:39-44,50` (the two `dd` blocks). In zsh, `interactivecomments` is **off** by
  default, and it is off in the user's own zsh (`zsh -i -c 'setopt'` does not list it). Pasted or typed as printed:
  - `diskutil list external physical # before inserting the stick: note what is listed (maybe nothing)` fails with
    `zsh: number expected`: the `(maybe nothing)` is read as a glob qualifier;
  - `diskutil list external physical # the one new entry is the stick` passes `#` and the words to diskutil, which
    prints its usage and exits 1, so the listing never appears;
  - the `N=   # its number, e.g. N=4 …` line fails (`unknown file attribute: y`);
  - the `dd` line, even with `N` set correctly, passes `# rdisk: the raw, fast node` as operands. macOS `dd` stops
    with `dd: unknown operand #` before it opens anything (checked: no output file created).

  So it **fails safe**, nothing is erased, but the procedure does not work as printed in the Mac's default shell.
  **Fix:** remove the trailing comments from the command lines in §2's two blocks. Put them in the prose, or on
  their own lines, where zsh only reports `command not found: #` (harmless). Keep `N=` alone on its line.
  Alternatively, start the block with `setopt interactivecomments 2>/dev/null` (bash ignores it with an error).
  The first option is cleaner.
- **R2-2 (nit): a stale read-only output file still produces "Written to".**
  `scripts/collect-linux-report.sh:74-76`. Fix in one line, before the report: `: > "$OUT" 2>/dev/null || { echo
  "Could not write $OUT" >&2; exit 1; }`. That keeps the script within its budget (78 lines).

### Round 2 verdict
**Approve, with one minor fix still to make (R2-1)**: take the trailing comments out of §2's command lines. All three
majors are resolved. M1 was re-run with the correct result, and M3's guard demonstrably stops every disk command
while `N` is empty, in both zsh and bash. R2-1 is not a data-loss risk; it fails safe, but the user would hit it on
the first try in his own shell, so it should be fixed before he uses the note. That is a two-minute edit and needs no
further review round. R2-2 and n7 are optional.
