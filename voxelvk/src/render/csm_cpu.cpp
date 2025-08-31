
#include "csm_cpu.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <array>
#include <cmath>

namespace voxelvk {

static inline float clampf(float x, float a, float b){ return std::max(a, std::min(b, x)); }

static void extract_fov_aspect(const glm::mat4& proj, float& fovY, float& aspect){
    // GLM uses column-major; proj[1][1] = 1/tan(fovY/2)
    float invTan = proj[1][1];
    fovY = 2.0f * std::atan(1.0f / std::max(1e-6f, invTan));
    aspect = (proj[1][1] / std::max(1e-6f, proj[0][0]));
}

static glm::mat4 build_light_view(const glm::vec3& dir){
    glm::vec3 z = -glm::normalize(dir);
    glm::vec3 up(0,1,0);
    if (std::abs(glm::dot(up, z)) > 0.99f) up = glm::vec3(1,0,0);
    glm::vec3 x = glm::normalize(glm::cross(up, z));
    glm::vec3 y = glm::cross(z, x);
    glm::mat4 V(1.0f);
    V[0][0]=x.x; V[1][0]=x.y; V[2][0]=x.z; V[3][0]=0.0f;
    V[0][1]=y.x; V[1][1]=y.y; V[2][1]=y.z; V[3][1]=0.0f;
    V[0][2]=z.x; V[1][2]=z.y; V[2][2]=z.z; V[3][2]=0.0f;
    V[0][3]=0.0f; V[1][3]=0.0f; V[2][3]=0.0f; V[3][3]=1.0f;
    return V;
}

static void frustum_slice_corners_ws(const CameraParams& cam, float nearD, float farD,
                                     std::array<glm::vec3,8>& outCorners){
    // Derive fov/aspect from projection
    float fovY, aspect;
    extract_fov_aspect(cam.proj, fovY, aspect);

    float tn = std::tan(fovY * 0.5f);
    float nh = tn * nearD;
    float nw = nh * aspect;
    float fh = tn * farD;
    float fw = fh * aspect;

    // View-space corners (right-handed, -Z forward in view)
    std::array<glm::vec3,8> vs = {
        glm::vec3(-nw, -nh, -nearD),
        glm::vec3( nw, -nh, -nearD),
        glm::vec3(-nw,  nh, -nearD),
        glm::vec3( nw,  nh, -nearD),
        glm::vec3(-fw, -fh, -farD),
        glm::vec3( fw, -fh, -farD),
        glm::vec3(-fw,  fh, -farD),
        glm::vec3( fw,  fh, -farD)
    };

    // Transform to world using invView
    for (int i=0;i<8;i++){
        glm::vec4 w = cam.invView * glm::vec4(vs[i], 1.0f);
        outCorners[i] = glm::vec3(w) / w.w;
    }
}

static glm::mat4 ortho_fit_points(const glm::mat4& lightView, const std::array<glm::vec3,8>& pts, float margin)
{
    glm::vec3 mn( 1e9f), mx(-1e9f);
    for (auto& p : pts){
        glm::vec4 lp = lightView * glm::vec4(p, 1.0f);
        glm::vec3 q(lp.x, lp.y, lp.z);
        mn = glm::min(mn, q);
        mx = glm::max(mx, q);
    }
    mn -= glm::vec3(margin); mx += glm::vec3(margin);
    // Ortho in light space
    return glm::ortho(mn.x, mx.x, mn.y, mx.y, -mx.z, -mn.z) * lightView;
}

// Texel-snapped ortho: snap the XY center in light space to the shadow map texel grid.
static glm::mat4 ortho_fit_points_snapped(const glm::mat4& lightView,
                                          const std::array<glm::vec3,8>& pts,
                                          float margin, int mapSize)
{
    if (mapSize <= 0) {
        return ortho_fit_points(lightView, pts, margin);
    }

    glm::vec3 mn( 1e9f), mx(-1e9f);
    for (auto& p : pts){
        glm::vec4 lp = lightView * glm::vec4(p, 1.0f);
        glm::vec3 q(lp.x, lp.y, lp.z);
        mn = glm::min(mn, q);
        mx = glm::max(mx, q);
    }
    mn -= glm::vec3(margin); mx += glm::vec3(margin);

    glm::vec3 center = 0.5f*(mn + mx);
    glm::vec2 extent = glm::vec2(mx.x - mn.x, mx.y - mn.y);
    glm::vec2 texel = extent / float(mapSize);
    // Avoid div-by-0
    texel = glm::max(texel, glm::vec2(1e-6f));

    glm::vec2 centerSnapped = glm::floor(glm::vec2(center.x, center.y) / texel) * texel;
    glm::vec2 delta = centerSnapped - glm::vec2(center.x, center.y);

    mn.x += delta.x; mx.x += delta.x;
    mn.y += delta.y; mx.y += delta.y;

    return glm::ortho(mn.x, mx.x, mn.y, mx.y, -mx.z, -mn.z) * lightView;
}

CSMCPUResult compute_csm(const CameraParams& cam, const glm::vec3& lightDir,
                         int cascades, float lambda, float margin)
{
    CSMCPUResult out{};
    cascades = std::max(1, std::min(4, cascades));
    out.cascadeCount = cascades;

    // Practical split scheme
    float n = cam.nearPlane, f = cam.farPlane;
    for (int i=1;i<=cascades;i++){
        float idm = float(i)/float(cascades);
        float logd = n * std::pow(f/n, idm);
        float unif = n + (f - n) * idm;
        out.splits[i-1] = lambda * logd + (1.0f - lambda) * unif;
    }

    glm::mat4 lightV = build_light_view(lightDir);
    std::array<glm::vec3,8> sliceCorners;

    for (int i=0;i<cascades;i++){
        float nearD = (i==0) ? n : out.splits[i-1];
        float farD  = out.splits[i];
        frustum_slice_corners_ws(cam, nearD, farD, sliceCorners);
        out.lightVP[i] = ortho_fit_points(lightV, sliceCorners, margin);
    }
    return out;
}

CSMCPUResult compute_csm_snapped(const CameraParams& cam, const glm::vec3& lightDir,
                                 int cascades, float lambda, float margin, int mapSize)
{
    CSMCPUResult out{};
    cascades = std::max(1, std::min(4, cascades));
    out.cascadeCount = cascades;

    float n = cam.nearPlane, f = cam.farPlane;
    for (int i=1;i<=cascades;i++){
        float idm = float(i)/float(cascades);
        float logd = n * std::pow(f/n, idm);
        float unif = n + (f - n) * idm;
        out.splits[i-1] = lambda * logd + (1.0f - lambda) * unif;
    }

    glm::mat4 lightV = build_light_view(lightDir);
    std::array<glm::vec3,8> sliceCorners;

    for (int i=0;i<cascades;i++){
        float nearD = (i==0) ? n : out.splits[i-1];
        float farD  = out.splits[i];
        frustum_slice_corners_ws(cam, nearD, farD, sliceCorners);
        out.lightVP[i] = ortho_fit_points_snapped(lightV, sliceCorners, margin, mapSize);
    }
    return out;
}

} // namespace voxelvk
