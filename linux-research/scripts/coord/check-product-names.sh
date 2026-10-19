#!/usr/bin/env bash
# Before every merge into hvoya-linux: iPlug2's shipped text must not name a plug-in (AGENTS §2, T-027).
# Searches the tree at <rev> (default HEAD) outside the work area and third-party code; exit 1 on any hit.
# Usage: check-product-names.sh [rev] [repo dir, default: this repo]
# Extend NAMES when a new product appears. "HvoyaAudio" (the manufacturer in the fork's example configs) is not
# a plug-in name and is not searched for.
set -uo pipefail
REV="${1:-HEAD}"
REPO="${2:-$(git -C "$(dirname "$0")" rev-parse --show-toplevel)}"
NAMES='gneiss|sand|melter|dmt|crust|scape'
hits=$(git -C "$REPO" grep -n -I -i -w -E "$NAMES" "$REV" -- . \
  ':!linux-research' ':!Dependencies' ':!WDL' ':!hvoya' 2>/dev/null)
if [ -n "$hits" ]; then
  echo "Plug-in names in iPlug2's shipped text ($REV):"
  echo "$hits"
  exit 1
fi
echo "check-product-names: clean ($REV)"
