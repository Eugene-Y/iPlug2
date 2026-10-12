set pagination off
set breakpoint pending on
set $f = 'iplug::igraphics::IGraphics::PopupHostContextMenuForParam(iplug::igraphics::IControl*, int, float, float)'
break *((char*)$f + 0x1c4)
commands
silent
printf "BEFORE POPUP\n"
shell date +%T.%N
continue
end
break *((char*)$f + 0x1c8)
commands
silent
printf "AFTER POPUP\n"
shell date +%T.%N
continue
end
continue
