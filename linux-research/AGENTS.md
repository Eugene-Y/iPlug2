# Linux port: instructions for any Claude instance (read first)

Several Claude instances work on this port in parallel; none of them shares memory with another. Everything you need is in this repo. If something you learned would help the next instance, write it into the task file or here, not into chat.

## 1. The project in 10 lines

- **Goal:** Linux builds (CLAP + VST3 + standalone APP, full IGraphics UI on X11 + NanoVG/GL2) of the user's iPlug2 plugins — **Gneiss** first, then **Sand**.
- **Repos:**
  - iPlug2 fork `/Volumes/Storage/code/IPlug2`: the main checkout, branch `hvoya` — the product branch. **Never touch it.** Gneiss on the Mac builds against it.
  - Gneiss `/Volumes/Storage/code/hvoya/Gneiss`: the main checkout, the user's working tree. **Never touch it.**
  - Sand `/Volumes/Storage/code/hvoya/Sand`: never touch.
  - (Only the coordinator, and only on the user's explicit command, commits there — e.g. merging a reviewed fix. See `COORDINATOR.md`.)
- **Integration branch:** `hvoya-linux`, in the worktree `/Volumes/Storage/code/IPlug2-linux`. Only the coordinator commits there (see §4).
- **Plan and history:**
  - `linux-research/PLAN.md` — §1–2 are **binding decisions**; then the Progress table and phases P0–P12.
  - Research and phase notes: `linux-research/notes/NN-*.md`.
  - Human summary: `linux-research/LINUX_PORT_RESEARCH.md`.
- **Done (reviewed):**
  - P0–P1: build, exports, paths;
  - P2: host-loop timers — everything on the host main thread;
  - P3a: X11 window + GL2;
  - P3b: keyboard, focus, text entry, clipboard, menus, fonts.
- **Tasks:** `linux-research/tasks/` — one file per task, board in `tasks/README.md`.
- **Current state of branches, waiting items and user decisions:** `linux-research/STATUS.md` (coordinator snapshot).
- **Taking over the coordinator role** (merging, the board, talking to the user about the whole port): `linux-research/COORDINATOR.md`.
- **Cross-platform (mac/win) bugs found during the port:** `linux-research/CROSS_PLATFORM_FINDINGS.md`.

## 2. Non-negotiable rules (from PLAN §1–2 and the user's CLAUDE.md)

- **Threads.**
  - Drawing, X events, `IPlugTimer`/`OnIdle` and editor callbacks run on the **host main thread**, via `GetHostLoop()` (`IPlug/Linux/IPlugLinuxHostLoop.h`).
  - No plugin-owned UI threads.
  - No `XInitThreads`.
- **No GTK inside the plugin process.**
- **Minimal footprint on business logic and other platforms** (the user's explicit requirement):
  - A port to Linux of a product (Gneiss, Sand) or an example (IPlugEffect, IPlugControls) **adds** a Linux build. It must not change how the product behaves or builds on macOS/Windows/iOS/web.
  - Do not modify the Xcode/VS projects, `config.h`, xcconfig/props or their flags, presets or state formats. Do not move Xcode/VS settings into CMake in a way that the other builds then depend on. The Linux CMake is an **additional** build description, not a replacement.
  - Touch business-logic sources only when the Linux build cannot compile or run otherwise. Typical cases: GCC rejects code that clang/MSVC accept, or undefined behaviour that GCC exposes. Make the smallest behaviour-neutral change, and explain it in the commit message.
  - **Bugs you find that are not Linux-specific** (they would happen on mac/win too): **report, don't fix** in the port branch. Write each one in the task file's Log and in a "Cross-platform findings" section: severity, repro, file:line. The coordinator turns them into decisions for the user.
  - **Read the product repo's own docs first** — Gneiss has `CLAUDE.md`, `ARCHITECTURE.md`, `docs/`, `BACKLOG.md` — and follow its rules (e.g. param enum order, comment rules, commit rules).
- **Keep Linux code in new files** (`IPlug/Linux/`, `IGraphics/Platforms/IGraphicsLinux*`), with thin `#ifdef OS_LINUX` hooks in shared files.
- **No product names in the framework** (the user, 2026-10-18). Plug-ins depend on iPlug2, not the other way round, so iPlug2's docs, README, code comments, scripts and examples never name a plug-in (Gneiss, Sand, …). Describe the use case instead: "a plug-in that builds as C++23 with `constexpr` tables needs clangd 19", not "Sand needs clangd 19". The reverse is fine: a plug-in's own docs may say anything about iPlug2. Commit messages and the `linux-research/` work area are not shipped docs; anything moved out of them into `Documentation/`, `README.md` or code follows this rule.
- **Shared code must not change mac/win behaviour.** If you touch a shared file, prove macOS still builds: IPlugEffect CLAP via xcodebuild, with DerivedData and install paths in your scratchpad, **never `~/Library`**. Restore the `.rc_mac` files that Xcode regenerates, after a snapshot.
- **Code style.**
  - 2-space indent, `mCamelCase` members, `pCamelCase` pointer args, C++17, WDL over STL in core.
  - **Comments only for the non-obvious WHY**: never restate code; fix or delete stale comments.
  - UI code should be declarative and readable.
- **Git.**
  - Commit in small steps, with the trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
  - **Never push.** The user pushes.
  - Never bare `git stash` / `stash pop`: the stash is shared by all worktrees and instances.
  - Before anything that restores or overwrites files (`checkout <path>`, `restore`, `reset --hard`, `clean`, a wholesale rewrite), snapshot first:
    `git add -A && git stash store -m "<task>: before <x>" "$(git stash create)"`
- **Never use passwords.** The VM is reached by SSH key only.

## 3. Tools

**Docker (colima)** — all Linux builds and headless tests.
```sh
colima status || colima start --cpu 6 --memory 8 --disk 50 --vm-type vz --vz-rosetta --mount-type virtiofs --mount /Volumes/Storage/code:w --mount /private/tmp/claude-501:w --mount "$HOME:w"
```
- Image `iplug2-linux:24.04` (Ubuntu 24.04, arm64; REAPER 7.82, clap-validator, VST3 validator, Xvfb, openbox, llvmpipe, xdotool, gdb). See `linux-research/docker/README.md`.
- Build: `linux-research/scripts/dock.sh -- scripts/build-cmake.sh <tree> <builddir> <targets…>`.
  Only `DOCK_ENV` reaches the container: `DOCK_ENV="BUILD_TYPE=Debug" scripts/dock.sh …`. A plain `BUILD_TYPE=Debug scripts/dock.sh …` silently builds Release.
- GUI test in REAPER under Xvfb: `GUI=<your-container> scripts/run-gui-test.sh <plugin> <outdir> [steps]`.
- **Use your own container name and build dirs**, prefixed with your task id, e.g. `GUI=t003`, `build/t003-…`.
- Other scripts in `scripts/`:
  - `host-loop-test.sh` — main-thread check;
  - `editor-churn-test.sh` — open/close leaks;
  - `validate.sh`, `inspect-binary.sh` — exports, glibc, ldd;
  - `xact.sh` — the step DSL;
  - `sheet.sh` — contact sheets.

**VM** — real GNOME/X11 desktop.
- UTM, Ubuntu 24.04.5 arm64, `ssh -i ~/.ssh/claude_linux_vm gene@192.168.64.2`, passwordless sudo, virgl GL 2.1.
- Scripts and usage: `linux-research/scripts/vm/README.md` (`vm-deploy.sh`, `vm-run.sh`, `vm-shot.sh`, `vm-x.sh`).
- If it is unreachable, the VM is stopped (no `bridge100` on the Mac). Note it in your task file and continue with Docker, or stop; don't try to start UTM yourself.
- **The VM is a single shared resource: take the lock (§5) before using it.**

**Screenshots.** Always look at them yourself (Read tool on the PNG). A green harness once reported PASS on a black window.

## 4. Workflow

1. **Pick a task** from `tasks/README.md` whose status is `ready` and whose dependencies are done. The user may hand you one directly.
2. **Claim it** with an atomic lock (§5), then create your branch and worktree:
   ```sh
   git -C /Volumes/Storage/code/IPlug2-linux worktree add -b linux/T-NNN /Volumes/Storage/code/IPlug2-T-NNN hvoya-linux
   ```
   - Gneiss tasks use the Gneiss repo's `linux-experiment` branch in the worktree `/Volumes/Storage/code/hvoya/Gneiss-linux`. If another task holds it, branch `linux-experiment-T-NNN` from it into a new worktree.
   - Update the task file's **Status/Owner/Branch** lines in your branch.
3. **Work in your worktree only.**
   - Keep a **Log** section at the bottom of the task file, updated as you go: what is done, the commit, what's next. If you are interrupted, the next instance continues from it.
   - Commit often.
   - **One subagent at a time** if you delegate — limits kill parallel agents and lose work.
4. **Independent review before handing over (mandatory).** When your work is done, start a **separate reviewer subagent** with fresh context — never review your own work in the same context.
   - **Give it:**
     - the task file;
     - the commit range (`git log hvoya-linux..linux/T-NNN`);
     - your results and evidence paths;
     - this file (AGENTS.md).
   - **It checks:**
     - the diff against the task's acceptance list and §2;
     - that the evidence really shows what the Log claims — it opens screenshots itself;
     - that the key tests still pass, re-run by itself;
     - code quality and the shared-code / mac-win rule.

     It writes its findings with severities into the task file's Log, or into `notes/NN-review-T-NNN.md` for larger tasks.
   - **You fix the findings**, then the reviewer re-checks. Repeat until it approves with no major finding left (user 2026-10-22: more rounds are fine when 2 aren't enough); a late fix of a major finding goes back to the reviewer too.
   - Run the reviewer **after** your own work, not in parallel with it, and keep only one subagent running at a time.
   - Record the final verdict in the task file.
5. **Hand over.** Set Status `review-done`, and list commits, evidence paths and open issues in the task file. Remove your Docker containers, release your locks, and leave the worktree in place. The **coordinator** merges `linux/T-NNN` into `hvoya-linux` (`--no-ff`), re-runs smoke tests, updates `tasks/README.md` and PLAN Progress, and removes the worktree.
6. **After an interruption** (limits, sleep):
   - `git status`;
   - look for orphaned scripts or containers (`ps aux | grep linux-research`, `docker ps`) and kill them;
   - commit unfinished work as `WIP:`;
   - then continue from the task's Log.

## 5. Locks (outside git, shared by all instances)

Directory: `/Volumes/Storage/code/.linux-port-locks/`. `mkdir` is atomic, so whoever creates the lock first owns it.
```sh
L=/Volumes/Storage/code/.linux-port-locks; mkdir -p $L
mkdir $L/T-NNN && echo "$(date '+%F %T') $(hostname) <who>" > $L/T-NNN/owner   # claim a task; fails if taken
mkdir $L/vm    && echo "$(date '+%F %T') T-NNN" > $L/vm/owner                 # take the VM before using it
rm -rf $L/vm                                                                  # release the VM as soon as you're done with it
```
- Hold the VM lock only while actually using the VM; release it between test batches.
- A lock older than about 6 h whose owner shows no activity (no commits, no processes) may be broken. Note in the task file that you broke it.

## 6. Where to look

| Need | File |
|---|---|
| Binding decisions, phases, progress | `linux-research/PLAN.md` |
| How X11/GL/host loops work here | notes/09 (host loop), notes/11 (window), notes/13 (input), and their reviews 10/12/15 |
| Best practices from JUCE/DPF/VSTGUI/pugl | notes/02 |
| Harness | `docker/README.md`, `scripts/vm/README.md` |
| What other ports got wrong | notes/04, notes/05 |
| Developer setup on Linux (presets, VS Code, debugging in REAPER), IDE choice | `Documentation/linux-development.md`, notes/20 |
