set pagination off
set breakpoint pending on
break IPlugVST3View<iplug::IPlugVST3>::onKeyDown(char16_t, short, short)
commands
silent
printf "HIT onKeyDown key=%d msg=%d mod=%d\n", $x1, $x2, $x3
continue
end
continue
