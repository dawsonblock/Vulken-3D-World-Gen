
# FG Depth-Array CSM + PCSS (Vulkan 1.3 Dynamic Rendering)

## Add sources (CMake)
```cmake
list(APPEND VOXELVK_ELITE_SOURCES
  src/render/csm_pass.cpp
)
target_sources(VoxelVK_Elite_ALL PRIVATE ${VOXELVK_ELITE_SOURCES})
```

Your shader glob already compiles `shaders_vk/shadows/*.*.glsl` to SPIR-V.

## Initialize once
```cpp
#include "src/render/csm_pass.hpp"
voxelvk::CSMShadowPass csmPass;
csmPass.init(physicalDevice, device, vmaAllocator, /*mapSize*/2048, /*cascades*/4);
```

## Per-frame CPU (build matrices + UBO)
Use your existing CSM matrix builder, or fill `CSMGpuUBO` directly:
```cpp
voxelvk::CSMGpuUBO u{};
for (int i=0;i<4;i++) u.lightVP[i] = lightViewProj[i];
u.splits[0]=split0; u.splits[1]=split1; u.splits[2]=split2; u.splits[3]=split3;
u.cascadeCount = 4;
u.mapTexelSize = 1.0f / 2048.0f;
u.pcssMin = 1.0f; u.pcssMax = 6.0f; u.pcssSearch = 25.0f;
csmPass.updateUBO(u);
```

## Record shadow pass each frame
```cpp
cmd = beginFrameCmdBuf();

// Transition csmPass depth image to DEPTH_ATTACHMENT_OPTIMAL here (your barrier helper)
auto drawDepth = [&](VkCommandBuffer cmd, int cas){
    // Bind your terrain/mesh vertex buffers (binding 0: position vec3) and draw
    // Example:
    // vkCmdBindVertexBuffers(cmd, 0, 1, &vbPositionOnly, offsets);
    // vkCmdBindIndexBuffer(cmd, ib, 0, VK_INDEX_TYPE_UINT32);
    // vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
};
csmPass.record(cmd, drawDepth);

// Transition to SHADER_READ_ONLY_OPTIMAL for sampling in lighting pass
```

## Sample in lighting pass (PCSS)
```glsl
#include "shaders_vk/shadows/csm_common.glsl"
#include "shaders_vk/lighting/pcss.glsl"
layout(binding=5) uniform sampler2DArray uShadowMap;

int cas = chooseCascade(vViewSpaceZ);
vec4 lp = uLightViewProj[cas] * vec4(vWorldPos,1.0);
vec3 uvw = lp.xyz / lp.w;
uvw.xy = uvw.xy * 0.5 + 0.5;

float vis = 1.0;
if (all(greaterThanEqual(uvw.xy, vec2(0.0))) && all(lessThanEqual(uvw.xy, vec2(1.0)))) {
    vis = pcss_visibility(uShadowMap, vec3(uvw.xy, cas), uMapTexelSize, uPCSSSearch, uPCSSMin, uPCSSMax, 0.0008);
}
color *= vis;
```

## Descriptor binding in lighting pass
Bind `csmPass.getDepthArrayView()` and `csmPass.getDepthSampler()` to `binding=5` as a `combined image sampler` (regular depth sampling, not shadow compare).

## Notes
- We render cascades **layer by layer**; no `gl_Layer` or geometry shader required.
- Pipeline expects **location=0 : vec3 position**. If your vertex format differs, adjust `VkVertexInputAttributeDescription`.
- Bias (`depthBiasConstantFactor`/`SlopeFactor`) is set dynamically; tweak as needed.
- The pass uses **Vulkan 1.3 dynamic rendering** (no render pass objects). Ensure `VK_KHR_dynamic_rendering` is enabled if you target <1.3.
