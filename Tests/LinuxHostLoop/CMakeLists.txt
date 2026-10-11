cmake_minimum_required(VERSION 3.20)

project(IPlugLinuxHostLoopTests LANGUAGES CXX)

enable_testing()

set(IPLUG2_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(VST3_SDK ${IPLUG2_ROOT}/Dependencies/IPlug/VST3_SDK)

add_executable(iplug-linux-host-loop-test
  IPlugLinuxHostLoopTest.cpp
  ${IPLUG2_ROOT}/IPlug/Linux/IPlugLinuxHostLoop.cpp
  ${IPLUG2_ROOT}/IPlug/Linux/IPlugLinuxTimer.cpp
  ${VST3_SDK}/pluginterfaces/base/funknown.cpp
)
target_compile_features(iplug-linux-host-loop-test PRIVATE cxx_std_17)
target_compile_definitions(iplug-linux-host-loop-test PRIVATE IPLUG_TRACE_HOST_LOOP)
target_include_directories(iplug-linux-host-loop-test PRIVATE ${IPLUG2_ROOT}/IPlug ${IPLUG2_ROOT}/WDL ${VST3_SDK})
target_compile_options(iplug-linux-host-loop-test PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -fsigned-char -include stdlib.h)
target_link_options(iplug-linux-host-loop-test PRIVATE -fsanitize=address,undefined)
target_link_libraries(iplug-linux-host-loop-test PRIVATE pthread)

add_test(NAME iplug-linux-host-loop COMMAND iplug-linux-host-loop-test)
