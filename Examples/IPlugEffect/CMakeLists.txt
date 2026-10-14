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
include(${IPLUG2_DIR}/hvoya/hvoya.cmake)
hvoya_add_utils(${PROJECT_NAME}-hvoya ${PROJECT_NAME})

# Build all formats except AUv3 (AUv3 is opt-in)
# To include AUv3: FORMATS ALL_AUV3
# To select specific formats: FORMATS APP VST3 AU
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
