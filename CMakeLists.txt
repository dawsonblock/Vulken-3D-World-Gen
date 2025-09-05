cmake_minimum_required(VERSION 3.24...3.28 FATAL_ERROR)
project(VoxelVK_Elite_ALL LANGUAGES CXX)

# Set policies for modern CMake
cmake_policy(SET CMP0077 NEW)  # option() honors normal variables
cmake_policy(SET CMP0144 NEW)  # FindPython3 uses FindPython
cmake_policy(SET CMP0155 NEW)  # CMAKE_FIND_PACKAGE_PREFER_CONFIG

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Global warning flags
if(MSVC)
  add_compile_options(/W4 /permissive- /Zc:__cplusplus)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wdouble-promotion -Wformat=2 -Wno-unused-parameter)
  add_link_options(-Wl,--no-undefined)
endif()

# Toolchain selection is driven by CMakePresets and CI; avoid overriding here

# === AI System Sources ===
set(VOXELVK_ENABLE_AI ON CACHE BOOL "Enable AI system")

if(VOXELVK_ENABLE_AI)
  file(GLOB AI_SRC
    src/ai/ai_palette_config_io.cpp
    src/ai/ai_biome_palette.hpp
    src/ai/ai_imgui_palette_panel.cpp
  )
endif()

# === Weather System Sources (defined early for use by other components) ===
set(VOXELVK_ENABLE_WEATHER ON CACHE BOOL "Enable weather system")

if(VOXELVK_ENABLE_WEATHER)
  file(GLOB WEATHER_SRC
    src/env/weather/weather_system.cpp
    src/env/weather/sky_model.cpp
    src/env/weather/lightning.cpp
    src/render/framegraph/frame_graph_min.cpp
    src/render/post/temporal_accum.cpp
    src/render/post/height_fog.cpp
  )
  
  # Find yaml-cpp dependency
  find_package(yaml-cpp QUIET)
  if(NOT yaml-cpp_FOUND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
      pkg_check_modules(YAML_CPP QUIET yaml-cpp)
    endif()
  endif()

  # Prefer modern CMake target if available; otherwise, wrap pkg-config in an INTERFACE target; else legacy
  if(TARGET yaml-cpp::yaml-cpp)
    set(WEATHER_YAML_LIB yaml-cpp::yaml-cpp)
    message(STATUS "Found yaml-cpp target: yaml-cpp::yaml-cpp")
  elseif(YAML_CPP_FOUND)
    if(NOT TARGET yaml_cpp_pkg)
      add_library(yaml_cpp_pkg INTERFACE)
      target_include_directories(yaml_cpp_pkg INTERFACE ${YAML_CPP_INCLUDE_DIRS})
      target_link_libraries(yaml_cpp_pkg INTERFACE ${YAML_CPP_LIBRARIES})
      add_library(yaml-cpp::yaml-cpp ALIAS yaml_cpp_pkg)
    endif()
    set(WEATHER_YAML_LIB yaml-cpp::yaml-cpp)
    message(STATUS "Found yaml-cpp via pkg-config (wrapped as target)")
  elseif(yaml-cpp_FOUND)
    # Legacy target alias maintained for backward compatibility in some distros
    set(WEATHER_YAML_LIB yaml-cpp)
    message(WARNING "Using deprecated target 'yaml-cpp'. Consider switching to yaml-cpp::yaml-cpp")
  else()
    message(WARNING "yaml-cpp not found - weather system will use fallback config")
  endif()
endif()

option(ENABLE_VALIDATION_LAYERS "Enable Vulkan validation layers" ON)
option(ENABLE_VK_DEBUG_MARKERS  "Enable VK_EXT_debug_utils markers" ON)
option(ENABLE_IMGUI_OVERLAY     "Enable ImGui debug HUD overlay" ON)
option(ENABLE_TESTS             "Enable CTest targets" ON)
option(VOXELVK_ENABLE_CUDA      "Enable CUDA acceleration" OFF)
option(VOXELVK_ENABLE_TENSORRT  "Enable TensorRT (requires CUDA)" OFF)
option(VOXELVK_HEADLESS_ONLY    "Build minimal headless utilities without Vulkan/GLFW" OFF)
option(VOXELVK_DEMO_WERROR      "Treat warnings as errors for demo/sample apps only" OFF)
set(VOXELVK_WERROR_TARGETS "" CACHE STRING "Semicolon-separated list of targets to build with -Werror; defaults to core demos if empty")
option(VOXELVK_ENABLE_REDIS     "Build Redis-backed asset loader and link demos against it" ON)
option(VOXELVK_ENABLE_ASAN      "Enable AddressSanitizer" OFF)
option(VOXELVK_ENABLE_UBSAN     "Enable UndefinedBehaviorSanitizer" OFF)

# Propagate debug markers flag to all targets when enabled
add_compile_definitions(
  $<$<BOOL:${ENABLE_VK_DEBUG_MARKERS}>:ENABLE_VK_DEBUG_MARKERS=1>
)

# Sanitizer configuration
if(VOXELVK_ENABLE_ASAN OR VOXELVK_ENABLE_UBSAN)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    if(VOXELVK_ENABLE_ASAN)
      add_compile_options(-fsanitize=address)
      add_link_options(-fsanitize=address)
    endif()
    if(VOXELVK_ENABLE_UBSAN)
      add_compile_options(-fsanitize=undefined)
      add_link_options(-fsanitize=undefined)
    endif()
    # Disable optimizations for sanitizers
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
    message(STATUS "Sanitizers enabled - forcing Debug build")
  else()
    message(WARNING "Sanitizers only supported with Clang/GCC")
  endif()
endif()

# Include shader compilation system
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CompileShaders)

