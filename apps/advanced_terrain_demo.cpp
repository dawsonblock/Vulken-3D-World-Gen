#ifdef ENABLE_GRAPHICS
#include <iostream>
#include <vector>
#include <random>
#include <cmath>
#include <fstream>
#include <chrono>

// Advanced terrain generation with 3D voxel support
class AdvancedTerrainGenerator {
public:
    struct Voxel {
        uint8_t type;
        float density;
        bool isSolid;

        Voxel() : type(0), density(0.0f), isSolid(false) {}
        Voxel(uint8_t t, float d, bool solid) : type(t), density(d), isSolid(solid) {}
    };

    struct TerrainConfig {
        int width = 128;
        int height = 64;
        int depth = 128;
        float noiseScale = 0.01f;
        float caveThreshold = 0.3f;
        float mountainHeight = 0.7f;
        float waterLevel = 0.2f;
        int seed = 12345;
    };

private:
    TerrainConfig config;
    std::vector<std::vector<std::vector<Voxel>>> voxels;
    std::mt19937 rng;

    // Noise functions for terrain generation
    float noise(float x, float y, float z) {
        // Simple 3D noise implementation
        float n = 0.0f;
        float frequency = 1.0f;
        float amplitude = 1.0f;

        for (int i = 0; i < 4; i++) {
            n += amplitude * std::sin(x * frequency * config.noiseScale) *
                        std::cos(y * frequency * config.noiseScale) *
                        std::sin(z * frequency * config.noiseScale);
            frequency *= 2.0f;
            amplitude *= 0.5f;
        }

        return n;
    }

    float fbm(float x, float y, float z) {
        float value = 0.0f;
        float frequency = 1.0f;
        float amplitude = 1.0f;

        for (int i = 0; i < 6; i++) {
            value += amplitude * noise(x * frequency, y * frequency, z * frequency);
            frequency *= 2.0f;
            amplitude *= 0.5f;
        }

        return value;
    }

    // Distance field for caves
    float caveDistance(float x, float y, float z) {
        // Create cave networks using distance fields
        float cave1 = std::sqrt((x - 64) * (x - 64) + (y - 32) * (y - 32) + (z - 64) * (z - 64)) - 15.0f;
        float cave2 = std::sqrt((x - 32) * (x - 32) + (y - 16) * (y - 16) + (z - 32) * (z - 32)) - 8.0f;
        float cave3 = std::sqrt((x - 96) * (x - 96) + (y - 48) * (y - 48) + (z - 96) * (z - 96)) - 12.0f;

        return std::min({cave1, cave2, cave3});
    }

public:
    AdvancedTerrainGenerator(const TerrainConfig& cfg) : config(cfg), rng(static_cast<unsigned int>(config.seed)) {
        voxels.resize(static_cast<size_t>(config.width),
                     std::vector<std::vector<Voxel>>(static_cast<size_t>(config.height),
                     std::vector<Voxel>(static_cast<size_t>(config.depth))));
    }

