#ifdef ENABLE_GRAPHICS
#include <iostream>
#include <vector>
#include <random>
#include <cmath>
#include <fstream>
#include <chrono>
#include <map>
#include <string>

// Advanced terrain texturing system
class AdvancedTexturingSystem {
public:
    struct TextureData {
        std::string name;
        std::vector<uint8_t> data;
        int width, height, channels;
        float scale;
        float blendFactor;

        TextureData(const std::string& n, int w, int h, int c, float s = 1.0f, float bf = 1.0f)
            : name(n), width(w), height(h), channels(c), scale(s), blendFactor(bf) {
            data.resize(static_cast<size_t>(w * h * c));
        }
    };

    struct DecalData {
        std::string name;
        int x, y, z;
        int width, height, depth;
        float rotation;
        float scale;
        std::vector<uint8_t> mask;

        DecalData(const std::string& n, int px, int py, int pz, int w, int h, int d)
            : name(n), x(px), y(py), z(pz), width(w), height(h), depth(d), rotation(0.0f), scale(1.0f) {
            mask.resize(static_cast<size_t>(w * h * d));
        }
    };

    struct TriplanarMapping {
        float xWeight, yWeight, zWeight;
        float xU, xV, yU, yV, zU, zV;
    };

private:
    std::vector<TextureData> textures;
    std::vector<DecalData> decals;
    std::mt19937 rng;

    // Generate procedural textures
    void generateProceduralTextures() {
        std::cout << "Generating procedural textures..." << std::endl;

        // Grass texture
        textures.emplace_back("grass", 64, 64, 3, 0.1f, 0.8f);
        generateGrassTexture(textures.back());

        // Stone texture
        textures.emplace_back("stone", 64, 64, 3, 0.2f, 1.0f);
        generateStoneTexture(textures.back());

        // Water texture
        textures.emplace_back("water", 64, 64, 3, 0.05f, 0.6f);
        generateWaterTexture(textures.back());

        // Mountain texture
        textures.emplace_back("mountain", 64, 64, 3, 0.3f, 1.0f);
        generateMountainTexture(textures.back());

        // Sand texture
        textures.emplace_back("sand", 64, 64, 3, 0.15f, 0.9f);
        generateSandTexture(textures.back());
    }

    void generateGrassTexture(TextureData& texture) {
        for (int y = 0; y < texture.height; y++) {
            for (int x = 0; x < texture.width; x++) {
                float noise = std::sin(x * 0.1f) * std::cos(y * 0.1f);
                float grassNoise = std::sin(x * 0.3f) * std::cos(y * 0.3f);

                int idx = (y * texture.width + x) * texture.channels;

                // Base grass color with variation
                texture.data[idx] = static_cast<uint8_t>(34 + noise * 20);     // R
                texture.data[idx + 1] = static_cast<uint8_t>(139 + grassNoise * 30); // G
                texture.data[idx + 2] = static_cast<uint8_t>(34 + noise * 15);      // B
            }
        }
    }

    void generateStoneTexture(TextureData& texture) {
        for (int y = 0; y < texture.height; y++) {
            for (int x = 0; x < texture.width; x++) {
                float noise1 = std::sin(x * 0.2f) * std::cos(y * 0.2f);
                float noise2 = std::sin(x * 0.5f) * std::cos(y * 0.5f);

                int idx = (y * texture.width + x) * texture.channels;

                float baseColor = 100 + noise1 * 40 + noise2 * 20;
                baseColor = std::max(0.0f, std::min(255.0f, baseColor));

                texture.data[idx] = static_cast<uint8_t>(baseColor);     // R
                texture.data[idx + 1] = static_cast<uint8_t>(baseColor); // G
                texture.data[idx + 2] = static_cast<uint8_t>(baseColor); // B
            }
        }
    }

    void generateWaterTexture(TextureData& texture) {
        for (int y = 0; y < texture.height; y++) {
            for (int x = 0; x < texture.width; x++) {
                float wave1 = std::sin(x * 0.3f + y * 0.1f);
                float wave2 = std::sin(x * 0.1f + y * 0.3f);

                int idx = (y * texture.width + x) * texture.channels;

                texture.data[idx] = static_cast<uint8_t>(30 + wave1 * 10);     // R
                texture.data[idx + 1] = static_cast<uint8_t>(144 + wave2 * 20); // G
                texture.data[idx + 2] = static_cast<uint8_t>(255);             // B
            }
        }
    }

    void generateMountainTexture(TextureData& texture) {
        for (int y = 0; y < texture.height; y++) {
            for (int x = 0; x < texture.width; x++) {
                float noise1 = std::sin(x * 0.1f) * std::cos(y * 0.1f);
                float noise2 = std::sin(x * 0.4f) * std::cos(y * 0.4f);

                int idx = (y * texture.width + x) * texture.channels;

                float baseColor = 80 + noise1 * 30 + noise2 * 15;
                baseColor = std::max(0.0f, std::min(255.0f, baseColor));

                texture.data[idx] = static_cast<uint8_t>(baseColor);     // R
                texture.data[idx + 1] = static_cast<uint8_t>(baseColor); // G
                texture.data[idx + 2] = static_cast<uint8_t>(baseColor); // B
            }
        }
    }

