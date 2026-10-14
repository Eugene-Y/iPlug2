cmake_minimum_required(VERSION 3.20)

project(IPlugLinuxScaleTests LANGUAGES CXX)

enable_testing()

set(IPLUG2_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

add_executable(iplug-linux-scale-test IPlugLinuxScaleTest.cpp)
target_compile_features(iplug-linux-scale-test PRIVATE cxx_std_17)
target_compile_definitions(iplug-linux-scale-test PRIVATE OS_LINUX)
target_include_directories(iplug-linux-scale-test PRIVATE
  ${IPLUG2_ROOT}/IPlug ${IPLUG2_ROOT}/IGraphics ${IPLUG2_ROOT}/IGraphics/Platforms ${IPLUG2_ROOT}/WDL)
target_compile_options(iplug-linux-scale-test PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(iplug-linux-scale-test PRIVATE -fsanitize=address,undefined)

# No X display needed: the X lookup itself is checked by the REAPER/APP runs (linux-research/notes/17).
add_test(NAME iplug-linux-scale COMMAND iplug-linux-scale-test)
