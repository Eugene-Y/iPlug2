# T-027 — No plug-in names in iPlug2's docs, README, comments and scripts

- **Status:** review next
- **Owner:** coordinator (chat f91df715), lock `.linux-port-locks/T-027`
- **Branches/worktrees:** iPlug2 `linux/T-027` from `hvoya-linux`, in `/Volumes/Storage/code/IPlug2-T-027`.
- **Depends on:** nothing. T-015 (README) depends on this.
- **Conflicts:** `Documentation/`, `LINUX_PORT_RESEARCH.md`, `CLAUDE.md`; any task that edits docs at the same time.

## The rule (user, 2026-10-18; AGENTS §2)
Plug-ins depend on iPlug2, not the other way round. iPlug2's shipped text — `Documentation/`, `README.md`, code comments, `Scripts/`, `Examples/` — never names a plug-in (Gneiss, Sand, …). A concrete use case is welcome, without the name: "a plug-in built as C++23 with `constexpr` lookup tables" instead of "Sand". A plug-in's own docs may say anything about iPlug2.

Out of scope: commit messages (history is pushed), the `linux-research/` work area, and the user's `hvoya/` utility library (its own product code, not the port's; it names plug-ins in comments — report, don't touch).

## Known hits (2026-10-18, `git grep -i -w -E 'gneiss|sand|melter|dmt|crust|scape'` outside `linux-research/`, `Dependencies/`, `WDL/`)
- `Documentation/linux-development.md:52` — "hvoya's `constexpr` lookup tables (Sand)".
- `LINUX_PORT_RESEARCH.md` — 14 lines. **User (2026-10-19): keep the research docs, inside `linux-research/`** — move this file there (it then falls under the work area, not the shipped docs).
- `CLAUDE.md:71` (Linux-port section, "Gneiss builds against it") — **user (2026-10-19): remove the section when the port work is finished**, not now; but keep a short note that the Linux port was done by this fork's authors, not by iPlug2's maintainers, so nobody mistakes it for upstream work. Until then, take the product name out of it.
- Code under `IPlug/`, `IGraphics/`, `Scripts/`, `Examples/`: none found.

## Steps
1. Re-run the grep (also catch `Gneiss-`, `Sand.clap` etc. — `-w` treats `-` and `.` as boundaries, so they are covered) and look at `IPlug/Linux`, `IGraphics/Platforms/IGraphicsLinux*`, `Scripts/linux`, `Documentation/` by hand for indirect mentions ("the morph plug-in", product-specific param names, paths like `hvoya/Gneiss`).
2. Rewrite each hit as a use case without the name. Keep the technical content.
3. Add a check the coordinator can run before every merge: `linux-research/scripts/coord/check-product-names.sh` (exit non-zero on a hit outside the allowed paths).
4. Independent reviewer (AGENTS §4), then hand over.

## Acceptance
- The grep above returns nothing outside the out-of-scope paths (or only lines the user explicitly kept).
- The check script exists and the coordinator's merge procedure (`COORDINATOR.md`) calls it.

## Log

