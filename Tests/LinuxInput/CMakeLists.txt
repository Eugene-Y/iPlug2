cmake_minimum_required(VERSION 3.20)

project(IPlugLinuxInputTests LANGUAGES CXX)

enable_testing()

set(IPLUG2_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
find_package(X11 REQUIRED)

add_executable(iplug-linux-input-test IPlugLinuxInputTest.cpp)
target_compile_features(iplug-linux-input-test PRIVATE cxx_std_17)
target_include_directories(iplug-linux-input-test PRIVATE
  ${IPLUG2_ROOT}/IPlug ${IPLUG2_ROOT}/IGraphics ${IPLUG2_ROOT}/IGraphics/Platforms ${IPLUG2_ROOT}/WDL ${X11_INCLUDE_DIR})
target_compile_options(iplug-linux-input-test PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -fsigned-char)
target_link_options(iplug-linux-input-test PRIVATE -fsanitize=address,undefined)
target_link_libraries(iplug-linux-input-test PRIVATE ${X11_LIBRARIES} dl)

# Needs an X display (Xvfb); the keyboard-layout cases also need `setxkbmap us,ru` (the test says when it skips).
add_test(NAME iplug-linux-input COMMAND iplug-linux-input-test)
# fontconfig keeps its parsed configuration for the life of the process
set_tests_properties(iplug-linux-input PROPERTIES ENVIRONMENT "LSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/lsan.supp")
