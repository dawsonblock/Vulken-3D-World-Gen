# Subdirectory CMake; project()/cmake_minimum_required() live at repo root.

# Testing & quality options
option(VULKEN_ENGINE_WITH_TESTS "Enable building tests for vulken_engine (auto-add src/engine/tests if present)" ON)
option(VULKEN_ENGINE_ENABLE_SANITIZERS "Enable Address/Undefined sanitizers for debug-like builds (non-MSVC)" OFF)
option(VULKEN_ENGINE_ENABLE_COVERAGE "Enable code coverage instrumentation for GCC/Clang debug-like builds" OFF)

file(GLOB_RECURSE VULKEN_ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
)

add_library(vulken_engine STATIC ${VULKEN_ENGINE_SOURCES})
add_library(voxelvk::engine ALIAS vulken_engine)

target_include_directories(vulken_engine
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_features(vulken_engine PUBLIC cxx_std_20)

set_target_properties(vulken_engine PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)

target_compile_definitions(vulken_engine PUBLIC VULKEN_ENGINE_ENABLE_LOGGING=1)

# Per-compiler warnings (optional)
if(MSVC)
  target_compile_options(vulken_engine PRIVATE /W4)
else()
  target_compile_options(vulken_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Sanitizers (debug-like configs, non-MSVC)
if(VULKEN_ENGINE_ENABLE_SANITIZERS AND NOT MSVC)
  set(_VULKEN_SAN_FLAGS "-fsanitize=address,undefined")
  target_compile_options(vulken_engine PRIVATE
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:${_VULKEN_SAN_FLAGS}>
  )
  target_link_options(vulken_engine PRIVATE
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:${_VULKEN_SAN_FLAGS}>
  )
endif()

# Coverage (GCC/Clang, debug-like configs)
if(VULKEN_ENGINE_ENABLE_COVERAGE AND NOT MSVC AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(vulken_engine PRIVATE
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:-O0>
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:-g>
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:--coverage>
  )
  target_link_options(vulken_engine PRIVATE
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:--coverage>
  )
endif()

# Optional tests under src/engine/tests
if(VULKEN_ENGINE_WITH_TESTS)
  if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/tests/CMakeLists.txt")
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
  else()
    message(STATUS "vulken_engine: tests enabled, but no tests/ directory found under ${CMAKE_CURRENT_LIST_DIR}")
  endif()
endif()

# Dependencies (link real Vulkan, vk_mem_alloc, stb, etc. later as needed)
# target_link_libraries(vulken_engine PUBLIC Vulkan::Vulkan vk_mem_alloc stb)
