#include <iostream>
#include <vector>
#include <random>
#include <cmath>

struct VoxelWorld {
    int width, height, depth;
    std::vector<uint8_t> voxels;
    
    VoxelWorld(int w, int h, int d) : width(w), height(h), depth(d) {
        voxels.resize(w * h * d, 0);
    }
    
    void setVoxel(int x, int y, int z, uint8_t type) {
        if (x >= 0 && x < width && y >= 0 && y < height && z >= 0 && z < depth) {
            voxels[x + y * width + z * width * height] = type;
        }
    }
    
    void generateTerrain() {
        std::mt19937 rng(12345);
        std::uniform_real_distribution<float> noise(-1.0f, 1.0f);
        
        // Generate heightmap
        for (int x = 0; x < width; x++) {
            for (int z = 0; z < depth; z++) {
                float height = 0.0f;
                float amplitude = 16.0f;
                float frequency = 0.01f;
                
                // Simple noise-based height generation
                for (int octave = 0; octave < 4; octave++) {
                    height += amplitude * std::sin(frequency * x) * std::cos(frequency * z);
                    amplitude *= 0.5f;
                    frequency *= 2.0f;
                }
                
                int terrainHeight = static_cast<int>(height + 32);
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
                
                // Add some trees randomly
                if (terrainHeight > 5 && terrainHeight < 40 && rng() % 100 < 5) {
                    int treeHeight = 3 + rng() % 5;
                    for (int ty = 1; ty <= treeHeight; ty++) {
                        setVoxel(x, terrainHeight + ty, z, 5); // Tree
                    }
                }
            }
        }
    }
    
    void saveToFile(const std::string& filename) {
        std::cout << "💾 Generating demo world: " << width << "x" << height << "x" << depth << std::endl;
        std::cout << "🌍 World file would be saved to: " << filename << std::endl;
        std::cout << "🎮 Total voxels generated: " << voxels.size() << std::endl;
        
        // Count different voxel types
        std::vector<int> typeCounts(6, 0);
        for (uint8_t voxel : voxels) {
            if (voxel < 6) typeCounts[voxel]++;
        }
        
        std::cout << "📊 Voxel distribution:" << std::endl;
        std::cout << "  Air: " << typeCounts[0] << std::endl;
        std::cout << "  Stone: " << typeCounts[1] << std::endl;
        std::cout << "  Grass: " << typeCounts[2] << std::endl;
        std::cout << "  Water: " << typeCounts[3] << std::endl;
        std::cout << "  Mountain: " << typeCounts[4] << std::endl;
        std::cout << "  Trees: " << typeCounts[5] << std::endl;
    }
};

int main() {
    std::cout << "🌟 VoxelVK Demo World Generator" << std::endl;
    std::cout << "================================" << std::endl;
    
    // Create a demo world
    VoxelWorld world(128, 64, 128);
    
    std::cout << "🏗️  Generating terrain..." << std::endl;
    world.generateTerrain();
    
    std::cout << "💾 Saving world data..." << std::endl;
    world.saveToFile("demo_world.vox");
    
    std::cout << "✅ Demo world generation complete!" << std::endl;
    std::cout << "🎮 Ready for visualization in VoxelVK engine!" << std::endl;
    
    return 0;
}
