
# Build Tagging + GPU Compat + Runtime Debug Toggle (F9)

## 1) CMake — add and include the generator
```cmake
# Top of CMakeLists (after find_package(Vulkan))
set(VOXELVK_BRANCH_SOURCES "project_fixed + VoxelVK_Elite_ALL_release_ready + voxel_upgrades_patch + project_compute_graphics_chain + project_dynamic_pbr + project_next_tier_all")

include(${CMAKE_SOURCE_DIR}/cmake/GenBuildInfo.cmake)
# This writes ${CMAKE_BINARY_DIR}/generated/build_info.hpp
include_directories(${CMAKE_BINARY_DIR}/generated)
```

## 2) Add sources to your target
```cmake
list(APPEND VOXELVK_ELITE_SOURCES
  src/core/build_info_print.cpp
  src/core/runtime_debug.cpp
  src/core/gpu_compat.cpp
)
target_sources(VoxelVK_Elite_ALL PRIVATE ${VOXELVK_ELITE_SOURCES})
```

## 3) Print build info
At app start:
```cpp
#include "src/core/build_info_print.hpp"
voxelvk::InitBuildInfoLogging();
voxelvk::PrintBuildInfoOnce();
```

At first Vulkan instance creation:
```cpp
#include "src/core/runtime_debug.hpp"
static voxelvk::DebugRuntime gDebug;
voxelvk::DebugRuntime_Init(gDebug, instance); // also prints build info once
```

## 4) Validation messages will auto-print
The debug messenger callback prints to console + `debug_vk_runtime.log` and injects build info on first hit.

## 5) GPU compatibility
After picking a physical device:
```cpp
#include "src/core/gpu_compat.hpp"
VkPhysicalDeviceProperties props{};
vkGetPhysicalDeviceProperties(phys, &props);
auto compat = voxelvk::ComputeCompatFor(props);
// Apply compat.* to your engine config as appropriate.
```

## 6) Hotkey (F9) to toggle at runtime
If you use GLFW, see `src/core/hotkey_glfw_example.md` for a 5‑line key callback snippet.
```

