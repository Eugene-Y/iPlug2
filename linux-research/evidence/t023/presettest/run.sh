#!/usr/bin/env bash
D=/Volumes/Storage/code/IPlug2-T-023/linux-research/build/t023r-presettest
P=$D/b/out/IPlugEffect.clap
for s in 0 1 2 3 4; do
  for mode in params noparams; do
    [ $s = 2 ] && [ $mode = noparams ] && continue
    echo "===== T023=$s host=$mode"
    T023=$s $D/host $P $mode 2>&1 | grep -E "^\[(host|plug)\]|ssert|isbehav|terminate|Abort" ; echo "exit=${PIPESTATUS[0]}"
  done
done
