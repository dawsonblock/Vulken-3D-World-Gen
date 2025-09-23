#ifdef ENABLE_GRAPHICS
#include <iostream>
#include <vector>
#include <random>
#include <cmath>
#include <fstream>
#include <chrono>
#include <map>
#include <string>
#include <array>

// Advanced triplanar mapping and decal system
class AdvancedTriplanarSystem {
public:
    struct TextureData {
        std::string name;
        std::vector<uint8_t> data;
        int width, height, channels;
        float scale;
        float blendFactor;
        float roughness;
        float metallic;

        TextureData(const std::string& n, int w, int h, int c, float s = 1.0f, float bf = 1.0f, float r = 0.5f, float m = 0.0f)
            : name(n), width(w), height(h), channels(c), scale(s), blendFactor(bf), roughness(r), metallic(m) {
            data.resize(static_cast<size_t>(w * h * c));
        }
    };

    struct DecalData {
        std::string name;
        int x, y, z;
        int width, height, depth;
        float rotation;
        float scale;
        float blendMode;
        std::vector<uint8_t> mask;
        std::vector<uint8_t> albedo;
        std::vector<uint8_t> normal;
        std::vector<uint8_t> roughness;

        DecalData(const std::string& n, int px, int py, int pz, int w, int h, int d)
            : name(n), x(px), y(py), z(pz), width(w), height(h), depth(d), rotation(0.0f), scale(1.0f), blendMode(1.0f) {
            mask.resize(static_cast<size_t>(w * h * d));
            albedo.resize(static_cast<size_t>(w * h * d * 3));
            normal.resize(static_cast<size_t>(w * h * d * 3));
            roughness.resize(static_cast<size_t>(w * h * d));
        }
    };

    struct TriplanarMapping {
        float xWeight, yWeight, zWeight;
        float xU, xV, yU, yV, zU, zV;
        std::array<float, 3> xNormal, yNormal, zNormal;
    };

    struct MaterialData {
        std::array<float, 3> albedo;
        std::array<float, 3> normal;
        float roughness;
        float metallic;
        float ao;
    };

private:
    std::vector<TextureData> textures;
    std::vector<DecalData> decals;
    std::mt19937 rng;

    // Generate advanced procedural textures
    void generateAdvancedTextures() {
        std::cout << "Generating advanced procedural textures..." << std::endl;

        // Grass texture with detail
        textures.emplace_back("grass", 128, 128, 3, 0.1f, 0.8f, 0.7f, 0.0f);
        generateGrassTexture(textures.back());

        // Stone texture with variation
        textures.emplace_back("stone", 128, 128, 3, 0.2f, 1.0f, 0.9f, 0.0f);
        generateStoneTexture(textures.back());

        // Water texture with waves
        textures.emplace_back("water", 128, 128, 3, 0.05f, 0.6f, 0.1f, 0.0f);
        generateWaterTexture(textures.back());

        // Mountain texture with cracks
        textures.emplace_back("mountain", 128, 128, 3, 0.3f, 1.0f, 0.8f, 0.0f);
        generateMountainTexture(textures.back());

        // Sand texture with grains
        textures.emplace_back("sand", 128, 128, 3, 0.15f, 0.9f, 0.6f, 0.0f);
        generateSandTexture(textures.back());

        // Snow texture
        textures.emplace_back("snow", 128, 128, 3, 0.2f, 0.7f, 0.3f, 0.0f);
        generateSnowTexture(textures.back());
    }

