# T-003 review 3 — handover audit: claims vs evidence, process (independent subagent, 2026-10-14)

Scope: whether the task file's Log and ticked acceptance items are backed by the evidence and repo state, and whether the AGENTS.md workflow was followed. Report only; the auditor changed nothing. Verbatim report below.

---

## T-003 handover audit (report only; nothing in any repo was changed)

**What I inspected:**
- AGENTS.md, tasks/README.md, the T-003 task file.
- Every file in `evidence/t003/`: RESULT/run.log/xact logs, the churn, hostloop and validator outputs, inspect-*.txt, and screenshot md5s. I looked at these images myself: CLAP r01, g04b, g05, g06, r02, stale g03; VST3 g03, g06, g06-morph-toggle-back; APP a00, a02, a03; contact sheet.
- Build-output mtimes in `linux-research/build/t003-*`, compared with commit times and source-file mtimes.
- `git log/reflog/merge-tree` in the T-003 worktree, the Gneiss-linux repo, and both main checkouts.
- Locks, `docker ps -a`, and the session scratchpad (`mac/xc-*.log`, `abtest/`). I ran the abtest binary.

### Discrepancies, most important first

1. **"Morph off/on (RemoveAllControls + resize) PASS in CLAP, VST3, APP": false in the final REAPER runs.**
   - In the final step file, MORPH is toggled back on *after* the `>` preset load.
   - `reaper-clap-steps/g06-morph-on.png` (21:21) and `reaper-vst3-steps/g06-morph-on.png`: MORPH is lit, but the editor stays 200 px with no morph pad. `r02-after-reopen.png` stays desynced too.
   - The harness passes only because `expect-change` checks the top 940×200, where a tooltip changed. The step-file comment "grows back to 620" is not true for this sequence. This is finding 2 (desync after a preset load) showing up in the smoke test.
   - Morph off→on growth is shown only by:
     - APP `a02` → `a03`;
     - stale leftovers from earlier runs that had no preset step: `reaper-vst3-steps/g06-morph-toggle-back.png` (20:46) and `reaper-clap-steps/g06-morph-toggle-back.png` (20:13).
   - Suggested fix: reword to "morph off→on resize works (APP a03; earlier VST3 run 20:46); after a preset load MORPH needs extra clicks (finding 2), so g06 in the final CLAP/VST3 runs stays at 200". Either fix `gneiss.txt` (toggle before `>`, or check the 620 rect) or note the harness false-positive.

2. **"Context menu (stand-in) PASS in VST3": no evidence; the evidence contradicts it.**
   - `reaper-vst3-steps` ran at 20:52 with an **older step file**: right-click before morph-off, while knobs are disabled.
   - The binary was built before the menu-font fix (`IGraphicsLinux.cpp` last modified 21:14:28; final VST3 `.so` built 21:15:19).
   - `g03-context-menu.png` and `g03b-menu-dismissed.png` are byte-identical (same md5): no menu was drawn.
   - Missing run: `run-gui-test.sh` VST3 with the committed `gneiss.txt` on the final binary.

3. **"PASS in … APP" overstates what was run.**
   - `gneiss-app.txt` (and `app/xact.log`) covers only knob drag + morph toggle twice. There is no menu, no os-cycle and no preset switch, although Step 5 asks for a menu and a preset switch.
   - The APP run (20:41) used a binary from before the final `IGraphicsLinux.cpp` (final APP binary 21:16:06, never run).
   - There is no `app/RESULT.txt`, and stderr shows `EXIT=143` (killed by the harness; fine).
   - In `a02`/`a03` the 620 px window runs off the bottom of the 1000 px Xvfb screen (cosmetic).
   - Suggested fix: reword to "APP: knob + morph resize only" and/or extend the step file and re-run on the final binary.

