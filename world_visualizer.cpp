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

// Export surface (top voxel) heights and types as JSON for WebGL viewer
void generateSurfaceJSON(const VoxelWorld& world, const std::string& filename) {
    std::ofstream out(filename);
    if (!out.is_open()) {
        std::cerr << "Failed to open " << filename << " for writing" << std::endl;
        return;
    }

    out << "{\n";
    out << "  \"width\": " << world.width << ",\n";
    out << "  \"depth\": " << world.depth << ",\n";
    out << "  \"maxHeight\": " << (world.height - 1) << ",\n";

    // Heights
    out << "  \"heights\": [\n";
    for (int z = 0; z < world.depth; ++z) {
        out << "    ";
        for (int x = 0; x < world.width; ++x) {
            int topY = 0;
            for (int y = world.height - 1; y >= 0; --y) {
                if (world.voxels[x][y][z].type != VoxelType::AIR) { topY = y; break; }
            }
            out << topY;
            if (!(z == world.depth - 1 && x == world.width - 1)) out << ",";
        }
        out << "\n";
    }
    out << "  ],\n";

    // Types
    out << "  \"types\": [\n";
    for (int z = 0; z < world.depth; ++z) {
        out << "    ";
        for (int x = 0; x < world.width; ++x) {
            int code = 0; // Air
            for (int y = world.height - 1; y >= 0; --y) {
                auto t = world.voxels[x][y][z].type;
                if (t != VoxelType::AIR) {
                    switch (t) {
                        case VoxelType::STONE: code = 1; break;
                        case VoxelType::GRASS: code = 2; break;
                        case VoxelType::WATER: code = 3; break;
                        case VoxelType::TREE:  code = 4; break;
                        default: code = 0; break;
                    }
                    break;
                }
            }
            out << code;
            if (!(z == world.depth - 1 && x == world.width - 1)) out << ",";
        }
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
}

// Generate a modern canvas-based HTML viewer with zoom
void generateHTMLViewer(const VoxelWorld& world, const std::string& filename) {
    std::ofstream file(filename);
    file << R"(<!DOCTYPE html>
<html>
<head>
  <meta charset=\"utf-8\" />
  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />
  <title>VoxelVK 3D World - Canvas Viewer</title>
  <style>
    :root { --accent: #00ff88; }
    body {
      font-family: system-ui, -apple-system, Segoe UI, Roboto, Arial, sans-serif;
      background: #0f1115; color: #dfe5ee; margin: 0; padding: 24px;
    }
    h1 { color: var(--accent); text-align: center; margin: 0 0 8px 0; }
    .subtitle { text-align: center; color: #94a3b8; margin-bottom: 16px; }
    .panel {
      background: #0b0d12; border: 1px solid #1f2937; border-radius: 12px;
      padding: 16px; box-shadow: 0 8px 24px rgba(0,0,0,0.3); max-width: 1000px; margin: 16px auto;
    }
    .controls { display:flex; gap:16px; align-items:center; justify-content:center; flex-wrap:wrap; margin-bottom:12px; }
    label { color:#a3b3c3; font-size:14px; }
    input[type=range] { width: 240px; }
    .canvas-wrap { display:flex; justify-content:center; overflow:auto; border: 1px solid #1f2937; border-radius: 10px; background:#0a0c10; padding: 12px;}
    canvas { image-rendering: pixelated; image-rendering: crisp-edges; cursor: grab; }
    .legend { text-align:center; margin-top: 12px; color:#a3b3c3; font-size:14px; }
    .legend .sw { display:inline-block; width:12px; height:12px; border-radius:2px; margin:0 6px -1px 12px; }
  </style>
</head>
<body>
  <h1>🌍 VoxelVK World Viewer</h1>
  <div class=\"subtitle\">Interactive top-down map with zoom</div>
  <div class=\"panel\">
    <div class=\"controls\">
      <label>Zoom
        <input id=\"zoom\" type=\"range\" min=\"4\" max=\"16\" step=\"1\" value=\"8\" />
      </label>
    </div>
    <div class=\"canvas-wrap\">
      <canvas id=\"mapCanvas\" width=\"256\" height=\"256\"></canvas>
    </div>
    <div class=\"legend\">
      <span class=\"sw\" style=\"background:#111\"></span> Air
      <span class=\"sw\" style=\"background:#888\"></span> Stone
      <span class=\"sw\" style=\"background:#3ddc84\"></span> Grass
      <span class=\"sw\" style=\"background:#3ec0ff\"></span> Water
      <span class=\"sw\" style=\"background:#e88020\"></span> Tree
    </div>
  </div>
  <script>\n)";

    // Emit compact map data: top-down type per (x,z). 0=Air,1=Stone,2=Grass,3=Water,4=Tree
    file << "const MAPW = " << world.width << ";\n";
    file << "const MAPH = " << world.depth << ";\n";
    file << "const MAP = [\n";
    for (int z = 0; z < world.depth; ++z) {
        file << "  ";
        for (int x = 0; x < world.width; ++x) {
            int code = 0; // default Air
            int topY = -1;
            for (int y = world.height - 1; y >= 0; --y) {
                VoxelType::Type type = world.voxels[x][y][z].type;
                if (type != VoxelType::AIR) {
                    switch (type) {
                        case VoxelType::STONE: code = 1; break;
                        case VoxelType::GRASS: code = 2; break;
                        case VoxelType::WATER: code = 3; break;
                        case VoxelType::TREE:  code = 4; break;
                        default: code = 0; break;
                    }
                    topY = y;
                    break;
                }
            }
            file << code;
            if (!(z == world.depth - 1 && x == world.width - 1)) file << ",";
        }
        file << "\n";
    }
    file << "];\n";

    // Emit elevation (0..255) based on top voxel height for shading
    file << "const ELEV = [\n";
    for (int z = 0; z < world.depth; ++z) {
        file << "  ";
        for (int x = 0; x < world.width; ++x) {
            int topY = 0;
            for (int y = world.height - 1; y >= 0; --y) {
                if (world.voxels[x][y][z].type != VoxelType::AIR) { topY = y; break; }
            }
            int elev = static_cast<int>(std::round(255.0 * (double(topY) / std::max(1, world.height - 1))));
            if (elev < 0) elev = 0; if (elev > 255) elev = 255;
            file << elev;
            if (!(z == world.depth - 1 && x == world.width - 1)) file << ",";
        }
        file << "\n";
    }
    file << "];\n";

    // Viewer script
    file << R"(const COLORS = ["#111","#888","#3ddc84","#3ec0ff","#e88020"];
const canvas = document.getElementById('mapCanvas');
const ctx = canvas.getContext('2d', { alpha: false });
const zoom = document.getElementById('zoom');

let offsetX = 0;
let offsetY = 0;
let isDragging = false;
let lastX = 0;
let lastY = 0;

function render() {
  const scale = parseInt(zoom.value, 10);
  canvas.width = Math.min(MAPW * scale, 1024);
  canvas.height = Math.min(MAPH * scale, 1024);
  ctx.imageSmoothingEnabled = false;

  // Clear
  ctx.setTransform(1, 0, 0, 1, 0, 0);
  ctx.fillStyle = '#0a0c10';
  ctx.fillRect(0, 0, canvas.width, canvas.height);

  // Apply pan offset
  ctx.setTransform(1, 0, 0, 1, offsetX, offsetY);

  // Draw tiles
  const scaleVal = parseInt(zoom.value, 10);
  let i = 0;
  for (let y = 0; y < MAPH; y++) {
    for (let x = 0; x < MAPW; x++) {
      const v = MAP[i];
      const h = ELEV[i] / 255; // 0..1
      i++;
      // Shade color by elevation (higher = brighter)
      const base = COLORS[v];
      const shade = 0.4 + 0.6 * h; // 0.4..1.0
      const r = parseInt(base.slice(1,3),16);
      const g = parseInt(base.slice(3,5),16);
      const b = parseInt(base.slice(5,7),16);
      const rr = Math.min(255, Math.round(r * shade));
      const gg = Math.min(255, Math.round(g * shade));
      const bb = Math.min(255, Math.round(b * shade));
      ctx.fillStyle = `rgb(${rr},${gg},${bb})`;
      ctx.fillRect(x * scaleVal, y * scaleVal, scaleVal, scaleVal);
    }
  }
}

function clampOffsets() {
  const scaleVal = parseInt(zoom.value, 10);
  const maxX = 0;
  const maxY = 0;
  const minX = canvas.width - MAPW * scaleVal;
  const minY = canvas.height - MAPH * scaleVal;
  offsetX = Math.min(maxX, Math.max(minX, offsetX));
  offsetY = Math.min(maxY, Math.max(minY, offsetY));
}

canvas.addEventListener('mousedown', (e) => {
  isDragging = true;
  canvas.style.cursor = 'grabbing';
  lastX = e.clientX;
  lastY = e.clientY;
});

canvas.addEventListener('mousemove', (e) => {
  if (!isDragging) return;
  const dx = e.clientX - lastX;
  const dy = e.clientY - lastY;
  lastX = e.clientX;
  lastY = e.clientY;
  offsetX += dx;
  offsetY += dy;
  clampOffsets();
  render();
});

['mouseup','mouseleave'].forEach(type => canvas.addEventListener(type, () => {
  isDragging = false;
  canvas.style.cursor = 'grab';
}));

zoom.addEventListener('input', () => { clampOffsets(); render(); });
render();
)";
    file << R"(
</script>
)";
    file << "</body>\n</html>\n";
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

    std::cout << "Exporting 3D surface JSON...\n";
    generateSurfaceJSON(world, "world_surface.json");
    
    std::cout << "\n✅ World generation complete!\n";
    std::cout << "📄 View ASCII map: cat world_map.txt\n";
    std::cout << "🌐 View HTML demo: open world_viewer.html in browser\n";
    
    return 0;
}
