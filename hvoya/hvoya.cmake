# What common-mac.xcconfig / common-win.props and the Xcode/VS pre-build steps give a hvoya plug-in,
# for CMake builds. Include after iPlug2.cmake:
#
#   include(${IPLUG2_DIR}/hvoya/hvoya.cmake)
#   hvoya_add_utils(${PROJECT_NAME}-hvoya ${PROJECT_NAME})
#   iplug_add_plugin(${PROJECT_NAME} ... LINK ${PROJECT_NAME}-hvoya)

set(HVOYA_DIR ${CMAKE_CURRENT_LIST_DIR})

# plug_build_info.hpp is written at configure time. Configure re-runs when the plug-in repo's HEAD
# moves, so the commit is never stale; date and time are the configure's, not the build's.
function(hvoya_write_plug_build_info out_dir product)
  execute_process(COMMAND git rev-parse --short=7 HEAD WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                  OUTPUT_VARIABLE _commit OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  execute_process(COMMAND git rev-parse --abbrev-ref HEAD WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                  OUTPUT_VARIABLE _branch OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  execute_process(COMMAND git rev-parse --path-format=absolute --git-path HEAD
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                  OUTPUT_VARIABLE _head_file OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  if(_head_file AND EXISTS "${_head_file}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_head_file}")
    if(NOT _branch STREQUAL "HEAD")
      execute_process(COMMAND git rev-parse --path-format=absolute --git-path "refs/heads/${_branch}"
                      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                      OUTPUT_VARIABLE _ref_file OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
      if(_ref_file AND EXISTS "${_ref_file}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_ref_file}")
      endif()
    endif()
  endif()
  string(TIMESTAMP _date "%Y.%m.%d")
  string(TIMESTAMP _time "%H:%M:%S")
  file(WRITE ${out_dir}/plug_build_info.hpp
"#ifndef plug_build_info_hpp
#define plug_build_info_hpp
#define PLUG_GIT_BRANCH_NAME \"${_branch}\"
#define PLUG_GIT_COMMIT_SHA  \"${_commit}\"
#define PLUG_BUILD_DATE      \"${_date}\"
#define PLUG_BUILD_TIME      \"${_time}\"
#define PLUG_PRODUCT         \"${product}\"
#define PLUG_ARCHS           \"${CMAKE_SYSTEM_PROCESSOR}\"
#endif
")
endfunction()

# Interface library <name>: every hvoya/utils source (as common-win.props globs them), the include
# roots the Xcode/VS projects use, and the generated plug_build_info.hpp.
function(hvoya_add_utils name product)
  set(_build_info_dir ${CMAKE_CURRENT_BINARY_DIR}/generated)
  hvoya_write_plug_build_info(${_build_info_dir} ${product})
  file(GLOB_RECURSE _utils_sources CONFIGURE_DEPENDS ${HVOYA_DIR}/hvoya/utils/*.cpp)
  add_library(${name} INTERFACE)
  target_compile_features(${name} INTERFACE cxx_std_20)
  target_include_directories(${name} INTERFACE
    ${HVOYA_DIR}
    ${HVOYA_DIR}/hvoya
    ${HVOYA_DIR}/hvoya/utils
    ${HVOYA_DIR}/hvoya/libs/plog/include
    ${_build_info_dir})
  target_sources(${name} INTERFACE ${_utils_sources})
  target_compile_definitions(${name} INTERFACE $<$<CONFIG:Release>:PLOG_DISABLE_LOGGING>)

  # The Xcode/VS pre-builds write plug_build_info.hpp next to the plug-in's sources, and #include "…"
  # finds that copy before the generated one: a CMake build in the same checkout would silently embed
  # the other build's platform, product and commit. Checked at every build, since that copy can appear
  # after the configure.
  set(_check_script ${CMAKE_CURRENT_BINARY_DIR}/${name}-check-build-info.cmake)
  file(WRITE ${_check_script}
"if(EXISTS \"${CMAKE_CURRENT_SOURCE_DIR}/plug_build_info.hpp\")
  message(FATAL_ERROR \"${CMAKE_CURRENT_SOURCE_DIR}/plug_build_info.hpp (written by an Xcode/VS build) would shadow the generated build info. Delete it; Xcode/VS write it again on their next build.\")
endif()
")
  add_custom_target(${name}-check-build-info COMMAND ${CMAKE_COMMAND} -P ${_check_script} VERBATIM)
  add_dependencies(${name} ${name}-check-build-info)
endfunction()
