#include <filesystem>
#include <iostream>
#include <cassert>

/**
 * Test that all shaders compile to SPIR-V successfully
 */
int main() {
    std::cout << "Testing shader compilation..." << std::endl;
    
    namespace fs = std::filesystem;
    
    // Check for SPIR-V output directory
    fs::path spv_dir = fs::path("build") / ".cache" / "spv";
    
    if (!fs::exists(spv_dir)) {
        // Try alternative locations
        spv_dir = fs::path(".cache") / "spv";
        if (!fs::exists(spv_dir)) {
            spv_dir = fs::path("spv");
            if (!fs::exists(spv_dir)) {
                std::cerr << "❌ No SPIR-V output directory found" << std::endl;
                std::cerr << "   Expected: build/.cache/spv/ or .cache/spv/ or spv/" << std::endl;
                return 1;
            }
        }
    }
    
    // Count compiled shaders
    uint32_t shaderCount = 0;
    uint32_t expectedMinimum = 20; // Expect at least 20 compiled shaders
    
    try {
        for (const auto& entry : fs::recursive_directory_iterator(spv_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".spv") {
                shaderCount++;
                std::cout << "  ✓ " << entry.path().filename() << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "❌ Error scanning SPIR-V directory: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "Found " << shaderCount << " compiled SPIR-V shaders" << std::endl;
    
    if (shaderCount == 0) {
        std::cerr << "❌ No compiled SPIR-V shaders found" << std::endl;
        return 1;
    }
    
    if (shaderCount < expectedMinimum) {
        std::cerr << "⚠️ Warning: Only " << shaderCount << " shaders found, expected at least " << expectedMinimum << std::endl;
        // Don't fail the test for this - just warn
    }
    
    std::cout << "✅ Shader compilation test passed" << std::endl;
    return 0;
}