set pagination off
set breakpoint pending on
break iplug::igraphics::IGraphics::Draw
commands
silent
printf "DRAW\n"
continue
end
break iplug::igraphics::IGraphicsLinux::ProcessXEvents
commands
silent
printf "DRAIN\n"
continue
end
run
