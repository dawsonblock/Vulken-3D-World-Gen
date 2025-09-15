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

add_executable(vulken_viewer
  src/main.cpp
)

target_include_directories(vulken_viewer PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(vulken_viewer PRIVATE
  Vulkan::Vulkan
)

if (glfw3_FOUND)
  target_link_libraries(vulken_viewer PRIVATE glfw)
else()
  target_link_libraries(vulken_viewer PRIVATE PkgConfig::GLFW3)
endif()

# On Linux/X11 we need pthread and dl occasionally; link if available
find_package(Threads)
if (Threads_FOUND)
  target_link_libraries(vulken_viewer PRIVATE Threads::Threads)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(vulken_viewer PRIVATE -Wall -Wextra -Wpedantic)
endif()
