set pagination off
set breakpoint pending on
break iplug::igraphics::IGraphicsLinux::CreatePlatformTextEntry
break iplug::igraphics::IGraphics::CreateTextEntry
break iplug::igraphics::IGraphics::OnMouseDblClick
break iplug::igraphics::ITextEntryControl::CommitEdit
break iplug::igraphics::ITextEntryControl::DismissEdit
break iplug::igraphics::IGraphicsLinux::HandleFocusOut
commands 1-6
silent
bt 3
continue
end
continue
