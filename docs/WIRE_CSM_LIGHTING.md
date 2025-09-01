
# Wire CSM depth array into your lighting pass (descriptor binding 5)

## CPU side
```cpp
#include "src/render/csm_pass.hpp"
#include "src/render/csm_cpu.hpp"
#include "src/render/csm_descriptor.hpp"

voxelvk::CSMShadowPass csm;
csm.init(phys, dev, vma, 2048, 4);

// once per frame
voxelvk::CameraParams cam;
cam.view = cameraView;
cam.invView = glm::inverse(cameraView);
cam.proj = cameraProj;
cam.nearPlane = camNear;
cam.farPlane  = camFar;

auto res = voxelvk::compute_csm(cam, lightDir, /*cascades*/4, /*lambda*/0.65f, /*margin*/10.0f);

voxelvk::CSMGpuUBO u{};
for (int i=0;i<res.cascadeCount;i++){ u.lightVP[i] = res.lightVP[i]; u.splits[i] = res.splits[i]; }
u.cascadeCount = res.cascadeCount;
u.mapTexelSize = 1.0f / 2048.0f;
u.pcssMin = 1.0f; u.pcssMax = 6.0f; u.pcssSearch = 25.0f;
csm.updateUBO(u);

// descriptor: write binding 5 in your lighting descriptor set
voxelvk::bind_csm_depth_array(dev, lightingDescriptorSet, /*binding*/5, csm.getDepthArrayView(), csm.getDepthSampler());
```

## GLSL
```glsl
#include "shaders_vk/lighting/use_csm_pcss_example.glsl"
// Then call: float shadowVis = computeShadowVisibility(vWorldPos, vViewSpaceZ);
```

**Notes**
- Depth array sampled as regular depth (no compare). PCSS does filtering + bias.
- Ensure the CSM depth image is transitioned to `SHADER_READ_ONLY_OPTIMAL` before lighting.
