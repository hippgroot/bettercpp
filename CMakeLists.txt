cmake_minimum_required(VERSION 3.21)

project(bettercpp
  VERSION 0.1.0
  LANGUAGES CXX
)

option(BETTERCPP_COPY_CLANG_FORMAT
  "Copy bettercpp's .clang-format into the including project's root"
  OFF
)
option(BETTERCPP_COPY_CLANG_TIDY
  "Copy bettercpp's .clang-tidy into the including project's root"
  OFF
)
option(
  BETTERCPP_COPY_CMAKE_PRESETS
  "Copy bettercpp's CMakePresets.json into the including project's root"
  OFF
)
option(BETTERCPP_COPY_GITIGNORE
  "Copy bettercpp's .gitignore into the including project's root"
  OFF
)
option(BETTERCPP_WITH_GLM "Enable the GLM extension (bettercpp-glm.hpp)" OFF)
option(BETTERCPP_WITH_DIAG
  "Enable the diagnostics extension (bettercpp-diag.hpp); requires C++23"
  OFF
)

add_library(bettercpp INTERFACE)
add_library(bettercpp::bettercpp ALIAS bettercpp)
target_include_directories(bettercpp
  INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(bettercpp INTERFACE cxx_std_20)

if(BETTERCPP_WITH_GLM)
  if(NOT TARGET glm::glm)
    find_package(glm CONFIG QUIET)
  endif()
  if(NOT TARGET glm::glm)
    set(GLM_BUILD_LIBRARY OFF CACHE BOOL "")
    include(FetchContent)
    FetchContent_Declare(glm
      GIT_REPOSITORY https://github.com/g-truc/glm.git
      GIT_TAG 1.0.1
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(glm)
  endif()
  target_link_libraries(bettercpp INTERFACE glm::glm)
  target_compile_definitions(bettercpp INTERFACE BETTERCPP_WITH_GLM)
endif()

if(BETTERCPP_WITH_DIAG)
  find_package(Threads REQUIRED)
  target_link_libraries(bettercpp INTERFACE Threads::Threads)
  target_compile_features(bettercpp INTERFACE cxx_std_23)
  target_compile_definitions(bettercpp INTERFACE BETTERCPP_WITH_DIAG)
endif()

function(_bettercpp_copy_to_root name)
  set(_src ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/${name})
  set(_dst ${CMAKE_SOURCE_DIR}/${name})
  if(NOT EXISTS ${_dst})
    file(COPY_FILE ${_src} ${_dst})
    message(STATUS "bettercpp: copied ${name} to ${CMAKE_SOURCE_DIR}")
  else()
    file(SHA256 ${_src} _src_hash)
    file(SHA256 ${_dst} _dst_hash)
    if(NOT _src_hash STREQUAL _dst_hash)
      message(STATUS
        "bettercpp: ${_dst} differs from bettercpp's; leaving it alone"
      )
    endif()
  endif()
endfunction()

if(NOT PROJECT_IS_TOP_LEVEL)
  if(BETTERCPP_COPY_CLANG_FORMAT)
    _bettercpp_copy_to_root(.clang-format)
  endif()
  if(BETTERCPP_COPY_CLANG_TIDY)
    _bettercpp_copy_to_root(.clang-tidy)
  endif()
  if(BETTERCPP_COPY_CMAKE_PRESETS)
    _bettercpp_copy_to_root(CMakePresets.json)
  endif()
  if(BETTERCPP_COPY_GITIGNORE)
    _bettercpp_copy_to_root(.gitignore)
  endif()
endif()
