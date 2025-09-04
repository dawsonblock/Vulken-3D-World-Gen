
#include <gtest/gtest.h>
#ifdef HAS_GLM
#include <glm/glm.hpp>
#include <algorithm>
#include <limits>

static bool ray_aabb_intersect(const glm::vec3& orig, const glm::vec3& dir,
                               const glm::vec3& bmin, const glm::vec3& bmax,
                               float& tmin_out, float& tmax_out)
{
    const float INF = std::numeric_limits<float>::infinity();
    glm::vec3 inv = glm::vec3(1.0f) / dir;
    glm::vec3 t0 = (bmin - orig) * inv;
    glm::vec3 t1 = (bmax - orig) * inv;

    glm::vec3 tsmaller = glm::min(t0, t1);
    glm::vec3 tbigger  = glm::max(t0, t1);

    float tmin = std::max(std::max(tsmaller.x, tsmaller.y), tsmaller.z);
    float tmax = std::min(std::min(tbigger.x,  tbigger.y ), tbigger.z);

    tmin_out = tmin; tmax_out = tmax;
    return tmax >= std::max(0.0f, tmin);
}

TEST(RayAABB, HitsCenteredBox) {
    glm::vec3 bmin(-1.0f), bmax(1.0f);
    glm::vec3 o(-5.0f, 0.0f, 0.0f), d(1.0f, 0.0f, 0.0f);
    float t0, t1;
    ASSERT_TRUE(ray_aabb_intersect(o, d, bmin, bmax, t0, t1));
    EXPECT_NEAR(t0, 4.0f, 1e-5);
    EXPECT_NEAR(t1, 6.0f, 1e-5);
}

TEST(RayAABB, MissesBox) {
    glm::vec3 bmin(-1.0f), bmax(1.0f);
    glm::vec3 o(-5.0f, 3.0f, 0.0f), d(1.0f, 0.0f, 0.0f);
    float t0, t1;
    ASSERT_FALSE(ray_aabb_intersect(o, d, bmin, bmax, t0, t1));
}

TEST(RayAABB, InsideBox) {
    glm::vec3 bmin(-1.0f), bmax(1.0f);
    glm::vec3 o(0.0f), d(1.0f, 0.0f, 0.0f);
    float t0, t1;
    ASSERT_TRUE(ray_aabb_intersect(o, d, bmin, bmax, t0, t1));
    EXPECT_LE(t0, 0.0f); // origin inside => t0 <= 0
    EXPECT_GT(t1, 0.0f);
}
#else
TEST(RayAABB, Placeholder) { GTEST_SKIP() << "GLM not available"; }
#endif
