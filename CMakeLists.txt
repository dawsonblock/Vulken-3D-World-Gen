cmake_minimum_required(VERSION 3.16)
project(Vulken3DWorldGen LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Output dirs
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

find_package(Vulkan REQUIRED)

# Try CMake GLFW first, then pkg-config fallback
find_package(glfw3 3.3 QUIET)
if (NOT glfw3_FOUND)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GLFW3 REQUIRED IMPORTED_TARGET glfw3)
endif()

include(FetchContent)

FetchContent_Declare(
  tracy
  GIT_REPOSITORY https://github.com/wolfpld/tracy.git
  GIT_TAG v0.11.1
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(tracy)

add_executable(vulken_viewer
  src/main.cpp
)

target_include_directories(vulken_viewer PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(vulken_viewer PRIVATE
  Vulkan::Vulkan
  TracyClient
)

target_compile_features(vulken_viewer PRIVATE cxx_std_20)

# Enable Tracy in Debug builds
target_compile_definitions(vulken_viewer PRIVATE
  $<$<CONFIG:Debug>:TRACY_ENABLE>
)

if (glfw3_FOUND)
  target_link_libraries(vulken_viewer PRIVATE glfw)
else()
  target_link_libraries(vulken_viewer PRIVATE PkgConfig::GLFW3)
endif()

find_package(Threads)
if (Threads_FOUND)
  target_link_libraries(vulken_viewer PRIVATE Threads::Threads dl)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(vulken_viewer PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Optional tests (opt-in)
option(ENABLE_TESTS "Build tests" OFF)
if (ENABLE_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
  include(CTest)
  add_subdirectory(tests)
endif()
