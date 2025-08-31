cmake_minimum_required(VERSION 3.20)
project(tools_imagediff CXX)

add_executable(imagediff imagediff.cpp)
target_compile_features(imagediff PRIVATE cxx_std_20)
if (MSVC)
  target_compile_options(imagediff PRIVATE /W3)
else()
  target_compile_options(imagediff PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# Locate stb headers from system (e.g., libstb-dev) or vcpkg toolchain
find_path(STB_INCLUDE_DIR NAMES stb/stb_image.h)
if (NOT STB_INCLUDE_DIR)
  message(FATAL_ERROR "stb headers not found. Install libstb-dev (Linux) or ensure vcpkg 'stb' is installed and toolchain is active.")
endif()

target_include_directories(imagediff PRIVATE ${STB_INCLUDE_DIR})