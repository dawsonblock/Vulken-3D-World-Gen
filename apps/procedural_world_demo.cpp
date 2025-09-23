#ifdef ENABLE_GRAPHICS
#include <iostream>
#include <vector>
#include <random>
#include <cmath>
#include <fstream>
#include <chrono>
#include <map>
#include <string>

// Modular procedural world generation system
class ProceduralWorldGenerator {
public:
    struct WorldModule {
        std::string name;
        std::string type;
        float weight;
        std::function<void(int, int, int, std::vector<std::vector<std::vector<uint8_t>>>&)> generator;

        WorldModule(const std::string& n, const std::string& t, float w,
                   std::function<void(int, int, int, std::vector<std::vector<std::vector<uint8_t>>>&)> g)
            : name(n), type(t), weight(w), generator(g) {}
    };

    struct WorldConfig {
        int width = 256;
        int height = 64;
        int depth = 256;
        int seed = 12345;
        float moduleDensity = 0.1f;
        std::vector<std::string> enabledModules;
    };

private:
    WorldConfig config;
    std::vector<std::vector<std::vector<uint8_t>>> world;
    std::vector<WorldModule> modules;
    std::mt19937 rng;

public:
    ProceduralWorldGenerator(const WorldConfig& cfg) : config(cfg), rng(static_cast<unsigned int>(config.seed)) {
        world.resize(static_cast<size_t>(config.width),
                    std::vector<std::vector<uint8_t>>(static_cast<size_t>(config.height),
                    std::vector<uint8_t>(static_cast<size_t>(config.depth), 0)));

        initializeModules();
    }

    void initializeModules() {
        // Terrain modules
        modules.emplace_back("mountains", "terrain", 0.3f, [this](int x, int y, int z, auto& w) {
            if (y < config.height * 0.7f) {
                float noise = std::sin(x * 0.01f) * std::cos(z * 0.01f) * 10.0f;
                if (y < noise + config.height * 0.5f) {
                    w[static_cast<size_t>(x)][static_cast<size_t>(y)][static_cast<size_t>(z)] = 4; // Mountain
                }
            }
        });

        modules.emplace_back("hills", "terrain", 0.4f, [this](int x, int y, int z, auto& w) {
            if (y < config.height * 0.4f) {
                float noise = std::sin(x * 0.02f) * std::cos(z * 0.02f) * 5.0f;
                if (y < noise + config.height * 0.3f) {
                    w[static_cast<size_t>(x)][static_cast<size_t>(y)][static_cast<size_t>(z)] = 1; // Stone
                }
            }
        });

        modules.emplace_back("plains", "terrain", 0.3f, [this](int x, int y, int z, auto& w) {
            if (y < config.height * 0.2f) {
                w[static_cast<size_t>(x)][static_cast<size_t>(y)][static_cast<size_t>(z)] = 2; // Grass
            }
        });

        // Water modules
        modules.emplace_back("rivers", "water", 0.2f, [this](int x, int y, int z, auto& w) {
            if (y < config.height * 0.15f) {
                float riverNoise = std::sin(x * 0.005f) * 20.0f;
                if (std::abs(z - riverNoise) < 3.0f) {
                    w[static_cast<size_t>(x)][static_cast<size_t>(y)][static_cast<size_t>(z)] = 3; // Water
                }
            }
        });

        modules.emplace_back("lakes", "water", 0.1f, [this](int x, int y, int z, auto& w) {
            if (y < config.height * 0.1f) {
                float dist = std::sqrt(static_cast<float>((x - 128) * (x - 128) + (z - 128) * (z - 128)));
                if (dist < 15.0f) {
                    w[static_cast<size_t>(x)][static_cast<size_t>(y)][static_cast<size_t>(z)] = 3; // Water
                }
            }
        });

        // Vegetation modules
        modules.emplace_back("forests", "vegetation", 0.2f, [this](int x, int y, int z, auto& w) {
            if (y < config.height * 0.3f && w[static_cast<size_t>(x)][static_cast<size_t>(y)][static_cast<size_t>(z)] == 2) {
                float forestNoise = std::sin(x * 0.03f) * std::cos(z * 0.03f);
                if (forestNoise > 0.5f) {
                    w[static_cast<size_t>(x)][static_cast<size_t>(y)][static_cast<size_t>(z)] = 5; // Tree
                }
            }
        });

        // Cave modules
        modules.emplace_back("caves", "underground", 0.1f, [this](int x, int y, int z, auto& w) {
            if (y < config.height * 0.5f) {
                float caveNoise = std::sin(x * 0.02f) * std::cos(y * 0.02f) * std::sin(z * 0.02f);
                if (caveNoise > 0.3f) {
                    w[static_cast<size_t>(x)][static_cast<size_t>(y)][static_cast<size_t>(z)] = 0; // Air
                }
            }
        });

        // Mineral modules
        modules.emplace_back("iron_ore", "minerals", 0.05f, [this](int x, int y, int z, auto& w) {
            if (y < config.height * 0.3f && w[static_cast<size_t>(x)][static_cast<size_t>(y)][static_cast<size_t>(z)] == 1) {
                float oreNoise = std::sin(x * 0.1f) * std::cos(z * 0.1f);
                if (oreNoise > 0.8f) {
                    w[static_cast<size_t>(x)][static_cast<size_t>(y)][static_cast<size_t>(z)] = 6; // Iron Ore
                }
            }
        });

        modules.emplace_back("gold_ore", "minerals", 0.02f, [this](int x, int y, int z, auto& w) {
            if (y < config.height * 0.2f && w[static_cast<size_t>(x)][static_cast<size_t>(y)][static_cast<size_t>(z)] == 1) {
                float oreNoise = std::sin(x * 0.15f) * std::cos(z * 0.15f);
                if (oreNoise > 0.9f) {
                    w[static_cast<size_t>(x)][static_cast<size_t>(y)][static_cast<size_t>(z)] = 7; // Gold Ore
                }
            }
        });
    }

