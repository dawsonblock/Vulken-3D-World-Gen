
# GenBuildInfo.cmake — configure-time build info header
# Expected variables:
#   VOXELVK_BRANCH_SOURCES : string describing merged branches
#   Vulkan_VERSION         : provided by find_package(Vulkan)
# Output:
#   ${CMAKE_BINARY_DIR}/generated/build_info.hpp

include(CheckCXXCompilerFlag)

# Git short hash (optional)
set(_GIT_HASH "UNKNOWN")
find_package(Git QUIET)
if(Git_FOUND)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE _GIT_HASH
    RESULT_VARIABLE _GIT_HASH_RES
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
endif()

if(NOT _GIT_HASH OR "${_GIT_HASH}" STREQUAL "")
  # Fallback UUID stub
  string(TIMESTAMP _TS "%Y%m%d%H%M%S" UTC)
  string(RANDOM LENGTH 8 ALPHABET 0123456789ABCDEF _RANDHEX)
  set(_GIT_HASH "${_TS}-${_RANDHEX}")
endif()

string(TIMESTAMP _BUILD_TS "%Y-%m-%d %H:%M:%S %Z")

if(NOT VOXELVK_BRANCH_SOURCES)
  set(VOXELVK_BRANCH_SOURCES "unknown")
endif()

# Configure the header
set(BUILD_INFO_OUT_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${BUILD_INFO_OUT_DIR}")
configure_file("${CMAKE_CURRENT_LIST_DIR}/../src/core/build_info.hpp.in"
               "${BUILD_INFO_OUT_DIR}/build_info.hpp" @ONLY)
message(STATUS "BuildInfo: hash=${_GIT_HASH}, time=${_BUILD_TS}")
