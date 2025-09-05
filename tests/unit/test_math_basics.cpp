#include <gtest/gtest.h>

TEST(MathBasics, Sanity) { 
    EXPECT_EQ(2 + 2, 4); 
}

TEST(MathBasics, FloatComparison) {
    EXPECT_FLOAT_EQ(1.0f, 1.0f);
    EXPECT_NEAR(3.14159f, 3.14f, 0.01f);
}

TEST(MathBasics, VectorMath) {
    // Basic vector operations test
    float dot_product = 1.0f * 2.0f + 3.0f * 4.0f + 5.0f * 6.0f;
    EXPECT_EQ(dot_product, 44.0f);
}