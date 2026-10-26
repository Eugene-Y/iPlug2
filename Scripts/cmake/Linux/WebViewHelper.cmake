#  ==============================================================================
#
#  This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.
#
#  See LICENSE.txt for  more info.
#
#  ==============================================================================

# The Linux WebView helper: WebKitGTK may not run inside a host's process, so a web view (WebViewEditorDelegate,
# IWebViewControl) runs it in iplug2-webview-helper, which every plug-in that has one carries in its resource
# folder. Only the helper links WebKitGTK; the plug-in's own NEEDED list stays as it is.
#
# Without webkit2gtk-4.1 (2.40 or newer) the plug-ins still build, with a warning, and their web view shows
# that the helper is missing.

include_guard(GLOBAL)

set(IPLUG2_LINUX_WEBVIEW_HELPER_MISSING OFF CACHE INTERNAL "A WebView target was built without its helper")

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(IPLUG2_WEBKITGTK QUIET IMPORTED_TARGET GLOBAL webkit2gtk-4.1>=2.40)
endif()

if(IPLUG2_WEBKITGTK_FOUND AND NOT TARGET iplug2-webview-helper)
  # The helper is C: then it needs no C++ runtime of its own beside the one WebKitGTK loads.
  enable_language(C)
  # Built only for the targets that bundle it (add_dependencies below), not by every project's "all"
  add_executable(iplug2-webview-helper EXCLUDE_FROM_ALL ${IPLUG2_DIR}/IPlug/Extras/WebView/IPlugWebViewLinuxHelper.c)
  target_include_directories(iplug2-webview-helper PRIVATE ${IPLUG2_DIR}/IPlug/Extras/WebView)
  target_link_libraries(iplug2-webview-helper PRIVATE PkgConfig::IPLUG2_WEBKITGTK)
  set_target_properties(iplug2-webview-helper PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/webview-helper"
  )
endif()

# Copies the helper into resource_dir after each build of target. Call it before the target's deploy step, which
# copies the resource folder.
function(iplug_linux_bundle_webview_helper target resource_dir)
  if(TARGET iplug2-webview-helper)
    add_dependencies(${target} iplug2-webview-helper)
    # POST_BUILD runs only when the target relinks: a rebuilt helper must relink it, or the copy stays stale.
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "$<TARGET_FILE:iplug2-webview-helper>")
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "${resource_dir}"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:iplug2-webview-helper>" "${resource_dir}/iplug2-webview-helper"
      COMMENT "Copying the WebView helper -> ${resource_dir}"
      VERBATIM
    )
  else()
    message(WARNING "[iPlug2] ${target}: webkit2gtk-4.1 (2.40 or newer) not found, so its web view has no helper "
      "and shows only an error. Install libwebkit2gtk-4.1-dev (Debian/Ubuntu) or webkit2gtk4.1-devel (Fedora).")
    set(IPLUG2_LINUX_WEBVIEW_HELPER_MISSING ON CACHE INTERNAL "A WebView target was built without its helper")
  endif()
endfunction()