    void generateGrassTexture(TextureData& texture) {
        for (int y = 0; y < texture.height; y++) {
            for (int x = 0; x < texture.width; x++) {
                float noise1 = std::sin(x * 0.1f) * std::cos(y * 0.1f);
                float noise2 = std::sin(x * 0.3f) * std::cos(y * 0.3f);
                float detail = std::sin(x * 0.8f) * std::cos(y * 0.8f);

                int idx = (y * texture.width + x) * texture.channels;

                // Base grass color with variation
                float r = 34 + noise1 * 20 + detail * 5;
                float g = 139 + noise2 * 30 + detail * 8;
                float b = 34 + noise1 * 15 + detail * 3;

                texture.data[idx] = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, r)));
                texture.data[idx + 1] = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, g)));
                texture.data[idx + 2] = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, b)));
            }
        }
    }

    void generateStoneTexture(TextureData& texture) {
        for (int y = 0; y < texture.height; y++) {
            for (int x = 0; x < texture.width; x++) {
                float noise1 = std::sin(x * 0.2f) * std::cos(y * 0.2f);
                float noise2 = std::sin(x * 0.5f) * std::cos(y * 0.5f);
                float cracks = std::sin(x * 0.1f + y * 0.1f) * 0.3f;

                int idx = (y * texture.width + x) * texture.channels;

                float baseColor = 100 + noise1 * 40 + noise2 * 20 + cracks * 30;
                baseColor = std::max(0.0f, std::min(255.0f, baseColor));

                texture.data[idx] = static_cast<uint8_t>(baseColor);
                texture.data[idx + 1] = static_cast<uint8_t>(baseColor);
                texture.data[idx + 2] = static_cast<uint8_t>(baseColor);
            }
        }
    }

    void generateWaterTexture(TextureData& texture) {
        for (int y = 0; y < texture.height; y++) {
            for (int x = 0; x < texture.width; x++) {
                float wave1 = std::sin(x * 0.3f + y * 0.1f);
                float wave2 = std::sin(x * 0.1f + y * 0.3f);
                float foam = std::sin(x * 0.8f + y * 0.8f) * 0.2f;

                int idx = (y * texture.width + x) * texture.channels;

                texture.data[idx] = static_cast<uint8_t>(30 + wave1 * 10 + foam * 20);
                texture.data[idx + 1] = static_cast<uint8_t>(144 + wave2 * 20 + foam * 15);
                texture.data[idx + 2] = static_cast<uint8_t>(255 - foam * 50);
            }
        }
    }

    void generateMountainTexture(TextureData& texture) {
        for (int y = 0; y < texture.height; y++) {
            for (int x = 0; x < texture.width; x++) {
                float noise1 = std::sin(x * 0.1f) * std::cos(y * 0.1f);
                float noise2 = std::sin(x * 0.4f) * std::cos(y * 0.4f);
                float cracks = std::sin(x * 0.05f + y * 0.05f) * 0.4f;

                int idx = (y * texture.width + x) * texture.channels;

                float baseColor = 80 + noise1 * 30 + noise2 * 15 + cracks * 25;
                baseColor = std::max(0.0f, std::min(255.0f, baseColor));

                texture.data[idx] = static_cast<uint8_t>(baseColor);
                texture.data[idx + 1] = static_cast<uint8_t>(baseColor);
                texture.data[idx + 2] = static_cast<uint8_t>(baseColor);
            }
        }
    }

    void generateSandTexture(TextureData& texture) {
        for (int y = 0; y < texture.height; y++) {
            for (int x = 0; x < texture.width; x++) {
                float noise = std::sin(x * 0.2f) * std::cos(y * 0.2f);
                float grains = std::sin(x * 0.6f) * std::cos(y * 0.6f) * 0.3f;

                int idx = (y * texture.width + x) * texture.channels;

                texture.data[idx] = static_cast<uint8_t>(238 + noise * 15 + grains * 10);
                texture.data[idx + 1] = static_cast<uint8_t>(203 + noise * 10 + grains * 8);
                texture.data[idx + 2] = static_cast<uint8_t>(173 + noise * 8 + grains * 5);
            }
        }
    }

    void generateSnowTexture(TextureData& texture) {
        for (int y = 0; y < texture.height; y++) {
            for (int x = 0; x < texture.width; x++) {
                float noise = std::sin(x * 0.3f) * std::cos(y * 0.3f);
                float sparkle = std::sin(x * 0.8f) * std::cos(y * 0.8f) * 0.2f;

                int idx = (y * texture.width + x) * texture.channels;

                float baseColor = 240 + noise * 10 + sparkle * 15;
                baseColor = std::max(0.0f, std::min(255.0f, baseColor));

                texture.data[idx] = static_cast<uint8_t>(baseColor);
                texture.data[idx + 1] = static_cast<uint8_t>(baseColor);
                texture.data[idx + 2] = static_cast<uint8_t>(baseColor);
            }
        }
    }

    // Advanced triplanar mapping calculation
    TriplanarMapping calculateAdvancedTriplanarMapping(float x, float y, float z, const TextureData& texture, const std::array<float, 3>& normal) {
        TriplanarMapping mapping;

        // Calculate weights based on surface normals with sharpness control
        float sharpness = 4.0f; // Controls blending sharpness
        float nx = std::pow(std::abs(normal[0]), sharpness);
        float ny = std::pow(std::abs(normal[1]), sharpness);
        float nz = std::pow(std::abs(normal[2]), sharpness);

        float total = nx + ny + nz;
        mapping.xWeight = nx / total;
        mapping.yWeight = ny / total;
        mapping.zWeight = nz / total;

        // Calculate UV coordinates for each plane with proper scaling
        mapping.xU = (z * texture.scale) - std::floor(z * texture.scale);
        mapping.xV = (y * texture.scale) - std::floor(y * texture.scale);

        mapping.yU = (x * texture.scale) - std::floor(x * texture.scale);
        mapping.yV = (z * texture.scale) - std::floor(z * texture.scale);

        mapping.zU = (x * texture.scale) - std::floor(x * texture.scale);
        mapping.zV = (y * texture.scale) - std::floor(y * texture.scale);

        // Store plane normals
        mapping.xNormal = {1.0f, 0.0f, 0.0f};
        mapping.yNormal = {0.0f, 1.0f, 0.0f};
        mapping.zNormal = {0.0f, 0.0f, 1.0f};

        return mapping;
    }

    // Sample texture with advanced triplanar mapping
    MaterialData sampleTextureAdvancedTriplanar(float x, float y, float z, const TextureData& texture, const std::array<float, 3>& normal) {
        auto mapping = calculateAdvancedTriplanarMapping(x, y, z, texture, normal);

        MaterialData result;

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
        for (int c = 0; c < 3; c++) {
            float blended = texture.data[xIdx + c] * mapping.xWeight +
                           texture.data[yIdx + c] * mapping.yWeight +
                           texture.data[zIdx + c] * mapping.zWeight;
            result.albedo[c] = blended / 255.0f; // Normalize to 0-1
        }

        // Set material properties
        result.roughness = texture.roughness;
        result.metallic = texture.metallic;
        result.ao = 1.0f; // Ambient occlusion

        return result;
    }

    // Generate advanced decals
    void generateAdvancedDecals() {
        std::cout << "Generating advanced terrain decals..." << std::endl;

        // Path decal with wear patterns
        decals.emplace_back("path", 64, 32, 64, 32, 1, 8);
        generatePathDecal(decals.back());

        // Road decal with tire marks
        decals.emplace_back("road", 32, 16, 32, 16, 1, 12);
        generateRoadDecal(decals.back());

        // River decal with flow patterns
        decals.emplace_back("river", 96, 8, 96, 8, 1, 6);
        generateRiverDecal(decals.back());

        // Footprint decals
        decals.emplace_back("footprints", 16, 8, 16, 8, 1, 4);
        generateFootprintDecal(decals.back());
    }

    void generatePathDecal(DecalData& decal) {
        for (int z = 0; z < decal.depth; z++) {
            for (int x = 0; x < decal.width; x++) {
                int idx = z * decal.width + x;

                // Create winding path with wear patterns
                float pathCenter = decal.width * 0.5f + std::sin(z * 0.1f) * 5.0f;
                float distFromCenter = std::abs(x - pathCenter);

                if (distFromCenter < 2.0f) {
                    decal.mask[idx] = 255; // Full decal
                    // Wear pattern in center
                    if (distFromCenter < 1.0f) {
                        decal.albedo[idx * 3] = 100;     // R
                        decal.albedo[idx * 3 + 1] = 80;  // G
                        decal.albedo[idx * 3 + 2] = 60;  // B
                    } else {
                        decal.albedo[idx * 3] = 120;     // R
                        decal.albedo[idx * 3 + 1] = 100; // G
                        decal.albedo[idx * 3 + 2] = 80;  // B
                    }
                } else if (distFromCenter < 3.0f) {
                    decal.mask[idx] = static_cast<uint8_t>(255 * (3.0f - distFromCenter));
                    decal.albedo[idx * 3] = 140;     // R
                    decal.albedo[idx * 3 + 1] = 120; // G
                    decal.albedo[idx * 3 + 2] = 100; // B
                } else {
                    decal.mask[idx] = 0; // No decal
                }

                // Set normal and roughness
                decal.normal[idx * 3] = 0;     // R
                decal.normal[idx * 3 + 1] = 0; // G
                decal.normal[idx * 3 + 2] = 255; // B (up)
                decal.roughness[idx] = 200; // Rough surface
            }
        }
    }

    void generateRoadDecal(DecalData& decal) {
        for (int z = 0; z < decal.depth; z++) {
            for (int x = 0; x < decal.width; x++) {
                int idx = z * decal.width + x;

                // Create straight road with tire marks
                float roadCenter = decal.width * 0.5f;
                float distFromCenter = std::abs(x - roadCenter);

                if (distFromCenter < 3.0f) {
                    decal.mask[idx] = 255; // Full decal
                    // Tire marks
                    if (std::abs(x - roadCenter) < 0.5f) {
                        decal.albedo[idx * 3] = 60;      // R
                        decal.albedo[idx * 3 + 1] = 60; // G
                        decal.albedo[idx * 3 + 2] = 60; // B
                    } else {
                        decal.albedo[idx * 3] = 80;     // R
                        decal.albedo[idx * 3 + 1] = 80; // G
                        decal.albedo[idx * 3 + 2] = 80; // B
                    }
                } else if (distFromCenter < 4.0f) {
                    decal.mask[idx] = static_cast<uint8_t>(255 * (4.0f - distFromCenter));
                    decal.albedo[idx * 3] = 100;     // R
                    decal.albedo[idx * 3 + 1] = 100; // G
                    decal.albedo[idx * 3 + 2] = 100; // B
                } else {
                    decal.mask[idx] = 0; // No decal
                }

                // Set normal and roughness
                decal.normal[idx * 3] = 0;     // R
                decal.normal[idx * 3 + 1] = 0; // G
                decal.normal[idx * 3 + 2] = 255; // B (up)
                decal.roughness[idx] = 150; // Smooth surface
            }
        }
    }

    void generateRiverDecal(DecalData& decal) {
        for (int z = 0; z < decal.depth; z++) {
            for (int x = 0; x < decal.width; x++) {
                int idx = z * decal.width + x;

                // Create meandering river with flow patterns
                float riverCenter = decal.width * 0.5f + std::sin(z * 0.05f) * 8.0f;
                float distFromCenter = std::abs(x - riverCenter);

                if (distFromCenter < 1.5f) {
                    decal.mask[idx] = 255; // Full decal
                    // Flow patterns
                    float flow = std::sin(z * 0.2f + x * 0.1f);
                    decal.albedo[idx * 3] = 30 + flow * 10;     // R
                    decal.albedo[idx * 3 + 1] = 144 + flow * 20; // G
                    decal.albedo[idx * 3 + 2] = 255;           // B
                } else if (distFromCenter < 2.5f) {
                    decal.mask[idx] = static_cast<uint8_t>(255 * (2.5f - distFromCenter));
                    decal.albedo[idx * 3] = 50;      // R
                    decal.albedo[idx * 3 + 1] = 160; // G
                    decal.albedo[idx * 3 + 2] = 240; // B
                } else {
                    decal.mask[idx] = 0; // No decal
                }

                // Set normal and roughness
                decal.normal[idx * 3] = 0;     // R
                decal.normal[idx * 3 + 1] = 0; // G
                decal.normal[idx * 3 + 2] = 255; // B (up)
                decal.roughness[idx] = 50; // Smooth water surface
            }
        }
    }

    void generateFootprintDecal(DecalData& decal) {
        for (int z = 0; z < decal.depth; z++) {
            for (int x = 0; x < decal.width; x++) {
                int idx = z * decal.width + x;

                // Create footprint pattern
                float centerX = decal.width * 0.5f;
                float centerZ = decal.depth * 0.5f;
                float dist = std::sqrt((x - centerX) * (x - centerX) + (z - centerZ) * (z - centerZ));

                if (dist < 2.0f) {
                    decal.mask[idx] = static_cast<uint8_t>(255 * (2.0f - dist) / 2.0f);
                    decal.albedo[idx * 3] = 120;     // R
                    decal.albedo[idx * 3 + 1] = 100; // G
                    decal.albedo[idx * 3 + 2] = 80; // B
                } else {
                    decal.mask[idx] = 0; // No decal
                }

                // Set normal and roughness
                decal.normal[idx * 3] = 0;     // R
                decal.normal[idx * 3 + 1] = 0; // G
                decal.normal[idx * 3 + 2] = 255; // B (up)
                decal.roughness[idx] = 180; // Rough surface
            }
        }
    }

