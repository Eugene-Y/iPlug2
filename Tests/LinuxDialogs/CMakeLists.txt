cmake_minimum_required(VERSION 3.20)

project(IPlugLinuxDialogsTests LANGUAGES CXX)

enable_testing()

set(IPLUG2_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

add_executable(iplug-linux-dialogs-test
  IPlugLinuxDialogsTest.cpp
  ${IPLUG2_ROOT}/IGraphics/Platforms/IGraphicsLinuxDialogs.cpp
  ${IPLUG2_ROOT}/IPlug/Linux/IPlugLinuxDBus.cpp
  ${IPLUG2_ROOT}/IPlug/Linux/IPlugLinuxHostLoop.cpp
  ${IPLUG2_ROOT}/IPlug/Linux/IPlugLinuxSubprocess.cpp
)
target_compile_features(iplug-linux-dialogs-test PRIVATE cxx_std_17)
target_compile_definitions(iplug-linux-dialogs-test PRIVATE OS_LINUX IGRAPHICS_NANOVG IGRAPHICS_GL2)
target_include_directories(iplug-linux-dialogs-test PRIVATE
  ${IPLUG2_ROOT}/IPlug ${IPLUG2_ROOT}/IGraphics ${IPLUG2_ROOT}/IGraphics/Platforms ${IPLUG2_ROOT}/WDL
  ${IPLUG2_ROOT}/Dependencies/IGraphics/NanoSVG/src ${IPLUG2_ROOT}/Dependencies/IGraphics/NanoVG/src)
target_compile_options(iplug-linux-dialogs-test PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -fsigned-char)
target_link_options(iplug-linux-dialogs-test PRIVATE -fsanitize=address,undefined)
target_link_libraries(iplug-linux-dialogs-test PRIVATE pthread dl)

# No X display needed. Under dbus-run-session it also covers a session bus without a portal.
add_test(NAME iplug-linux-dialogs COMMAND dbus-run-session -- $<TARGET_FILE:iplug-linux-dialogs-test>)
