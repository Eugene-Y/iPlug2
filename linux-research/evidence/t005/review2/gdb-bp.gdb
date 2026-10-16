set pagination off
set breakpoint pending on
handle SIGPIPE nostop noprint
break IPlugEffect::ProcessBlock
commands
  silent
  printf "BP-HIT IPlugEffect::ProcessBlock thread=%d\n", $_thread
  bt 3
  disable 1
  continue
end
run
