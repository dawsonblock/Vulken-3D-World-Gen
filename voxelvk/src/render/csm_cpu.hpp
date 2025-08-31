
#pragma once
#include <glm/glm.hpp>
#include <array>

namespace voxelvk {

struct CameraParams {
    glm::mat4 view;      // camera view matrix
    glm::mat4 invView;   // inverse of view
    glm::mat4 proj;      // camera projection (Vulkan 0..1 depth)
    float nearPlane;
    float farPlane;
};

struct CSMCPUResult {
    std::array<glm::mat4,4> lightVP;
    std::array<float,4>     splits;
    int                     cascadeCount{4};
};

// lambda in [0,1], 0=uniform, 1=logarithmic. 0.6..0.7 is typical.
CSMCPUResult compute_csm(const CameraParams& cam, const glm::vec3& lightDir,
                         int cascades = 4, float lambda = 0.65f, float margin = 10.0f);

// Same as compute_csm, but with texel snapping for perfectly stable shadows.
// mapSize is your shadow map resolution (e.g., 2048).
CSMCPUResult compute_csm_snapped(const CameraParams& cam, const glm::vec3& lightDir,
                                 int cascades, float lambda, float margin, int mapSize);

} // namespace voxelvk
