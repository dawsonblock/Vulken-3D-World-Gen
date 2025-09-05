#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <algorithm>

// Mock noise generators - in real implementation these would be from src/noise/generators.hpp
namespace voxelvk {
    class PerlinNoise {
    private:
        std::vector<int> permutation;
        
        double fade(double t) const {
            return t * t * t * (t * (t * 6 - 15) + 10);
        }
        
        double lerp(double t, double a, double b) const {
            return a + t * (b - a);
        }
        
        double grad(int hash, double x, double y, double z) const {
            int h = hash & 15;
            double u = h < 8 ? x : y;
            double v = h < 4 ? y : h == 12 || h == 14 ? x : z;
            return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
        }
        
    public:
        PerlinNoise(unsigned int seed = 0) {
            permutation.resize(512);
            for (int i = 0; i < 256; ++i) {
                permutation[i] = i;
            }
            
            // Shuffle using seed
            if (seed != 0) {
                std::srand(seed);
                std::random_shuffle(permutation.begin(), permutation.begin() + 256);
            }
            
            // Duplicate for wrapping
            for (int i = 0; i < 256; ++i) {
                permutation[256 + i] = permutation[i];
            }
        }
        
        double noise(double x, double y, double z) const {
            int X = static_cast<int>(std::floor(x)) & 255;
            int Y = static_cast<int>(std::floor(y)) & 255;
            int Z = static_cast<int>(std::floor(z)) & 255;
            
            x -= std::floor(x);
            y -= std::floor(y);
            z -= std::floor(z);
            
            double u = fade(x);
            double v = fade(y);
            double w = fade(z);
            
            int A = permutation[X] + Y;
            int AA = permutation[A] + Z;
            int AB = permutation[A + 1] + Z;
            int B = permutation[X + 1] + Y;
            int BA = permutation[B] + Z;
            int BB = permutation[B + 1] + Z;
            
            return lerp(w, lerp(v, lerp(u, grad(permutation[AA], x, y, z),
                                          grad(permutation[BA], x - 1, y, z)),
                                   lerp(u, grad(permutation[AB], x, y - 1, z),
                                          grad(permutation[BB], x - 1, y - 1, z))),
                           lerp(v, lerp(u, grad(permutation[AA + 1], x, y, z - 1),
                                          grad(permutation[BA + 1], x - 1, y, z - 1)),
                                   lerp(u, grad(permutation[AB + 1], x, y - 1, z - 1),
                                          grad(permutation[BB + 1], x - 1, y - 1, z - 1))));
        }
        
        double octaveNoise(double x, double y, double z, int octaves, double persistence) const {
            double total = 0;
            double frequency = 1;
            double amplitude = 1;
            double maxValue = 0;
            
            for (int i = 0; i < octaves; ++i) {
                total += noise(x * frequency, y * frequency, z * frequency) * amplitude;
                maxValue += amplitude;
                amplitude *= persistence;
                frequency *= 2;
            }
            
            return total / maxValue;
        }
    };
    
    class SimplexNoise {
    private:
        std::vector<int> perm;
        
    public:
        SimplexNoise(unsigned int seed = 0) : perm(512) {
            for (int i = 0; i < 256; ++i) {
                perm[i] = i;
            }
            
            if (seed != 0) {
                std::srand(seed);
                std::random_shuffle(perm.begin(), perm.begin() + 256);
            }
            
            for (int i = 0; i < 256; ++i) {
                perm[256 + i] = perm[i];
            }
        }
        
        // Simplified 2D simplex noise for testing
        double noise2D(double x, double y) const {
            // This is a simplified implementation for testing purposes
            // Real simplex noise would be more complex
            double n0, n1, n2;
            
            const double F2 = 0.5 * (std::sqrt(3.0) - 1.0);
            const double G2 = (3.0 - std::sqrt(3.0)) / 6.0;
            
            double s = (x + y) * F2;
            int i = static_cast<int>(std::floor(x + s));
            int j = static_cast<int>(std::floor(y + s));
            
            double t = (i + j) * G2;
            double X0 = i - t;
            double Y0 = j - t;
            double x0 = x - X0;
            double y0 = y - Y0;
            
            int i1, j1;
            if (x0 > y0) { i1 = 1; j1 = 0; }
            else { i1 = 0; j1 = 1; }
            
            double x1 = x0 - i1 + G2;
            double y1 = y0 - j1 + G2;
            double x2 = x0 - 1.0 + 2.0 * G2;
            double y2 = y0 - 1.0 + 2.0 * G2;
            
            int ii = i & 255;
            int jj = j & 255;
            int gi0 = perm[ii + perm[jj]] % 12;
            int gi1 = perm[ii + i1 + perm[jj + j1]] % 12;
            int gi2 = perm[ii + 1 + perm[jj + 1]] % 12;
            
            // Simplified gradient calculation
            double t0 = 0.5 - x0 * x0 - y0 * y0;
            if (t0 < 0) n0 = 0.0;
            else {
                t0 *= t0;
                n0 = t0 * t0 * (x0 * (gi0 % 2 == 0 ? 1 : -1) + y0 * (gi0 % 4 < 2 ? 1 : -1));
            }
            
            double t1 = 0.5 - x1 * x1 - y1 * y1;
            if (t1 < 0) n1 = 0.0;
            else {
                t1 *= t1;
                n1 = t1 * t1 * (x1 * (gi1 % 2 == 0 ? 1 : -1) + y1 * (gi1 % 4 < 2 ? 1 : -1));
            }
            
            double t2 = 0.5 - x2 * x2 - y2 * y2;
            if (t2 < 0) n2 = 0.0;
            else {
                t2 *= t2;
                n2 = t2 * t2 * (x2 * (gi2 % 2 == 0 ? 1 : -1) + y2 * (gi2 % 4 < 2 ? 1 : -1));
            }
            
            return 70.0 * (n0 + n1 + n2);
        }
    };
}

