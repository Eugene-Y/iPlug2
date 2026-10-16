set pagination off
set breakpoint pending on
handle SIGPIPE nostop noprint
break IPlugControls::ProcessBlock
commands
  silent
  printf "BP-HIT IPlugControls::ProcessBlock thread=%d\n", $_thread
  bt 4
  disable 1
  continue
end
run
