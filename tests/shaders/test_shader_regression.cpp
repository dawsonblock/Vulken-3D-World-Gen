#include <gtest/gtest.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <memory>
#include <array>

// Mock shader regression testing framework
namespace voxelvk {
    struct ImageData {
        std::vector<uint8_t> pixels;
        uint32_t width;
        uint32_t height;
        uint32_t channels;
        
        ImageData(uint32_t w, uint32_t h, uint32_t c = 4) 
            : width(w), height(h), channels(c) {
            pixels.resize(width * height * channels);
        }
        
        void fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
            for (size_t i = 0; i < pixels.size(); i += channels) {
                pixels[i] = r;
                pixels[i + 1] = g;
                pixels[i + 2] = b;
                if (channels == 4) pixels[i + 3] = a;
            }
        }
        
        void setPixel(uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
            if (x >= width || y >= height) return;
            size_t index = (y * width + x) * channels;
            pixels[index] = r;
            pixels[index + 1] = g;
            pixels[index + 2] = b;
            if (channels == 4) pixels[index + 3] = a;
        }
        
        std::array<uint8_t, 4> getPixel(uint32_t x, uint32_t y) const {
            if (x >= width || y >= height) return {0, 0, 0, 0};
            size_t index = (y * width + x) * channels;
            return {
                pixels[index],
                pixels[index + 1],
                pixels[index + 2],
                channels == 4 ? pixels[index + 3] : uint8_t(255)
            };
        }
        
        bool savePNG(const std::string& path) const {
            // Mock PNG saving - in real implementation would use stb_image_write
            std::ofstream file(path, std::ios::binary);
            if (!file) return false;
            
            // Write a simple header (not actual PNG format, just for testing)
            file.write("MOCK_PNG", 8);
            file.write(reinterpret_cast<const char*>(&width), sizeof(width));
            file.write(reinterpret_cast<const char*>(&height), sizeof(height));
            file.write(reinterpret_cast<const char*>(&channels), sizeof(channels));
            file.write(reinterpret_cast<const char*>(pixels.data()), pixels.size());
            
            return file.good();
        }
        