4. **Stale runs relative to the last code change (`IGraphicsLinux.cpp` at 21:14:28, committed in 545d8a03a).**
   - Churn 20× CLAP/VST3 ran at 20:35 and 20:37, churn 100× at 20:40. The trace build (hostloop-*) was built at 20:38–20:39 and run at 20:42–20:44.
   - The self-review says the menu-font flag "follows the GL context". That touches the open/close path, so "no crash in 20× open/close" and "trace clean" are not shown on the committed code.
   - The churn-clap run may even predate 4d08f1ef5 (committed 20:34:01, run finished 20:35:54).
   - Only the final CLAP steps (21:21), the debug CLAP steps (21:18), IPlugControls regress (21:23–21:32) and the validators (21:37) post-date it.
   - Suggested fix: re-run churn 20× (CLAP+VST3) and the trace build on HEAD, or state in the Log which build each run used.
   - Also, `churn.txt` doesn't record N. The script default is 100, so "20×" can't be checked from the evidence.

5. **The macOS claims have no evidence in the repo.**
   - "mac IPlugEffect CLAP Debug builds" and "mac APP fails identically on plain hvoya-linux" are backed only by logs in the session scratchpad: `…/14c8073c-…/scratchpad/mac/xc-CLAP.log` (BUILD SUCCEEDED, 21:32, HEAD code), `xc-APP.log` and `xc-APP-base.log` (both BUILD FAILED, 20 identical Cocoa-include errors).
   - The "base" build ran in the T-003 worktree, apparently with `IPlugAPP.cpp` swapped in place (its mtime is 21:32:47, the same second as the base log). The reflog shows no checkout of hvoya-linux, so "plain hvoya-linux" means "the T-003 tree with the hunk removed".
   - That swap was not preceded by a snapshot. The only stash entry, "T-003: before restoring xcodebuild-regenerated main.rc_mac_dlg", is at 21:32:55. The risk is low because the content was committed.
   - The AudioBuffer::reserve mac test exists only as `scratchpad/abtest/t.cpp`; no output was recorded. I ran it and it prints "STALE (still the freed allocation)", so the claim is true.
   - Suggested fix: copy the BUILD tail lines and the abtest source+output into `evidence/t003/mac/`, because the scratchpad is ephemeral.

6. **Evidence durability and mixed runs.**
   - Every screenshot except `contact-t003.png` is gitignored and will be lost when the coordinator removes the worktree.
   - `reaper-clap-steps/` and `reaper-vst3-steps/` mix runs with different step files. For example, `g02-os-menu`, `g03-context-menu`, `g04-next-preset`, `g05-morph-toggle` and `g06-morph-toggle-back` are from 20:02–20:49, and the stale CLAP `g03-context-menu.png` shows only a tooltip, no menu.
   - The contact sheet doesn't show a VST3 menu, an APP menu, or morph-on in REAPER.
   - Suggested fix: delete the stale PNGs, or keep and name them as earlier runs. Consider adding a VST3 menu tile and a morph-pad tile to the contact sheet.

7. **Small text issues.**
   - The Status line says it "merges cleanly into hvoya-linux at 6b14ca1d9". hvoya-linux is now 269cbe1d7; `git merge-tree` against it is still clean, so update the hash.
   - The "Open: T-002 also edits IGraphicsLinux.cpp; expect a trivial merge" line is stale: T-002 is already merged (6b14ca1d9) and the merge is clean.
   - Self-review: the Log says "fresh pass" but doesn't say whether a separate subagent did it. hvoya-linux's AGENTS.md (269cbe1d7) now makes an independent subagent mandatory; this audit may serve, so record it.
   - Gneiss 7e3a1c9 was committed at 21:36, after every 24.04 run. `Gneiss.cpp`'s mtime is 19:40:34, so all builds included it. Fine, but worth one line in the Log.
   - Linux IPlugEffect evidence (`build-iplugeffect-24.04.log`, 19:39) predates the IGraphicsLinux commits. IPlugControls (21:17) covers that compile, so this is low priority.

