#!/usr/bin/env bash
# Before every merge into hvoya-linux: iPlug2's shipped text must not name a plug-in (AGENTS.md, "No product names").
# Searches the committed tree at <rev> (default HEAD) outside the work area and third-party code.
# Exit 0 clean, 1 on a hit, 2 when git grep itself failed (bad rev or repo: nothing was checked).
# Usage: check-product-names.sh [rev] [repo dir, default: this repo]
# Extend the lists when a new product appears. "HvoyaAudio" (the manufacturer in the fork's example configs) is not
# a plug-in name and is not searched for.
set -u
REV="${1:-HEAD}"
REPO="${2:-$(git -C "$(dirname "$0")" rev-parse --show-toplevel)}"
DISTINCT='gneiss|melter'         # also inside identifiers (GneissUI, kGneiss)
WORDS='sand|dmt|crust|scape'     # ordinary words or short names: whole words only
SCOPE=(-- . ':!linux-research' ':!Dependencies' ':!WDL' ':!hvoya')

run() { git -C "$REPO" grep -n -I -i "$@" "$REV" "${SCOPE[@]}"; }
out1=$(run -E "$DISTINCT"); rc1=$?
out2=$(run -w -E "$WORDS"); rc2=$?
if [ $rc1 -gt 1 ] || [ $rc2 -gt 1 ]; then
  echo "check-product-names: git grep failed for '$REV' in $REPO, nothing was checked" >&2
  exit 2
fi
if [ $rc1 -eq 0 ] || [ $rc2 -eq 0 ]; then
  echo "Plug-in names in iPlug2's shipped text ($REV):"
  printf '%s\n' "$out1" "$out2" | sed '/^$/d'
  exit 1
fi
echo "check-product-names: clean ($REV)"
