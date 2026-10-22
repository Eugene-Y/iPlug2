set pagination off
set breakpoint pending on
handle SIGPIPE nostop noprint
break MyCtl::ProcessBlock
commands
  silent
  printf "BP-HIT ProcessBlock thread=%d\n", $_thread
  bt 3
  disable 1
  continue
end
break OnMouseDown
commands
  silent
  printf "BP-HIT OnMouseDown thread=%d\n", $_thread
  bt 1
  continue
end
run
