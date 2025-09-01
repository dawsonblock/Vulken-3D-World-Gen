#include <gtest/gtest.h>
#ifdef HAS_GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#endif

TEST(Math, VecNormalize) {
#ifdef HAS_GLM
    glm::vec3 v(3.0f, 0.0f, 4.0f);
    float len = glm::length(v);
    auto n = v / len;
    EXPECT_NEAR(glm::length(n), 1.0f, 1e-5);
#else
    GTEST_SKIP() << "GLM not available";
#endif
}

TEST(Geometry, AABBUnion) {
#ifdef HAS_GLM
    glm::vec3 a_min(0.0f), a_max(1.0f);
    glm::vec3 b_min(-2.0f, -0.5f, 3.0f), b_max(0.5f, 2.0f, 4.0f);
    glm::vec3 umin = glm::min(a_min, b_min);
    glm::vec3 umax = glm::max(a_max, b_max);
    EXPECT_FLOAT_EQ(umin.x, -2.0f);
    EXPECT_FLOAT_EQ(umax.y, 2.0f);
#else
    GTEST_SKIP() << "GLM not available";
#endif
}

TEST(Math, TransformPoint) {
#ifdef HAS_GLM
    glm::mat4 M(1.0f);
    M = glm::translate(M, glm::vec3(10.0f, 0.0f, 0.0f));
    glm::vec4 p(1.0f, 2.0f, 3.0f, 1.0f);
    glm::vec4 q = M * p;
    EXPECT_FLOAT_EQ(q.x, 11.0f);
#else
    GTEST_SKIP() << "GLM not available";
#endif
}
