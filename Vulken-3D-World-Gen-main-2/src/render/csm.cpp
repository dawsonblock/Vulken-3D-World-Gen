
#include "csm.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace voxelvk {

std::vector<float> PracticalSplits(float nearPlane, float farPlane, int cascades, float lambda) {
    std::vector<float> s(cascades);
    for (int i = 1; i <= cascades; ++i) {
        float idm = float(i) / float(cascades);
        float logd = nearPlane * std::pow(farPlane / nearPlane, idm);
        float uni  = nearPlane + (farPlane - nearPlane) * idm;
        s[i-1] = lambda * logd + (1.0f - lambda) * uni;
    }
    return s;
}

static glm::mat4 OrthoFromPoints(const std::vector<glm::vec3>& pts, const glm::vec3& lightDir, float margin) {
    glm::vec3 up(0,1,0);
    if (std::abs(glm::dot(lightDir, up)) > 0.99f) up = glm::vec3(1,0,0);
    glm::vec3 z = -glm::normalize(lightDir);
    glm::vec3 x = glm::normalize(glm::cross(up, z));
    glm::vec3 y = glm::cross(z, x);
    glm::mat4 V(1.0f);
    V[0] = glm::vec4(x, 0.0f);
    V[1] = glm::vec4(y, 0.0f);
    V[2] = glm::vec4(z, 0.0f);

    std::vector<glm::vec3> lp; lp.reserve(pts.size());
    for (auto& p : pts) {
        glm::vec3 q = glm::vec3(V * glm::vec4(p, 1.0));
        lp.push_back(q);
    }
    glm::vec3 mn( 1e9f), mx(-1e9f);
    for (auto& p : lp) { mn = glm::min(mn, p); mx = glm::max(mx, p); }
    mn -= glm::vec3(margin); mx += glm::vec3(margin);

    glm::mat4 O(1.0f);
    O[0][0] = 2.0f / (mx.x - mn.x);
    O[1][1] = 2.0f / (mx.y - mn.y);
    O[2][2] = -2.0f / (mx.z - mn.z);
    O[3][0] = -(mx.x + mn.x) / (mx.x - mn.x);
    O[3][1] = -(mx.y + mn.y) / (mx.y - mn.y);
    O[3][2] = -(mx.z + mn.z) / (mx.z - mn.z);
    return O * V;
}

static std::vector<glm::vec3> FrustumCornersWS(const glm::mat4& invViewProj) {
    static const glm::vec4 cornersNDC[8] = {
        {-1,-1,0,1},{ 1,-1,0,1},{-1, 1,0,1},{ 1, 1,0,1},
        {-1,-1,1,1},{ 1,-1,1,1},{-1, 1,1,1},{ 1, 1,1,1}
    };
    std::vector<glm::vec3> pts(8);
    for (int i=0;i<8;i++) {
        glm::vec4 p = invViewProj * cornersNDC[i];
        pts[i] = glm::vec3(p) / p.w;
    }
    return pts;
}

CSMDataCPU BuildCSM(const glm::mat4& camView,
                    const glm::mat4& camProj,
                    const glm::vec3& lightDirWS,
                    int cascades,
                    float splitLambda,
                    float margin) {
    CSMDataCPU out{};
    out.cascadeCount = cascades;
    glm::mat4 invVP = glm::inverse(camProj * camView);

    // Build per-cascade volume by slicing the projection (we approximate by interpolating depths)
    // Compute split depths in clip space [0..1] mapped from near..far
    // For simplicity assume caller provides correct camProj with near/far; else derive from inverse.
    float nearPlane = 0.1f, farPlane = 2000.0f;
    // Heuristic: extract near/far from camProj if it's a standard perspective matrix
    if (std::abs(camProj[3][3]) < 1e-6f) {
        // P = [*,*,*,*; *,*,*,*; 0,0,a,b; 0,0,-1,0] -> near,far recoverable
        float a = camProj[2][2];
        float b = camProj[3][2];
        farPlane = b / (a + 1.0f);
        nearPlane = b / (a - 1.0f);
        if (!(farPlane > nearPlane && nearPlane > 0.0f)) { nearPlane = 0.1f; farPlane = 2000.0f; }
    }

    auto splits = PracticalSplits(nearPlane, farPlane, cascades, splitLambda);
    out.splits = splits;

    out.lightViewProj.resize(cascades);

    for (int i=0;i<cascades;i++) {
        // Build an invVP for the sub-frustum slice by adjusting projection near/far
        float prev = (i==0) ? nearPlane : splits[i-1];
        float curr = splits[i];
        // Perspective with sliced near/far
        // Note: We approximate by modifying near/far and recomputing invVP; caller should supply fov/aspect if exact.
        // This is sufficient for stable cascades after adding margin.
        // Reconstruct perspective params from camProj (assume standard layout)
        float f = camProj[1][1]; // ~ 1/tan(fov/2)
        float aspect = camProj[1][1] / camProj[0][0];
        glm::mat4 proj = glm::perspective(2.0f * std::atan(1.0f/f), aspect, prev, curr);
        proj[1][1] *= -1.0f; // GLM produces GL clip; flip for Vulkan

        glm::mat4 invSlice = glm::inverse(proj * camView);
        auto slicePts = FrustumCornersWS(invSlice);
        out.lightViewProj[i] = OrthoFromPoints(slicePts, glm::normalize(lightDirWS), margin);
    }

    return out;
}

} // namespace voxelvk