        static ImageData loadPNG(const std::string& path) {
            std::ifstream file(path, std::ios::binary);
            if (!file) return ImageData(0, 0);
            
            char header[8];
            file.read(header, 8);
            if (std::string(header, 8) != "MOCK_PNG") return ImageData(0, 0);
            
            uint32_t w, h, c;
            file.read(reinterpret_cast<char*>(&w), sizeof(w));
            file.read(reinterpret_cast<char*>(&h), sizeof(h));
            file.read(reinterpret_cast<char*>(&c), sizeof(c));
            
            ImageData image(w, h, c);
            file.read(reinterpret_cast<char*>(image.pixels.data()), image.pixels.size());
            
            return image;
        }
    };
    
    class ShaderTester {
    public:
        struct RenderResult {
            ImageData image;
            bool success = false;
            std::string errorMessage;
            
            RenderResult(uint32_t width, uint32_t height) : image(width, height) {}
        };
        
        struct ComparisonResult {
            bool passed = false;
            double ssim = 0.0;
            double psnr = 0.0;
            uint32_t differentPixels = 0;
            double maxDifference = 0.0;
            ImageData diffImage;
            
            ComparisonResult(uint32_t width, uint32_t height) : diffImage(width, height) {}
        };
        
        RenderResult renderShader(const std::string& shaderName, uint32_t width = 512, uint32_t height = 512) {
            RenderResult result(width, height);
            
            // Mock shader rendering based on shader name
            if (shaderName == "basic_vertex_color") {
                renderBasicVertexColor(result.image);
                result.success = true;
            } else if (shaderName == "texture_sample") {
                renderTextureSample(result.image);
                result.success = true;
            } else if (shaderName == "lighting_phong") {
                renderPhongLighting(result.image);
                result.success = true;
            } else if (shaderName == "compute_particles") {
                renderComputeParticles(result.image);
                result.success = true;
            } else {
                result.errorMessage = "Unknown shader: " + shaderName;
                result.success = false;
            }
            
            return result;
        }
        
        ComparisonResult compareImages(const ImageData& reference, const ImageData& test, double tolerance = 0.02) {
            ComparisonResult result(reference.width, reference.height);
            
            if (reference.width != test.width || reference.height != test.height) {
                return result; // Size mismatch
            }
            
            uint32_t totalPixels = reference.width * reference.height;
            double mse = 0.0;
            uint32_t differentPixels = 0;
            double maxDiff = 0.0;
            
            for (uint32_t y = 0; y < reference.height; ++y) {
                for (uint32_t x = 0; x < reference.width; ++x) {
                    auto refPixel = reference.getPixel(x, y);
                    auto testPixel = test.getPixel(x, y);
                    
                    double diffR = std::abs(int(refPixel[0]) - int(testPixel[0])) / 255.0;
                    double diffG = std::abs(int(refPixel[1]) - int(testPixel[1])) / 255.0;
                    double diffB = std::abs(int(refPixel[2]) - int(testPixel[2])) / 255.0;
                    
                    double pixelDiff = std::max({diffR, diffG, diffB});
                    maxDiff = std::max(maxDiff, pixelDiff);
                    
                    if (pixelDiff > tolerance) {
                        differentPixels++;
                    }
                    
                    // Calculate MSE for PSNR
                    mse += diffR * diffR + diffG * diffG + diffB * diffB;
                    
                    // Create diff visualization
                    uint8_t diffIntensity = static_cast<uint8_t>(pixelDiff * 255);
                    result.diffImage.setPixel(x, y, diffIntensity, diffIntensity, diffIntensity);
                }
            }
            
            mse /= (totalPixels * 3); // 3 channels
            result.psnr = mse > 0 ? 20 * std::log10(1.0 / std::sqrt(mse)) : 100.0;
            
            // Simplified SSIM calculation (normally more complex)
            result.ssim = 1.0 - (mse * 10); // Simplified approximation
            result.ssim = std::max(0.0, std::min(1.0, result.ssim));
            
            result.differentPixels = differentPixels;
            result.maxDifference = maxDiff;
            result.passed = (differentPixels == 0 || maxDiff <= tolerance);
            
            return result;
        }
        
    private:
        void renderBasicVertexColor(ImageData& image) {
            // Render a simple gradient
            for (uint32_t y = 0; y < image.height; ++y) {
                for (uint32_t x = 0; x < image.width; ++x) {
                    uint8_t r = static_cast<uint8_t>((x * 255) / image.width);
                    uint8_t g = static_cast<uint8_t>((y * 255) / image.height);
                    uint8_t b = 128;
                    image.setPixel(x, y, r, g, b);
                }
            }
        }
        
        void renderTextureSample(ImageData& image) {
            // Render a checkerboard pattern
            for (uint32_t y = 0; y < image.height; ++y) {
                for (uint32_t x = 0; x < image.width; ++x) {
                    bool checker = ((x / 32) + (y / 32)) % 2 == 0;
                    uint8_t color = checker ? 255 : 64;
                    image.setPixel(x, y, color, color, color);
                }
            }
        }
        
        void renderPhongLighting(ImageData& image) {
            // Render a sphere with Phong lighting
            float centerX = image.width * 0.5f;
            float centerY = image.height * 0.5f;
            float radius = std::min(image.width, image.height) * 0.4f;
            
            for (uint32_t y = 0; y < image.height; ++y) {
                for (uint32_t x = 0; x < image.width; ++x) {
                    float dx = x - centerX;
                    float dy = y - centerY;
                    float dist = std::sqrt(dx * dx + dy * dy);
                    
                    if (dist <= radius) {
                        float intensity = 1.0f - (dist / radius);
                        intensity = std::pow(intensity, 0.5f); // Lighting falloff
                        uint8_t color = static_cast<uint8_t>(intensity * 255);
                        image.setPixel(x, y, color, color, color);
                    } else {
                        image.setPixel(x, y, 32, 32, 64); // Background
                    }
                }
            }
        }
        
        void renderComputeParticles(ImageData& image) {
            // Render scattered particles
            image.fill(16, 16, 32); // Dark background
            
            // Add some "particles"
            for (int i = 0; i < 100; ++i) {
                uint32_t x = (i * 73) % image.width;
                uint32_t y = (i * 137) % image.height;
                uint8_t intensity = static_cast<uint8_t>(128 + (i % 128));
                
                // Draw small particle
                for (int dy = -2; dy <= 2; ++dy) {
                    for (int dx = -2; dx <= 2; ++dx) {
                        if (dx * dx + dy * dy <= 4) {
                            uint32_t px = x + dx;
                            uint32_t py = y + dy;
                            if (px < image.width && py < image.height) {
                                image.setPixel(px, py, intensity, intensity / 2, intensity / 4);
                            }
                        }
                    }
                }
            }
        }
    };
}

class ShaderRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir = std::filesystem::temp_directory_path() / "voxelvk_shader_test";
        goldenDir = testDir / "golden";
        outputDir = testDir / "output";
        diffDir = testDir / "diffs";
        
        std::filesystem::create_directories(goldenDir);
        std::filesystem::create_directories(outputDir);
        std::filesystem::create_directories(diffDir);
    }
    
    void TearDown() override {
        if (std::filesystem::exists(testDir)) {
            std::filesystem::remove_all(testDir);
        }
    }
    
    std::filesystem::path testDir;
    std::filesystem::path goldenDir;
    std::filesystem::path outputDir;
    std::filesystem::path diffDir;
    voxelvk::ShaderTester tester;
};

