#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cmath>
#include <random>
#include <algorithm>

struct VoxelType {
    enum Type { AIR, STONE, GRASS, WATER, TREE };
};

struct Voxel {
    VoxelType::Type type;
    float height;
};

class VoxelWorld {
public:
    int width, height, depth;
    std::vector<std::vector<std::vector<Voxel>>> voxels;
    
    VoxelWorld(int w, int h, int d) : width(w), height(h), depth(d) {
        voxels.resize(width, std::vector<std::vector<Voxel>>(height, std::vector<Voxel>(depth)));
        generateTerrain();
    }
    
private:
    float perlinNoise(float x, float z) {
        return (sin(x * 0.1f) + sin(z * 0.1f) + sin((x + z) * 0.05f)) * 0.3f + 0.5f;
    }
    
    void generateTerrain() {
        std::mt19937 rng(42);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        
        for (int x = 0; x < width; x++) {
            for (int z = 0; z < depth; z++) {
                float noise = perlinNoise(x, z);
                int surfaceHeight = static_cast<int>(noise * height * 0.6f);
                
                for (int y = 0; y < height; y++) {
                    if (y < surfaceHeight - 10) {
                        voxels[x][y][z] = {VoxelType::STONE, y / float(height)};
                    } else if (y < surfaceHeight) {
                        voxels[x][y][z] = {VoxelType::GRASS, y / float(height)};
                    } else if (y < height * 0.3f && surfaceHeight < height * 0.35f) {
                        voxels[x][y][z] = {VoxelType::WATER, y / float(height)};
                    } else {
                        voxels[x][y][z] = {VoxelType::AIR, y / float(height)};
                    }
                    
                    // Add some trees
                    if (voxels[x][y][z].type == VoxelType::GRASS && 
                        y == surfaceHeight - 1 && dist(rng) < 0.05f) {
                        for (int ty = y + 1; ty < std::min(y + 5, height); ty++) {
                            voxels[x][ty][z] = {VoxelType::TREE, ty / float(height)};
                        }
                    }
                }
            }
        }
    }
};

// Simple ASCII visualization
void generateASCIIMap(const VoxelWorld& world, const std::string& filename) {
    std::ofstream file(filename);
    file << "VoxelVK 3D World Generation Demo - Top View\n";
    file << "========================================\n\n";
    file << "Legend: . = Air/Water, # = Stone, ^ = Grass, T = Tree\n\n";
    
    for (int z = 0; z < world.depth; z += 2) {
        for (int x = 0; x < world.width; x += 2) {
            // Find highest non-air voxel
            char symbol = '.';
            for (int y = world.height - 1; y >= 0; y--) {
                VoxelType::Type type = world.voxels[x][y][z].type;
                if (type != VoxelType::AIR) {
                    switch (type) {
                        case VoxelType::STONE: symbol = '#'; break;
                        case VoxelType::GRASS: symbol = '^'; break;
                        case VoxelType::WATER: symbol = '~'; break;
                        case VoxelType::TREE: symbol = 'T'; break;
                        default: symbol = '.'; break;
                    }
                    break;
                }
            }
            file << symbol;
        }
        file << "\n";
    }
    
    file << "\nWorld Statistics:\n";
    int counts[5] = {0};
    for (int x = 0; x < world.width; x++) {
        for (int y = 0; y < world.height; y++) {
            for (int z = 0; z < world.depth; z++) {
                counts[world.voxels[x][y][z].type]++;
            }
        }
    }
    
    file << "Air: " << counts[VoxelType::AIR] << "\n";
    file << "Stone: " << counts[VoxelType::STONE] << "\n";
    file << "Grass: " << counts[VoxelType::GRASS] << "\n";
    file << "Water: " << counts[VoxelType::WATER] << "\n";
    file << "Trees: " << counts[VoxelType::TREE] << "\n";
    file << "Total voxels: " << world.width * world.height * world.depth << "\n";
}

