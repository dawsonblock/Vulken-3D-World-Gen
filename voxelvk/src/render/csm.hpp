
#pragma once
#include <glm/glm.hpp>
#include <vector>

namespace voxelvk {

struct CSMDataCPU {
    std::vector<glm::mat4> lightViewProj; // size=cascades
    std::vector<float>     splits;        // view-space (or clip) split depths
    int                    cascadeCount = 0;
};

// Practical split scheme (λ in [0,1], ~0.65 good)
std::vector<float> PracticalSplits(float nearPlane, float farPlane, int cascades, float lambda);

// Build orthographic light VP per cascade, tightly fitting the camera slice.
CSMDataCPU BuildCSM(const glm::mat4& camView,
                    const glm::mat4& camProj,
                    const glm::vec3& lightDirWS,
                    int cascades,
                    float splitLambda,
                    float margin = 10.0f);

} // namespace voxelvk