public:
    AdvancedTriplanarSystem() : rng(42) {
        generateAdvancedTextures();
        generateAdvancedDecals();
    }

    void printTextureInfo() {
        std::cout << "\nAdvanced Triplanar Texturing System:" << std::endl;
        std::cout << "====================================" << std::endl;

        std::cout << "\nGenerated Textures:" << std::endl;
        for (const auto& texture : textures) {
            std::cout << "- " << texture.name << " (" << texture.width << "x" << texture.height
                     << ", scale: " << texture.scale << ", blend: " << texture.blendFactor
                     << ", roughness: " << texture.roughness << ", metallic: " << texture.metallic << ")" << std::endl;
        }

        std::cout << "\nGenerated Decals:" << std::endl;
        for (const auto& decal : decals) {
            std::cout << "- " << decal.name << " at (" << decal.x << "," << decal.y << "," << decal.z
                     << ") size " << decal.width << "x" << decal.height << "x" << decal.depth << std::endl;
        }
    }

    void demonstrateAdvancedTriplanarMapping() {
        std::cout << "\nAdvanced Triplanar Mapping Demonstration:" << std::endl;
        std::cout << "=========================================" << std::endl;

        // Test triplanar mapping at various positions with different normals
        std::vector<std::tuple<float, float, float, std::array<float, 3>>> testCases = {
            {10.0f, 5.0f, 10.0f, {0.0f, 1.0f, 0.0f}}, // Flat surface
            {25.0f, 15.0f, 25.0f, {0.707f, 0.707f, 0.0f}}, // Diagonal surface
            {50.0f, 30.0f, 50.0f, {0.0f, 0.0f, 1.0f}} // Vertical surface
        };

        for (const auto& texture : textures) {
            std::cout << "\nTesting " << texture.name << " texture:" << std::endl;

            for (const auto& testCase : testCases) {
                float x, y, z;
                std::array<float, 3> normal;
                std::tie(x, y, z, normal) = testCase;

                auto material = sampleTextureAdvancedTriplanar(x, y, z, texture, normal);

                std::cout << "  Position (" << x << "," << y << "," << z << ") Normal ("
                         << normal[0] << "," << normal[1] << "," << normal[2] << "):" << std::endl;
                std::cout << "    Albedo: R=" << material.albedo[0] << " G=" << material.albedo[1] << " B=" << material.albedo[2] << std::endl;
                std::cout << "    Roughness: " << material.roughness << " Metallic: " << material.metallic << std::endl;
            }
        }
    }

    void exportTextures(const std::string& basePath) {
        std::cout << "\nExporting advanced textures..." << std::endl;

        for (const auto& texture : textures) {
            std::string filename = basePath + "/" + texture.name + "_advanced.raw";
            std::ofstream file(filename, std::ios::binary);

            if (file.is_open()) {
                file.write(reinterpret_cast<const char*>(texture.data.data()),
                          static_cast<std::streamsize>(texture.data.size()));
                std::cout << "Exported " << texture.name << " to " << filename << std::endl;
            }
        }
    }

    void exportDecals(const std::string& basePath) {
        std::cout << "\nExporting advanced decals..." << std::endl;

        for (const auto& decal : decals) {
            std::string filename = basePath + "/" + decal.name + "_advanced_decal.raw";
            std::ofstream file(filename, std::ios::binary);

            if (file.is_open()) {
                file.write(reinterpret_cast<const char*>(decal.mask.data()),
                          static_cast<std::streamsize>(decal.mask.size()));
                file.write(reinterpret_cast<const char*>(decal.albedo.data()),
                          static_cast<std::streamsize>(decal.albedo.size()));
                file.write(reinterpret_cast<const char*>(decal.normal.data()),
                          static_cast<std::streamsize>(decal.normal.size()));
                file.write(reinterpret_cast<const char*>(decal.roughness.data()),
                          static_cast<std::streamsize>(decal.roughness.size()));
                std::cout << "Exported " << decal.name << " decal to " << filename << std::endl;
            }
        }
    }
};

