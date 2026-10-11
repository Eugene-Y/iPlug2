#!/usr/bin/env bash
# Run a command inside the iplug2-linux container with the repo mounted at its host path.
# Usage: dock.sh [-i image] [-p platform] [-n name] [-d] -- cmd args...
#   -d  detached (prints container id); default is interactive-less foreground run
# Env passthrough: anything in DOCK_ENV="A=1 B=2".
set -euo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
REPO="$(cd "$HERE/.." && pwd)"
IMAGE="${IMAGE:-iplug2-linux:24.04}"
PLATFORM=""; NAME=""; DETACH=""
while [ $# -gt 0 ]; do
  case "$1" in
    -i) IMAGE="$2"; shift 2;;
    -p) PLATFORM="--platform $2"; shift 2;;
    -n) NAME="--name $2"; shift 2;;
    -d) DETACH="-d"; shift;;
    --) shift; break;;
    *) break;;
  esac
done
ENVARGS=()
for kv in ${DOCK_ENV:-}; do ENVARGS+=(-e "$kv"); done
# When REPO is itself a linked worktree, nested worktrees' .git links point into the main checkout's git dir.
MOUNTS=(-v "$REPO:$REPO")
GITCOMMON="$(git -C "$REPO" rev-parse --path-format=absolute --git-common-dir 2>/dev/null || true)"
case "$GITCOMMON" in
  ""|"$REPO"/*) ;;
  *) MOUNTS+=(-v "$GITCOMMON:$GITCOMMON") ;;
esac
# no core dumps (a REAPER crash otherwise drops ~580 MB into the cwd); shm + ptrace for gdb; repo mounted at the identical path so worktree .git links resolve.
exec docker run --rm --init $DETACH $NAME $PLATFORM \
  --ulimit core=0 --shm-size=512m --cap-add=SYS_PTRACE --security-opt seccomp=unconfined \
  "${MOUNTS[@]}" -w "$HERE" ${ENVARGS[@]+"${ENVARGS[@]}"} \
  "$IMAGE" "$@"
