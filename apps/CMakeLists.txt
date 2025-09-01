find_package(Vulkan REQUIRED)

# --- Build tagging injection ---
set(VOXELVK_BRANCH_SOURCES "project_fixed + VoxelVK_Elite_ALL_release_ready + voxel_upgrades_patch + project_compute_graphics_chain + project_dynamic_pbr + project_next_tier_all")
include(${CMAKE_SOURCE_DIR}/cmake/GenBuildInfo.cmake)
include_directories(${CMAKE_BINARY_DIR}/generated)


add_executable(smoke_headless smoke_headless.cpp)
set_target_properties(smoke_headless PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
target_link_libraries(smoke_headless PRIVATE Vulkan::Vulkan)

add_executable(smoke_graphics_headless smoke_graphics_headless.cpp)
set_target_properties(smoke_graphics_headless PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
target_link_libraries(smoke_graphics_headless PRIVATE Vulkan::Vulkan)

# --- Build tagging sources ---
if(TARGET VoxelVK_Elite_ALL)
  target_sources(VoxelVK_Elite_ALL PRIVATE
    src/core/build_info_print.cpp
    src/core/runtime_debug.cpp
    src/core/gpu_compat.cpp)
endif()

# --- CSM+PCSS integration sources ---
if(TARGET VoxelVK_Elite_ALL)
  target_sources(VoxelVK_Elite_ALL PRIVATE src/render/csm_pass.cpp)
endif()

# --- CSM wiring helpers ---
if(TARGET VoxelVK_Elite_ALL)
  target_sources(VoxelVK_Elite_ALL PRIVATE src/render/csm_cpu.cpp src/render/csm_pass.cpp)
endif()

# --- CSM descriptor layout helper ---
if(TARGET VoxelVK_Elite_ALL)
  target_sources(VoxelVK_Elite_ALL PRIVATE src/render/csm_layout.cpp)
endif()
