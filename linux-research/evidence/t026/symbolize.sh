#!/usr/bin/env bash
# Symbolizes render-t026 bt= output (stdin) with addr2line: frames inside .so/.clap/.vst3 files only.
# Runs INSIDE the container.
while IFS= read -r l; do
  case "$l" in
    ---*) echo "$l" ;;
    *"(+0x"*) f="${l%%(*}"; o="${l#*(+}"; o="${o%%)*}"
              echo "   $(addr2line -f -C -i -p -e "$f" "$o" 2>/dev/null | head -3 | tr '\n' ' ' | sed 's#/Volumes/Storage/code/##g')" ;;
    *) echo "   $l" | sed 's#/Volumes/Storage/code/##g' ;;
  esac
done