    void generateWorld() {
        std::cout << "Generating procedural world with " << modules.size() << " modules..." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();

        // Apply enabled modules
        for (const auto& module : modules) {
            if (config.enabledModules.empty() ||
                std::find(config.enabledModules.begin(), config.enabledModules.end(), module.name) != config.enabledModules.end()) {

                std::cout << "Applying module: " << module.name << " (type: " << module.type << ")" << std::endl;

                for (int x = 0; x < config.width; x++) {
                    for (int y = 0; y < config.height; y++) {
                        for (int z = 0; z < config.depth; z++) {
                            if (static_cast<float>(rng()) / rng.max() < module.weight * config.moduleDensity) {
                                module.generator(x, y, z, world);
                            }
                        }
                    }
                }
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "World generation completed in " << duration.count() << "ms" << std::endl;
    }

    void printStatistics() {
        std::map<uint8_t, int> typeCounts;
        int totalVoxels = 0;

        for (int x = 0; x < config.width; x++) {
            for (int y = 0; y < config.height; y++) {
                for (int z = 0; z < config.depth; z++) {
                    uint8_t type = world[static_cast<size_t>(x)][static_cast<size_t>(y)][static_cast<size_t>(z)];
                    typeCounts[type]++;
                    totalVoxels++;
                }
            }
        }

        std::cout << "\nProcedural World Statistics:" << std::endl;
        std::cout << "============================" << std::endl;
        std::cout << "Dimensions: " << config.width << "x" << config.height << "x" << config.depth << std::endl;
        std::cout << "Total voxels: " << totalVoxels << std::endl;
        std::cout << "Active modules: " << config.enabledModules.size() << std::endl;

        std::map<uint8_t, std::string> typeNames = {
            {0, "Air"}, {1, "Stone"}, {2, "Grass"}, {3, "Water"},
            {4, "Mountain"}, {5, "Tree"}, {6, "Iron Ore"}, {7, "Gold Ore"}
        };

        for (const auto& pair : typeCounts) {
            std::string name = typeNames.count(pair.first) ? typeNames[pair.first] : "Unknown";
            std::cout << name << ": " << pair.second << " voxels ("
                     << (pair.second * 100.0f / totalVoxels) << "%)" << std::endl;
        }
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
                    uint8_t type = world[static_cast<size_t>(x)][static_cast<size_t>(y)][static_cast<size_t>(z)];
                    file.write(reinterpret_cast<const char*>(&type), sizeof(type));
                }
            }
        }

