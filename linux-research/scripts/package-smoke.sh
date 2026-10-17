#!/usr/bin/env bash
# Smoke-test a release archive (Scripts/linux/package.sh) on a clean distro, from the HOST (macOS).
# Usage: package-smoke.sh <archive.tar.gz> <outdir> [image=iplug2-rt:ubuntu22.04]
# The image is a runtime-only one (docker/runtime/Dockerfile); x86_64 archives need an -amd64 image.
# The archive (and its _debug-symbols twin) must live under the repo, which is what the container sees.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ARCHIVE="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"; OUT="$2"; IMAGE="${3:-iplug2-rt:ubuntu22.04}"
C="t021-smoke-$(echo "$IMAGE" | tr -c 'a-zA-Z0-9\n' '-')"
mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"
PLATFORM=linux/arm64; EMULATED=0
[[ "$IMAGE" == *-amd64 ]] && { PLATFORM=linux/amd64; [ "$(uname -m)" = arm64 ] && EMULATED=1; }
IMAGE="$IMAGE" DOCK_PLATFORM="$PLATFORM" "$HERE/gui-up.sh" "$C" >/dev/null
trap 'docker rm -f "$C" >/dev/null 2>&1' EXIT
"$HERE/gx.sh" "$C" env SMOKE_EMULATED=$EMULATED scripts/package-smoke-in.sh "$ARCHIVE" "$OUT" 2>&1 | tee "$OUT/smoke.log"
grep -q 'RESULT: PASS' "$OUT/smoke.log"
