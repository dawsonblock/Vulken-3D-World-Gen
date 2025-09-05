# Apps subdirectory - additional app definitions
# Note: Core apps (smoke_headless, smoke_graphics_headless, gui_fullscreen_demo, vulkan_fullscreen_demo, rl_nav_demo)
# are defined in root CMakeLists.txt. This file only handles additional/legacy app configurations.

# Operator console - gated behind BUILD_EXTRAS
if(BUILD_EXTRAS)
  add_executable(operator_console operator_console.cpp)
  set_target_properties(operator_console PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES)
  target_link_libraries(operator_console PRIVATE Vulkan::Vulkan)
  target_include_directories(operator_console PRIVATE ${CMAKE_SOURCE_DIR}/src)
endif()

# --- Build tagging sources ---
if(TARGET VoxelVK_Elite_ALL)
  target_sources(VoxelVK_Elite_ALL PRIVATE
    src/core/build_info_print.cpp
    src/core/runtime_debug.cpp
    src/core/gpu_compat.cpp)
endif()

# --- CSM sources (deduplicated) ---
if(TARGET VoxelVK_Elite_ALL)
  target_sources(VoxelVK_Elite_ALL PRIVATE
    src/render/csm_cpu.cpp
    src/render/csm_pass.cpp
    src/render/csm_layout.cpp
  )
endif()
