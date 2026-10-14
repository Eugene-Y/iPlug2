set pagination off
set breakpoint pending on
break iplug::IPlugAPIBase::EditorResizeFromUI
commands
silent
printf "EditorResizeFromUI %d x %d, needsPlatformResize %d\n", $w1, $w2, $w3
continue
end
break iplug::igraphics::IGEditorDelegate::OnParentWindowResize
commands
silent
printf "host resize (OnParentWindowResize) %d x %d\n", $w1, $w2
continue
end
break iplug::igraphics::IGraphicsLinux::TellHostTheOpenedSize
commands
silent
printf "TellHostTheOpenedSize\n"
continue
end
continue
