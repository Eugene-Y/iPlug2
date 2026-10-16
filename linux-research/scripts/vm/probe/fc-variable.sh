#!/usr/bin/env bash
# Fonts by name on a variable-only family (Ubuntu Sans on Ubuntu 24.04): what FindSystemFont offers for Normal, Bold
# and Italic as installed, and with a fixture that adds a static bold face to the same family (a copy of a static bold
# file whose family fontconfig renames at scan time), which Bold must then take instead of the variable font.
# Usage: fc-variable.sh <out> <fc-probe binary> [family="Ubuntu Sans"] [static-bold=/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf]
set -uo pipefail
OUT="$1"; PROBE="$2"; FAMILY="${3:-Ubuntu Sans}"; BOLD="${4:-/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf}"
mkdir -p "$OUT"; R="$OUT/RESULT.txt"; : > "$R"
log() { echo "$*" | tee -a "$R"; }
log "== installed: $(fc-list ":family=$FAMILY" file variable | sort | tr '\n' ' ')"
"$PROBE" "$FAMILY" | tee -a "$R"
FIX=$(mktemp -d)
cp "$BOLD" "$FIX/fixture-static-bold.ttf"
cat > "$FIX/fonts.conf" <<EOF
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">
<fontconfig>
  <include ignore_missing="yes">/etc/fonts/fonts.conf</include>
  <dir>$FIX</dir>
  <cachedir>$FIX/cache</cachedir>
  <match target="scan">
    <test name="file"><string>$FIX/fixture-static-bold.ttf</string></test>
    <edit name="family" mode="assign" binding="same"><string>$FAMILY</string></edit>
  </match>
</fontconfig>
EOF
log "== fixture: $BOLD as a static bold of \"$FAMILY\": $(FONTCONFIG_FILE=$FIX/fonts.conf fc-list ":family=$FAMILY" file weight variable | sort | tr '\n' ' ')"
FONTCONFIG_FILE="$FIX/fonts.conf" "$PROBE" "$FAMILY" | tee -a "$R"
rm -rf "$FIX"
