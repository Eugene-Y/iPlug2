#!/usr/bin/env bash
# Copy cached SDKs into <worktree>/Dependencies/IPlug (what the download-*.sh scripts would do).
# Usage: install-sdks.sh <worktree>
set -euo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
WT="$(cd "$1" && pwd)"
DEST="$WT/Dependencies/IPlug"
"$HERE/scripts/fetch-sdks.sh" >/dev/null
for sdk in VST3_SDK CLAP_SDK CLAP_HELPERS; do
  # The placeholder dirs in the repo only hold a README; replace their contents.
  mkdir -p "$DEST/$sdk"
  rsync -a --exclude .git "$HERE/cache/sdks/$sdk/" "$DEST/$sdk/"
done
cp "$HERE/cache/sdks/VERSIONS.txt" "$DEST/SDK_VERSIONS.linux-research.txt"
echo "SDKs installed into $DEST"
