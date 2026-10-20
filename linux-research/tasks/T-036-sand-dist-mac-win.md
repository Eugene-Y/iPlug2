# T-036 — Sand: makedist-mac.sh and makedist-win.ps1 like Gneiss'

- **Status:** ready
- **Owner:** —
- **Branch/worktree:** Sand repo, branch `dist-scripts` from Sand `master`, worktree `/Volumes/Storage/code/hvoya/Sand-dist`.
  (Not an iPlug2 branch. The coordinator merges into Sand `master` only on the user's command.)
- **Conflicts:** none. Never touch `/Volumes/Storage/code/hvoya/Sand` (the user's main checkout) or `Sand-linux`.

## Why
Gneiss has `scripts/makedist-mac.sh` and `scripts/makedist-win.ps1` that build every shipped format and stage them,
with docs, into `/Volumes/Storage/code/hvoya/distr/Gneiss/Gneiss_v<version>_<os>_<edition>/` (+ optional zip).
Sand's mac/win releases are assembled by hand (`/Volumes/Storage/code/hvoya/distr/Sand/Sand_v1.1.10_mac.zip`: Sand.clap,
Sand.vst3, Sand.component; `…_win.zip`: Sand.clap, Sand.vst3). Sand now has `scripts/makedist-linux.sh` on
`linux-experiment` (aa3e46d) publishing to `distr/Sand/`.

## Deliverables
- `scripts/makedist-mac.sh`: Release VST3 + AUv2 + CLAP via `projects/Sand-macOS.xcodeproj`, staged into
  `distr/Sand/Sand_v<version>_mac/` (matching the existing hand-made naming), `--zip` makes `Sand_v<version>_mac.zip`,
  `--clean`, `--no-install`/install like Gneiss'. **Sand has no free/paid editions** — drop all edition logic.
  Packaging must never install into the machine's plug-in folders unless asked (Gneiss' pattern: own SYMROOT/OBJROOT/DSTROOT).
  No docs are shipped today; keep a `DOCS=()` hook but empty (ask in the hand-over whether to ship anything).
- `scripts/makedist-win.ps1`: same for Windows (VST3 + CLAP; AAX is not in the hand-made zips — leave it out, note it),
  modelled on Gneiss' script (`/Volumes/Storage/code/hvoya/Gneiss/scripts/makedist-win.ps1`, read-only reference).
  It cannot be run here: review it line by line against Gneiss' version and Sand's `.vcxproj` names; say so plainly.
- Follow Sand's own CLAUDE.md and comment rules; read Gneiss' scripts first and keep the structure parallel.

## Verification
- mac: run `scripts/makedist-mac.sh --no-install --zip --out <scratch>` (add an `--out` option like the Linux script's;
  default stays `distr/Sand`). Check the bundles are Release, load (`auval -v aufx <code> <mfr>` for the AU; clap-validator
  is not installed on the mac unless T-023 left one in `~/` — look; otherwise `pluginval` if present, else note it),
  and that nothing landed in `~/Library/Audio/Plug-Ins`. Never write into the real `distr/Sand` during testing.
- Build products and generated files must not dirty the worktree (`git status` clean after a run, apart from ignored
  build dirs) — Gneiss' mac build regenerates `resources/main.rc_mac_dlg` and `plug_build_info.hpp`; check Sand's.

## Acceptance
- [ ] Both scripts, mac one run end-to-end into scratch, archives' contents listed in the Log.
- [ ] Independent review (AGENTS §4).

## Log
