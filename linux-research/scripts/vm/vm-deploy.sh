#!/usr/bin/env bash
# Install a build's plug-ins and apps on the VM.
#   <Name>.clap (+ <Name>.resources/)  -> <prefix>/.clap/
#   <Name>.vst3/                         -> <prefix>/.vst3/
#   <Name> APP executable (+ resources)  -> <prefix>/iplug-apps/
# Usage: vm-deploy.sh [-p <vm-prefix>] <builddir> [Name...]
#   <builddir>: local CMake build dir (its out/ is used if present); Names restrict what is copied.
#   -p: install under this VM directory instead of $HOME, e.g. -p iplug-builds/trace to stage a variant
#       without overwriting the plug-ins REAPER sees (relative paths are relative to the VM's $HOME).
set -euo pipefail
. "$(dirname "$0")/vm-env.sh"
PREFIX='~'
[ "${1:-}" = -p ] && { PREFIX="$2"; shift 2; }
B="$1"; shift
OUTD="$B"; [ -d "$B/out" ] && OUTD="$B/out"
NAMES=("$@")
want() { [ ${#NAMES[@]} -eq 0 ] && return 0; local n; for n in "${NAMES[@]}"; do [ "$n" = "$1" ] && return 0; done; return 1; }

CLAP=(); VST3=(); APP=()
for f in "$OUTD"/*; do
  n=$(basename "$f"); base="${n%.*}"
  case "$n" in
    *.clap) want "$base" && CLAP+=("$f") && { [ -d "$OUTD/$base.resources" ] && CLAP+=("$OUTD/$base.resources") || true; } ;;
    # CMake creates a bundle skeleton for every example at configure time; only built ones hold a .so
    *.vst3) want "$base" && [ -n "$(find "$f/Contents" -name '*.so' 2>/dev/null)" ] && VST3+=("$f") ;;
    *.*) ;;
    *) [ -f "$f" ] && [ -x "$f" ] && want "$n" && APP+=("$f") && { [ -d "$OUTD/$n.resources" ] && APP+=("$OUTD/$n.resources") || true; } ;;
  esac
done
vm "mkdir -p $PREFIX/.clap $PREFIX/.vst3 $PREFIX/iplug-apps"
dest() { [ "$PREFIX" = '~' ] && echo "$VM_HOST:$1" || echo "$VM_HOST:$PREFIX/$1"; }
[ ${#CLAP[@]} -gt 0 ] && vm_rsync --delete "${CLAP[@]}" "$(dest .clap/)"
for v in ${VST3[@]+"${VST3[@]}"}; do vm_rsync --delete "$v" "$(dest .vst3/)"; done
[ ${#APP[@]} -gt 0 ] && vm_rsync "${APP[@]}" "$(dest iplug-apps/)"
printf 'deployed to %s:%s\n' "$VM_HOST" "$PREFIX"
printf '  %s\n' ${CLAP[@]+"${CLAP[@]##*/}"} ${VST3[@]+"${VST3[@]##*/}"} ${APP[@]+"${APP[@]##*/}"}
