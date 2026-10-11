# Out-of-tree Linux build driver for an iPlug2 tree whose own build system has no Linux plugin targets
# (stillwellaudio/iPlug2: their product CMake/Python scripts are not in the public repo).
# Builds CLAP + VST3 for the listed examples with IGraphics Skia (Ganesh GL3/GLX), matching what
# that fork's IGraphicsLinux.cpp requires.
#   cmake -S linux-research/cmake/stillwell -B <build> -G Ninja -DIPLUG2_DIR=<worktree> -DSKIA_DIR=<cache/skia/arch>
cmake_minimum_required(VERSION 3.20)
project(iplug2_linux_examples C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(IPLUG2_DIR "" CACHE PATH "iPlug2 worktree")
set(SKIA_DIR "" CACHE PATH "dir with lib/ (static Skia) and src/ (Skia source root for headers)")
set(EXAMPLES "IPlugEffect;IPlugControls" CACHE STRING "examples to build")
set(DEPS ${IPLUG2_DIR}/Dependencies)
set(VST3_SDK ${DEPS}/IPlug/VST3_SDK)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FONTCONFIG REQUIRED fontconfig)
pkg_check_modules(FREETYPE REQUIRED freetype2)
find_package(X11 REQUIRED)
find_package(OpenGL REQUIRED COMPONENTS OpenGL GLX)
find_package(Threads REQUIRED)

set(SKIA_LIBS skparagraph svg skshaper skunicode_icu skunicode_core skottie sksg skia)
set(SKIA_LINK)
foreach(l ${SKIA_LIBS})
  list(APPEND SKIA_LINK ${SKIA_DIR}/lib/lib${l}.a)
endforeach()

set(IPLUG_SRC
  ${IPLUG2_DIR}/IPlug/IPlugAPIBase.cpp ${IPLUG2_DIR}/IPlug/IPlugParameter.cpp
  ${IPLUG2_DIR}/IPlug/IPlugPaths.cpp ${IPLUG2_DIR}/IPlug/IPlugPluginBase.cpp
  ${IPLUG2_DIR}/IPlug/IPlugProcessor.cpp ${IPLUG2_DIR}/IPlug/IPlugTimer.cpp)
set(IGRAPHICS_SRC
  ${IPLUG2_DIR}/IGraphics/IControl.cpp ${IPLUG2_DIR}/IGraphics/IGraphics.cpp
  ${IPLUG2_DIR}/IGraphics/IGraphicsEditorDelegate.cpp
  ${IPLUG2_DIR}/IGraphics/Controls/IControls.cpp ${IPLUG2_DIR}/IGraphics/Controls/IPopupMenuControl.cpp
  ${IPLUG2_DIR}/IGraphics/Controls/ITextEntryControl.cpp
  ${IPLUG2_DIR}/IGraphics/Platforms/IGraphicsLinux.cpp
  ${IPLUG2_DIR}/IGraphics/Drawing/IGraphicsSkia.cpp
  ${DEPS}/IGraphics/glad_GL3/src/glad.c)
