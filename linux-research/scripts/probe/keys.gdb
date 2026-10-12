set pagination off
set breakpoint pending on
break iplug::igraphics::IGraphicsLinux::HandleKey
commands
silent
printf "KEY type=%d keycode=%u state=0x%x send=%d win=0x%lx\n", *(int*)$x1, *(unsigned*)($x1+84), *(unsigned*)($x1+80), *(int*)($x1+16), *(unsigned long*)($x1+32)
continue
end
break iplug::igraphics::IGraphicsLinux::ForwardKeyToHost
commands
silent
printf "FORWARD keycode=%u\n", *(unsigned*)($x1+84)
continue
end
break iplug::igraphics::IGraphicsLinux::TakeKeyboardFocus
commands
silent
printf "TAKE\n"
continue
end
break iplug::igraphics::IGraphicsLinux::ReleaseKeyboardFocus
commands
silent
printf "RELEASE\n"
continue
end
break iplug::igraphics::IGraphicsLinux::HandleFocusOut
commands
silent
printf "FOCUSOUT mode=%d detail=%d\n", *(int*)($x1+40), *(int*)($x1+44)
continue
end
break iplug::igraphics::ITextEntryControl::DismissEdit
commands
silent
printf "DISMISS\n"
continue
end
break iplug::igraphics::ITextEntryControl::CommitEdit
commands
silent
printf "COMMIT\n"
continue
end
continue
