
#include <gtest/gtest.h>
#ifdef HAS_GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>

struct Plane { glm::vec3 n; float d; }; // n.x*x + n.y*y + n.z*z + d >= 0 inside

static std::array<Plane,6> extract_frustum(const glm::mat4& VP){
    std::array<Plane,6> p;
    // Left, Right, Bottom, Top, Near, Far (row-major assumption)
    // Plane eq from columns of VP (OpenGL-style)
    // L: 4th col + 1st col
    p[0] = { glm::vec3(VP[0][3]+VP[0][0], VP[1][3]+VP[1][0], VP[2][3]+VP[2][0]), VP[3][3]+VP[3][0] };
    // R: 4th col - 1st
    p[1] = { glm::vec3(VP[0][3]-VP[0][0], VP[1][3]-VP[1][0], VP[2][3]-VP[2][0]), VP[3][3]-VP[3][0] };
    // B: 4th col + 2nd
    p[2] = { glm::vec3(VP[0][3]+VP[0][1], VP[1][3]+VP[1][1], VP[2][3]+VP[2][1]), VP[3][3]+VP[3][1] };
    // T: 4th col - 2nd
    p[3] = { glm::vec3(VP[0][3]-VP[0][1], VP[1][3]-VP[1][1], VP[2][3]-VP[2][1]), VP[3][3]-VP[3][1] };
    // N: 4th col + 3rd
    p[4] = { glm::vec3(VP[0][3]+VP[0][2], VP[1][3]+VP[1][2], VP[2][3]+VP[2][2]), VP[3][3]+VP[3][2] };
    // F: 4th col - 3rd
    p[5] = { glm::vec3(VP[0][3]-VP[0][2], VP[1][3]-VP[1][2], VP[2][3]-VP[2][2]), VP[3][3]-VP[3][2] };

    // normalize planes
    for (auto& q : p) {
        float len = glm::length(q.n);
        if (len > 0.0f) { q.n /= len; q.d /= len; }
    }
    return p;
}

static bool aabb_in_frustum(const std::array<Plane,6>& F, const glm::vec3& bmin, const glm::vec3& bmax){
    // For each plane, test the vertex most outside (p-vertex)
    for (const auto& pl : F) {
        glm::vec3 p = glm::vec3(
            (pl.n.x >= 0.0f ? bmax.x : bmin.x),
            (pl.n.y >= 0.0f ? bmax.y : bmin.y),
            (pl.n.z >= 0.0f ? bmax.z : bmin.z)
        );
        if (glm::dot(pl.n, p) + pl.d < 0.0f) return false; // outside this plane
    }
    return true;
}

TEST(Frustum, AABBVisibility){
    glm::mat4 P = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f);
    glm::mat4 V = glm::lookAt(glm::vec3(0,0,5), glm::vec3(0,0,0), glm::vec3(0,1,0));
    auto F = extract_frustum(P*V);

    glm::vec3 bmin(-1.0f), bmax(1.0f);
    EXPECT_TRUE(aabb_in_frustum(F, bmin, bmax));

    glm::vec3 bmin2(100.0f), bmax2(101.0f);
    EXPECT_FALSE(aabb_in_frustum(F, bmin2, bmax2));
}
#else
TEST(Frustum, Placeholder){ GTEST_SKIP() << "GLM not available"; }
#endif