# Enhanced shader build pipeline
option(VOXELVK_USE_PYTHON_SHADER_BUILD "Use Python shader build pipeline" ON)
if(VOXELVK_USE_PYTHON_SHADER_BUILD)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  
  set(SHADER_SOURCE_DIRS "shaders" "shaders_vk")
  set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/resources/spv")
  
  add_custom_target(compile_all_shaders
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/shader_build.py
      --source-dirs ${SHADER_SOURCE_DIRS}
      --output-dir ${SHADER_OUTPUT_DIR}
      --verbose
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Building shaders with Python pipeline"
    VERBATIM
  )
  
  # Make shader compilation a dependency of the main build
  add_dependencies(ShaderSPV compile_all_shaders)
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build type" FORCE)
endif()

# Force use of system libstdc++ instead of conda's
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU" AND EXISTS "/usr/lib/x86_64-linux-gnu/libstdc++.so.6")
  add_link_options(-Wl,--rpath=/usr/lib/x86_64-linux-gnu)
  add_link_options(-L/usr/lib/x86_64-linux-gnu)
endif()

# --- Build tagging injection ---
set(VOXELVK_BRANCH_SOURCES "project_fixed + VoxelVK_Elite_ALL_release_ready + voxel_upgrades_patch + project_compute_graphics_chain + project_dynamic_pbr + project_next_tier_all")
include(${CMAKE_SOURCE_DIR}/cmake/GenBuildInfo.cmake)
include_directories(${CMAKE_BINARY_DIR}/generated)

find_package(Threads REQUIRED)
find_package(nlohmann_json QUIET)
if(nlohmann_json_FOUND)
  set(JSON_LIB nlohmann_json::nlohmann_json)
  message(STATUS "Found nlohmann_json for JSON support")
else()
  message(WARNING "nlohmann_json not found - JSON features will be disabled or may fail to compile")
endif()

# Find GLM (OpenGL Mathematics)
find_package(glm QUIET)
if(NOT glm_FOUND)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(GLM QUIET glm)
  endif()
endif()

if(glm_FOUND OR GLM_FOUND)
  set(GLM_LIB glm::glm)
  if(GLM_FOUND)
    set(GLM_LIB ${GLM_LIBRARIES})
  endif()
  message(STATUS "Found GLM for mathematics")
else()
  message(WARNING "GLM not found - math functionality may not link")
endif()

# Only configure graphics stack when not headless-only
if(NOT VOXELVK_HEADLESS_ONLY)
  find_package(Vulkan REQUIRED)

  # VMA (Vulkan Memory Allocator) integration
  find_package(VulkanMemoryAllocator QUIET)
  if(VulkanMemoryAllocator_FOUND)
    set(VMA_TARGET GPUOpen::VulkanMemoryAllocator)
    message(STATUS "VulkanMemoryAllocator found")
  else()
    # Fallback: look for the header manually
    find_path(VMA_INCLUDE_DIR vk_mem_alloc.h HINTS /usr/include)
    if(VMA_INCLUDE_DIR)
      if(NOT TARGET vma_header)
        add_library(vma_header INTERFACE)
        target_include_directories(vma_header INTERFACE ${VMA_INCLUDE_DIR})
      endif()
      set(VMA_TARGET vma_header)
      message(STATUS "VMA header found at: ${VMA_INCLUDE_DIR}")
    else()
      message(WARNING "VulkanMemoryAllocator not found - CSM functionality may not link")
    endif()
  endif()

  # Find GLFW
  find_package(glfw3 QUIET)
  if(glfw3_FOUND)
    set(GLFW_LIB glfw)
  else()
    find_package(GLFW REQUIRED)
    set(GLFW_LIB GLFW::GLFW)
  endif()
endif()

# === RL (Reinforcement Learning) Backend Systems ===
set(VOXELVK_ENABLE_RL ON CACHE BOOL "Enable reinforcement learning systems")
set(VOXELVK_RL_MINIMAL ON CACHE BOOL "Enable minimal CPU MLP RL backend")

