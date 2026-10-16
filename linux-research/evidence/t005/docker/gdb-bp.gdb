set pagination off
set breakpoint pending on
handle SIGPIPE nostop noprint
break IPlugEffect.cpp:53
commands
  silent
  printf "BP-HIT ProcessBlock thread=%d\n", $_thread
  bt 4
  disable 1
  continue
end
break IControls.cpp:687
commands
  silent
  printf "BP-HIT IVKnobControl::OnMouseDown thread=%d\n", $_thread
  bt 6
  disable 2
  continue
end
run
