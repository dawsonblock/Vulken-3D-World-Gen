#ifdef ENABLE_GRAPHICS
#include <iostream>
#include <vector>
#include <random>
#include <cmath>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <functional>

// Advanced 3D terrain system with complex geometry support
class Advanced3DTerrainGenerator {
public:
    struct VoxelData {
        uint8_t type;
        float density;
        bool isSolid;
        float temperature;
        float humidity;

        VoxelData() : type(0), density(0.0f), isSolid(false), temperature(20.0f), humidity(0.5f) {}
        VoxelData(uint8_t t, float d, bool solid, float temp = 20.0f, float hum = 0.5f)
            : type(t), density(d), isSolid(solid), temperature(temp), humidity(hum) {}
    };

    struct TerrainConfig {
        int width = 256;
        int height = 128;
        int depth = 256;
        float noiseScale = 0.005f;
        float caveThreshold = 0.2f;
        float archThreshold = 0.3f;
        float overhangThreshold = 0.4f;
        float erosionFactor = 0.1f;
        int seed = 12345;
    };

    struct ComplexGeometry {
        std::vector<std::tuple<int, int, int, float>> caves;
        std::vector<std::tuple<int, int, int, float>> arches;
        std::vector<std::tuple<int, int, int, float>> overhangs;
        std::vector<std::tuple<int, int, int, float>> cliffs;
    };

private:
    TerrainConfig config;
    std::vector<std::vector<std::vector<VoxelData>>> voxels;
    std::mt19937 rng;
    ComplexGeometry complexGeos;

    // Advanced noise functions
    float simplexNoise(float x, float y, float z) {
        // Simplified 3D simplex noise implementation
        float n = 0.0f;
        float frequency = 1.0f;
        float amplitude = 1.0f;

        for (int i = 0; i < 6; i++) {
            float nx = x * frequency * config.noiseScale;
            float ny = y * frequency * config.noiseScale;
            float nz = z * frequency * config.noiseScale;

            n += amplitude * std::sin(nx) * std::cos(ny) * std::sin(nz);
            frequency *= 2.0f;
            amplitude *= 0.5f;
        }

        return n;
    }

    float ridgedNoise(float x, float y, float z) {
        float n = simplexNoise(x, y, z);
        return 1.0f - std::abs(n);
    }

    float domainWarping(float x, float y, float z) {
        float warpX = simplexNoise(x + 100, y, z) * 10.0f;
        float warpY = simplexNoise(x, y + 100, z) * 10.0f;
        float warpZ = simplexNoise(x, y, z + 100) * 10.0f;

        return simplexNoise(x + warpX, y + warpY, z + warpZ);
    }

