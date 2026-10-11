#!/usr/bin/env bash
# Static checks for a built plugin/app: layout, exported symbols, deps, GLIBC/GLIBCXX floor.
# Runs INSIDE the container. Usage: inspect-binary.sh <X.vst3|X.clap|app-binary> ...
set -uo pipefail
for P in "$@"; do
  echo "################ $P"
  if [ -d "$P" ]; then
    echo "--- bundle layout"; (cd "$P" && find . | sort)
    SO=$(find "$P/Contents" -name '*.so' | head -1)
  else
    SO="$P"
  fi
  [ -n "$SO" ] && [ -f "$SO" ] || { echo "NO BINARY"; continue; }
  echo "--- file"; file -L "$SO"
  echo "--- size"; stat -c '%s bytes' "$SO"
  echo "--- exported (defined dynamic) symbols: $(nm -D --defined-only "$SO" | wc -l)"
  nm -D --defined-only "$SO" | awk '{print $NF}' | grep -vE '^_(init|fini|edata|end|_bss_start)$|^__bss_start$|^_edata$|^_end$' | head -40
  echo "--- NEEDED"; objdump -p "$SO" | awk '/NEEDED/{print $2}'
  echo "--- ldd (not found only)"; ldd "$SO" | grep 'not found' || echo "(all resolved)"
  echo "--- GLIBC floor: $(objdump -T "$SO" | grep -oE 'GLIBC_[0-9.]+' | sort -Vu | tail -1)"
  echo "--- GLIBCXX refs: $(objdump -T "$SO" | grep -oE 'GLIBCXX_[0-9.]+' | sort -Vu | tail -1 || true) (empty = static libstdc++)"
  echo "--- undefined symbols needing a provider (sample)"; nm -D --undefined-only "$SO" | grep -vE 'GLIBC|@' | head -10
done
