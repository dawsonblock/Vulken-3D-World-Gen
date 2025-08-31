
# CSM Texel Snap + Debug Overlay

## Use snapped cascades (perfectly stable)
```cpp
#include "src/render/csm_cpu.hpp"

const int shadowMapSize = 2048;
auto res = voxelvk::compute_csm_snapped(cam, lightDir, /*cascades*/4, /*lambda*/0.65f, /*margin*/10.0f, shadowMapSize);

voxelvk::CSMGpuUBO u{};
for (int i=0;i<res.cascadeCount;i++){ u.lightVP[i] = res.lightVP[i]; u.splits[i] = res.splits[i]; }
u.cascadeCount = res.cascadeCount;
u.mapTexelSize = 1.0f / float(shadowMapSize);
```

## Debug overlay in lighting shader
```glsl
#include "shadows/csm_debug.glsl"

// Blend a small tint to visualize cascades:
bool ok;
vec3 tint = csm_debug_tint_from_world(vWorldPos, vViewSpaceZ, ok);
vec3 debugOverlay = ok ? tint : vec3(0.0);
finalColor = mix(finalColor, debugOverlay, 0.18); // 18% overlay
```