set(CLAP_SRC ${IPLUG2_DIR}/IPlug/CLAP/IPlugCLAP.cpp)
set(VST3_SRC
  ${IPLUG2_DIR}/IPlug/VST3/IPlugVST3.cpp ${IPLUG2_DIR}/IPlug/VST3/IPlugVST3_ProcessorBase.cpp
  ${VST3_SDK}/base/source/baseiids.cpp ${VST3_SDK}/base/source/fdebug.cpp
  ${VST3_SDK}/base/source/fobject.cpp ${VST3_SDK}/base/source/fstring.cpp
  ${VST3_SDK}/base/source/timer.cpp ${VST3_SDK}/base/source/updatehandler.cpp
  ${VST3_SDK}/base/thread/source/flock.cpp
  ${VST3_SDK}/pluginterfaces/base/funknown.cpp ${VST3_SDK}/pluginterfaces/base/ustring.cpp
  ${VST3_SDK}/pluginterfaces/base/coreiids.cpp
  ${VST3_SDK}/public.sdk/source/common/pluginview.cpp ${VST3_SDK}/public.sdk/source/common/commoniids.cpp
  ${VST3_SDK}/public.sdk/source/vst/vstbus.cpp ${VST3_SDK}/public.sdk/source/vst/vstcomponent.cpp
  ${VST3_SDK}/public.sdk/source/vst/vstcomponentbase.cpp ${VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp
  ${VST3_SDK}/public.sdk/source/vst/vstparameters.cpp
  ${VST3_SDK}/public.sdk/source/vst/vstsinglecomponenteffect.cpp
  ${VST3_SDK}/public.sdk/source/main/pluginfactory.cpp ${VST3_SDK}/public.sdk/source/main/linuxmain.cpp)

set(COMMON_INCLUDES
  ${IPLUG2_DIR}/IPlug ${IPLUG2_DIR}/IPlug/Extras ${IPLUG2_DIR}/IPlug/Extras/Synth ${IPLUG2_DIR}/WDL
  ${IPLUG2_DIR}/IGraphics ${IPLUG2_DIR}/IGraphics/Controls ${IPLUG2_DIR}/IGraphics/Drawing
  ${IPLUG2_DIR}/IGraphics/Platforms ${IPLUG2_DIR}/IGraphics/Extras
  ${DEPS}/IGraphics/NanoSVG/src ${DEPS}/IGraphics/STB ${DEPS}/IGraphics/glad_GL3/include
  ${SKIA_DIR}/src ${SKIA_DIR}/src/include
  ${FONTCONFIG_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS})
# WDL asserts a signed default char; aarch64 Linux defaults to unsigned char.
add_compile_options(-fsigned-char)
set(COMMON_DEFS IGRAPHICS_SKIA IGRAPHICS_GL3 SK_GANESH SK_GL NOMINMAX
  $<$<CONFIG:Debug>:DEVELOPMENT> $<$<NOT:$<CONFIG:Debug>>:RELEASE>)
set(COMMON_LINK ${SKIA_LINK} ${FONTCONFIG_LIBRARIES} ${FREETYPE_LIBRARIES}
  X11::X11 OpenGL::GL OpenGL::GLX Threads::Threads ${CMAKE_DL_LIBS})

function(add_example name)
  set(ex ${IPLUG2_DIR}/Examples/${name})
  file(GLOB res_files ${ex}/resources/fonts/* ${ex}/resources/img/*)
  set(extra_src)
  if(name STREQUAL "IPlugInstrument")
    list(APPEND extra_src ${IPLUG2_DIR}/IPlug/Extras/Synth/MidiSynth.cpp ${IPLUG2_DIR}/IPlug/Extras/Synth/VoiceAllocator.cpp)
    # VoiceAllocator.h uses UCHAR_MAX / std::unique_ptr without including <climits>/<memory> (libstdc++ is strict)
    set_source_files_properties(${extra_src} ${ex}/${name}.cpp PROPERTIES COMPILE_OPTIONS "-include;climits;-include;memory")
  endif()
  foreach(fmt CLAP VST3)
    string(TOLOWER ${fmt} lfmt)
    set(t ${name}-${lfmt})
    add_library(${t} MODULE ${ex}/${name}.cpp ${IPLUG_SRC} ${IGRAPHICS_SRC} ${${fmt}_SRC} ${extra_src})
    target_include_directories(${t} PRIVATE ${ex} ${ex}/resources ${COMMON_INCLUDES})
    target_compile_definitions(${t} PRIVATE ${COMMON_DEFS} ${fmt}_API IPLUG_EDITOR=1 IPLUG_DSP=1)
    target_link_libraries(${t} PRIVATE ${COMMON_LINK})
    target_link_options(${t} PRIVATE -static-libstdc++ -static-libgcc)
    set_target_properties(${t} PROPERTIES PREFIX "" C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)
    if(fmt STREQUAL "CLAP")
      target_include_directories(${t} PRIVATE ${IPLUG2_DIR}/IPlug/CLAP ${DEPS}/IPlug/CLAP_SDK/include ${DEPS}/IPlug/CLAP_HELPERS/include ${DEPS}/IPlug/CLAP_HELPERS/include/clap/helpers)
      set_target_properties(${t} PROPERTIES OUTPUT_NAME ${name} SUFFIX ".clap" LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out)
      set(resdir ${CMAKE_BINARY_DIR}/out/${name}.resources)
    else()
      target_include_directories(${t} PRIVATE ${IPLUG2_DIR}/IPlug/VST3 ${VST3_SDK})
      set(bundle ${CMAKE_BINARY_DIR}/out/${name}.vst3/Contents)
      set_target_properties(${t} PROPERTIES OUTPUT_NAME ${name} SUFFIX ".so"
        LIBRARY_OUTPUT_DIRECTORY ${bundle}/${CMAKE_SYSTEM_PROCESSOR}-linux)
      set(resdir ${bundle}/Resources)
    endif()
    add_custom_command(TARGET ${t} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory ${resdir}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${res_files} ${resdir})
  endforeach()
endfunction()

foreach(e ${EXAMPLES})
  add_example(${e})
endforeach()
