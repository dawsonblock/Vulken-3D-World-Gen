
# VoxelVK — Vulkan Upgrades (C++/GLSL)

## 1) BRDF LUT (Compute)
- Shader: `shaders_vk/ibl/brdf_lut.comp.glsl` → `spv/ibl/brdf_lut.comp.spv`
- C++: `src/render/ibl_brdf.(hpp|cpp)` (VMA required)

**Usage (one-time at startup):**
```cpp
#include "src/render/ibl_brdf.hpp"
voxelvk::BRDFLUT brdf;
bool ok = voxelvk::CreateBRDFLUT(device, physicalDevice, vmaAllocator, cmdPool, graphicsQueue, brdf, 256);
// Bind `brdf.view + brdf.sampler` as sampler2D uBRDFLUT in your PBR shader.
```

## 2) CSM + PCSS
- CPU: `src/render/csm.(hpp|cpp)` → compute `lightViewProj[]` and `splits[]`
- GLSL: `shaders_vk/shadows/csm_common.glsl`, `shaders_vk/lighting/pcss.glsl`

**Uniforms (std140 UBO binding=3)**
```glsl
layout(std140, binding=3) uniform CSMData {
    mat4  uLightViewProj[4];
    float uCascadeSplits[4];
    int   uCascadeCount;
    float uMapTexelSize;   // 1.0 / shadowMapSize
    float uPCSSMin; float uPCSSMax; float uPCSSSearch;
};
```

**Sampling (fragment):**
```glsl
#include "shaders_vk/shadows/csm_common.glsl"
#include "shaders_vk/lighting/pcss.glsl"
uniform sampler2DArray uShadowMap; // depth values in .r
// compute cascade id from view-space z (pass vViewZ from VS), then:
float vis = pcss_visibility(uShadowMap, vec3(uv, receiverDepth), uMapTexelSize, uPCSSSearch, uPCSSMin, uPCSSMax, 0.0008);
```

## 3) Atmosphere & Clouds
- Fullscreen vert: `shaders_vk/common/fullscreen.vert.glsl`
- Sky: `shaders_vk/post/atmosphere.frag.glsl`
- Clouds: `shaders_vk/post/clouds.frag.glsl`

## 4) CMake (snippet to append)
```cmake
# Add new C++ sources
list(APPEND VOXELVK_ELITE_SOURCES
  src/render/ibl_brdf.cpp
  src/render/csm.cpp
)

# VMA include if not already
target_include_directories(VoxelVK_Elite_ALL PRIVATE ${CMAKE_SOURCE_DIR}/external/vma)

# Ensure shaders are picked up (your existing glob covers this pattern)
# For runtime SPV path expected by ibl_brdf.cpp, mirror the file:
add_custom_target(mirror_spv_ibl ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/spv/ibl
  COMMAND ${CMAKE_COMMAND} -E copy
          ${CMAKE_BINARY_DIR}/spv/ibl/brdf_lut.comp.spv
          ${CMAKE_BINARY_DIR}/spv/ibl/brdf_lut.comp.spv
  DEPENDS VoxelVK_Shaders)
add_dependencies(VoxelVK_Elite_ALL mirror_spv_ibl)
```

> If your engine loads SPV from `spv/...`, keep the mirror. Otherwise, adjust `ibl_brdf.cpp` search path.

## 5) Descriptor & Pipeline bindings
- BRDF LUT compute: single descriptor set with binding 0 = storage image.
- PBR fragment: add `sampler2D uBRDFLUT;` and sample with split-sum as usual (`pbr_common.glsl` helpers).

## 6) Notes
- The compute path avoids render-pass boilerplate and works on any Vulkan 1.1+ device with storage images.
- Use a depth **array** for cascades. Each layer = cascade. Set `uMapTexelSize = 1.0/size`.