### Claims verified OK
- **Builds:** 24.04 CLAP, VST3 and APP PASS (`build-24.04.log`); 22.04 CLAP and VST3 PASS (`build-22.04.log`). 22.04 APP was not required.
- **Exports:** `clap_entry`; `GetPluginFactory`, `ModuleEntry`, `ModuleExit`. NEEDED is libX11 + libc/libm only, with no GLIBCXX.
- **GLIBC floor:** 2.35 on 22.04 (`inspect-22.04.txt`), and `hypot@GLIBC_2.35` / `hypotf@GLIBC_2.35` are confirmed with nm. The examples' 2.34 matches notes/08. 24.04 is 2.38.
- **VST3 validator:** "537 tests passed, 0 failed", on the final 24.04 binary.
- **clap-validator:** 44 run, 31 pass, 7 fail, 6 skip. The 7 failures are exactly the ones listed.
- **ASan reports:** the heap-use-after-free in `Oversampler::upsample`/`processChunk` and the null SEGV in `NChanDelayLine` via `PassThroughBuffers` are as described.
- **CLAP REAPER final run (21:21, final binary):** knob, os-cycle, context menu (`g04b` shows the "preFlt A hz / MIDI CC" stand-in menu with text), `>` loads "1 : CLEAN MOOG" and REAPER sees the new values, reopen ×5 and 2 instances. All draws are on tid 1.
- **Debug CLAP run:** PASS, menu included.
- **VST3 after `>`:** REAPER reports the old values (finding 4).
- **UI renders:** the CLAP, VST3 and APP renders look right by eye (fonts, MDI icons, logo, morph pad at 620 on first open). The `gdb-crash.txt` files are SIGINT snapshots, not crashes.
- **Hostloop trace:** every `tid`/`lwp` equals the REAPER main pid, in every phase, for both CLAP and VST3. Latency `pdc 0`.
- **Churn numbers:** fds, threads and sockets return to baseline; RSS is flat over 100×.
- **IPlugControls regression:** CLAP and VST3 PASS, APP xact log has 0 fails, all on the 21:17 builds.
- **Commits:** all 7 iPlug2 commits and both Gneiss commits carry the exact trailer. Hashes match the Log. Gneiss diff vs d8e95fd is only CMakeLists.txt, Gneiss.cpp and GneissUI.cpp.
- **Nothing pushed:** linux/T-003 has no upstream, and Gneiss linux-experiment has no upstream.
- **No Dependencies/ or WDL/ files committed.** The `VST3_SDK/README.md` change (mtime Oct 10) is uncommitted and in no commit.
- **Locks and containers:** the T-003 lock is released (only T-007 holds one). No T-003 containers (only t007/t007b) and no orphaned processes. The worktree is left in place.
- **Main checkouts untouched:**
  - IPlug2 main is on hvoya at a94544ddf and clean. Its files dated today come from pre-task commits at 13:10 and 16:05.
  - Gneiss main is on master at d8e95fd. It holds only the author's own edits, dated Oct 9 and earlier; `.git/objects` changes come from the shared object store.
- **Evidence size:** about 2.7 MB tracked (108 files, of which `contact-t003.png` is 1.6 MB and the VST3 validator log 190 KB); about 20 MB on disk, mostly ignored PNGs. Reasonable.

### Verdict
The handover is mostly accurate on builds, exports, GLIBC, validators, the CLAP interaction run, regression and process hygiene. It is **not accurate** on the interaction summary:
- "PASS in CLAP, VST3, APP" overstates it: there is no working menu in VST3, the APP covers only knob + morph, and morph-on after a preset fails in both REAPER runs despite the harness PASS.
- The churn and trace runs predate the last `IGraphicsLinux.cpp` change.
- The mac evidence exists only in the ephemeral scratchpad.

Before merge, I recommend:
- correcting the Log wording (items 1–3, 7);
- re-running VST3 steps, APP (with a menu and a preset step), churn 20× and the trace build on HEAD;
- saving the mac logs and the abtest into `evidence/t003/mac/`.
