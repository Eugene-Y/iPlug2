set pagination off
set breakpoint pending on
break iplug::igraphics::IGraphicsLinux::HandleXEvent
commands
silent
set $t = *(int*)$x1
if $t == 9 || $t == 10
  printf "FOCUS%s win=0x%lx mode=%d detail=%d send=%d\n", ($t==9?"In":"Out"), *(unsigned long*)($x1+32), *(int*)($x1+40), *(int*)($x1+44), *(int*)($x1+16)
end
if $t == 4 || $t == 5
  printf "BUTTON%d\n", $t
end
continue
end
break iplug::igraphics::ITextEntryControl::CommitEdit
commands
silent
printf "COMMIT\n"
continue
end
continue
