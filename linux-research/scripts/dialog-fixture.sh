#!/usr/bin/env bash
# Test folders for the dialog checks: Cyrillic and spaces in every name. Runs INSIDE the GUI container.
# Prints the start folder.
set -euo pipefail
ROOT=/tmp/dlg
rm -rf "$ROOT"; D="$ROOT/Пресеты с пробелами"
mkdir -p "$D/подпапка два"
printf 'old' > "$D/старый пресет.fxp"
printf 'n' > "$D/notes.txt"
printf 'w' > "$D/other.wav"
echo "$D"
