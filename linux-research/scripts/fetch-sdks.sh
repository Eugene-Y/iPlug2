#!/usr/bin/env bash
# Download plugin SDKs once into linux-research/cache/sdks (pinned refs), then
# install-sdks.sh copies them into a worktree's Dependencies/IPlug.
set -euo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
SDKS="$HERE/cache/sdks"
VST3SDK_TAG="${VST3SDK_TAG:-v3.8.1_build_84}"
CLAP_TAG="${CLAP_TAG:-main}"
CLAP_HELPERS_REF="${CLAP_HELPERS_REF:-main}"
mkdir -p "$SDKS"; cd "$SDKS"

if [ ! -d VST3_SDK ]; then
  git clone --branch "$VST3SDK_TAG" --depth 1 https://github.com/steinbergmedia/vst3sdk.git VST3_SDK
  (cd VST3_SDK && git submodule update --init --depth 1 pluginterfaces base public.sdk cmake vstgui4)
fi
[ -d CLAP_SDK ] || git clone --branch "$CLAP_TAG" --depth 1 https://github.com/free-audio/clap.git CLAP_SDK
[ -d CLAP_HELPERS ] || git clone --branch "$CLAP_HELPERS_REF" --depth 1 https://github.com/free-audio/clap-helpers.git CLAP_HELPERS

{
  echo "VST3_SDK     $VST3SDK_TAG $(git -C VST3_SDK rev-parse HEAD)"
  echo "CLAP_SDK     $CLAP_TAG $(git -C CLAP_SDK rev-parse HEAD)"
  echo "CLAP_HELPERS $CLAP_HELPERS_REF $(git -C CLAP_HELPERS rev-parse HEAD)"
} | tee "$SDKS/VERSIONS.txt"
