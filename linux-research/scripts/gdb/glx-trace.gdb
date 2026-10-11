set pagination off
break glXMakeCurrent
commands
silent
printf "MC drawable=%lx ctx=%lx\n", $x1, $x2
finish
end
break glXSwapBuffers
commands
silent
printf "SWAP drawable=%lx\n", $x1
continue
end
continue