    void generateTerrain() {
        std::cout << "Generating advanced 3D terrain..." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();

        // Generate terrain using 3D noise
        for (int x = 0; x < config.width; x++) {
            for (int z = 0; z < config.depth; z++) {
                // Calculate height using 2D noise
                float height = fbm(x, 0, z) * 20.0f + config.height * 0.3f;
                height = std::max(1.0f, std::min(height, static_cast<float>(config.height - 1)));

                for (int y = 0; y < config.height; y++) {
                    float normalizedY = static_cast<float>(y) / config.height;

                    // 3D noise for density
                    float density = fbm(x, y, z);

                    // Cave generation using distance fields
                    float caveDist = caveDistance(x, y, z);
                    bool isCave = caveDist < 0.0f && density > -0.2f;

                    // Determine voxel type based on position and noise
                    uint8_t type = 0; // Air
                    bool isSolid = false;

                    if (y < height) {
                        if (isCave) {
                            type = 0; // Air in caves
                        } else if (normalizedY < config.waterLevel) {
                            type = 3; // Water
                            isSolid = true;
                        } else if (y == static_cast<int>(height) - 1) {
                            type = 2; // Grass on surface
                            isSolid = true;
                        } else if (normalizedY > config.mountainHeight) {
                            type = 4; // Mountain/Stone
                            isSolid = true;
                        } else {
                            type = 1; // Stone
                            isSolid = true;
                        }
                    } else if (normalizedY < config.waterLevel) {
                        type = 3; // Water above terrain
                        isSolid = true;
                    }

                    voxels[static_cast<size_t>(x)][static_cast<size_t>(y)][static_cast<size_t>(z)] =
                        Voxel(type, density, isSolid);
                }
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Terrain generation completed in " << duration.count() << "ms" << std::endl;
    }

    void printStatistics() {
        std::vector<int> typeCounts(6, 0);
        int totalVoxels = 0;
        int solidVoxels = 0;

        for (int x = 0; x < config.width; x++) {
            for (int y = 0; y < config.height; y++) {
                for (int z = 0; z < config.depth; z++) {
                    const auto& voxel = voxels[static_cast<size_t>(x)][static_cast<size_t>(y)][static_cast<size_t>(z)];
                    typeCounts[voxel.type]++;
                    totalVoxels++;
                    if (voxel.isSolid) solidVoxels++;
                }
            }
        }

        std::cout << "\nAdvanced Terrain Statistics:" << std::endl;
        std::cout << "===========================" << std::endl;
        std::cout << "Dimensions: " << config.width << "x" << config.height << "x" << config.depth << std::endl;
        std::cout << "Total voxels: " << totalVoxels << std::endl;
        std::cout << "Solid voxels: " << solidVoxels << " (" << (solidVoxels * 100.0f / totalVoxels) << "%)" << std::endl;
        std::cout << "Air: " << typeCounts[0] << " voxels" << std::endl;
        std::cout << "Stone: " << typeCounts[1] << " voxels" << std::endl;
        std::cout << "Grass: " << typeCounts[2] << " voxels" << std::endl;
        std::cout << "Water: " << typeCounts[3] << " voxels" << std::endl;
        std::cout << "Mountain: " << typeCounts[4] << " voxels" << std::endl;
        std::cout << "Caves: " << (typeCounts[0] - (totalVoxels - solidVoxels)) << " voxels" << std::endl;
    }

    void exportToFile(const std::string& filename) {
        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << filename << std::endl;
            return;
        }

        // Write header
        file.write(reinterpret_cast<const char*>(&config.width), sizeof(config.width));
        file.write(reinterpret_cast<const char*>(&config.height), sizeof(config.height));
        file.write(reinterpret_cast<const char*>(&config.depth), sizeof(config.depth));

        // Write voxel data
        for (int x = 0; x < config.width; x++) {
            for (int y = 0; y < config.height; y++) {
                for (int z = 0; z < config.depth; z++) {
                    const auto& voxel = voxels[static_cast<size_t>(x)][static_cast<size_t>(y)][static_cast<size_t>(z)];
                    file.write(reinterpret_cast<const char*>(&voxel.type), sizeof(voxel.type));
                    file.write(reinterpret_cast<const char*>(&voxel.density), sizeof(voxel.density));
                    file.write(reinterpret_cast<const char*>(&voxel.isSolid), sizeof(voxel.isSolid));
                }
            }
        }

        std::cout << "Advanced terrain exported to: " << filename << std::endl;
    }
};

int main() {
    std::cout << "Advanced Terrain Generator Demo" << std::endl;
    std::cout << "===============================" << std::endl;

    // Create terrain configuration
    AdvancedTerrainGenerator::TerrainConfig config;
    config.width = 64;
    config.height = 32;
    config.depth = 64;
    config.noiseScale = 0.02f;
    config.caveThreshold = 0.3f;
    config.mountainHeight = 0.6f;
    config.waterLevel = 0.25f;
    config.seed = 42;

    // Generate terrain
    AdvancedTerrainGenerator generator(config);
    generator.generateTerrain();
    generator.printStatistics();
    generator.exportToFile("advanced_terrain.vox");

    std::cout << "\nAdvanced terrain generation complete!" << std::endl;
    std::cout << "Features:" << std::endl;
    std::cout << "- 3D voxel-based terrain" << std::endl;
    std::cout << "- Procedural cave generation" << std::endl;
    std::cout << "- Multi-layer terrain (water, grass, stone, mountains)" << std::endl;
    std::cout << "- Fractal noise for realistic height variation" << std::endl;
    std::cout << "- Distance field-based cave networks" << std::endl;

    return 0;
}

#else
#include <iostream>
int main() {
    std::cout << "Advanced terrain demo requires graphics support (ENABLE_GRAPHICS=ON)" << std::endl;
    return 1;
}
#endif
