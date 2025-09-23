# Apps subdirectory - additional app definitions
# Note: Core apps (smoke_headless, smoke_graphics_headless, gui_fullscreen_demo, vulkan_fullscreen_demo, rl_nav_demo)
# are defined in root CMakeLists.txt. This file only handles additional/legacy app configurations.

# World generator demo (headless)
add_executable(world_generator_demo world_generator_demo.cpp)
set_target_properties(world_generator_demo PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES)
target_include_directories(world_generator_demo PRIVATE ${CMAKE_SOURCE_DIR}/src)

# World visualizer demo (headless)
add_executable(world_visualizer_demo world_visualizer_demo.cpp)
set_target_properties(world_visualizer_demo PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES)
target_include_directories(world_visualizer_demo PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Simple world viewer demo (graphics only, requires OpenGL)
if(ENABLE_GRAPHICS)
  find_package(OpenGL QUIET)
  if(OpenGL_FOUND)
    add_executable(simple_world_viewer_demo simple_world_viewer_demo.cpp)
    set_target_properties(simple_world_viewer_demo PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES)
    if(GLFW_LIB)
      target_link_libraries(simple_world_viewer_demo PRIVATE ${GLFW_LIB})
    endif()
    target_link_libraries(simple_world_viewer_demo PRIVATE OpenGL::GL)
    target_include_directories(simple_world_viewer_demo PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_compile_definitions(simple_world_viewer_demo PRIVATE ENABLE_GRAPHICS=1)
  else()
    message(STATUS "OpenGL not found - skipping simple_world_viewer_demo")
  endif()
endif()

# Advanced terrain demo (graphics only)
if(ENABLE_GRAPHICS)
  add_executable(advanced_terrain_demo advanced_terrain_demo.cpp)
  set_target_properties(advanced_terrain_demo PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES)
  target_include_directories(advanced_terrain_demo PRIVATE ${CMAKE_SOURCE_DIR}/src)
  target_compile_definitions(advanced_terrain_demo PRIVATE ENABLE_GRAPHICS=1)
endif()

# Procedural world demo (graphics only)
if(ENABLE_GRAPHICS)
  add_executable(procedural_world_demo procedural_world_demo.cpp)
  set_target_properties(procedural_world_demo PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES)
  target_include_directories(procedural_world_demo PRIVATE ${CMAKE_SOURCE_DIR}/src)
  target_compile_definitions(procedural_world_demo PRIVATE ENABLE_GRAPHICS=1)
endif()

# Advanced 3D terrain demo (graphics only)
if(ENABLE_GRAPHICS)
  add_executable(advanced_3d_terrain_demo advanced_3d_terrain_demo.cpp)
  set_target_properties(advanced_3d_terrain_demo PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES)
  target_include_directories(advanced_3d_terrain_demo PRIVATE ${CMAKE_SOURCE_DIR}/src)
  target_compile_definitions(advanced_3d_terrain_demo PRIVATE ENABLE_GRAPHICS=1)
endif()

# Advanced texturing demo (graphics only)
if(ENABLE_GRAPHICS)
  add_executable(advanced_texturing_demo advanced_texturing_demo.cpp)
  set_target_properties(advanced_texturing_demo PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES)
  target_include_directories(advanced_texturing_demo PRIVATE ${CMAKE_SOURCE_DIR}/src)
  target_compile_definitions(advanced_texturing_demo PRIVATE ENABLE_GRAPHICS=1)
endif()

# GPU terrain demo (graphics only)
if(ENABLE_GRAPHICS)
  add_executable(gpu_terrain_demo gpu_terrain_demo.cpp)
  set_target_properties(gpu_terrain_demo PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES)
  target_include_directories(gpu_terrain_demo PRIVATE ${CMAKE_SOURCE_DIR}/src)
  target_compile_definitions(gpu_terrain_demo PRIVATE ENABLE_GRAPHICS=1)
endif()

# Marching cubes terrain demo (graphics only)
if(ENABLE_GRAPHICS)
  add_executable(marching_cubes_terrain_demo marching_cubes_terrain_demo.cpp)
  set_target_properties(marching_cubes_terrain_demo PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES)
  target_include_directories(marching_cubes_terrain_demo PRIVATE ${CMAKE_SOURCE_DIR}/src)
  target_compile_definitions(marching_cubes_terrain_demo PRIVATE ENABLE_GRAPHICS=1)
endif()

# Simplified marching cubes terrain demo (graphics only)
if(ENABLE_GRAPHICS)
  add_executable(simple_marching_cubes_demo simple_marching_cubes_demo.cpp)
  set_target_properties(simple_marching_cubes_demo PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES)
  target_include_directories(simple_marching_cubes_demo PRIVATE ${CMAKE_SOURCE_DIR}/src)
  target_compile_definitions(simple_marching_cubes_demo PRIVATE ENABLE_GRAPHICS=1)
endif()

# Advanced triplanar mapping demo (graphics only)
if(ENABLE_GRAPHICS)
  add_executable(advanced_triplanar_demo advanced_triplanar_demo.cpp)
  set_target_properties(advanced_triplanar_demo PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES)
  target_include_directories(advanced_triplanar_demo PRIVATE ${CMAKE_SOURCE_DIR}/src)
  target_compile_definitions(advanced_triplanar_demo PRIVATE ENABLE_GRAPHICS=1)
endif()

# GPU compute shader terrain demo (graphics only)
if(ENABLE_GRAPHICS)
  add_executable(gpu_compute_terrain_demo gpu_compute_terrain_demo.cpp)
  set_target_properties(gpu_compute_terrain_demo PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES)
  target_include_directories(gpu_compute_terrain_demo PRIVATE ${CMAKE_SOURCE_DIR}/src)
  target_compile_definitions(gpu_compute_terrain_demo PRIVATE ENABLE_GRAPHICS=1)
endif()

# Operator console - gated behind BUILD_EXTRAS
if(BUILD_EXTRAS)
  add_executable(operator_console operator_console.cpp)
  set_target_properties(operator_console PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES)
  target_link_libraries(operator_console PRIVATE Vulkan::Vulkan)
  target_include_directories(operator_console PRIVATE ${CMAKE_SOURCE_DIR}/src)
endif()

# --- Build tagging sources (graphics only) ---
if(TARGET VoxelVK_Elite_ALL AND ENABLE_GRAPHICS)
  target_sources(VoxelVK_Elite_ALL PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core/build_info_print.cpp
    ${CMAKE_SOURCE_DIR}/src/core/runtime_debug.cpp
    ${CMAKE_SOURCE_DIR}/src/core/gpu_compat.cpp)
endif()

# --- CSM sources (deduplicated, graphics only) ---
if(TARGET VoxelVK_Elite_ALL AND ENABLE_GRAPHICS)
  target_sources(VoxelVK_Elite_ALL PRIVATE
    ${CMAKE_SOURCE_DIR}/src/render/csm_cpu.cpp
    ${CMAKE_SOURCE_DIR}/src/render/csm_pass.cpp
    ${CMAKE_SOURCE_DIR}/src/render/csm_layout.cpp
  )
endif()