    void generateSandTexture(TextureData& texture) {
        for (int y = 0; y < texture.height; y++) {
            for (int x = 0; x < texture.width; x++) {
                float noise = std::sin(x * 0.2f) * std::cos(y * 0.2f);

                int idx = (y * texture.width + x) * texture.channels;

                texture.data[idx] = static_cast<uint8_t>(238 + noise * 15);     // R
                texture.data[idx + 1] = static_cast<uint8_t>(203 + noise * 10); // G
                texture.data[idx + 2] = static_cast<uint8_t>(173 + noise * 8);  // B
            }
        }
    }

    // Triplanar mapping calculation
    TriplanarMapping calculateTriplanarMapping(float x, float y, float z, const TextureData& texture) {
        TriplanarMapping mapping;

        // Calculate weights based on surface normals
        float nx = std::abs(std::sin(x * 0.1f));
        float ny = std::abs(std::cos(y * 0.1f));
        float nz = std::abs(std::sin(z * 0.1f));

        float total = nx + ny + nz;
        mapping.xWeight = nx / total;
        mapping.yWeight = ny / total;
        mapping.zWeight = nz / total;

        // Calculate UV coordinates for each plane
        mapping.xU = (z * texture.scale) - std::floor(z * texture.scale);
        mapping.xV = (y * texture.scale) - std::floor(y * texture.scale);

        mapping.yU = (x * texture.scale) - std::floor(x * texture.scale);
        mapping.yV = (z * texture.scale) - std::floor(z * texture.scale);

        mapping.zU = (x * texture.scale) - std::floor(x * texture.scale);
        mapping.zV = (y * texture.scale) - std::floor(y * texture.scale);

        return mapping;
    }

    // Sample texture with triplanar mapping
    std::vector<uint8_t> sampleTextureTriplanar(float x, float y, float z, const TextureData& texture) {
        auto mapping = calculateTriplanarMapping(x, y, z, texture);

        std::vector<uint8_t> result(texture.channels, 0);

        // Sample from X plane
        int xU = static_cast<int>(mapping.xU * texture.width) % texture.width;
        int xV = static_cast<int>(mapping.xV * texture.height) % texture.height;
        int xIdx = (xV * texture.width + xU) * texture.channels;

        // Sample from Y plane
        int yU = static_cast<int>(mapping.yU * texture.width) % texture.width;
        int yV = static_cast<int>(mapping.yV * texture.height) % texture.height;
        int yIdx = (yV * texture.width + yU) * texture.channels;

        // Sample from Z plane
        int zU = static_cast<int>(mapping.zU * texture.width) % texture.width;
        int zV = static_cast<int>(mapping.zV * texture.height) % texture.height;
        int zIdx = (zV * texture.width + zU) * texture.channels;

        // Blend samples based on weights
        for (int c = 0; c < texture.channels; c++) {
            float blended = texture.data[xIdx + c] * mapping.xWeight +
                           texture.data[yIdx + c] * mapping.yWeight +
                           texture.data[zIdx + c] * mapping.zWeight;
            result[c] = static_cast<uint8_t>(blended);
        }

        return result;
    }

    // Generate decals
    void generateDecals() {
        std::cout << "Generating terrain decals..." << std::endl;

        // Path decal
        decals.emplace_back("path", 64, 32, 64, 32, 1, 8);
        generatePathDecal(decals.back());

        // Road decal
        decals.emplace_back("road", 32, 16, 32, 16, 1, 12);
        generateRoadDecal(decals.back());

        // River decal
        decals.emplace_back("river", 96, 8, 96, 8, 1, 6);
        generateRiverDecal(decals.back());
    }

    void generatePathDecal(DecalData& decal) {
        for (int z = 0; z < decal.depth; z++) {
            for (int x = 0; x < decal.width; x++) {
                int idx = z * decal.width + x;

                // Create winding path
                float pathCenter = decal.width * 0.5f + std::sin(z * 0.1f) * 5.0f;
                float distFromCenter = std::abs(x - pathCenter);

                if (distFromCenter < 2.0f) {
                    decal.mask[idx] = 255; // Full decal
                } else if (distFromCenter < 3.0f) {
                    decal.mask[idx] = static_cast<uint8_t>(255 * (3.0f - distFromCenter)); // Fade edges
                } else {
                    decal.mask[idx] = 0; // No decal
                }
            }
        }
    }

    void generateRoadDecal(DecalData& decal) {
        for (int z = 0; z < decal.depth; z++) {
            for (int x = 0; x < decal.width; x++) {
                int idx = z * decal.width + x;

                // Create straight road
                float roadCenter = decal.width * 0.5f;
                float distFromCenter = std::abs(x - roadCenter);

                if (distFromCenter < 3.0f) {
                    decal.mask[idx] = 255; // Full decal
                } else if (distFromCenter < 4.0f) {
                    decal.mask[idx] = static_cast<uint8_t>(255 * (4.0f - distFromCenter)); // Fade edges
                } else {
                    decal.mask[idx] = 0; // No decal
                }
            }
        }
    }

