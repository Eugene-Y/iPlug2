cmake_minimum_required(VERSION 3.14)
project(IPlugEffect VERSION 1.0.0)

# Discover IPLUG2_DIR if not already set (for independent builds)
if(NOT DEFINED IPLUG2_DIR)
  set(IPLUG2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "iPlug2 root directory")
endif()

# Include iPlug2 CMake configuration
include(${IPLUG2_DIR}/iPlug2.cmake)

# IGraphics backend/renderer can be set via command line:
#   cmake -DIGRAPHICS_BACKEND=SKIA -DIGRAPHICS_RENDERER=GL3 ..
# Defaults: NANOVG/METAL on macOS, NANOVG/GL2 on Windows
# See iPlug2/Scripts/cmake/IGraphics.cmake for all options

find_package(iPlug2 REQUIRED)

# Build all formats except AUv3 (AUv3 is opt-in)
# To include AUv3: FORMATS ALL_AUV3
# To select specific formats: FORMATS APP VST3 AU
# hvoya: the Xcode/VS projects generate plug_build_info.hpp in a pre-build step and get the
# hvoya utils from common-mac.xcconfig / common-win.props; CMake has to provide both itself.
set(HVOYA_DIR ${IPLUG2_DIR}/hvoya)
set(PLUG_BUILD_INFO_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
execute_process(COMMAND git rev-parse --short=7 HEAD WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                OUTPUT_VARIABLE PLUG_GIT_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
execute_process(COMMAND git rev-parse --abbrev-ref HEAD WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                OUTPUT_VARIABLE PLUG_GIT_BRANCH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
string(TIMESTAMP PLUG_BUILD_DATE "%Y.%m.%d")
string(TIMESTAMP PLUG_BUILD_TIME "%H:%M:%S")
file(WRITE ${PLUG_BUILD_INFO_DIR}/plug_build_info.hpp
"#ifndef plug_build_info_hpp
#define plug_build_info_hpp
#define PLUG_GIT_BRANCH_NAME \"${PLUG_GIT_BRANCH}\"
#define PLUG_GIT_COMMIT_SHA  \"${PLUG_GIT_COMMIT}\"
#define PLUG_BUILD_DATE      \"${PLUG_BUILD_DATE}\"
#define PLUG_BUILD_TIME      \"${PLUG_BUILD_TIME}\"
#define PLUG_PRODUCT         \"${PROJECT_NAME}\"
#define PLUG_ARCHS           \"${CMAKE_SYSTEM_PROCESSOR}\"
#endif
")

add_library(${PROJECT_NAME}-hvoya INTERFACE)
target_compile_features(${PROJECT_NAME}-hvoya INTERFACE cxx_std_20)
target_include_directories(${PROJECT_NAME}-hvoya INTERFACE
  ${HVOYA_DIR} ${HVOYA_DIR}/hvoya/libs/plog/include ${PLUG_BUILD_INFO_DIR})
target_sources(${PROJECT_NAME}-hvoya INTERFACE
  ${HVOYA_DIR}/hvoya/utils/audio_buffer.cpp
  ${HVOYA_DIR}/hvoya/utils/host_time_info.cpp
  ${HVOYA_DIR}/hvoya/utils/hxa_pow.cpp
  ${HVOYA_DIR}/hvoya/utils/soft_limiter.cpp
  ${HVOYA_DIR}/hvoya/utils/utils.cpp
  ${HVOYA_DIR}/hvoya/utils/log/logger.cpp
  ${HVOYA_DIR}/hvoya/utils/midi_cc/mapper.cpp
  ${HVOYA_DIR}/hvoya/utils/midi_cc/mapper_serializer.cpp
)

iplug_add_plugin(${PROJECT_NAME}
  SOURCES
    IPlugEffect.cpp
    IPlugEffectUI.cpp
    IPlugEffect.hpp
    resources/resource.h
  RESOURCES
    resources/fonts/Roboto-Regular.ttf
    resources/fonts/RobotoMono-Regular.ttf
    resources/img/hvoya_logo_white_single.svg
  LINK
    ${PROJECT_NAME}-hvoya
)
