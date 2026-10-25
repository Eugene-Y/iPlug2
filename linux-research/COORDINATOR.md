# Taking over as coordinator (cold start)

You are a fresh Claude instance taking over the **coordinator** role of the Linux port from another instance (it ran out of limits on 2026-10-17). Nothing from its chat or memory reaches you; everything it knew is in this repo. Read, in this order:
1. `AGENTS.md` — rules for every instance (minimal footprint, tools, workflow with the independent reviewer, locks).
2. `STATUS.md` — **current snapshot**: branches and their state, what waits on the user, every user decision so far, environment gotchas.
3. `tasks/README.md` — the board, the "Priority" section (who does what next), the merge log.
4. This file — how the coordinator works, step by step.
5. On demand: `PLAN.md` §1–2 (binding design decisions), `CROSS_PLATFORM_FINDINGS.md` (mac/win bugs and their status), the task file of whatever you touch.

## The role
- The coordinator is the chat the user talks to about the port as a whole. Other chats ("2nd instance", "the Sand chat") each take one task file, work in their own branch + worktree, get an independent review, and hand over. **The user tells you when a task is done** ("T-0NN готов"); then you merge it.
- You own: `hvoya-linux` (the only one who commits there), the board, `STATUS.md`, merge smoke tests, task files for new work, cleanup of finished worktrees.
- You may also do tasks yourself (planned for the coordinator: see the board). Use **one subagent at a time** and a **mandatory fresh-context reviewer** (AGENTS §4) — the user insisted on both.
- The user's own words are in Russian; answer in Russian, concise. Code, commits, docs: English (except `CROSS_PLATFORM_FINDINGS.md` and `LINUX_PORT_RESEARCH.md`, which are Russian for the user).

## User preferences that apply to this work (the old instance had them in its global config)
- **Discuss vs do.** A question ("что думаешь", "можно ли", "какую задачу дать") is a discussion: answer, recommend, don't change things. An imperative ("вмерджи", "создай задачу", "убирай") is a command: do it. If unsure, say in one line how you read it and continue.
- **Prioritisation and release scope are the user's decision.** Rank and recommend; let the user choose.
- **Ask with a real choice** (AskUserQuestion, options with a recommended first) only when blocked on the user's decision; otherwise pick the conventional option and say so.
- **Anything that leaves the machine asks first. The user pushes; you never push** (the classifier blocks it anyway).
- **Main checkouts** (`/Volumes/Storage/code/IPlug2` on `hvoya`, `/Volumes/Storage/code/hvoya/Gneiss`, `/Volumes/Storage/code/hvoya/Sand`, other product repos): **only on the user's explicit command in the current request** (e.g. "мерджи фиксы в основные ветки Sand и IPlug"). Gneiss' CLAUDE.md additionally says: never commit without an explicit command. When you do touch them: commit only the files the task needs; the user has uncommitted work there (Gneiss: `.RPP`, xcscheme, `main.rc_mac_dlg`; Sand: untracked `Media/`) — never stage, revert or clean it.
- **Snapshot before anything that can discard work** (`git checkout -- <path>`, `restore`, `reset --hard`, `clean`, a scripted rewrite of a file): `git add -A && git stash store -m "before <x>" "$(git stash create)"` — leaves the tree as it is. Never bare `git stash`/`pop` (the stash is shared by all worktrees and instances).
- **Code comments**: only the non-obvious WHY; never restate the code; fix stale comments you read. UI controls: explicit named setters, no decoding of style slots.
- **The user's living text files** (`BACKLOG.md`, todo/idea files, product `CLAUDE.md`s): propose edits, or change them only with the Edit tool and only when asked.
- **Cleanup after a merged task is expected** ("подчисти ненужные артефакты": worktrees, branches, containers, build dirs), but see "Removing a worktree" below.
- The user's plug-ins are sold; licence terms matter (e.g. CLion's free licence is out).

## Merging a finished task (what the old coordinator did every time)
1. **Inventory.** `git -C /Volumes/Storage/code/IPlug2-linux log --oneline hvoya-linux..linux/T-NNN`, `git diff --stat hvoya-linux...linux/T-NNN -- . ':!linux-research/evidence'`, the task file's Log/Hand-over (decisions, shared-code commits, open items), the lock dir `/Volumes/Storage/code/.linux-port-locks/` and `docker ps -a` (the worker should have released/removed its own).
2. **Shared-code check.** Any change that also builds on mac/win needs the user's decision (AGENTS §2) unless the task file records it. Read those diffs yourself.
3. **Secrets / junk check before merging** — the branch's whole history lands in `hvoya-linux`, which the user pushes to GitHub: `git log hvoya-linux..linux/T-NNN --name-only --format= | grep -i -E 'cookie|\.pulse|token|secret|\.ssh|\.pem'`, and look at evidence sizes (`git ls-tree -r -l`). T-004 had three VM PulseAudio cookies in its history: the branch was rewritten before the merge with `git branch backup/T-NNN-before-filter linux/T-NNN; FILTER_BRANCH_SQUELCH_WARNING=1 git filter-branch --index-filter 'git rm -q --cached --ignore-unmatch -- "<glob>"' -- hvoya-linux..linux/T-NNN`, then old SHAs mentioned in docs were updated. Only for unpushed branches.
   **No plug-in names** in iPlug2's shipped text (AGENTS §2): `linux-research/scripts/coord/check-product-names.sh linux/T-NNN` must exit 0 (1 = names found, 2 = nothing checked); run it again on `HEAD` after the merge commit, which also covers conflict resolutions.
