cmake_minimum_required(VERSION 3.20)
project(tools_imagediff CXX)

add_executable(imagediff imagediff.cpp)
target_compile_features(imagediff PRIVATE cxx_std_20)
if (MSVC)
  target_compile_options(imagediff PRIVATE /W3)
else()
  target_compile_options(imagediff PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# stb headers (header-only) are provided by vcpkg; add include dir explicitly for this tool
set(VCPKG_INC_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/include")
find_path(STB_INCLUDE_DIR stb_image.h
  HINTS
    ${VCPKG_INC_DIR}
    "${CMAKE_SOURCE_DIR}/build/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/include"
    /usr/include /usr/local/include
)
if (STB_INCLUDE_DIR)
  target_include_directories(imagediff PRIVATE ${STB_INCLUDE_DIR})
else()
  message(WARNING "stb headers not found; ensure vcpkg provides stb and include dirs are visible.")
endif()
