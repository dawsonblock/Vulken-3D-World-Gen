#ifdef ENABLE_GRAPHICS
#include <iostream>
#include <vector>
#include <random>
#include <cmath>
#include <fstream>
#include <chrono>

// Simplified marching cubes implementation for terrain generation
class SimpleMarchingCubesGenerator {
public:
    struct Vertex {
        float x, y, z;
        float nx, ny, nz;

        Vertex(float px = 0, float py = 0, float pz = 0,
               float npx = 0, float npy = 0, float npz = 0)
            : x(px), y(py), z(pz), nx(npx), ny(npy), nz(npz) {}
    };

    struct Triangle {
        Vertex v0, v1, v2;

        Triangle(const Vertex& p0, const Vertex& p1, const Vertex& p2)
            : v0(p0), v1(p1), v2(p2) {}
    };

    struct TerrainConfig {
        int width = 64;
        int height = 32;
        int depth = 64;
        float voxelSize = 1.0f;
        float isoLevel = 0.5f;
        float noiseScale = 0.02f;
        int seed = 12345;
    };

private:
    TerrainConfig config;
    std::vector<std::vector<std::vector<float>>> densityField;
    std::vector<Triangle> triangles;
    std::mt19937 rng;

    // Simple noise function
    float simpleNoise(float x, float y, float z) {
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

    // Generate density field
    void generateDensityField() {
        std::cout << "Generating density field..." << std::endl;

        densityField.resize(static_cast<size_t>(config.width + 1),
                           std::vector<std::vector<float>>(static_cast<size_t>(config.height + 1),
                           std::vector<float>(static_cast<size_t>(config.depth + 1))));

        for (int x = 0; x <= config.width; x++) {
            for (int y = 0; y <= config.height; y++) {
                for (int z = 0; z <= config.depth; z++) {
                    float density = simpleNoise(x, y, z);

                    // Add height-based terrain
                    float heightFactor = static_cast<float>(y) / config.height;
                    density += (1.0f - heightFactor) * 0.3f;

                    densityField[static_cast<size_t>(x)][static_cast<size_t>(y)][static_cast<size_t>(z)] = density;
                }
            }
        }
    }

    // Simple mesh generation (simplified marching cubes)
    void generateSimpleMesh() {
        std::cout << "Generating simplified mesh..." << std::endl;

        triangles.clear();

        for (int x = 0; x < config.width; x++) {
            for (int y = 0; y < config.height; y++) {
                for (int z = 0; z < config.depth; z++) {
                    // Get density values for cube corners
                    float d000 = densityField[static_cast<size_t>(x)][static_cast<size_t>(y)][static_cast<size_t>(z)];
                    float d001 = densityField[static_cast<size_t>(x)][static_cast<size_t>(y)][static_cast<size_t>(z + 1)];
                    float d010 = densityField[static_cast<size_t>(x)][static_cast<size_t>(y + 1)][static_cast<size_t>(z)];
                    float d011 = densityField[static_cast<size_t>(x)][static_cast<size_t>(y + 1)][static_cast<size_t>(z + 1)];
                    float d100 = densityField[static_cast<size_t>(x + 1)][static_cast<size_t>(y)][static_cast<size_t>(z)];
                    float d101 = densityField[static_cast<size_t>(x + 1)][static_cast<size_t>(y)][static_cast<size_t>(z + 1)];
                    float d110 = densityField[static_cast<size_t>(x + 1)][static_cast<size_t>(y + 1)][static_cast<size_t>(z)];
                    float d111 = densityField[static_cast<size_t>(x + 1)][static_cast<size_t>(y + 1)][static_cast<size_t>(z + 1)];

                    // Simple case: if any corner is above iso level, create a quad
                    if (d000 >= config.isoLevel || d001 >= config.isoLevel ||
                        d010 >= config.isoLevel || d011 >= config.isoLevel ||
                        d100 >= config.isoLevel || d101 >= config.isoLevel ||
                        d110 >= config.isoLevel || d111 >= config.isoLevel) {

                        // Create a simple quad for this voxel
                        Vertex v0(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), 0, 1, 0);
                        Vertex v1(static_cast<float>(x + 1), static_cast<float>(y), static_cast<float>(z), 0, 1, 0);
                        Vertex v2(static_cast<float>(x + 1), static_cast<float>(y), static_cast<float>(z + 1), 0, 1, 0);
                        Vertex v3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z + 1), 0, 1, 0);

                        // Create two triangles
                        triangles.emplace_back(v0, v1, v2);
                        triangles.emplace_back(v0, v2, v3);
                    }
                }
            }
        }

        std::cout << "Generated " << triangles.size() << " triangles" << std::endl;
    }