    // Complex geometry generation
    void generateCaves() {
        std::cout << "Generating cave networks..." << std::endl;

        for (int x = 0; x < config.width; x++) {
            for (int y = 0; y < config.height; y++) {
                for (int z = 0; z < config.depth; z++) {
                    float caveNoise = domainWarping(x, y, z);

                    if (caveNoise > config.caveThreshold && y < config.height * 0.7f) {
                        // Create cave network
                        float caveSize = (caveNoise - config.caveThreshold) * 5.0f;
                        complexGeos.caves.emplace_back(x, y, z, caveSize);

                        // Mark voxels as air in cave areas
                        for (int dx = -2; dx <= 2; dx++) {
                            for (int dy = -2; dy <= 2; dy++) {
                                for (int dz = -2; dz <= 2; dz++) {
                                    int nx = x + dx;
                                    int ny = y + dy;
                                    int nz = z + dz;

                                    if (nx >= 0 && nx < config.width &&
                                        ny >= 0 && ny < config.height &&
                                        nz >= 0 && nz < config.depth) {

                                        float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
                                        if (dist <= caveSize) {
                                            voxels[static_cast<size_t>(nx)][static_cast<size_t>(ny)][static_cast<size_t>(nz)].type = 0; // Air
                                            voxels[static_cast<size_t>(nx)][static_cast<size_t>(ny)][static_cast<size_t>(nz)].isSolid = false;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    void generateArches() {
        std::cout << "Generating natural arches..." << std::endl;

        for (int x = 0; x < config.width; x++) {
            for (int z = 0; z < config.depth; z++) {
                float archNoise = ridgedNoise(x, 0, z);

                if (archNoise > config.archThreshold) {
                    // Find ground level
                    int groundY = 0;
                    for (int y = config.height - 1; y >= 0; y--) {
                        if (voxels[static_cast<size_t>(x)][static_cast<size_t>(y)][static_cast<size_t>(z)].isSolid) {
                            groundY = y;
                            break;
                        }
                    }

                    if (groundY > 10) {
                        float archHeight = (archNoise - config.archThreshold) * 20.0f;
                        complexGeos.arches.emplace_back(x, groundY, z, archHeight);

                        // Create arch opening
                        for (int y = groundY; y < groundY + static_cast<int>(archHeight); y++) {
                            if (y < config.height) {
                                voxels[static_cast<size_t>(x)][static_cast<size_t>(y)][static_cast<size_t>(z)].type = 0; // Air
                                voxels[static_cast<size_t>(x)][static_cast<size_t>(y)][static_cast<size_t>(z)].isSolid = false;
                            }
                        }
                    }
                }
            }
        }
    }

    void generateOverhangs() {
        std::cout << "Generating overhangs and cliffs..." << std::endl;

        for (int x = 0; x < config.width; x++) {
            for (int z = 0; z < config.depth; z++) {
                float overhangNoise = simplexNoise(x, 0, z);

                if (overhangNoise > config.overhangThreshold) {
                    // Find terrain height
                    int terrainHeight = 0;
                    for (int y = config.height - 1; y >= 0; y--) {
                        if (voxels[static_cast<size_t>(x)][static_cast<size_t>(y)][static_cast<size_t>(z)].isSolid) {
                            terrainHeight = y;
                            break;
                        }
                    }

                    if (terrainHeight > 5) {
                        float overhangSize = (overhangNoise - config.overhangThreshold) * 8.0f;
                        complexGeos.overhangs.emplace_back(x, terrainHeight, z, overhangSize);

                        // Create overhang
                        for (int dx = 1; dx <= static_cast<int>(overhangSize); dx++) {
                            int nx = x + dx;
                            if (nx < config.width) {
                                for (int dy = 0; dy < 3; dy++) {
                                    int ny = terrainHeight - dy;
                                    if (ny >= 0) {
                                        voxels[static_cast<size_t>(nx)][static_cast<size_t>(ny)][static_cast<size_t>(z)].type = 4; // Mountain
                                        voxels[static_cast<size_t>(nx)][static_cast<size_t>(ny)][static_cast<size_t>(z)].isSolid = true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    void applyErosion() {
        std::cout << "Applying erosion effects..." << std::endl;

        // Simple erosion simulation
        for (int iteration = 0; iteration < 3; iteration++) {
            for (int x = 1; x < config.width - 1; x++) {
                for (int y = 1; y < config.height - 1; y++) {
                    for (int z = 1; z < config.depth - 1; z++) {
                        auto& voxel = voxels[static_cast<size_t>(x)][static_cast<size_t>(y)][static_cast<size_t>(z)];

                        if (voxel.isSolid && voxel.type == 1) { // Stone
                            // Check if voxel is exposed to air
                            bool exposed = false;
                            for (int dx = -1; dx <= 1; dx++) {
                                for (int dy = -1; dy <= 1; dy++) {
                                    for (int dz = -1; dz <= 1; dz++) {
                                        int nx = x + dx;
                                        int ny = y + dy;
                                        int nz = z + dz;

                                        if (nx >= 0 && nx < config.width &&
                                            ny >= 0 && ny < config.height &&
                                            nz >= 0 && nz < config.depth) {

                                            if (!voxels[static_cast<size_t>(nx)][static_cast<size_t>(ny)][static_cast<size_t>(nz)].isSolid) {
                                                exposed = true;
                                                break;
                                            }
                                        }
                                    }
                                    if (exposed) break;
                                }
                                if (exposed) break;
                            }

                            if (exposed && static_cast<float>(rng()) / rng.max() < config.erosionFactor) {
                                voxel.type = 0; // Air
                                voxel.isSolid = false;
                            }
                        }
                    }
                }
            }
        }
    }

public:
    Advanced3DTerrainGenerator(const TerrainConfig& cfg) : config(cfg), rng(static_cast<unsigned int>(config.seed)) {
        voxels.resize(static_cast<size_t>(config.width),
                     std::vector<std::vector<VoxelData>>(static_cast<size_t>(config.height),
                     std::vector<VoxelData>(static_cast<size_t>(config.depth))));
    }

    void generateTerrain() {
        std::cout << "Generating advanced 3D terrain with complex geometries..." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();

        // Generate base terrain
        for (int x = 0; x < config.width; x++) {
            for (int z = 0; z < config.depth; z++) {
                float height = simplexNoise(x, 0, z) * 30.0f + config.height * 0.3f;
                height = std::max(1.0f, std::min(height, static_cast<float>(config.height - 1)));

                for (int y = 0; y < config.height; y++) {
                    float normalizedY = static_cast<float>(y) / config.height;
                    float density = simplexNoise(x, y, z);

                    uint8_t type = 0; // Air
                    bool isSolid = false;
                    float temperature = 20.0f - (normalizedY * 20.0f); // Temperature decreases with height
                    float humidity = 0.5f + simplexNoise(x, y, z) * 0.3f;

                    if (y < height) {
                        if (normalizedY < 0.1f) {
                            type = 3; // Water
                            isSolid = true;
                        } else if (y == static_cast<int>(height) - 1) {
                            type = 2; // Grass
                            isSolid = true;
                        } else if (normalizedY > 0.7f) {
                            type = 4; // Mountain
                            isSolid = true;
                        } else {
                            type = 1; // Stone
                            isSolid = true;
                        }
                    }

                    voxels[static_cast<size_t>(x)][static_cast<size_t>(y)][static_cast<size_t>(z)] =
                        VoxelData(type, density, isSolid, temperature, humidity);
                }
            }
        }

        // Generate complex geometries
        generateCaves();
        generateArches();
        generateOverhangs();
        applyErosion();

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Advanced 3D terrain generation completed in " << duration.count() << "ms" << std::endl;
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

        std::cout << "\nAdvanced 3D Terrain Statistics:" << std::endl;
        std::cout << "===============================" << std::endl;
        std::cout << "Dimensions: " << config.width << "x" << config.height << "x" << config.depth << std::endl;
        std::cout << "Total voxels: " << totalVoxels << std::endl;
        std::cout << "Solid voxels: " << solidVoxels << " (" << (solidVoxels * 100.0f / totalVoxels) << "%)" << std::endl;
        std::cout << "Air: " << typeCounts[0] << " voxels" << std::endl;
        std::cout << "Stone: " << typeCounts[1] << " voxels" << std::endl;
        std::cout << "Grass: " << typeCounts[2] << " voxels" << std::endl;
        std::cout << "Water: " << typeCounts[3] << " voxels" << std::endl;
        std::cout << "Mountain: " << typeCounts[4] << " voxels" << std::endl;

        std::cout << "\nComplex Geometries:" << std::endl;
        std::cout << "Caves: " << complexGeos.caves.size() << " cave networks" << std::endl;
        std::cout << "Arches: " << complexGeos.arches.size() << " natural arches" << std::endl;
        std::cout << "Overhangs: " << complexGeos.overhangs.size() << " overhangs and cliffs" << std::endl;
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
                    file.write(reinterpret_cast<const char*>(&voxel.temperature), sizeof(voxel.temperature));
                    file.write(reinterpret_cast<const char*>(&voxel.humidity), sizeof(voxel.humidity));
                }
            }
        }

        std::cout << "Advanced 3D terrain exported to: " << filename << std::endl;
    }
};

int main() {
    std::cout << "Advanced 3D Terrain Generator Demo" << std::endl;
    std::cout << "===================================" << std::endl;

    // Create terrain configuration
    Advanced3DTerrainGenerator::TerrainConfig config;
    config.width = 128;
    config.height = 64;
    config.depth = 128;
    config.noiseScale = 0.01f;
    config.caveThreshold = 0.2f;
    config.archThreshold = 0.3f;
    config.overhangThreshold = 0.4f;
    config.erosionFactor = 0.1f;
    config.seed = 42;

    // Generate terrain
    Advanced3DTerrainGenerator generator(config);
    generator.generateTerrain();
    generator.printStatistics();
    generator.exportToFile("advanced_3d_terrain.vox");

    std::cout << "\nAdvanced 3D terrain generation complete!" << std::endl;
    std::cout << "Features:" << std::endl;
    std::cout << "- Complex 3D geometries (caves, arches, overhangs)" << std::endl;
    std::cout << "- Advanced noise functions (simplex, ridged, domain warping)" << std::endl;
    std::cout << "- Erosion simulation for realistic terrain" << std::endl;
    std::cout << "- Multi-layer terrain with temperature and humidity" << std::endl;
    std::cout << "- Support for caves, arches, and overhangs" << std::endl;

    return 0;
}

#else
#include <iostream>
int main() {
    std::cout << "Advanced 3D terrain demo requires graphics support (ENABLE_GRAPHICS=ON)" << std::endl;
    return 1;
}
#endif