    void generateRiverDecal(DecalData& decal) {
        for (int z = 0; z < decal.depth; z++) {
            for (int x = 0; x < decal.width; x++) {
                int idx = z * decal.width + x;

                // Create meandering river
                float riverCenter = decal.width * 0.5f + std::sin(z * 0.05f) * 8.0f;
                float distFromCenter = std::abs(x - riverCenter);

                if (distFromCenter < 1.5f) {
                    decal.mask[idx] = 255; // Full decal
                } else if (distFromCenter < 2.5f) {
                    decal.mask[idx] = static_cast<uint8_t>(255 * (2.5f - distFromCenter)); // Fade edges
                } else {
                    decal.mask[idx] = 0; // No decal
                }
            }
        }
    }

public:
    AdvancedTexturingSystem() : rng(42) {
        generateProceduralTextures();
        generateDecals();
    }

    void printTextureInfo() {
        std::cout << "\nAdvanced Texturing System:" << std::endl;
        std::cout << "==========================" << std::endl;

        std::cout << "\nGenerated Textures:" << std::endl;
        for (const auto& texture : textures) {
            std::cout << "- " << texture.name << " (" << texture.width << "x" << texture.height
                     << ", scale: " << texture.scale << ", blend: " << texture.blendFactor << ")" << std::endl;
        }

        std::cout << "\nGenerated Decals:" << std::endl;
        for (const auto& decal : decals) {
            std::cout << "- " << decal.name << " at (" << decal.x << "," << decal.y << "," << decal.z
                     << ") size " << decal.width << "x" << decal.height << "x" << decal.depth << std::endl;
        }
    }

    void demonstrateTriplanarMapping() {
        std::cout << "\nTriplanar Mapping Demonstration:" << std::endl;
        std::cout << "=================================" << std::endl;

        // Test triplanar mapping at various positions
        std::vector<std::tuple<float, float, float>> testPositions = {
            {10.0f, 5.0f, 10.0f},
            {25.0f, 15.0f, 25.0f},
            {50.0f, 30.0f, 50.0f}
        };

        for (const auto& texture : textures) {
            std::cout << "\nTesting " << texture.name << " texture:" << std::endl;

            for (const auto& pos : testPositions) {
                float x, y, z;
                std::tie(x, y, z) = pos;

                auto mapping = calculateTriplanarMapping(x, y, z, texture);
                auto color = sampleTextureTriplanar(x, y, z, texture);

                std::cout << "  Position (" << x << "," << y << "," << z << "):" << std::endl;
                std::cout << "    Weights: X=" << mapping.xWeight << " Y=" << mapping.yWeight << " Z=" << mapping.zWeight << std::endl;
                std::cout << "    Color: R=" << static_cast<int>(color[0]) << " G=" << static_cast<int>(color[1]) << " B=" << static_cast<int>(color[2]) << std::endl;
            }
        }
    }

    void exportTextures(const std::string& basePath) {
        std::cout << "\nExporting textures..." << std::endl;

        for (const auto& texture : textures) {
            std::string filename = basePath + "/" + texture.name + ".raw";
            std::ofstream file(filename, std::ios::binary);

            if (file.is_open()) {
                file.write(reinterpret_cast<const char*>(texture.data.data()), texture.data.size());
                std::cout << "Exported " << texture.name << " to " << filename << std::endl;
            }
        }
    }

    void exportDecals(const std::string& basePath) {
        std::cout << "\nExporting decals..." << std::endl;

        for (const auto& decal : decals) {
            std::string filename = basePath + "/" + decal.name + "_decal.raw";
            std::ofstream file(filename, std::ios::binary);

            if (file.is_open()) {
                file.write(reinterpret_cast<const char*>(decal.mask.data()), decal.mask.size());
                std::cout << "Exported " << decal.name << " decal to " << filename << std::endl;
            }
        }
    }
};

int main() {
    std::cout << "Advanced Texturing System Demo" << std::endl;
    std::cout << "==============================" << std::endl;

    // Create texturing system
    AdvancedTexturingSystem texturingSystem;

    // Print information
    texturingSystem.printTextureInfo();

    // Demonstrate triplanar mapping
    texturingSystem.demonstrateTriplanarMapping();

    // Export textures and decals
    texturingSystem.exportTextures("textures");
    texturingSystem.exportDecals("decals");

    std::cout << "\nAdvanced texturing system demo complete!" << std::endl;
    std::cout << "Features:" << std::endl;
    std::cout << "- Procedural texture generation (grass, stone, water, mountain, sand)" << std::endl;
    std::cout << "- Triplanar mapping for seamless texture application" << std::endl;
    std::cout << "- Decal system for paths, roads, and rivers" << std::endl;
    std::cout << "- Weighted blending based on surface normals" << std::endl;
    std::cout << "- UV coordinate calculation for each plane" << std::endl;

    return 0;
}

#else
#include <iostream>
int main() {
    std::cout << "Advanced texturing demo requires graphics support (ENABLE_GRAPHICS=ON)" << std::endl;
    return 1;
}
#endif
