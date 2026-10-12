set pagination off
set breakpoint pending on
break iplug::igraphics::IGraphicsLinux::LoadPlatformFont(char const*, char const*, iplug::igraphics::ETextStyle)
commands
silent
printf "BY NAME id='%s' name='%s'\n", (char*)$x1, (char*)$x2
continue
end
break iplug::igraphics::IGraphicsLinux::LoadPlatformFont(char const*, char const*)
commands
silent
printf "BY FILE id='%s' file='%s'\n", (char*)$x1, (char*)$x2
continue
end
break iplug::igraphics::IGraphicsNanoVG::LoadAPIFont
commands
silent
printf "API LOAD id='%s'\n", (char*)$x1
continue
end
continue
