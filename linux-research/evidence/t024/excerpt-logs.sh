#!/usr/bin/env bash
# Replace validator/ASan logs over 20 KB in the given dirs by <name>.excerpt.txt and move the full logs to <archive>.
# Usage: excerpt-logs.sh <archive-dir> <evidence-dir>...
set -euo pipefail
ARCHIVE="$1"; shift
for d in "$@"; do
  find "$d" -name '*.txt' ! -name '*.excerpt.txt' -size +20k | while read -r f; do
    out="${f%.txt}.excerpt.txt"
    {
      echo "# excerpt of $(basename "$f") ($(wc -l < "$f") lines): test progression, Result lines, ASan reports, last 30 lines"
      grep -E 'Info:  ===|Result: [0-9]+ tests|tests run' "$f" || true
      grep -E -A40 'ERROR: AddressSanitizer' "$f" | cut -c1-300 || true
      echo '# --- last 30 lines'
      tail -30 "$f" | cut -c1-300
    } > "$out"
    mkdir -p "$ARCHIVE/$(dirname "$f")"
    mv "$f" "$ARCHIVE/$f"
  done
done