public:
    SimpleMarchingCubesGenerator(const TerrainConfig& cfg) : config(cfg), rng(static_cast<unsigned int>(config.seed)) {}

    void generateTerrain() {
        std::cout << "Generating simplified marching cubes terrain..." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();

        generateDensityField();
        generateSimpleMesh();

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Simplified marching cubes terrain generation completed in " << duration.count() << "ms" << std::endl;
    }

    void printStatistics() {
        std::cout << "\nSimplified Marching Cubes Terrain Statistics:" << std::endl;
        std::cout << "==============================================" << std::endl;
        std::cout << "Dimensions: " << config.width << "x" << config.height << "x" << config.depth << std::endl;
        std::cout << "Voxel size: " << config.voxelSize << std::endl;
        std::cout << "Iso level: " << config.isoLevel << std::endl;
        std::cout << "Triangles generated: " << triangles.size() << std::endl;
        std::cout << "Vertices: " << (triangles.size() * 3) << std::endl;

        // Calculate mesh statistics
        int solidVoxels = 0;
        float totalVolume = 0.0f;

        for (int x = 0; x < config.width; x++) {
            for (int y = 0; y < config.height; y++) {
                for (int z = 0; z < config.depth; z++) {
                    if (densityField[static_cast<size_t>(x)][static_cast<size_t>(y)][static_cast<size_t>(z)] >= config.isoLevel) {
                        solidVoxels++;
                        totalVolume += config.voxelSize * config.voxelSize * config.voxelSize;
                    }
                }
            }
        }

        std::cout << "Solid voxels: " << solidVoxels << std::endl;
        std::cout << "Total volume: " << totalVolume << " cubic units" << std::endl;
    }

    void exportToOBJ(const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << filename << std::endl;
            return;
        }

        file << "# Simplified Marching Cubes Terrain Mesh\n";
        file << "# Generated by Vulken-3D-World-Gen\n\n";

        // Write vertices
        for (const auto& triangle : triangles) {
            file << "v " << triangle.v0.x << " " << triangle.v0.y << " " << triangle.v0.z << "\n";
            file << "v " << triangle.v1.x << " " << triangle.v1.y << " " << triangle.v1.z << "\n";
            file << "v " << triangle.v2.x << " " << triangle.v2.y << " " << triangle.v2.z << "\n";
        }

        // Write normals
        for (const auto& triangle : triangles) {
            file << "vn " << triangle.v0.nx << " " << triangle.v0.ny << " " << triangle.v0.nz << "\n";
            file << "vn " << triangle.v1.nx << " " << triangle.v1.ny << " " << triangle.v1.nz << "\n";
            file << "vn " << triangle.v2.nx << " " << triangle.v2.ny << " " << triangle.v2.nz << "\n";
        }

        // Write faces
        int vertexIndex = 1;
        for (size_t i = 0; i < triangles.size(); i++) {
            file << "f " << vertexIndex << "//" << vertexIndex
                 << " " << (vertexIndex + 1) << "//" << (vertexIndex + 1)
                 << " " << (vertexIndex + 2) << "//" << (vertexIndex + 2) << "\n";
            vertexIndex += 3;
        }

        std::cout << "Simplified marching cubes mesh exported to: " << filename << std::endl;
    }
};

int main() {
    std::cout << "Simplified Marching Cubes Terrain Generator Demo" << std::endl;
    std::cout << "================================================" << std::endl;

    // Create terrain configuration
    SimpleMarchingCubesGenerator::TerrainConfig config;
    config.width = 32;
    config.height = 16;
    config.depth = 32;
    config.voxelSize = 1.0f;
    config.isoLevel = 0.5f;
    config.noiseScale = 0.05f;
    config.seed = 42;

    // Generate terrain
    SimpleMarchingCubesGenerator generator(config);
    generator.generateTerrain();
    generator.printStatistics();
    generator.exportToOBJ("simple_marching_cubes_terrain.obj");

    std::cout << "\nSimplified marching cubes terrain generation complete!" << std::endl;
    std::cout << "Features:" << std::endl;
    std::cout << "- Simplified marching cubes algorithm for terrain generation" << std::endl;
    std::cout << "- Multi-octave noise for realistic terrain features" << std::endl;
    std::cout << "- Height-based terrain generation" << std::endl;
    std::cout << "- Export to OBJ format" << std::endl;
    std::cout << "- Stable implementation without complex lookup tables" << std::endl;

    return 0;
}

#else
#include <iostream>
int main() {
    std::cout << "Simplified marching cubes terrain demo requires graphics support (ENABLE_GRAPHICS=ON)" << std::endl;
    return 1;
}
#endif