4. **Trial merge:** `git merge --no-ff --no-commit linux/T-NNN` in `/Volumes/Storage/code/IPlug2-linux`. Conflicts so far were almost always the board table (`tasks/README.md`): keep both sides' rows. Copy a conflicted file to the scratchpad before resolving.
5. **Smoke test on the trial-merged tree** (Docker; never trust a worker's PASS alone — look at screenshots when a run is visual). Templates from earlier merges: `scripts/coord/merge-smoke-t0NN.sh` (APP: t004; CLAP state: t006; Sand + xact changes: t012; packaging + clean-distro: t021; product fixes, validators, Sand/Gneiss steps: t024) and `scripts/coord/mac-xcodebuild.sh` (mac build of a product from a **temporary detached worktree**, so the build doesn't dirty the main checkout: `git -C <repo> worktree add --detach ../<Name>-verify master`, remove it afterwards). Use your own container and build-dir names (`m0NN`), remove them after.
6. **Commit the merge** with a message that says what landed, the shared-code commits and the smoke result; then a follow-up commit with the merge-log row (`tasks/README.md` "Merge log": date, task, merge sha, smoke summary), board status, `STATUS.md`, `CROSS_PLATFORM_FINDINGS.md` rows for new mac/win findings.
7. **Sync**: when `hvoya` gets commits (user-approved fixes), `git merge --no-ff hvoya` into `hvoya-linux`; when a product's `master` moves, merge it into its local `linux-experiment` worktree (Gneiss-linux, Sand-linux). Re-run the relevant smoke.
8. **Cleanup** (the user expects it): worktree, merged branch (`git branch -d` refuses unmerged ones — good), the worker's containers/locks if left, your `m0NN` build dirs.
9. Tell the user in a few lines: merged sha, smoke result, what waits on them, what is unpushed, what the free tasks are.

### Removing a worktree
Before `git worktree remove --force`, check **`git -C <wt> status --porcelain --ignored`**, not only tracked/untracked: the repo ignores `build-*` and `_*`, so e.g. a helper `build-sand.sh` was never committed and was lost with T-024's worktree (recreated as `scripts/sand-build.sh`). Builds, `cache/`, `Dependencies/` SDKs are regenerable; anything else: save or commit first (`git add -f` for ignored names).

## Useful commands
- Build a product tree (Gneiss/Sand `linux-experiment` or a fix branch) in Docker: `linux-research/scripts/sand-build.sh <tree> <builddir> [release|asan]` (ASan runs need `LD_PRELOAD=$(gcc -print-file-name=libasan.so)` in the container). Or `DOCK_MOUNTS="<tree> <repo>/.git:ro" DOCK_ENV="EXTRA_CMAKE_ARGS=-DIPLUG2_DIR=/Volumes/Storage/code/IPlug2-linux" scripts/dock.sh -- scripts/build-cmake.sh <tree> build/<dir> <targets>`. `DOCK_ENV` splits on spaces.
- Package a product: `Scripts/linux/package.sh <product-tree> [--arch aarch64|x86_64] --out <dir under the repo>`; Gneiss: `IPLUG2_DIR=/Volumes/Storage/code/IPlug2-linux <Gneiss-linux>/scripts/makedist-linux.sh [--only paid|free]`. Smoke an archive on a clean distro: `linux-research/scripts/package-smoke.sh <archive> <outdir> iplug2-rt:ubuntu22.04[-amd64]`.
- Validators in Docker: `clap-validator validate <X.clap>`, `validator -e <X.vst3>`; known failure lists: `scripts/known-clap-failures/{Gneiss,Sand}.txt`.
- REAPER GUI steps: `GUI=<container> scripts/run-gui-test.sh <plugin> <outdir> scripts/steps/<file>.txt <container>`.
- Long jobs: run in the background and wait for the completion notice; don't poll with sleeps. No `timeout` on macOS: `perl -e 'alarm N; exec @ARGV' cmd`.

