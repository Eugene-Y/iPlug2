set pagination off
set breakpoint pending on
break iplug::IPlugCLAP::guiSetScale
commands
silent
printf "clap set_scale %g\n", $d0
continue
end
break iplug::IPlugCLAP::guiSetSize
commands
silent
printf "clap set_size %d x %d\n", $w1, $w2
continue
end
break iplug::IPlugCLAP::guiGetSize
commands
silent
printf "clap get_size\n"
continue
end
break iplug::IPlugCLAP::EditorResize
commands
silent
printf "clap EditorResize (request_resize if changed) %d x %d\n", $w1, $w2
continue
end
break IPlugVST3View<iplug::IPlugVST3>::setContentScaleFactor
commands
silent
printf "vst3 setContentScaleFactor %g\n", $s0
continue
end
break IPlugVST3View<iplug::IPlugVST3>::getSize
commands
silent
printf "vst3 getSize\n"
continue
end
break IPlugVST3View<iplug::IPlugVST3>::onSize
commands
silent
printf "vst3 onSize %d x %d\n", *(int*)($x1+8) - *(int*)$x1, *(int*)($x1+12) - *(int*)($x1+4)
continue
end
break iplug::IPlugVST3::EditorResize
commands
silent
printf "vst3 EditorResize (resizeView if changed) %d x %d\n", $w1, $w2
continue
end
break iplug::igraphics::IGEditorDelegate::UpdateEditorSizeForScreenScale
commands
silent
printf "UpdateEditorSizeForScreenScale\n"
continue
end
break iplug::igraphics::linux_scale::QueryDesktopScale
commands
silent
printf "QueryDesktopScale\n"
continue
end
break iplug::igraphics::IGraphics::SetScreenScale
commands
silent
printf "IGraphics::SetScreenScale %g\n", $s0
continue
end
continue
