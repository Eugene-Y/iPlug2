set pagination off
set breakpoint pending on
break glXSwapIntervalEXT
commands
silent
printf "HIT glXSwapIntervalEXT interval=%d\n", $x2
continue
end
break glXSwapIntervalMESA
commands
silent
printf "HIT glXSwapIntervalMESA interval=%d\n", $x0
continue
end
break iplug::igraphics::IGraphics::Draw(iplug::igraphics::IRECTList&)
commands
silent
printf "AT Draw: glXGetSwapIntervalMESA()=%d (our context current)\n", ((int(*)(void))glXGetSwapIntervalMESA)()
delete
continue
end
continue
