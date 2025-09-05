#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

TEST(MathUtil, BasicOperations) {
  // Test basic math operations
  glm::vec3 v1(1.0f, 2.0f, 3.0f);
  glm::vec3 v2(4.0f, 5.0f, 6.0f);
  glm::vec3 result = v1 + v2;
  
  EXPECT_FLOAT_EQ(result.x, 5.0f);
  EXPECT_FLOAT_EQ(result.y, 7.0f);
  EXPECT_FLOAT_EQ(result.z, 9.0f);
}

TEST(MathUtil, MatrixOperations) {
  // Test matrix operations
  glm::mat4 identity = glm::mat4(1.0f);
  glm::mat4 translation = glm::translate(identity, glm::vec3(1.0f, 2.0f, 3.0f));
  
  glm::vec4 point(0.0f, 0.0f, 0.0f, 1.0f);
  glm::vec4 transformed = translation * point;
  
  EXPECT_FLOAT_EQ(transformed.x, 1.0f);
  EXPECT_FLOAT_EQ(transformed.y, 2.0f);
  EXPECT_FLOAT_EQ(transformed.z, 3.0f);
}