### 2026-10-19 — done
- `LINUX_PORT_RESEARCH.md` moved to `linux-research/` (user's decision); its link to the work area fixed; `AGENTS.md` and `PLAN.md` point to the new path.
- `Documentation/linux-development.md`: the clangd note says "projects built as C++23 with `constexpr` lookup tables" (no product, no utility-library name); new paragraph near the top: Linux support in this fork is the fork's work, not iPlug2's maintainers' — report Linux problems to the fork.
- `CLAUDE.md` port section: "the fork's plug-ins build against it" instead of the product name, plus "The Linux port is this fork's own work, not upstream iPlug2's." The section itself goes when the port is finished (user); the authorship note stays then (in `Documentation/linux-development.md` it already does).
- Hand search of `IPlug/Linux`, `IGraphics/Platforms/IGraphicsLinux*`, `Scripts/linux`, `Documentation/` for indirect mentions (morph, USER tab, `hvoya/` paths): none.
- Not changed: "HvoyaAudio" in `Examples/IPlugEffect` (the fork's manufacturer name in its example config, on `hvoya` before the port; not a plug-in name). Out of scope as before: `linux-research/`, `hvoya/`, commit messages.
- `linux-research/scripts/coord/check-product-names.sh [rev]`: `git grep -w -i` of the names in the tree at `rev` outside `linux-research`, `Dependencies`, `WDL`, `hvoya`; exit 1 on a hit. On this branch: clean; on hvoya-linux before: the hits above. `COORDINATOR.md`'s merge procedure runs it (step 3).

### Review round 1 (independent reviewer)

Checked: `git log/diff hvoya-linux...linux/T-027`; the script on `linux/T-027` (clean, exit 0) and `hvoya-linux` (the 14+2 known hits, exit 1); a scratch repo for `-w`/pathspec/binary cases; my own greps over the whole tree at `linux/T-027` outside `linux-research`, `Dependencies`, `WDL`, `hvoya` (names without `-w`; `hvoya`, `morph`, `user tab`, `ladder`, `.hvoya`, `paid`, other product dirs under `code/hvoya/`: Bromo, Delta, Scape, Melter, …); every reference to `LINUX_PORT_RESEARCH.md` in the repo, in `code/hvoya/` and the sibling checkouts (none outside `linux-research/`; all in-repo ones correct, the moved file's own link `./` and its repo-root code spans still right); blame/history of "HvoyaAudio".

**Major**
1. **The script reports "clean" when it checked nothing.** `git grep … 2>/dev/null` with `hits=$(…)` cannot tell "no match" (git exit 1) from an error (exit 128): `check-product-names.sh no-such-rev` and `check-product-names.sh linux/T-027 /nonexistent` both print "check-product-names: clean" and exit 0. A typo in the branch name at merge time passes the gate. Fix: keep stderr, branch on git grep's status (0 → hits, exit 1; 1 → clean; else → error, exit 2), or `git rev-parse --verify "$REV^{tree}"` first. Then COORDINATOR step 3 can say "must exit 0" instead of "must print clean".
2. **The authorship note is not accurate as worded.** `Documentation/linux-development.md`: "Linux support in this fork was written by the fork's authors". `hvoya..hvoya-linux` has 11 commits by Benjamin Reed (RangerRick, upstream PR #1336), among them "Add Linux CMake build infrastructure", "Add IPlug core Linux support (partial)", "Wire Linux into APP, VST3, and CLAP plugin formats (partial)"; 025764cd5 also says the Linux paths come from Stillwell Audio's fork. None of this is credited in shipped text. The note's point ("not by iPlug2's maintainers, report here") is right; "written by the fork's authors" claims others' code. Suggested: "Linux support is not part of upstream iPlug2. This fork's authors assembled and maintain it, starting from parts of RangerRick's upstream PR #1336 and Stillwell Audio's fork; iPlug2's maintainers have not reviewed it. Report Linux problems to this fork." `CLAUDE.md` ("this fork's own work, not upstream iPlug2's") is acceptable but would read better as "not upstream iPlug2's work". The user's own decision said "done by this fork's authors", so this is a wording question for the user.

**Minor**
3. **`-w` misses names inside identifiers.** `-` and `.` are boundaries (scratch repo: `Sand.clap`, `Gneiss-linux`, `hvoya/Gneiss/x` are caught; `Examples/hvoya/` is not wrongly excluded; NUL files skipped by `-I`). But `_` and letters are word characters: `GneissUI`, `kGneiss`, `gneiss_plugin` and `SandLinux` all pass. `-w` is needed only for the ordinary words (`sand`, `scape`, `crust`: "sandbox"/"landscape" give hundreds of hits without it). Search the distinctive names (`gneiss`, `melter`, also `bromo`) without `-w`, in a second `git grep`. Today that adds no hits.
4. **`Documentation/linux-development.md:145`: `--variant free:-DHVOYA_IS_PAID=0`.** This is Gneiss's licence-gate flag (T-003/T-010/T-021), added to shipped docs by T-021. It names no plug-in, but it is a product-specific build flag, and it brings back the utility-library name the clangd line just lost. Suggest a neutral example, e.g. `--variant free:-DMYPLUG_FREE_EDITION=1`.
5. **The clangd sentence is now broader than the fact.** The cause (T-012 D2) is a `constexpr` constructor with a non-literal (`std::function`) parameter, which is valid only under C++23's relaxed `constexpr` rules (P2448), and clang implements that from 19. "Projects built as C++23 with `constexpr` lookup tables need clangd 19" suggests that any constexpr table needs it. Suggest: "Projects that rely on C++23's relaxed `constexpr` rules (P2448, e.g. a `constexpr` constructor taking a `std::function`) need clangd 19 or newer". This keeps the technical content (task step 2).
6. **An indirect mention from before the port:** `IPlug/CLAP/IPlugCLAP.cpp:426` "large state (e.g. a full morph rig)". The comment comes from eee9468ee (2026-08-05) and is on `hvoya` as well. This is the user's call; if it is reworded ("e.g. many stored snapshots"), do it on `hvoya` so the branches don't diverge.
7. **NAMES may be incomplete.** `code/hvoya/` also has `Bromo` and `Delta` (and `hemisphere`). There are no hits for them today. Ask the user which are products. `bromo` can go into the list. `delta` can't be grepped (DSP word), so it needs a manual check.

**Nit**
8. Script header: "AGENTS §2" cites a section number from code (the user's global rule: sections renumber, so state the reason). The reason is already in the next words.
9. `set -uo pipefail`: the script has no pipeline, so `pipefail` does nothing.
10. Rev vs. working tree: checking the committed branch tip is the right choice. The merge brings in commits, not the worker's or the coordinator's dirty trees. It does not see a conflict resolution made in step 4, though. A second run on `HEAD` after the merge commit (step 6) closes that gap cheaply.
11. `linux-research/LINUX_PORT_RESEARCH.md:153` "(см. `LINUX_VM_SETUP.md`)": the file now lives one level down, so the bare name reads as `linux-research/LINUX_VM_SETUP.md`. Make it `../LINUX_VM_SETUP.md` or a repo-root path. `LINUX_VM_SETUP.md` itself is a research doc at the repo root with no product names. Moving it is optional and outside this task.

**Correctly left alone:** "HvoyaAudio" (`PLUG_MFR`/`BUNDLE_MFR`, plists, `.RPP`) has been on `hvoya` since f9e793f2f "init WIP" (2025-05-26). It is unchanged by the port (`git diff hvoya...linux/T-027 -- Examples/IPlugEffect/config.h` is empty) and is a manufacturer, not a plug-in. AGENTS also forbids touching `config.h`. The same goes for the `hvoya` paths in `common-*.xcconfig`/`common-win.props` and the `hvoya::` utility use in `Examples/IPlugEffect`, which is library, not product.

**Verdict: changes requested.** Fix 1 and settle 2 with the user. 3–5 are cheap and worth doing in this round. 6–7 are for the user.

### 2026-10-19 (2) — review round 1 fixes
- **M1:** `check-product-names.sh` branches on `git grep`'s exit status: 0 clean, 1 hits, **2 when git grep failed** (bad rev or repo: nothing was checked). Checked: clean on this branch (0), `no-such-rev` → 2, hvoya-linux → 1. `COORDINATOR.md`: "must exit 0", and run it again on `HEAD` after the merge commit.
- **M2:** the authorship note now credits the sources the history shows: 11 commits by Benjamin Reed (upstream PR #1336) and Stillwell Audio's path utilities (025764cd5, 5e23641b8). Wording: "Linux support in this fork is not upstream iPlug2's: it builds on Benjamin Reed's upstream pull request #1336 and Stillwell Audio's Linux work, and was completed in this fork. Report Linux problems to this fork, not upstream." `CLAUDE.md` says the same in one line. This keeps the user's intent (no confusion with upstream's maintainers) and is accurate; **the user may want to word it differently.**
- **m3:** distinctive names (`gneiss|melter`) are searched inside identifiers too; ordinary words (`sand|dmt|crust|scape`) as whole words. No new hits.
- **m4:** `HVOYA_IS_PAID` example in `Documentation/linux-development.md` → `--variant lite:-DMY_PLUGIN_LITE=1`.
- **m5:** clangd: "rely on its relaxed `constexpr` rules (P2448)".
- Nits: the script header no longer cites a section number; no `pipefail`; `LINUX_VM_SETUP.md` linked as `../LINUX_VM_SETUP.md` from the moved file.
- **For the user:** (m6) "a full morph rig" in `IPlug/CLAP/IPlugCLAP.cpp:426` predates the port (on `hvoya`): reword there or keep? (m7) other products in `code/hvoya/` — Bromo, Delta: add them to the names list? ("delta" is also an ordinary word: would be whole-word.)

### Review round 2 (independent reviewer)

Checked at 45d8df693: `git log`/`diff hvoya-linux...linux/T-027`; the script run on several revs and repos and in a scratch repo; my own greps over the tree at `HEAD`; the history behind the authorship note; the clangd claim compiled locally.

**Round-1 findings: all addressed as the Log says.**
- **M1:** exit codes are right. `HEAD` and `linux/T-027` give 0 ("clean"). `hvoya-linux` gives 1 with the 16 known hits (CLAUDE.md:71, LINUX_PORT_RESEARCH.md ×14, linux-development.md:52). `no-such-rev`, `/nonexistent` and `/tmp` (not a repo) give 2: git's `fatal:` stays on stderr, plus "nothing was checked". An empty rev falls back to `HEAD`, and the script also works from another cwd. The logic is sound: a status above 1 from either grep wins over hits. Both greps use `-I -i -n` with the same scope. `:!hvoya` excludes only the top-level dir, so `Examples/hvoya/` is still searched (scratch repo). Scratch-repo results: `GneissUI`, `kGneiss`, `gneiss_plugin`, `MelterX`, `Gneiss-linux`, `Sand.clap`, `DMT`, `Scape.` hit; `sandbox`, `sandbox_sand`, `landscape`, `crusty` don't; a NUL-containing file is skipped. As intended, `-w` applies only to the ordinary words. COORDINATOR step 3 says "must exit 0", and the second run on `HEAD` after the merge commit covers conflict resolutions.
- **M2:** accurate, not misleading. `git log --format=%an hvoya..hvoya-linux`: 11 Benjamin Reed, 297 Eugene Yakshin. 025764cd5 is Reed's partial cherry-pick of PR #1336. 5e23641b8 takes Stillwell's `IPlugLinuxPathUtils.h` and the Linux `IPlugPaths` branch (stillwellaudio 910303d27). The research also credits Stillwell's VST3 run-loop and editor lifecycle as the model. "Builds on … and was completed in this fork" matches this. It also keeps the user's point: not upstream, report here.
- **m3/m4/m5:** done. P2448 is the right paper: P2448R2, "Relaxing some constexpr restrictions" (C++23), lets a `constexpr` function or constructor take non-literal parameter types. Clang lists it as implemented in 19. Local check (clang 23): a `constexpr` constructor taking `std::function` errors under `-std=c++20` ("1st parameter type … is not a literal type") and compiles under `-std=c++23`. This matches T-012 D2. `-DMY_PLUGIN_LITE=1` is neutral.
- **Nits 8–11:** done. The moved file's `../LINUX_VM_SETUP.md` resolves to the repo-root file. Its `notes/…` links resolve inside `linux-research/`, and the `AGENTS.md`/`PLAN.md` paths are right. No other reference to the old path remains.

**Shipped text:** clean. The script passes, and my own greps over `HEAD` outside `linux-research`, `Dependencies`, `WDL` and `hvoya` found nothing. I searched `gneiss|melter|bromo|hemisphere` without `-w`; `sand|dmt|crust|scape` as whole words; `Delta` in Documentation/README/CLAUDE.md/Scripts/Linux platform code/Examples (only minified `p5.min.js`); and `morph`, `user tab`, `ribs`, `yantra`, `HVOYA_`, `paid`. The only remaining indirect mention is the pre-port `IPlugCLAP.cpp:426` "a full morph rig" (m6, on `hvoya` at :415), which waits on the user. `HvoyaAudio`, `HVOYA_PATH` and the hvoya logo in `Examples/IPlugEffect` are the fork's manufacturer and utility library, not plug-ins.

**New, all nits (optional):**
1. `CLAUDE.md:72` is now 169 columns in a paragraph hard-wrapped at ~118. Rewrap it.
2. The script prints a line twice when it matches both greps (e.g. `Sand.clap Gneiss-linux`). Cosmetic; `sort -u` on the output would fix it.
3. `COORDINATOR.md:59` records the user's decision as "this fork's authors did the port, not iPlug2's maintainers". Whoever later removes the `CLAUDE.md` section may copy that wording, which is the one corrected in M2. Add "(worded as in `Documentation/linux-development.md`)".
4. Optional: in linux-development.md, "upstream pull request #1336" could say "unmerged" (it is still open), so nobody reads it as already upstream.
5. Accepted trade-off, so the coordinator knows: an ordinary-word name inside an identifier (`SandLinux`, `kScapeMix`) is not caught. Review by eye covers it.

**Still for the user:** m6 (the morph-rig comment, reword on `hvoya`) and m7 (Bromo/Delta in the names list).

**Verdict: approved.** The nits can be fixed in passing or at merge time; none blocks.
