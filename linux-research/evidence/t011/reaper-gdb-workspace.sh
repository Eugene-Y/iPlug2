#!/usr/bin/env bash
# reaper-gdb-workspace.sh <project-dir> <outdir>: inside the GUI container, run REAPER under gdb with the launch args
# and environment of the project's own -linux workspace ("Debug CLAP in REAPER"), break in <Name>::ProcessBlock
# and in any OnMouseDown, click the middle of the editor, log the hits.
set -u
P="$1"; OUT="$2"; N=$(basename "$P"); mkdir -p "$OUT"
export HOME=/tmp/rhome-$N; rm -rf "$HOME"; mkdir -p "$HOME/.clap" "$HOME/.config/REAPER"
B="$P/build-linux/linux-debug/out"
cp -a "$B/$N.clap" "$B/$N.resources" "$HOME/.clap/"
printf '[REAPER]\nlinux_audio_mode=2\nsplashscreen=0\n' > "$HOME/.config/REAPER/reaper.ini"
python3 - "$P/$N-linux.code-workspace" "$P" > "$OUT/launch.txt" <<'PY'
import json, sys
ws = json.load(open(sys.argv[1]))
c = [c for c in ws["launch"]["configurations"] if c["name"] == "Debug CLAP in REAPER"][0]
for e in c["environment"]: print("ENV " + e["name"] + "=" + e["value"])
for a in c["args"]: print("ARG " + a.replace("${workspaceFolder}", sys.argv[2]))
PY
ARGS=(); while IFS= read -r l; do case "$l" in ENV\ *) export "${l#ENV }";; ARG\ *) ARGS+=("${l#ARG }");; esac; done < "$OUT/launch.txt"
cat "$OUT/launch.txt"; for a in "${ARGS[@]}"; do case "$a" in *.lua) ls -la "$a";; esac; done
cat > "$OUT/bp.gdb" <<G
set pagination off
set breakpoint pending on
handle SIGPIPE nostop noprint
break $N::ProcessBlock
commands
  silent
  printf "BP-HIT ProcessBlock thread=%d\n", \$_thread
  bt 3
  disable 1
  continue
end
break OnMouseDown
commands
  silent
  printf "BP-HIT OnMouseDown thread=%d\n", \$_thread
  bt 1
  continue
end
run
G
gdb -batch -x "$OUT/bp.gdb" --args /opt/REAPER/reaper "${ARGS[@]}" > "$OUT/gdb-reaper.log" 2>&1 &
W=""; for i in $(seq 240); do W=$(xdotool search --onlyvisible --name "CLAP: $N" 2>/dev/null | tail -1); [ -n "$W" ] && break; sleep 0.5; done
echo "FX window: ${W:-none}"; for w in $(xdotool search --onlyvisible --name . 2>/dev/null); do echo "  win $w: $(xdotool getwindowname $w)"; done
for i in $(seq 10); do ws=$(xdotool search --onlyvisible --name '^About REAPER' 2>/dev/null) || break; for w in $ws; do xdotool windowclose "$w"; done; sleep 1; done
sleep 3
W=$(xdotool search --onlyvisible --name "CLAP.*: $N" 2>/dev/null | tail -1)
if [ -n "$W" ]; then
  xdotool windowactivate --sync "$W" 2>/dev/null; eval "$(xdotool getwindowgeometry --shell "$W")"
  xdotool mousemove --window "$W" $((WIDTH / 2)) $((HEIGHT / 2)); sleep 0.3; xdotool mousedown 1; sleep 0.08; xdotool mouseup 1
  sleep 4; import -display :99 -window root "$OUT/reaper-$N.png"
fi
pkill -INT -f "^gdb -batch" ; sleep 2; pkill -x reaper; pkill -f "^gdb -batch"; sleep 1
grep -A3 'BP-HIT' "$OUT/gdb-reaper.log"