        std::cout << "Procedural world exported to: " << filename << std::endl;
    }

    void listAvailableModules() {
        std::cout << "\nAvailable World Modules:" << std::endl;
        std::cout << "========================" << std::endl;

        std::map<std::string, std::vector<std::string>> modulesByType;
        for (const auto& module : modules) {
            modulesByType[module.type].push_back(module.name);
        }

        for (const auto& pair : modulesByType) {
            std::cout << "\n" << pair.first << ":" << std::endl;
            for (const auto& moduleName : pair.second) {
                std::cout << "  - " << moduleName << std::endl;
            }
        }
    }
};

int main(int argc, char* argv[]) {
    std::cout << "Procedural World Generator Demo" << std::endl;
    std::cout << "===============================" << std::endl;

    // Parse command line arguments
    ProceduralWorldGenerator::WorldConfig config;
    std::vector<std::string> enabledModules;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --width W        Set world width (default: 256)" << std::endl;
            std::cout << "  --height H       Set world height (default: 64)" << std::endl;
            std::cout << "  --depth D        Set world depth (default: 256)" << std::endl;
            std::cout << "  --seed S         Set random seed (default: 12345)" << std::endl;
            std::cout << "  --modules M      Comma-separated list of modules to enable" << std::endl;
            std::cout << "  --list-modules   List all available modules" << std::endl;
            std::cout << "  --help, -h       Show this help message" << std::endl;
            return 0;
        } else if (arg == "--list-modules") {
            ProceduralWorldGenerator generator(config);
            generator.listAvailableModules();
            return 0;
        } else if (arg == "--width" && i + 1 < argc) {
            config.width = std::stoi(argv[++i]);
        } else if (arg == "--height" && i + 1 < argc) {
            config.height = std::stoi(argv[++i]);
        } else if (arg == "--depth" && i + 1 < argc) {
            config.depth = std::stoi(argv[++i]);
        } else if (arg == "--seed" && i + 1 < argc) {
            config.seed = std::stoi(argv[++i]);
        } else if (arg == "--modules" && i + 1 < argc) {
            std::string modulesStr = argv[++i];
            std::stringstream ss(modulesStr);
            std::string module;
            while (std::getline(ss, module, ',')) {
                enabledModules.push_back(module);
            }
        }
    }

    config.enabledModules = enabledModules;

    // Generate world
    ProceduralWorldGenerator generator(config);
    generator.generateWorld();
    generator.printStatistics();
    generator.exportToFile("procedural_world.vox");

    std::cout << "\nProcedural world generation complete!" << std::endl;
    std::cout << "Features:" << std::endl;
    std::cout << "- Modular world generation system" << std::endl;
    std::cout << "- Multiple terrain types (mountains, hills, plains)" << std::endl;
    std::cout << "- Water features (rivers, lakes)" << std::endl;
    std::cout << "- Vegetation (forests)" << std::endl;
    std::cout << "- Underground features (caves, minerals)" << std::endl;
    std::cout << "- User-configurable parameters" << std::endl;

    return 0;
}

#else
#include <iostream>
int main() {
    std::cout << "Procedural world demo requires graphics support (ENABLE_GRAPHICS=ON)" << std::endl;
    return 1;
}
#endif