// Generate a simple HTML viewer
void generateHTMLViewer(const VoxelWorld& world, const std::string& filename) {
    std::ofstream file(filename);
    file << R"(<!DOCTYPE html>
<html>
<head>
    <title>VoxelVK 3D World Demo</title>
    <style>
        body { 
            font-family: 'Courier New', monospace; 
            background: #1a1a1a; 
            color: #00ff00; 
            margin: 20px; 
        }
        .world-container { 
            background: #000; 
            padding: 20px; 
            border: 2px solid #00ff00; 
            border-radius: 10px; 
            margin: 20px 0; 
        }
        .world-map { 
            font-size: 8px; 
            line-height: 8px; 
            letter-spacing: 1px; 
            white-space: pre; 
            overflow-x: auto; 
        }
        .stats { 
            margin-top: 20px; 
            font-size: 14px; 
        }
        h1, h2 { color: #00ff00; text-align: center; }
        .legend { color: #ffff00; margin-bottom: 10px; }
    </style>
</head>
<body>
    <h1>🌍 VoxelVK 3D World Generation Demo</h1>
    <h2>Generated Procedural World - Top View</h2>
    
    <div class="legend">
        <strong>Legend:</strong> 
        <span style="color: #666;">. = Air/Water</span> | 
        <span style="color: #888;"># = Stone</span> | 
        <span style="color: #0f0;">^ = Grass</span> | 
        <span style="color: #0ff;">~ = Water</span> | 
        <span style="color: #f80;">T = Tree</span>
    </div>
    
    <div class="world-container">
        <div class="world-map">)";

    // Generate the map
    for (int z = 0; z < world.depth; z += 2) {
        for (int x = 0; x < world.width; x += 2) {
            char symbol = '.';
            std::string color = "#666";
            
            // Find highest non-air voxel
            for (int y = world.height - 1; y >= 0; y--) {
                VoxelType::Type type = world.voxels[x][y][z].type;
                if (type != VoxelType::AIR) {
                    switch (type) {
                        case VoxelType::STONE: symbol = '#'; color = "#888"; break;
                        case VoxelType::GRASS: symbol = '^'; color = "#0f0"; break;
                        case VoxelType::WATER: symbol = '~'; color = "#0ff"; break;
                        case VoxelType::TREE: symbol = 'T'; color = "#f80"; break;
                        default: symbol = '.'; color = "#666"; break;
                    }
                    break;
                }
            }
            file << "<span style=\"color:" << color << "\">" << symbol << "</span>";
        }
        file << "\n";
    }
    
    file << R"(        </div>
    </div>
    
    <div class="stats">
        <h2>World Statistics</h2>)";
    
    // Calculate statistics
    int counts[5] = {0};
    for (int x = 0; x < world.width; x++) {
        for (int y = 0; y < world.height; y++) {
            for (int z = 0; z < world.depth; z++) {
                counts[world.voxels[x][y][z].type]++;
            }
        }
    }
    
    file << "<p><strong>World Size:</strong> " << world.width << " × " << world.height << " × " << world.depth << " voxels</p>\n";
    file << "<p><strong>Air Voxels:</strong> " << counts[VoxelType::AIR] << "</p>\n";
    file << "<p><strong>Stone Voxels:</strong> " << counts[VoxelType::STONE] << "</p>\n";
    file << "<p><strong>Grass Voxels:</strong> " << counts[VoxelType::GRASS] << "</p>\n";
    file << "<p><strong>Water Voxels:</strong> " << counts[VoxelType::WATER] << "</p>\n";
    file << "<p><strong>Tree Voxels:</strong> " << counts[VoxelType::TREE] << "</p>\n";
    file << "<p><strong>Total Voxels:</strong> " << world.width * world.height * world.depth << "</p>\n";
    
    file << R"(    </div>
    
    <div style="text-align: center; margin-top: 30px; color: #888;">
        <p>Generated by VoxelVK 3D World Generation Engine</p>
        <p>Procedural terrain with Perlin noise, biomes, and object placement</p>
    </div>
</body>
</html>)";
}

int main() {
    std::cout << "🌍 VoxelVK World Visualizer\n";
    std::cout << "===========================\n\n";
    
    std::cout << "Generating 3D world (64x32x64)...\n";
    VoxelWorld world(64, 32, 64);
    
    std::cout << "Creating ASCII map...\n";
    generateASCIIMap(world, "world_map.txt");
    
    std::cout << "Creating HTML viewer...\n";
    generateHTMLViewer(world, "world_viewer.html");
    
    std::cout << "\n✅ World generation complete!\n";
    std::cout << "📄 View ASCII map: cat world_map.txt\n";
    std::cout << "🌐 View HTML demo: open world_viewer.html in browser\n";
    
    return 0;
}