## Where the coordinator stopped (2026-10-25)
See `STATUS.md` "Waiting on the user". Merged and pushed 2026-10-22..25 (each via worker agent with its own
independent reviewer loop): T-036 (Sand `master` 4669ede), T-022 (other chat), T-032, T-037, T-018 (Skia), T-038 (VST3
fallback-thread notifications), T-013 (complete), T-009 (complete). Gneiss `linux-experiment` 6246fb7 (IDE setup).
- **Left on the board:** T-017 WebView, T-030 multi-touch (uinput on the VM), T-014 LV2, T-015 README (last), T-029
  (waits for the user), T-033 (workflow change, user first). Ideas: GLib main-loop timers (STATUS).
- **Worktrees kept:** T-018 (Skia source + libs for later Skia work), T-022 and T-031 (other chats'), T-029.
- **VM:** needs a re-login (X0 socket deleted by a nested-shell script bug, fixed). Release builds from T-038 in
  `~/.clap`/`~/.vst3`.
- **Agents hit usage limits often:** resume a stopped agent with SendMessage (its work is saved) after checking the
  VM lock and its leftovers.

## Where the coordinator stopped (2026-10-21 day)
See `STATUS.md` "Waiting on the user". Since the night section below:
- **Merged 2026-10-21** (each via worker agent → independent reviewer agent, ≤ 2 rounds): T-013 Renoise (d6a361df0; IGraphicsLinux XEmbed FOCUS_OUT/FOCUS_IN pairing), T-013 drops + T-020 VM checks + PC checklist (c62128b28), T-011 (839e1b51d; `duplicate.py`, shared mac/win, not on `hvoya`), T-031 (cbb4d7105; done and reviewed in another chat, smoke here).
- **Unpushed:** hvoya-linux after 1cd9110fa (everything above), Sand `dist-scripts`.
- **Other chats:** T-022 (APP JACK MIDI) in progress, lock `T-022`.
- **VM state:** `~/.clap/Gneiss.clap` is T-011's Debug build: redeploy before the next VM test. `~/dev/t011/` holds T-011's duplicates for the user to try. Renoise OSC off, colour scheme `prefer-dark`.
- **T-013 left:** Qtractor, Zrythm (install approved in the task's Hosts section), a Wayland session, the final review. T-009's VM run still open (coordinator holds T-009 and T-013 locks).
- **Smoke gotcha:** `reaper-ext-test.sh` runs in the GUI container with cwd `linux-research`: pass `build/<dir>/out/reaper_X.so`, not `../build/...`.

## Where the coordinator stopped (2026-10-21 night, second coordinator, before a context compaction)
See `STATUS.md` "Waiting on the user" for the authoritative list; the user decides those in the morning.
- **Done 2026-10-20:** T-034 (NanoVG GL main-FBO stencil; merged 29022e440, `hvoya` 07953bf26), T-023 (CLAP preset rescan + flush check; merged 4cbaaef7a, `hvoya` b5dc25d0d), shared fixes ported to `hvoya` (CLAP `paramsValueToText` off-by-one 593293cfb, APP A1-A4, IPlugEffect example 5b263fc83/f743742bc), Gneiss `master` b1733b7 / Sand `master` d2294da picks, `makedist-linux.sh` for Gneiss and Sand (publish to `hvoya/distr/<Plugin>/`, `debug-symbols/` subfolder, `--allow-dirty`), T-013 Ardour results (`linux/T-013` f932e7e0c), T-035 merged 6ba2a22bf (real-hardware kit, notes/25-*), T-036 review-done (Sand `dist-scripts` bcb22ec in `/Volumes/Storage/code/hvoya/Sand-dist`, not merged). Everything up to 5cb35ed39 pushed with the user's permission; **unpushed:** hvoya-linux after 1cd9110fa (board/STATUS/T-035 merge/T-036 records, HEAD 0b3fcc9fe+), Sand `dist-scripts`.
- **Review practice (the user asked 2026-10-20):** every task runs worker agent → independent reviewer agent (rounds until approve with no major finding; user 2026-10-22), one subagent at a time; today's earlier unreviewed changes got a retro review (`notes/24-review-2026-10-20.md`, approved). Don't skip the reviewer again, even when the user says "merge".
- **VM:** restarted by the user 2026-10-20; Ardour done; set to the Dummy backend (`~/.config/ardour8/config`, backup `config.t013-alsa`). `mutter-x11-frames` was restarted by hand after `xdotool windowclose` crashed it (STATUS gotcha). Locks held: T-009, T-013 (both coordinator, VM work pending). No VM lock held.
- **Next for the coordinator (the user: "take priority tasks you can do yourself during the night"):** pick from: T-013 (Renoise, drops from GNOME Files, item 9 = T-020's VM checks; needs the VM lock), T-031 (REAPER extensions, 2-3 d), T-011 (duplicate.py, ready), T-009's VM part. Run each through worker + reviewer agents. Don't merge shared-code (mac/win) changes or anything into plug-in `master`s without the user.
- Worktrees: `IPlug2-T-009`, `-T-013`, `-T-029` (T-029 proposal waits for the user), Sand `Sand-dist` (T-036), products `Gneiss-linux`, `Sand-linux`.

