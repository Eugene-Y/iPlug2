#!/usr/bin/env bash
# CLIPBOARD between the editor's text entry and GNOME apps (zenity = GTK4 entry, gnome-text-editor = GTK4 text view),
# both directions, then what is left for other apps once the copying editor has closed.
# Usage: gnome-clip.sh <out> "<FMT: IPlugControls>" <plugin>
set -uo pipefail
OUT="$1"; FX="$2"; PLUG="$3"; NAME="${FX#*: }"
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$OUT"; R="$OUT/RESULT.txt"; : > "$R"
log() { echo "$*" | tee -a "$R"; }
FAILS=0; check() { if grep -Eqx "$3" <<<"$2"; then log "ok   $1: '$2'"; else log "FAIL $1: '$2', expected '$3'"; FAILS=$((FAILS + 1)); fi; }
"$HERE/reaper-up.sh" "$PLUG"; RPID=$(cat /tmp/reaper.pid)
rpr() { timeout 15 "$HERE/rpr.sh" "$1"; }
rpr "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); reaper.TrackFX_AddByName(t,\"$FX\",false,-1); reaper.TrackFX_Show(t,0,3)" >/dev/null
sleep 2.5
fxwin() { for w in $(xdotool search --onlyvisible --name "$NAME.*Track 1" 2>/dev/null); do xprop -id "$w" WM_CLASS | grep -q '"REAPER"' && echo "$w"; done | tail -1; }
wmctrl -i -r "$(fxwin)" -e 0,420,260,-1,-1; sleep 1
read -r OX OY < <(xwininfo -id "$(fxwin)" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3] } } END { print r }')
gain() { rpr "local ok,s=reaper.TrackFX_GetFormattedParamValue(reaper.GetTrack(0,0),0,0,''); out((s:gsub('[ %%].*','')))"; }
edit() { xdotool mousemove $((OX + 941)) $((OY + 437)) click --repeat 2 --delay 80 1; sleep 1; }
win() { for i in $(seq 50); do w=$(xdotool search --onlyvisible --name "$1" 2>/dev/null | tail -1); [ -n "$w" ] && { echo "$w"; return; }; sleep 0.2; done; }
focus() { wmctrl -i -a "$1"; sleep 0.8; }
clip() { timeout 3 xclip -o -selection clipboard 2>/dev/null; }

log "clipboard managers running: $(pgrep -l -f 'clipboard|gpaste|clipit|parcellite|copyq|diodon' | grep -v gnome-clip | tr '\n' ' ')"
log "--- zenity (GTK4) -> editor"
zenity --entry --title=clip-src --text=src --entry-text=37 >/dev/null 2>&1 &
w=$(win '^clip-src$'); focus "$w"; xdotool key ctrl+a ctrl+c; sleep 0.5
check "xclip reads zenity's copy" "$(clip)" '37'
edit; xdotool key ctrl+v; sleep 0.5; xdotool key Return; sleep 0.7
check "gain after Ctrl+V of zenity's text" "$(gain)" '37(\.00)?'
import -window root "$OUT/g01-zenity-to-editor.png"
wmctrl -i -c "$w"; sleep 0.5

log "--- editor -> zenity"
edit; xdotool key ctrl+c; sleep 0.3; xdotool key Escape; sleep 0.5
zenity --entry --title=clip-dst --text=dst > "$OUT/zenity-dst.txt" 2>/dev/null &
w=$(win '^clip-dst$'); focus "$w"; xdotool key ctrl+v; sleep 0.5
import -window root "$OUT/g02-editor-to-zenity.png"
xdotool key Return; sleep 1
check "zenity entry after Ctrl+V of the editor's copy" "$(cat "$OUT/zenity-dst.txt")" '37(\.00)?'

log "--- gnome-text-editor (GTK4) -> editor"
GTE_STATE="$HOME/.local/share/org.gnome.TextEditor"; GTE_HAD_STATE=0; [ -e "$GTE_STATE" ] && GTE_HAD_STATE=1
gnome-text-editor --standalone --new-window >/dev/null 2>&1 &
GTE=$!
w=$(win 'Text Editor$'); [ -n "$w" ] || w=$(win 'Untitled'); log "gnome-text-editor window: '$(xdotool getwindowname "$w" 2>/dev/null)'"
focus "$w"; xdotool type --delay 40 58; xdotool key ctrl+a ctrl+c; sleep 0.5
check "xclip reads gnome-text-editor's copy" "$(clip)" '58'
edit; xdotool key ctrl+v; sleep 0.5; xdotool key Return; sleep 0.7
check "gain after Ctrl+V of gnome-text-editor's text" "$(gain)" '58(\.00)?'

log "--- editor -> gnome-text-editor"
edit; xdotool key ctrl+c; sleep 0.3; xdotool key Escape; sleep 0.5
focus "$w"; xdotool key ctrl+a BackSpace ctrl+v; sleep 0.7
import -window root "$OUT/g03-editor-to-gnome-text-editor.png"
xdotool key ctrl+a ctrl+c; sleep 0.5
check "gnome-text-editor text (copied back out) after Ctrl+V" "$(clip)" '58(\.00)?'
# leave nothing behind: empty the buffer, then close; it must not ask to save
xdotool key ctrl+a BackSpace; sleep 0.3; wmctrl -i -c "$w"; sleep 1.5
import -window root "$OUT/g04-gnome-text-editor-closed.png"
# an emptied draft still counts as unsaved: it asks to save. Cancel, quit it, and drop the draft and session it kept.
if kill -0 "$GTE" 2>/dev/null; then log "gnome-text-editor asks to save (expected for a draft)"; xdotool key Escape; sleep 0.5; kill "$GTE" 2>/dev/null; sleep 0.5; fi
[ "$GTE_HAD_STATE" = 1 ] || rm -rf "$GTE_STATE"

log "--- the editor's copy after the editor closed"
edit; xdotool key ctrl+c; sleep 0.3; xdotool key Escape; sleep 0.5
check "xclip while the editor is open" "$(clip)" '58(\.00)?'
rpr "reaper.TrackFX_Show(reaper.GetTrack(0,0),0,2)" >/dev/null; sleep 1
log "xclip after the editor closed: '$(clip)' (TARGETS: $(timeout 3 xclip -o -selection clipboard -t TARGETS 2>&1 | tr '\n' ' '))"
rpr "reaper.TrackFX_Show(reaper.GetTrack(0,0),0,3)" >/dev/null; sleep 2
log "xclip after reopening the editor: '$(clip)'"
log "RESULT: $([ $FAILS = 0 ] && echo PASS || echo "FAIL ($FAILS)")"
pkill -x zenity 2>/dev/null
pkill -f '^/opt/REAPER/reaper -nonewinst' 2>/dev/null
