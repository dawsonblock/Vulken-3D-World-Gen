cmake_minimum_required(VERSION 3.24)
project(VoxelVK_Elite_ALL LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# vcpkg manifest toolchain (if available)
if(DEFINED ENV{VCPKG_ROOT})
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "" FORCE)
  message(STATUS "Using vcpkg toolchain from: $ENV{VCPKG_ROOT}")
endif()

option(ENABLE_VALIDATION_LAYERS "Enable Vulkan validation layers" ON)
option(ENABLE_VK_DEBUG_MARKERS  "Enable VK_EXT_debug_utils markers" ON)
option(ENABLE_IMGUI_OVERLAY     "Enable ImGui debug HUD overlay" ON)
option(ENABLE_TESTS             "Enable CTest targets" ON)
option(VOXELVK_ENABLE_CUDA      "Enable CUDA acceleration" OFF)
option(VOXELVK_ENABLE_TENSORRT  "Enable TensorRT (requires CUDA)" OFF)
option(VOXELVK_HEADLESS_ONLY    "Build minimal headless utilities without Vulkan/GLFW" OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build type" FORCE)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wdouble-promotion -Wformat=2 -Wno-unused-parameter)
  add_link_options(-Wl,--no-undefined)
endif()

# --- Build tagging injection ---
set(VOXELVK_BRANCH_SOURCES "project_fixed + VoxelVK_Elite_ALL_release_ready + voxel_upgrades_patch + project_compute_graphics_chain + project_dynamic_pbr + project_next_tier_all")
include(${CMAKE_SOURCE_DIR}/cmake/GenBuildInfo.cmake)
include_directories(${CMAKE_BINARY_DIR}/generated)

find_package(Threads REQUIRED)

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

  find_package(glfw3 QUIET)
  if(glfw3_FOUND)
    set(GLFW_LIB glfw)
  else()
    find_package(GLFW REQUIRED)
    set(GLFW_LIB GLFW::GLFW)
  endif()

  # Shader pipeline (glslc preferred; fallback glslangValidator)
  find_program(GLSLC_EXECUTABLE NAMES glslc HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin" "/usr/bin" "/usr/local/bin")
  if(NOT GLSLC_EXECUTABLE)
    find_program(GLSLANG_VALIDATOR NAMES glslangValidator HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin" "/usr/bin" "/usr/local/bin")
    if(NOT GLSLANG_VALIDATOR)
      message(FATAL_ERROR "No shader compiler found (glslc/glslangValidator). Install Vulkan SDK.")
    endif()
    set(SHADER_COMPILER ${GLSLANG_VALIDATOR})
    set(SHADER_FLAGS -V --target-env vulkan1.3 -g)
  else()
    set(SHADER_COMPILER ${GLSLC_EXECUTABLE})
    set(SHADER_FLAGS --target-env=vulkan1.3 -O -g)
  endif()

  file(GLOB_RECURSE GLSL_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/shaders_vk/*.*.glsl")
  set(SPV_DIR "${CMAKE_BINARY_DIR}/spv")
  set(SPV_OUT "")
  foreach(GLSL_FILE IN LISTS GLSL_SOURCES)
    get_filename_component(FILE_NAME ${GLSL_FILE} NAME)
    get_filename_component(FILE_DIR  ${GLSL_FILE} DIRECTORY)
    file(RELATIVE_PATH REL_DIR "${CMAKE_SOURCE_DIR}/shaders_vk" "${FILE_DIR}")
    set(OUT_DIR "${SPV_DIR}/${REL_DIR}")
    file(MAKE_DIRECTORY "${OUT_DIR}")
    set(SPV_FILE "${OUT_DIR}/${FILE_NAME}.spv")
    add_custom_command(OUTPUT ${SPV_FILE}
      COMMAND ${SHADER_COMPILER} ${SHADER_FLAGS} -o ${SPV_FILE} ${GLSL_FILE}
      DEPENDS ${GLSL_FILE}
      COMMENT "Compiling ${REL_DIR}/${FILE_NAME}")
    list(APPEND SPV_OUT ${SPV_FILE})
  endforeach()
  add_custom_target(VoxelVK_Shaders ALL DEPENDS ${SPV_OUT})
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
  
  if(yaml-cpp_FOUND OR YAML_CPP_FOUND)
    set(WEATHER_YAML_LIB yaml-cpp)
    if(YAML_CPP_FOUND)
      set(WEATHER_YAML_LIB ${YAML_CPP_LIBRARIES})
    endif()
    message(STATUS "Found yaml-cpp for weather system")
  else()
    message(WARNING "yaml-cpp not found - weather system will use fallback config")
  endif()

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
  add_dependencies(smoke_headless VoxelVK_Shaders)
  target_include_directories(smoke_headless PRIVATE ${CMAKE_SOURCE_DIR}/src)
  target_link_libraries(smoke_headless PRIVATE Vulkan::Vulkan Threads::Threads ${GLFW_LIB})
  target_compile_definitions(smoke_headless PRIVATE
    $<$<BOOL:${ENABLE_VALIDATION_LAYERS}>:ENABLE_VALIDATION_LAYERS=1>
    $<$<BOOL:${ENABLE_IMGUI_OVERLAY}>:ENABLE_IMGUI_OVERLAY=1>)
else()
  # Headless-only: no Vulkan/GLFW linkage required
  target_link_libraries(smoke_headless PRIVATE Threads::Threads)
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
    add_executable(VoxelVK_Elite_ALL apps/smoke_headless.cpp ${VOXELVK_ELITE_SOURCES})
    add_dependencies(VoxelVK_Elite_ALL VoxelVK_Shaders)
    target_include_directories(VoxelVK_Elite_ALL PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(VoxelVK_Elite_ALL PRIVATE Vulkan::Vulkan Threads::Threads ${GLFW_LIB})
    if(VMA_TARGET)
      target_link_libraries(VoxelVK_Elite_ALL PRIVATE ${VMA_TARGET})
    endif()
    target_compile_definitions(VoxelVK_Elite_ALL PRIVATE
      $<$<BOOL:${ENABLE_VALIDATION_LAYERS}>:ENABLE_VALIDATION_LAYERS=1>
      $<$<BOOL:${ENABLE_IMGUI_OVERLAY}>:ENABLE_IMGUI_OVERLAY=1>)
    target_compile_definitions(VoxelVK_Elite_ALL PRIVATE
      $<$<BOOL:${VOXELVK_ENABLE_CUDA}>:VOXELVK_ENABLE_CUDA=1>
      $<$<BOOL:${VOXELVK_ENABLE_TENSORRT}>:VOXELVK_ENABLE_TENSORRT=1>)
    # Link weather system dependencies
    if(VOXELVK_ENABLE_WEATHER AND WEATHER_YAML_LIB)
      target_link_libraries(VoxelVK_Elite_ALL PRIVATE ${WEATHER_YAML_LIB})
      target_compile_definitions(VoxelVK_Elite_ALL PRIVATE VOXELVK_ENABLE_WEATHER=1)
    endif()
  endif()
  # Demo: GUI fullscreen toggle (requires GLFW + OpenGL)
  add_executable(gui_fullscreen_demo apps/gui_fullscreen_demo.cpp)
  target_link_libraries(gui_fullscreen_demo PRIVATE ${GLFW_LIB})
  add_dependencies(gui_fullscreen_demo VoxelVK_Shaders)

  # Weather system demo
  if(VOXELVK_ENABLE_WEATHER)
    add_executable(weather_demo apps/weather_demo.cpp ${WEATHER_SRC})
    target_include_directories(weather_demo PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(weather_demo PRIVATE Threads::Threads)
    if(WEATHER_YAML_LIB)
      target_link_libraries(weather_demo PRIVATE ${WEATHER_YAML_LIB})
      target_compile_definitions(weather_demo PRIVATE VOXELVK_ENABLE_WEATHER=1)
    endif()
  
  # Weather system integration test
  if(VOXELVK_ENABLE_WEATHER)
    add_executable(weather_integration_test apps/weather_integration_test.cpp ${WEATHER_SRC})
    target_include_directories(weather_integration_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(weather_integration_test PRIVATE Threads::Threads)
    if(WEATHER_YAML_LIB)
      target_link_libraries(weather_integration_test PRIVATE ${WEATHER_YAML_LIB})
      target_compile_definitions(weather_integration_test PRIVATE VOXELVK_ENABLE_WEATHER=1)
    endif()
  endif()
  endif()

endif()

  # Optional RAG integrations (FAISS, ONNX Runtime)
  # Use soft-detection to avoid breaking builds if libs not available
  find_package(PkgConfig QUIET)
  set(VOXELVK_HAS_FAISS 0)
  set(VOXELVK_HAS_ONNXRUNTIME 0)
  # Link ImGui to demo if available via vcpkg (header+backends)
  find_package(imgui QUIET)
  if(imgui_FOUND)
    target_link_libraries(gui_fullscreen_demo PRIVATE imgui::imgui)
    target_compile_definitions(gui_fullscreen_demo PRIVATE GUI_DEMO_HAS_IMGUI_VCPKG=1)
  # OpenGL linkage for demo (platform-provided)
  find_package(OpenGL QUIET)
  if(OpenGL_FOUND)
    target_link_libraries(gui_fullscreen_demo PRIVATE OpenGL::GL)
  endif()
  # Production-grade main ImGui fullscreen app (optional)
  add_executable(main_imgui_fullscreen apps/main_imgui_fullscreen.cpp)
  target_link_libraries(main_imgui_fullscreen PRIVATE ${GLFW_LIB})
  if(imgui_FOUND)
    target_link_libraries(main_imgui_fullscreen PRIVATE imgui::imgui)
  endif()
  if(OpenGL_FOUND)
    target_link_libraries(main_imgui_fullscreen PRIVATE OpenGL::GL)
  endif()

  # Vulkan fullscreen demo (optional): GLFW Vulkan window + swapchain recreate on toggle
  add_executable(vulkan_fullscreen_demo apps/vulkan_fullscreen_demo.cpp)
  target_link_libraries(vulkan_fullscreen_demo PRIVATE ${GLFW_LIB} Vulkan::Vulkan)


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


# Tests
if(ENABLE_TESTS)
  include(CTest)
  enable_testing()
  add_test(NAME smoke_headless COMMAND smoke_headless)
  set_tests_properties(smoke_headless PROPERTIES TIMEOUT 30)
endif()

message(STATUS "=== Build Summary ===")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
if(NOT VOXELVK_HEADLESS_ONLY)
  message(STATUS "Shader Compiler: ${SHADER_COMPILER}")
else()
  message(STATUS "Headless-only build: Vulkan/GLFW disabled")
endif()
message(STATUS "=====================")

if(ENABLE_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# Tools
add_subdirectory(tools)
