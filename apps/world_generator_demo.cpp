#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <cmath>
#include <chrono>

struct VoxelWorld {
    int width, height, depth;
    std::vector<uint8_t> voxels;

    VoxelWorld(int w, int h, int d) : width(w), height(h), depth(d) {
        voxels.resize(static_cast<size_t>(w * h * d), 0);
    }

    void setVoxel(int x, int y, int z, uint8_t type) {
        if (x >= 0 && x < width && y >= 0 && y < height && z >= 0 && z < depth) {
            voxels[static_cast<size_t>(x + y * width + z * width * height)] = type;
        }
    }

    void generateTerrain() {
        std::mt19937 rng(12345);
        std::uniform_real_distribution<float> noise(-1.0f, 1.0f);

        // Generate heightmap
        for (int x = 0; x < width; x++) {
            for (int z = 0; z < depth; z++) {
                float terrain_height = 0.0f;
                float amplitude = 16.0f;
                float frequency = 0.01f;

                // Simple noise-based height generation
                for (int octave = 0; octave < 4; octave++) {
                    terrain_height += amplitude * std::sin(frequency * x) * std::cos(frequency * z);
                    amplitude *= 0.5f;
                    frequency *= 2.0f;
                }

                int terrainHeight = static_cast<int>(terrain_height + 32);
                terrainHeight = std::max(0, std::min(terrainHeight, static_cast<int>(height) - 1));

                // Fill voxels
                for (int y = 0; y <= terrainHeight; y++) {
                    uint8_t voxelType = 1; // Stone
                    if (y == terrainHeight) {
                        if (terrainHeight < 5) voxelType = 3; // Water
                        else if (terrainHeight < 30) voxelType = 2; // Grass
                        else voxelType = 4; // Mountain
                    }
                    setVoxel(x, y, z, voxelType);
                }
            }
        }
    }

    void printStats() {
        int airCount = 0, stoneCount = 0, grassCount = 0, waterCount = 0, mountainCount = 0;

        for (uint8_t voxel : voxels) {
            switch (voxel) {
                case 0: airCount++; break;
                case 1: stoneCount++; break;
                case 2: grassCount++; break;
                case 3: waterCount++; break;
                case 4: mountainCount++; break;
            }
        }

        std::cout << "World Statistics:\n";
        std::cout << "  Air: " << airCount << " voxels\n";
        std::cout << "  Stone: " << stoneCount << " voxels\n";
        std::cout << "  Grass: " << grassCount << " voxels\n";
        std::cout << "  Water: " << waterCount << " voxels\n";
        std::cout << "  Mountain: " << mountainCount << " voxels\n";
        std::cout << "  Total: " << voxels.size() << " voxels\n";
    }

    void exportToFile(const std::string& filename) {
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            std::cerr << "Failed to open file: " << filename << std::endl;
            return;
        }

        // Write header
        file.write(reinterpret_cast<const char*>(&width), sizeof(width));
        file.write(reinterpret_cast<const char*>(&height), sizeof(height));
        file.write(reinterpret_cast<const char*>(&depth), sizeof(depth));

        // Write voxel data
        file.write(reinterpret_cast<const char*>(voxels.data()), static_cast<std::streamsize>(voxels.size()));

        std::cout << "World exported to: " << filename << std::endl;
    }
};

int main(int argc, char* argv[]) {
    std::cout << "Voxel World Generator Demo\n";
    std::cout << "========================\n\n";

    // Parse command line arguments
    int width = 64, height = 64, depth = 64;
    std::string outputFile = "world.vox";

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --width W     Set world width (default: 64)\n";
            std::cout << "  --height H    Set world height (default: 64)\n";
            std::cout << "  --depth D     Set world depth (default: 64)\n";
            std::cout << "  --output F    Set output file (default: world.vox)\n";
            std::cout << "  --help, -h    Show this help message\n";
            return 0;
        } else if (arg == "--width" && i + 1 < argc) {
            width = std::stoi(argv[++i]);
        } else if (arg == "--height" && i + 1 < argc) {
            height = std::stoi(argv[++i]);
        } else if (arg == "--depth" && i + 1 < argc) {
            depth = std::stoi(argv[++i]);
        } else if (arg == "--output" && i + 1 < argc) {
            outputFile = argv[++i];
        }
    }

    std::cout << "Generating world: " << width << "x" << height << "x" << depth << "\n";

    auto start = std::chrono::high_resolution_clock::now();

    VoxelWorld world(width, height, depth);
    world.generateTerrain();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Generation completed in " << duration.count() << "ms\n\n";

    world.printStats();
    world.exportToFile(outputFile);

    return 0;
}