class NoiseGeneratorTest : public ::testing::Test {
protected:
    voxelvk::PerlinNoise perlin;
    voxelvk::SimplexNoise simplex;
};

TEST_F(NoiseGeneratorTest, PerlinNoiseBasicProperties) {
    // Test that noise returns values in expected range
    double value = perlin.noise(0.5, 0.3, 0.7);
    EXPECT_GE(value, -1.0);
    EXPECT_LE(value, 1.0);
    
    // Test deterministic behavior
    double value1 = perlin.noise(1.0, 2.0, 3.0);
    double value2 = perlin.noise(1.0, 2.0, 3.0);
    EXPECT_DOUBLE_EQ(value1, value2);
}

TEST_F(NoiseGeneratorTest, PerlinNoiseSeedConsistency) {
    voxelvk::PerlinNoise perlin1(12345);
    voxelvk::PerlinNoise perlin2(12345);
    voxelvk::PerlinNoise perlin3(54321);
    
    double x = 2.5, y = 1.7, z = 3.2;
    
    // Same seed should produce same results
    EXPECT_DOUBLE_EQ(perlin1.noise(x, y, z), perlin2.noise(x, y, z));
    
    // Different seeds should (very likely) produce different results
    EXPECT_NE(perlin1.noise(x, y, z), perlin3.noise(x, y, z));
}

TEST_F(NoiseGeneratorTest, PerlinOctaveNoise) {
    double baseValue = perlin.noise(1.0, 1.0, 1.0);
    double octaveValue = perlin.octaveNoise(1.0, 1.0, 1.0, 4, 0.5);
    
    // Octave noise should be in valid range
    EXPECT_GE(octaveValue, -1.0);
    EXPECT_LE(octaveValue, 1.0);
    
    // With single octave and persistence 1.0, should match base noise
    double singleOctave = perlin.octaveNoise(1.0, 1.0, 1.0, 1, 1.0);
    EXPECT_NEAR(baseValue, singleOctave, 1e-10);
}

TEST_F(NoiseGeneratorTest, PerlinNoiseContinuity) {
    // Test that nearby points have similar values (continuity)
    double value1 = perlin.noise(1.0, 1.0, 1.0);
    double value2 = perlin.noise(1.001, 1.001, 1.001);
    
    // Values should be close for nearby points
    EXPECT_LT(std::abs(value1 - value2), 0.1);
}

TEST_F(NoiseGeneratorTest, SimplexNoise2DBasicProperties) {
    double value = simplex.noise2D(0.5, 0.3);
    
    // Should return finite values
    EXPECT_TRUE(std::isfinite(value));
    
    // Test deterministic behavior
    double value1 = simplex.noise2D(1.0, 2.0);
    double value2 = simplex.noise2D(1.0, 2.0);
    EXPECT_DOUBLE_EQ(value1, value2);
}

TEST_F(NoiseGeneratorTest, SimplexNoiseSeedConsistency) {
    voxelvk::SimplexNoise simplex1(12345);
    voxelvk::SimplexNoise simplex2(12345);
    voxelvk::SimplexNoise simplex3(54321);
    
    double x = 2.5, y = 1.7;
    
    // Same seed should produce same results
    EXPECT_DOUBLE_EQ(simplex1.noise2D(x, y), simplex2.noise2D(x, y));
    
    // Different seeds should (very likely) produce different results
    EXPECT_NE(simplex1.noise2D(x, y), simplex3.noise2D(x, y));
}

TEST_F(NoiseGeneratorTest, NoiseDistribution) {
    // Test that noise values are reasonably distributed
    std::vector<double> values;
    values.reserve(1000);
    
    for (int i = 0; i < 1000; ++i) {
        double x = i * 0.1;
        double y = i * 0.07;
        double z = i * 0.13;
        values.push_back(perlin.noise(x, y, z));
    }
    
    // Calculate mean and standard deviation
    double sum = 0.0;
    for (double v : values) {
        sum += v;
    }
    double mean = sum / values.size();
    
    double variance = 0.0;
    for (double v : values) {
        variance += (v - mean) * (v - mean);
    }
    variance /= values.size();
    double stddev = std::sqrt(variance);
    
    // Mean should be close to 0
    EXPECT_LT(std::abs(mean), 0.1);
    
    // Standard deviation should be reasonable (not too small, indicating variety)
    EXPECT_GT(stddev, 0.1);
    EXPECT_LT(stddev, 1.0);
}