if(VOXELVK_ENABLE_RL)
  # RL backend sources
  file(GLOB RL_BACKEND_SRC
    src/rl/rl_backend_dummy.cpp
    src/rl/rl_backend_minimal.cpp
  )
  
  list(APPEND VOXELVK_ELITE_SOURCES ${RL_BACKEND_SRC})
  
  # RL navigation demo
  if(VOXELVK_RL_MINIMAL AND NOT VOXELVK_HEADLESS_ONLY)
    add_executable(rl_nav_demo apps/rl_nav_demo.cpp ${RL_BACKEND_SRC} src/core/logger.cpp)
    target_include_directories(rl_nav_demo PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(rl_nav_demo PRIVATE Threads::Threads)
    target_compile_definitions(rl_nav_demo PRIVATE VOXELVK_ENABLE_RL=1)
  endif()
endif()

# === Enhanced Applications ===
if(NOT VOXELVK_HEADLESS_ONLY)
  # Production ImGui application with Vulkan backend
  add_executable(main_imgui_vulkan apps/main_imgui_vulkan.cpp ${WEATHER_SRC} src/core/logger.cpp src/core/performance_monitor.cpp ${AI_SRC})
  target_include_directories(main_imgui_vulkan PRIVATE ${CMAKE_SOURCE_DIR}/src)
  # Provide stb include dir (vcpkg header-only) for Linux screenshots
  if(EXISTS "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/include/stb_image_write.h")
    target_include_directories(main_imgui_vulkan PRIVATE "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/include")
  endif()
  target_link_libraries(main_imgui_vulkan PRIVATE Vulkan::Vulkan Threads::Threads ${GLFW_LIB})
  # Link ImGui if available (provides headers/backends include dirs via target)
  find_package(imgui QUIET)
  if(imgui_FOUND)
    target_link_libraries(main_imgui_vulkan PRIVATE imgui::imgui)
  else()
    message(WARNING "imgui not found; main_imgui_vulkan will run in minimal mode without Vulkan backend UI")
  endif()
  if(JSON_LIB)
    target_link_libraries(main_imgui_vulkan PRIVATE ${JSON_LIB})
  endif()
  # Link X11 for Linux-specific screenshot helper if available
  find_package(X11 QUIET)
  if(X11_FOUND)
    target_include_directories(main_imgui_vulkan PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(main_imgui_vulkan PRIVATE ${X11_LIBRARIES})
  endif()
  add_dependencies(main_imgui_vulkan ShaderSPV)
  
  if(WEATHER_YAML_LIB)
    target_link_libraries(main_imgui_vulkan PRIVATE ${WEATHER_YAML_LIB})
    target_compile_definitions(main_imgui_vulkan PRIVATE VOXELVK_ENABLE_WEATHER=1)
  endif()
  
  if(GLM_LIB)
    target_link_libraries(main_imgui_vulkan PRIVATE ${GLM_LIB})
  endif()
  
  target_compile_definitions(main_imgui_vulkan PRIVATE
    VOXELVK_ENABLE_P0_RELIABILITY=1
    VOXELVK_ENABLE_P1_MEMORY=1
    VOXELVK_ENABLE_P2_PERFORMANCE=1
  )
endif()

# === P2 Frame Pacing & Performance ===
set(VOXELVK_ENABLE_P2_PERFORMANCE ON CACHE BOOL "Enable P2 frame pacing and performance systems")

if(VOXELVK_ENABLE_P2_PERFORMANCE)
  # P2 frame pacing and performance sources
  file(GLOB P2_PERFORMANCE_SRC
    src/render/frame_graph.cpp
    src/render/taa_system.cpp
    src/render/screen_space_effects.cpp
    src/core/performance_monitor.cpp
  )
  
  list(APPEND VOXELVK_ELITE_SOURCES ${P2_PERFORMANCE_SRC})
  
  # P2 performance shaders
  set(P2_SHADERS
    shaders_vk/taa/motion_vectors.vert
    shaders_vk/taa/motion_vectors.frag
    shaders_vk/taa/taa_resolve.frag
    shaders_vk/taa/weather_taa_blend.frag
    shaders_vk/post/ssao.frag
    shaders_vk/post/ssr.frag
    shaders_vk/post/ssao_blur.frag
  )
  
  # Ensure P2 shaders are in the compilation list
  foreach(SHADER IN LISTS P2_SHADERS)
    if(EXISTS "${CMAKE_SOURCE_DIR}/${SHADER}")
      # Shader will be compiled by existing GLSL_SOURCES glob
    endif()
  endforeach()
  
  # P2 concepts demo (simplified)
  if(NOT VOXELVK_HEADLESS_ONLY)
    add_executable(p2_concepts_validator apps/p2_concepts_validator.cpp ${WEATHER_SRC} src/core/logger.cpp)
    target_include_directories(p2_concepts_validator PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(p2_concepts_validator PRIVATE Threads::Threads)
    
    if(WEATHER_YAML_LIB)
      target_link_libraries(p2_concepts_validator PRIVATE ${WEATHER_YAML_LIB})
      target_compile_definitions(p2_concepts_validator PRIVATE VOXELVK_ENABLE_WEATHER=1)
    endif()
    
    target_compile_definitions(p2_concepts_validator PRIVATE 
      VOXELVK_ENABLE_P2_PERFORMANCE=1
    )
    
    # P2 120 FPS target validator  
    add_executable(p2_target_validator apps/p2_target_validator.cpp ${WEATHER_SRC} src/core/logger.cpp)
    target_include_directories(p2_target_validator PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(p2_target_validator PRIVATE Threads::Threads)
    
    if(WEATHER_YAML_LIB)
      target_link_libraries(p2_target_validator PRIVATE ${WEATHER_YAML_LIB})
      target_compile_definitions(p2_target_validator PRIVATE VOXELVK_ENABLE_WEATHER=1)
    endif()
  endif()
endif()

# === P1 Memory & Resource Management ===
set(VOXELVK_ENABLE_P1_MEMORY ON CACHE BOOL "Enable P1 memory management systems")

if(VOXELVK_ENABLE_P1_MEMORY)
  # Try to find VMA (Vulkan Memory Allocator)
  find_package(VulkanMemoryAllocator QUIET)
  
  if(VulkanMemoryAllocator_FOUND)
    set(VMA_TARGET GPUOpen::VulkanMemoryAllocator)
    message(STATUS "VulkanMemoryAllocator found via package")
  else()
    # Fallback: look for VMA header manually  
    find_path(VMA_INCLUDE_DIR vk_mem_alloc.h 
      HINTS /usr/include /usr/local/include ${CMAKE_SOURCE_DIR}/third_party
      PATH_SUFFIXES vma VulkanMemoryAllocator)
      
    if(VMA_INCLUDE_DIR)
      if(NOT TARGET vma_header)
        add_library(vma_header INTERFACE)
        target_include_directories(vma_header INTERFACE ${VMA_INCLUDE_DIR})
      endif()
      set(VMA_TARGET vma_header)
      message(STATUS "VMA header found at: ${VMA_INCLUDE_DIR}")
    else()
      message(WARNING "VMA not found - P1 memory management will use fallback")
      set(VOXELVK_ENABLE_P1_MEMORY OFF)
    endif()
  endif()
  
  if(VOXELVK_ENABLE_P1_MEMORY)
    # P1 memory management sources
    file(GLOB P1_MEMORY_SRC
      src/vk/memory_manager.cpp
      src/vk/frame_allocator.cpp
      src/render/texture_manager.cpp
      src/render/mesh_optimizer.cpp
    )
    
    list(APPEND VOXELVK_ELITE_SOURCES ${P1_MEMORY_SRC})
    
    # P1 concepts demo (simplified)
    if(NOT VOXELVK_HEADLESS_ONLY)
      add_executable(p1_concepts_simple apps/p1_concepts_simple.cpp ${WEATHER_SRC} src/core/logger.cpp)
      target_include_directories(p1_concepts_simple PRIVATE ${CMAKE_SOURCE_DIR}/src)
      target_link_libraries(p1_concepts_simple PRIVATE Threads::Threads)
      
      if(WEATHER_YAML_LIB)
        target_link_libraries(p1_concepts_simple PRIVATE ${WEATHER_YAML_LIB})
        target_compile_definitions(p1_concepts_simple PRIVATE VOXELVK_ENABLE_WEATHER=1)
      endif()
      
      target_compile_definitions(p1_concepts_simple PRIVATE 
        VOXELVK_ENABLE_P1_MEMORY=1
      )
    endif()
  endif()
endif()

# === P0 Reliability Core ===
set(VOXELVK_ENABLE_P0_RELIABILITY ON CACHE BOOL "Enable P0 production reliability systems")

if(VOXELVK_ENABLE_P0_RELIABILITY)
  # P0 reliability sources
  file(GLOB P0_RELIABILITY_SRC
    src/vk/device_caps.cpp
    src/vk/error_handling.cpp
    src/vk/swapchain_manager.cpp
    src/vk/pipeline_cache_manager.cpp
  )
  
  list(APPEND VOXELVK_ELITE_SOURCES ${P0_RELIABILITY_SRC})
  
  # P0 demo application (simplified)
  if(NOT VOXELVK_HEADLESS_ONLY)
    add_executable(p0_reliability_test apps/p0_reliability_test.cpp ${WEATHER_SRC} src/core/logger.cpp)
    target_include_directories(p0_reliability_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(p0_reliability_test PRIVATE Threads::Threads)
    
    if(WEATHER_YAML_LIB)
      target_link_libraries(p0_reliability_test PRIVATE ${WEATHER_YAML_LIB})
      target_compile_definitions(p0_reliability_test PRIVATE VOXELVK_ENABLE_WEATHER=1)
    endif()
    
    target_compile_definitions(p0_reliability_test PRIVATE 
      VOXELVK_ENABLE_P0_RELIABILITY=1
    )
  endif()
endif()

# === Weather System ===
# (Sources defined earlier in file)

if(VOXELVK_ENABLE_WEATHER)
  # Weather shaders
  set(WEATHER_SHADERS
    shaders_vk/common/weather_ubo.glsl
    shaders_vk/sky/sky_fullscreen.vert
    shaders_vk/sky/sky_hw.frag
    shaders_vk/clouds/clouds_fullscreen.frag
    shaders_vk/particles/precip_update.comp
    shaders_vk/particles/precip_render.vert
    shaders_vk/particles/precip_render.frag
    shaders_vk/post/temporal_accum.comp
    shaders_vk/post/height_fog.frag
    shaders_vk/material/weather_material.glsl
  )
  
  # Ensure weather shaders are compiled
  foreach(SHADER IN LISTS WEATHER_SHADERS)
    if(EXISTS "${CMAKE_SOURCE_DIR}/${SHADER}")
      get_filename_component(SHADER_NAME ${SHADER} NAME)
      get_filename_component(SHADER_DIR ${SHADER} DIRECTORY)
      file(RELATIVE_PATH REL_DIR "${CMAKE_SOURCE_DIR}/shaders_vk" "${CMAKE_SOURCE_DIR}/${SHADER_DIR}")
      set(OUT_DIR "${SPV_DIR}/${REL_DIR}")
      set(SPV_FILE "${OUT_DIR}/${SHADER_NAME}.spv")
      # Already handled by existing GLSL_SOURCES glob
    endif()
  endforeach()
endif()

# --- Targets ---
# Minimal smoke target always available
add_executable(smoke_headless apps/smoke_headless.cpp)
if(NOT VOXELVK_HEADLESS_ONLY)
  add_dependencies(smoke_headless ShaderSPV)
  target_include_directories(smoke_headless PRIVATE ${CMAKE_SOURCE_DIR}/src)
  target_link_libraries(smoke_headless PRIVATE Vulkan::Vulkan Threads::Threads ${GLFW_LIB})
  target_compile_definitions(smoke_headless PRIVATE
    $<$<BOOL:${ENABLE_VALIDATION_LAYERS}>:ENABLE_VALIDATION_LAYERS=1>
    $<$<BOOL:${ENABLE_IMGUI_OVERLAY}>:ENABLE_IMGUI_OVERLAY=1>)
else()
  # Headless-only: no Vulkan/GLFW linkage required
  target_link_libraries(smoke_headless PRIVATE Threads::Threads)
endif()

if(NOT VOXELVK_HEADLESS_ONLY)
  add_executable(smoke_graphics_headless apps/smoke_graphics_headless.cpp)
  add_dependencies(smoke_graphics_headless ShaderSPV)
  target_include_directories(smoke_graphics_headless PRIVATE ${CMAKE_SOURCE_DIR}/src)
  target_link_libraries(smoke_graphics_headless PRIVATE Vulkan::Vulkan Threads::Threads ${GLFW_LIB})
  target_compile_definitions(smoke_graphics_headless PRIVATE
    $<$<BOOL:${ENABLE_VALIDATION_LAYERS}>:ENABLE_VALIDATION_LAYERS=1>
    $<$<BOOL:${ENABLE_IMGUI_OVERLAY}>:ENABLE_IMGUI_OVERLAY=1>)
endif()

# CUDA/TRT guards if you later add GPU/AI targets
if(NOT VOXELVK_HEADLESS_ONLY)
  target_compile_definitions(smoke_headless PRIVATE
    $<$<BOOL:${VOXELVK_ENABLE_CUDA}>:VOXELVK_ENABLE_CUDA=1>
    $<$<BOOL:${VOXELVK_ENABLE_TENSORRT}>:VOXELVK_ENABLE_TENSORRT=1>)
endif()

# Full engine target only when graphics stack is available
if(NOT VOXELVK_HEADLESS_ONLY)
  # --- Build tagging sources ---
  list(APPEND VOXELVK_ELITE_SOURCES
    src/core/build_info_print.cpp
    src/core/runtime_debug.cpp
    src/core/gpu_compat.cpp
    src/core/vma_impl.cpp)

  # --- CSM+PCSS integration sources ---
  list(APPEND VOXELVK_ELITE_SOURCES src/render/csm_pass.cpp)

  # --- CSM wiring helpers ---
  list(APPEND VOXELVK_ELITE_SOURCES src/render/csm_cpu.cpp)

  # --- CSM descriptor layout helper ---
  list(APPEND VOXELVK_ELITE_SOURCES src/render/csm_layout.cpp)
  
  # --- Weather System Integration ---
  if(VOXELVK_ENABLE_WEATHER)
    list(APPEND VOXELVK_ELITE_SOURCES ${WEATHER_SRC})
  endif()

  if(NOT TARGET VoxelVK_Elite_ALL)
  add_executable(VoxelVK_Elite_ALL apps/smoke_headless.cpp ${VOXELVK_ELITE_SOURCES} src/core/logger.cpp src/core/thread_pool.cpp)
    add_dependencies(VoxelVK_Elite_ALL ShaderSPV)
    target_include_directories(VoxelVK_Elite_ALL PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(VoxelVK_Elite_ALL PRIVATE Vulkan::Vulkan Threads::Threads ${GLFW_LIB})
    if(JSON_LIB)
      target_link_libraries(VoxelVK_Elite_ALL PRIVATE ${JSON_LIB})
    endif()
    if(VMA_TARGET)
      target_link_libraries(VoxelVK_Elite_ALL PRIVATE ${VMA_TARGET})
    endif()
    if(GLM_LIB)
      target_link_libraries(VoxelVK_Elite_ALL PRIVATE ${GLM_LIB})
    endif()
    target_compile_definitions(VoxelVK_Elite_ALL PRIVATE
      $<$<BOOL:${ENABLE_VALIDATION_LAYERS}>:ENABLE_VALIDATION_LAYERS=1>
      $<$<BOOL:${ENABLE_IMGUI_OVERLAY}>:ENABLE_IMGUI_OVERLAY=1>)
    # Link weather system dependencies
    if(VOXELVK_ENABLE_WEATHER AND WEATHER_YAML_LIB)
      target_link_libraries(VoxelVK_Elite_ALL PRIVATE ${WEATHER_YAML_LIB})
      target_compile_definitions(VoxelVK_Elite_ALL PRIVATE VOXELVK_ENABLE_WEATHER=1)
    endif()
    target_compile_definitions(VoxelVK_Elite_ALL PRIVATE
      $<$<BOOL:${VOXELVK_ENABLE_CUDA}>:VOXELVK_ENABLE_CUDA=1>
      $<$<BOOL:${VOXELVK_ENABLE_TENSORRT}>:VOXELVK_ENABLE_TENSORRT=1>)
  endif()
  # Demo: GUI fullscreen toggle (requires GLFW + OpenGL)
  find_package(OpenGL QUIET)
  if(OpenGL_FOUND)
    add_executable(gui_fullscreen_demo apps/gui_fullscreen_demo.cpp ${AI_SRC})
    target_link_libraries(gui_fullscreen_demo PRIVATE ${GLFW_LIB} OpenGL::GL)
    add_dependencies(gui_fullscreen_demo ShaderSPV)
  else()
    message(WARNING "OpenGL not found; gui_fullscreen_demo will be skipped")
  endif()

  # Weather system demo and integration test
  if(VOXELVK_ENABLE_WEATHER)
    add_executable(weather_demo apps/weather_demo.cpp ${WEATHER_SRC})
    target_include_directories(weather_demo PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(weather_demo PRIVATE Threads::Threads)
    if(JSON_LIB)
      target_link_libraries(weather_demo PRIVATE ${JSON_LIB})
    endif()
    if(WEATHER_YAML_LIB)
      target_link_libraries(weather_demo PRIVATE ${WEATHER_YAML_LIB})
      target_compile_definitions(weather_demo PRIVATE VOXELVK_ENABLE_WEATHER=1)
    endif()

    add_executable(weather_integration_test apps/weather_integration_test.cpp ${WEATHER_SRC})
    target_include_directories(weather_integration_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(weather_integration_test PRIVATE Threads::Threads)
    if(JSON_LIB)
      target_link_libraries(weather_integration_test PRIVATE ${JSON_LIB})
    endif()
    if(WEATHER_YAML_LIB)
      target_link_libraries(weather_integration_test PRIVATE ${WEATHER_YAML_LIB})
      target_compile_definitions(weather_integration_test PRIVATE VOXELVK_ENABLE_WEATHER=1)
    endif()
  endif()

endif() # NOT VOXELVK_HEADLESS_ONLY

  # Optional RAG integrations (FAISS, ONNX Runtime) and GUI demos
  if(NOT VOXELVK_HEADLESS_ONLY)
    # Use soft-detection to avoid breaking builds if libs not available
    find_package(PkgConfig QUIET)
    set(VOXELVK_HAS_FAISS 0)
    set(VOXELVK_HAS_ONNXRUNTIME 0)
    # Link ImGui to demo if available via vcpkg (header+backends)
    find_package(imgui QUIET)
    if(TARGET gui_fullscreen_demo)
      if(imgui_FOUND)
        target_link_libraries(gui_fullscreen_demo PRIVATE imgui::imgui)
        target_compile_definitions(gui_fullscreen_demo PRIVATE GUI_DEMO_HAS_IMGUI_VCPKG=1)
      endif()
    endif()
    # Production-grade main ImGui fullscreen app (optional)
    add_executable(main_imgui_fullscreen apps/main_imgui_fullscreen.cpp ${AI_SRC})
    target_link_libraries(main_imgui_fullscreen PRIVATE ${GLFW_LIB})
    if(imgui_FOUND)
      target_link_libraries(main_imgui_fullscreen PRIVATE imgui::imgui)
    endif()
    if(OpenGL_FOUND)
      target_link_libraries(main_imgui_fullscreen PRIVATE OpenGL::GL)
    endif()

    # Vulkan fullscreen demo (optional): GLFW Vulkan window + swapchain recreate on toggle
    option(VOXELVK_BUILD_VULKAN_FULLSCREEN_DEMO "Build the simple Vulkan fullscreen demo" ON)
    if(VOXELVK_BUILD_VULKAN_FULLSCREEN_DEMO)
      add_executable(vulkan_fullscreen_demo apps/vulkan_fullscreen_demo.cpp ${AI_SRC})
      target_link_libraries(vulkan_fullscreen_demo PRIVATE ${GLFW_LIB} Vulkan::Vulkan)
    endif()

    # Minimal X11 visual GUI (no GL/Vulkan)
    find_package(X11)
    if(X11_FOUND)
      add_executable(x11_basic_demo apps/x11_basic_demo.cpp)
      target_include_directories(x11_basic_demo PRIVATE ${X11_INCLUDE_DIR})
      target_link_libraries(x11_basic_demo PRIVATE ${X11_LIBRARIES})
    endif()
  endif()


    if(TARGET gui_fullscreen_demo)
      message(STATUS "Linking ImGui to gui_fullscreen_demo")
    endif()

  if(PkgConfig_FOUND)
    pkg_check_modules(FAISS QUIET faiss)
    if(FAISS_FOUND)
      add_compile_definitions(VOXELVK_HAS_FAISS=1)
      include_directories(${FAISS_INCLUDE_DIRS})
      link_libraries(${FAISS_LIBRARIES})
      message(STATUS "FAISS found for RAG")
    endif()
    pkg_check_modules(ONNXRUNTIME QUIET onnxruntime)
    if(ONNXRUNTIME_FOUND)
      add_compile_definitions(VOXELVK_HAS_ONNXRUNTIME=1)
      include_directories(${ONNXRUNTIME_INCLUDE_DIRS})
      link_libraries(${ONNXRUNTIME_LIBRARIES})
      message(STATUS "ONNX Runtime found for RAG")
    endif()
  endif()

# Add engine sublibrary
add_subdirectory(src/engine)

# Link into selected apps if they exist
foreach(tgt IN ITEMS p0_reliability_test p1_concepts_simple p2_concepts_validator weather_integration_test weather_demo)
  if(TARGET ${tgt})
    target_link_libraries(${tgt} PRIVATE vulken_engine)
  endif()
endforeach()

# Tests
if(ENABLE_TESTS)
  include(CTest)
  enable_testing()
  
  # Core functionality tests
  add_executable(test_shaders_compile tests/test_shaders_compile.cpp)
  add_dependencies(test_shaders_compile ShaderSPV)
  add_test(NAME test_shaders_compile COMMAND test_shaders_compile)
  set_tests_properties(test_shaders_compile PROPERTIES TIMEOUT 30)
  
  add_test(NAME test_smoke_headless COMMAND smoke_headless)
  set_tests_properties(test_smoke_headless PROPERTIES TIMEOUT 10)
  
  # Weather system tests
  if(VOXELVK_ENABLE_WEATHER)
    add_executable(test_weather_system tests/test_weather_system.cpp ${WEATHER_SRC} src/core/logger.cpp)
    target_include_directories(test_weather_system PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_weather_system PRIVATE Threads::Threads)
    if(WEATHER_YAML_LIB)
      target_link_libraries(test_weather_system PRIVATE ${WEATHER_YAML_LIB})
      target_compile_definitions(test_weather_system PRIVATE VOXELVK_ENABLE_WEATHER=1)
    endif()
    add_test(NAME test_weather_system COMMAND test_weather_system)
    set_tests_properties(test_weather_system PROPERTIES TIMEOUT 30)
  endif()
  
  # RL backend tests
  if(VOXELVK_ENABLE_RL)
    add_executable(test_rl_backend tests/test_rl_backend.cpp ${RL_BACKEND_SRC} src/core/logger.cpp)
    target_include_directories(test_rl_backend PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_rl_backend PRIVATE Threads::Threads)
    target_compile_definitions(test_rl_backend PRIVATE VOXELVK_ENABLE_RL=1)
    
    add_test(NAME test_rl_backend COMMAND test_rl_backend)
    set_tests_properties(test_rl_backend PROPERTIES TIMEOUT 30)
  endif()
  
  # Performance monitoring tests
  if(VOXELVK_ENABLE_P2_PERFORMANCE)
    if(NOT VOXELVK_HEADLESS_ONLY)
      add_executable(test_performance_monitoring tests/test_performance_monitoring.cpp src/core/logger.cpp src/core/performance_monitor.cpp)
      target_include_directories(test_performance_monitoring PRIVATE ${CMAKE_SOURCE_DIR}/src)
      target_link_libraries(test_performance_monitoring PRIVATE Threads::Threads Vulkan::Vulkan)
      if(JSON_LIB)
        target_link_libraries(test_performance_monitoring PRIVATE ${JSON_LIB})
      endif()
      target_compile_definitions(test_performance_monitoring PRIVATE VOXELVK_ENABLE_P2_PERFORMANCE=1)
      add_test(NAME test_performance_monitoring COMMAND test_performance_monitoring)
      set_tests_properties(test_performance_monitoring PROPERTIES TIMEOUT 30)
    endif()
  endif()
  
  # Integration tests
  if(NOT VOXELVK_HEADLESS_ONLY)
    add_executable(test_integration tests/test_integration.cpp ${WEATHER_SRC} src/core/logger.cpp src/core/performance_monitor.cpp)
    target_include_directories(test_integration PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_integration PRIVATE Threads::Threads Vulkan::Vulkan)
    if(JSON_LIB)
      target_link_libraries(test_integration PRIVATE ${JSON_LIB})
    endif()
    if(WEATHER_YAML_LIB)
      target_link_libraries(test_integration PRIVATE ${WEATHER_YAML_LIB})
      target_compile_definitions(test_integration PRIVATE VOXELVK_ENABLE_WEATHER=1)
    endif()
    if(VOXELVK_ENABLE_RL)
      target_sources(test_integration PRIVATE ${RL_BACKEND_SRC})
      target_compile_definitions(test_integration PRIVATE VOXELVK_ENABLE_RL=1)
    endif()
    add_test(NAME test_integration COMMAND test_integration)
    set_tests_properties(test_integration PROPERTIES TIMEOUT 60)
  endif()

  # Also include tests/ subdirectory once
  add_subdirectory(tests)
endif()

message(STATUS "=== Build Summary ===")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
if(NOT VOXELVK_HEADLESS_ONLY)
  message(STATUS "Shader Compiler: ${SHADER_COMPILER}")
else()
  message(STATUS "Headless-only build: Vulkan/GLFW disabled")
endif()
message(STATUS "=====================")

# Tools
add_subdirectory(tools)

# Redis dependencies (optional)
if(VOXELVK_ENABLE_REDIS)
  find_path(REDISPLUSPLUS_INCLUDE_DIR sw)
  find_library(REDISPLUSPLUS_LIBRARY NAMES redis++ redisplusplus)
  find_library(HIREDIS_LIBRARY NAMES hiredis)
  if(NOT REDISPLUSPLUS_INCLUDE_DIR OR NOT REDISPLUSPLUS_LIBRARY OR NOT HIREDIS_LIBRARY)
    message(FATAL_ERROR "redis-plus-plus / hiredis not found. Install via vcpkg or disable with -DVOXELVK_ENABLE_REDIS=OFF.")
  endif()

  add_library(redis_assets src/redis_asset_store.cpp)
  target_include_directories(redis_assets PUBLIC ${REDISPLUSPLUS_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src)

  # Ensure yaml-cpp is linked for config parsing used by redis_assets
  set(REDIS_YAML_LIB "")
  if(DEFINED WEATHER_YAML_LIB AND WEATHER_YAML_LIB)
    set(REDIS_YAML_LIB ${WEATHER_YAML_LIB})
  else()
    find_package(yaml-cpp QUIET)
    if(TARGET yaml-cpp::yaml-cpp)
      set(REDIS_YAML_LIB yaml-cpp::yaml-cpp)
    elseif(yaml-cpp_FOUND)
      set(REDIS_YAML_LIB yaml-cpp)
    elseif(TARGET yaml_cpp_pkg)
      set(REDIS_YAML_LIB yaml-cpp::yaml-cpp)
    endif()
  endif()

  target_link_libraries(redis_assets PUBLIC ${REDISPLUSPLUS_LIBRARY} ${HIREDIS_LIBRARY} ${REDIS_YAML_LIB})
  target_compile_features(redis_assets PUBLIC cxx_std_20)

  # Link into chosen apps for hot-reload (only if those targets exist)
  if(TARGET gui_fullscreen_demo)
    target_link_libraries(gui_fullscreen_demo PRIVATE redis_assets)
  endif()
  if(TARGET vulkan_fullscreen_demo)
    target_link_libraries(vulkan_fullscreen_demo PRIVATE redis_assets)
  endif()
  if(TARGET smoke_graphics_headless)
    target_link_libraries(smoke_graphics_headless PRIVATE redis_assets)
  endif()
endif()

# Optionally enforce warnings-as-errors on demo/sample executables only
if(VOXELVK_DEMO_WERROR)
  set(_WX_FLAG "")
  if(MSVC)
    set(_WX_FLAG "/WX")
  else()
    set(_WX_FLAG "-Werror")
  endif()

  # Default set kept lightweight to preserve green builds
  set(_DEFAULT_WERROR_TARGETS
    smoke_headless
    smoke_graphics_headless
    vulkan_fullscreen_demo
    gui_fullscreen_demo
    x11_basic_demo)

  # Allow callers to expand/override via VOXELVK_WERROR_TARGETS (semicolon-separated list)
  set(_WERROR_TARGETS ${_DEFAULT_WERROR_TARGETS})
  if(DEFINED VOXELVK_WERROR_TARGETS AND NOT "${VOXELVK_WERROR_TARGETS}" STREQUAL "")
    set(_WERROR_TARGETS ${VOXELVK_WERROR_TARGETS})
  endif()

  # Apply -Werror to selected targets if they exist
  foreach(TGT IN LISTS _WERROR_TARGETS)
    if(TARGET ${TGT})
      target_compile_options(${TGT} PRIVATE ${_WX_FLAG})
    endif()
  endforeach()

  string(JOIN ", " _WERROR_TARGETS_STR ${_WERROR_TARGETS})
  message(STATUS "Werror enabled for targets: ${_WERROR_TARGETS_STR}")
endif()