TEST_F(ShaderRegressionTest, BasicVertexColorShader) {
    const std::string shaderName = "basic_vertex_color";
    
    // Render current version
    auto result = tester.renderShader(shaderName);
    ASSERT_TRUE(result.success) << "Shader rendering failed: " << result.errorMessage;
    
    // Save output
    std::string outputPath = (outputDir / (shaderName + ".png")).string();
    ASSERT_TRUE(result.image.savePNG(outputPath));
    
    // Generate golden image if it doesn't exist
    std::string goldenPath = (goldenDir / (shaderName + ".png")).string();
    if (!std::filesystem::exists(goldenPath)) {
        ASSERT_TRUE(result.image.savePNG(goldenPath));
        GTEST_SKIP() << "Golden image created: " << goldenPath;
    }
    
    // Load golden image and compare
    auto goldenImage = voxelvk::ImageData::loadPNG(goldenPath);
    ASSERT_GT(goldenImage.width, 0) << "Failed to load golden image";
    
    auto comparison = tester.compareImages(goldenImage, result.image);
    
    if (!comparison.passed) {
        std::string diffPath = (diffDir / (shaderName + "_diff.png")).string();
        comparison.diffImage.savePNG(diffPath);
        
        FAIL() << "Shader regression detected for " << shaderName << ":\n"
               << "  Different pixels: " << comparison.differentPixels << "\n"
               << "  Max difference: " << comparison.maxDifference << "\n"
               << "  PSNR: " << comparison.psnr << " dB\n"
               << "  SSIM: " << comparison.ssim << "\n"
               << "  Diff saved to: " << diffPath;
    }
}

TEST_F(ShaderRegressionTest, TextureSampleShader) {
    const std::string shaderName = "texture_sample";
    
    auto result = tester.renderShader(shaderName);
    ASSERT_TRUE(result.success);
    
    std::string outputPath = (outputDir / (shaderName + ".png")).string();
    ASSERT_TRUE(result.image.savePNG(outputPath));
    
    std::string goldenPath = (goldenDir / (shaderName + ".png")).string();
    if (!std::filesystem::exists(goldenPath)) {
        ASSERT_TRUE(result.image.savePNG(goldenPath));
        GTEST_SKIP() << "Golden image created: " << goldenPath;
    }
    
    auto goldenImage = voxelvk::ImageData::loadPNG(goldenPath);
    auto comparison = tester.compareImages(goldenImage, result.image);
    
    EXPECT_TRUE(comparison.passed) << "Texture sampling regression detected";
}

TEST_F(ShaderRegressionTest, PhongLightingShader) {
    const std::string shaderName = "lighting_phong";
    
    auto result = tester.renderShader(shaderName);
    ASSERT_TRUE(result.success);
    
    std::string outputPath = (outputDir / (shaderName + ".png")).string();
    ASSERT_TRUE(result.image.savePNG(outputPath));
    
    std::string goldenPath = (goldenDir / (shaderName + ".png")).string();
    if (!std::filesystem::exists(goldenPath)) {
        ASSERT_TRUE(result.image.savePNG(goldenPath));
        GTEST_SKIP() << "Golden image created: " << goldenPath;
    }
    
    auto goldenImage = voxelvk::ImageData::loadPNG(goldenPath);
    auto comparison = tester.compareImages(goldenImage, result.image, 0.05); // Higher tolerance for lighting
    
    EXPECT_TRUE(comparison.passed) << "Phong lighting regression detected";
    EXPECT_GT(comparison.psnr, 30.0) << "PSNR too low: " << comparison.psnr;
    EXPECT_GT(comparison.ssim, 0.9) << "SSIM too low: " << comparison.ssim;
}

TEST_F(ShaderRegressionTest, ComputeParticleShader) {
    const std::string shaderName = "compute_particles";
    
    auto result = tester.renderShader(shaderName);
    ASSERT_TRUE(result.success);
    
    std::string outputPath = (outputDir / (shaderName + ".png")).string();
    ASSERT_TRUE(result.image.savePNG(outputPath));
    
    std::string goldenPath = (goldenDir / (shaderName + ".png")).string();
    if (!std::filesystem::exists(goldenPath)) {
        ASSERT_TRUE(result.image.savePNG(goldenPath));
        GTEST_SKIP() << "Golden image created: " << goldenPath;
    }
    
    auto goldenImage = voxelvk::ImageData::loadPNG(goldenPath);
    auto comparison = tester.compareImages(goldenImage, result.image);
    
    EXPECT_TRUE(comparison.passed) << "Compute particle regression detected";
}

TEST_F(ShaderRegressionTest, UnknownShaderHandling) {
    auto result = tester.renderShader("nonexistent_shader");
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(ShaderRegressionTest, ImageComparisonAccuracy) {
    // Create two identical images
    voxelvk::ImageData image1(100, 100);
    voxelvk::ImageData image2(100, 100);
    
    image1.fill(128, 64, 192);
    image2.fill(128, 64, 192);
    
    auto comparison = tester.compareImages(image1, image2);
    EXPECT_TRUE(comparison.passed);
    EXPECT_EQ(0, comparison.differentPixels);
    EXPECT_DOUBLE_EQ(0.0, comparison.maxDifference);
    
    // Modify one pixel
    image2.setPixel(50, 50, 255, 255, 255);
    comparison = tester.compareImages(image1, image2, 0.01);
    EXPECT_FALSE(comparison.passed);
    EXPECT_EQ(1, comparison.differentPixels);
    EXPECT_GT(comparison.maxDifference, 0.4);
}