int main() {
    std::cout << "Advanced Triplanar Mapping Demo" << std::endl;
    std::cout << "===============================" << std::endl;

    // Create advanced triplanar system
    AdvancedTriplanarSystem triplanarSystem;

    // Print information
    triplanarSystem.printTextureInfo();

    // Demonstrate advanced triplanar mapping
    triplanarSystem.demonstrateAdvancedTriplanarMapping();

    // Export textures and decals
    triplanarSystem.exportTextures("textures");
    triplanarSystem.exportDecals("decals");

    std::cout << "\nAdvanced triplanar mapping demo complete!" << std::endl;
    std::cout << "Features:" << std::endl;
    std::cout << "- Advanced procedural texture generation with material properties" << std::endl;
    std::cout << "- Enhanced triplanar mapping with normal-based blending" << std::endl;
    std::cout << "- Advanced decal system with albedo, normal, and roughness maps" << std::endl;
    std::cout << "- Material property integration (roughness, metallic, AO)" << std::endl;
    std::cout << "- Sharpness control for blending transitions" << std::endl;

    return 0;
}

#else
#include <iostream>
int main() {
    std::cout << "Advanced triplanar mapping demo requires graphics support (ENABLE_GRAPHICS=ON)" << std::endl;
    return 1;
}
#endif
