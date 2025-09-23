#ifdef ENABLE_GRAPHICS
#include <iostream>
#include <vector>
#include <random>
#include <cmath>
#include <fstream>
#include <chrono>
#include <array>
#include <thread>
#include <future>

// GPU-accelerated terrain generation using compute shaders simulation
class GPUComputeTerrainGenerator {
public:
    struct TerrainConfig {
        int width = 512;
        int height = 256;
        int depth = 512;
        float voxelSize = 1.0f;
        float noiseScale = 0.002f;
        int seed = 12345;
        int workGroupSize = 64;
        int numWorkGroups = 8;
    };

    struct VoxelData {
        float density;
        uint8_t material;
        float temperature;
        float humidity;
        float pressure;

        VoxelData() : density(0.0f), material(0), temperature(20.0f), humidity(0.5f), pressure(1.0f) {}
    };

    struct ComputeShaderData {
        std::vector<float> inputData;
        std::vector<VoxelData> outputData;
        std::vector<float> intermediateResults;
        int workGroupCount;
        int workGroupSize;
    };

    struct PerformanceMetrics {
        float totalTime;
        float computeTime;
        float memoryTransferTime;
        int voxelsProcessed;
        float voxelsPerSecond;
        float memoryBandwidth;
        int workGroupsExecuted;
    };

private:
    TerrainConfig config;
    std::vector<std::vector<std::vector<VoxelData>>> voxels;
    std::mt19937 rng;
    PerformanceMetrics metrics;

    // Simulate GPU compute shader execution
    std::vector<VoxelData> simulateComputeShader(const std::vector<float>& inputData, int workGroupId) {
        std::vector<VoxelData> results;
        results.reserve(static_cast<size_t>(config.workGroupSize));

        // Simulate parallel processing within work group
        for (int i = 0; i < config.workGroupSize; i++) {
            int globalIndex = workGroupId * config.workGroupSize + i;
            if (globalIndex >= static_cast<int>(inputData.size())) break;

            VoxelData voxel;

            // Simulate complex GPU computation
            float x = static_cast<float>(globalIndex % config.width);
            float y = static_cast<float>((globalIndex / config.width) % config.height);
            float z = static_cast<float>(globalIndex / (config.width * config.height));

            // Advanced noise computation (simulating GPU shader)
            float density = 0.0f;
            float frequency = 1.0f;
            float amplitude = 1.0f;

            // Multiple octaves of noise
            for (int octave = 0; octave < 8; octave++) {
                float nx = x * frequency * config.noiseScale;
                float ny = y * frequency * config.noiseScale;
                float nz = z * frequency * config.noiseScale;

                density += amplitude * std::sin(nx) * std::cos(ny) * std::sin(nz);
                frequency *= 2.0f;
                amplitude *= 0.5f;
            }

            // Add terrain features
            float heightFactor = y / config.height;
            density += (1.0f - heightFactor) * 0.3f;

            // Cave generation
            float caveNoise = std::sin(x * config.noiseScale * 0.5f) *
                             std::cos(y * config.noiseScale * 0.5f) *
                             std::sin(z * config.noiseScale * 0.5f);
            if (caveNoise > 0.3f && y < config.height * 0.7f) {
                density -= 0.4f;
            }

            // Mountain generation
            float mountainNoise = std::sin(x * config.noiseScale * 0.2f) *
                                 std::cos(z * config.noiseScale * 0.2f);
            if (mountainNoise > 0.4f) {
                density += mountainNoise * 0.3f;
            }

            voxel.density = density;

            // Determine material based on density and position
            if (density < 0.2f) {
                voxel.material = 0; // Air
            } else if (heightFactor < 0.1f) {
                voxel.material = 3; // Water
            } else if (heightFactor < 0.3f) {
                voxel.material = 2; // Grass
            } else if (heightFactor < 0.7f) {
                voxel.material = 1; // Stone
            } else {
                voxel.material = 4; // Mountain
            }

            // Calculate environmental properties
            voxel.temperature = 20.0f - (heightFactor * 20.0f);
            voxel.humidity = 0.5f + std::sin(x * 0.1f) * 0.3f;
            voxel.pressure = 1.0f - (heightFactor * 0.3f);

            results.push_back(voxel);
        }

        return results;
    }

    // Simulate GPU memory transfer
    void simulateMemoryTransfer(const std::vector<VoxelData>& data, int workGroupId) {
        // Simulate memory bandwidth limitations
        std::this_thread::sleep_for(std::chrono::microseconds(10));

        // Copy data to main memory (simulated)
        int startIndex = workGroupId * config.workGroupSize;
        for (size_t i = 0; i < data.size() && startIndex + static_cast<int>(i) < static_cast<int>(voxels.size() * voxels[0].size() * voxels[0][0].size()); i++) {
            int x = (startIndex + static_cast<int>(i)) % config.width;
            int y = ((startIndex + static_cast<int>(i)) / config.width) % config.height;
            int z = (startIndex + static_cast<int>(i)) / (config.width * config.height);

            if (x < config.width && y < config.height && z < config.depth) {
                voxels[static_cast<size_t>(x)][static_cast<size_t>(y)][static_cast<size_t>(z)] = data[i];
            }
        }
    }

    // Generate terrain using GPU compute shaders
    void generateTerrainGPU() {
        std::cout << "Generating terrain using GPU compute shaders..." << std::endl;

        auto start = std::chrono::high_resolution_clock::now();

        // Initialize voxel grid
        voxels.resize(static_cast<size_t>(config.width),
                     std::vector<std::vector<VoxelData>>(static_cast<size_t>(config.height),
                     std::vector<VoxelData>(static_cast<size_t>(config.depth))));

        // Prepare input data for GPU
        std::vector<float> inputData;
        inputData.reserve(static_cast<size_t>(config.width * config.height * config.depth));

        for (int x = 0; x < config.width; x++) {
            for (int y = 0; y < config.height; y++) {
                for (int z = 0; z < config.depth; z++) {
                    inputData.push_back(static_cast<float>(x + y + z));
                }
            }
        }

        auto computeStart = std::chrono::high_resolution_clock::now();

        // Simulate GPU compute shader execution
        std::vector<std::future<std::vector<VoxelData>>> futures;
        int totalWorkGroups = (static_cast<int>(inputData.size()) + config.workGroupSize - 1) / config.workGroupSize;

        for (int workGroupId = 0; workGroupId < totalWorkGroups; workGroupId++) {
            futures.push_back(std::async(std::launch::async, [this, &inputData, workGroupId]() {
                return simulateComputeShader(inputData, workGroupId);
            }));
        }

        // Wait for all work groups to complete
        for (auto& future : futures) {
            auto result = future.get();
            int workGroupId = static_cast<int>(&future - &futures[0]);
            simulateMemoryTransfer(result, workGroupId);
        }

        auto computeEnd = std::chrono::high_resolution_clock::now();
        auto end = std::chrono::high_resolution_clock::now();

        // Calculate performance metrics
        auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        auto computeDuration = std::chrono::duration_cast<std::chrono::milliseconds>(computeEnd - computeStart);

        metrics.totalTime = totalDuration.count();
        metrics.computeTime = computeDuration.count();
        metrics.memoryTransferTime = metrics.totalTime - metrics.computeTime;
        metrics.voxelsProcessed = config.width * config.height * config.depth;
        metrics.voxelsPerSecond = metrics.voxelsProcessed / (metrics.totalTime / 1000.0f);
        metrics.memoryBandwidth = (metrics.voxelsProcessed * sizeof(VoxelData)) / (metrics.totalTime / 1000.0f) / (1024.0f * 1024.0f); // MB/s
        metrics.workGroupsExecuted = totalWorkGroups;

        std::cout << "GPU terrain generation completed in " << metrics.totalTime << "ms" << std::endl;
    }

    // Generate terrain using CPU for comparison
    void generateTerrainCPU() {
        std::cout << "Generating terrain using CPU for comparison..." << std::endl;

        auto start = std::chrono::high_resolution_clock::now();

        // Initialize voxel grid
        voxels.resize(static_cast<size_t>(config.width),
                     std::vector<std::vector<VoxelData>>(static_cast<size_t>(config.height),
                     std::vector<VoxelData>(static_cast<size_t>(config.depth))));

        for (int x = 0; x < config.width; x++) {
            for (int y = 0; y < config.height; y++) {
                for (int z = 0; z < config.depth; z++) {
                    VoxelData& voxel = voxels[static_cast<size_t>(x)][static_cast<size_t>(y)][static_cast<size_t>(z)];

                    // Simple noise computation
                    float density = std::sin(x * config.noiseScale) * std::cos(y * config.noiseScale) * std::sin(z * config.noiseScale);

                    float heightFactor = static_cast<float>(y) / config.height;
                    density += (1.0f - heightFactor) * 0.3f;

                    voxel.density = density;

                    if (density < 0.2f) {
                        voxel.material = 0; // Air
                    } else if (heightFactor < 0.1f) {
                        voxel.material = 3; // Water
                    } else if (heightFactor < 0.3f) {
                        voxel.material = 2; // Grass
                    } else if (heightFactor < 0.7f) {
                        voxel.material = 1; // Stone
                    } else {
                        voxel.material = 4; // Mountain
                    }

                    voxel.temperature = 20.0f - (heightFactor * 20.0f);
                    voxel.humidity = 0.5f + std::sin(x * 0.1f) * 0.3f;
                    voxel.pressure = 1.0f - (heightFactor * 0.3f);
                }
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "CPU terrain generation completed in " << duration.count() << "ms" << std::endl;
    }

public:
    GPUComputeTerrainGenerator(const TerrainConfig& cfg) : config(cfg), rng(static_cast<unsigned int>(config.seed)) {
        metrics = {0.0f, 0.0f, 0.0f, 0, 0.0f, 0.0f, 0};
    }

    void generateTerrain() {
        generateTerrainGPU();
    }

    void printPerformanceMetrics() {
        std::cout << "\nGPU Compute Shader Performance Metrics:" << std::endl;
        std::cout << "=======================================" << std::endl;
        std::cout << "Total time: " << metrics.totalTime << " ms" << std::endl;
        std::cout << "Compute time: " << metrics.computeTime << " ms" << std::endl;
        std::cout << "Memory transfer time: " << metrics.memoryTransferTime << " ms" << std::endl;
        std::cout << "Voxels processed: " << metrics.voxelsProcessed << std::endl;
        std::cout << "Voxels per second: " << metrics.voxelsPerSecond << std::endl;
        std::cout << "Memory bandwidth: " << metrics.memoryBandwidth << " MB/s" << std::endl;
        std::cout << "Work groups executed: " << metrics.workGroupsExecuted << std::endl;
        std::cout << "Work group size: " << config.workGroupSize << std::endl;
    }

    void printStatistics() {
        std::vector<int> materialCounts(6, 0);
        int totalVoxels = 0;

        for (int x = 0; x < config.width; x++) {
            for (int y = 0; y < config.height; y++) {
                for (int z = 0; z < config.depth; z++) {
                    const auto& voxel = voxels[static_cast<size_t>(x)][static_cast<size_t>(y)][static_cast<size_t>(z)];
                    materialCounts[voxel.material]++;
                    totalVoxels++;
                }
            }
        }

        std::cout << "\nGPU Compute Terrain Statistics:" << std::endl;
        std::cout << "===============================" << std::endl;
        std::cout << "Dimensions: " << config.width << "x" << config.height << "x" << config.depth << std::endl;
        std::cout << "Total voxels: " << totalVoxels << std::endl;
        std::cout << "Air: " << materialCounts[0] << " voxels" << std::endl;
        std::cout << "Stone: " << materialCounts[1] << " voxels" << std::endl;
        std::cout << "Grass: " << materialCounts[2] << " voxels" << std::endl;
        std::cout << "Water: " << materialCounts[3] << " voxels" << std::endl;
        std::cout << "Mountain: " << materialCounts[4] << " voxels" << std::endl;
    }

    void benchmarkCPUvsGPU() {
        std::cout << "\nBenchmarking CPU vs GPU Performance..." << std::endl;
        std::cout << "======================================" << std::endl;

        // Test CPU performance
        auto cpuStart = std::chrono::high_resolution_clock::now();
        generateTerrainCPU();
        auto cpuEnd = std::chrono::high_resolution_clock::now();
        auto cpuDuration = std::chrono::duration_cast<std::chrono::milliseconds>(cpuEnd - cpuStart);

        // Test GPU performance
        auto gpuStart = std::chrono::high_resolution_clock::now();
        generateTerrainGPU();
        auto gpuEnd = std::chrono::high_resolution_clock::now();
        auto gpuDuration = std::chrono::duration_cast<std::chrono::milliseconds>(gpuEnd - gpuStart);

        // Print comparison
        std::cout << "\nPerformance Comparison:" << std::endl;
        std::cout << "CPU Time: " << cpuDuration.count() << " ms" << std::endl;
        std::cout << "GPU Time: " << gpuDuration.count() << " ms" << std::endl;
        std::cout << "Speedup: " << (static_cast<float>(cpuDuration.count()) / gpuDuration.count()) << "x" << std::endl;
        std::cout << "CPU Voxels/sec: " << (metrics.voxelsProcessed / (cpuDuration.count() / 1000.0f)) << std::endl;
        std::cout << "GPU Voxels/sec: " << metrics.voxelsPerSecond << std::endl;
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
        file.write(reinterpret_cast<const char*>(&config.workGroupSize), sizeof(config.workGroupSize));

        // Write voxel data
        for (int x = 0; x < config.width; x++) {
            for (int y = 0; y < config.height; y++) {
                for (int z = 0; z < config.depth; z++) {
                    const auto& voxel = voxels[static_cast<size_t>(x)][static_cast<size_t>(y)][static_cast<size_t>(z)];
                    file.write(reinterpret_cast<const char*>(&voxel.density), sizeof(voxel.density));
                    file.write(reinterpret_cast<const char*>(&voxel.material), sizeof(voxel.material));
                    file.write(reinterpret_cast<const char*>(&voxel.temperature), sizeof(voxel.temperature));
                    file.write(reinterpret_cast<const char*>(&voxel.humidity), sizeof(voxel.humidity));
                    file.write(reinterpret_cast<const char*>(&voxel.pressure), sizeof(voxel.pressure));
                }
            }
        }

        std::cout << "GPU compute terrain exported to: " << filename << std::endl;
    }
};

int main() {
    std::cout << "GPU Compute Shader Terrain Generator Demo" << std::endl;
    std::cout << "========================================" << std::endl;

    // Create terrain configuration
    GPUComputeTerrainGenerator::TerrainConfig config;
    config.width = 128;
    config.height = 64;
    config.depth = 128;
    config.voxelSize = 1.0f;
    config.noiseScale = 0.01f;
    config.seed = 42;
    config.workGroupSize = 64;
    config.numWorkGroups = 8;

    // Generate terrain
    GPUComputeTerrainGenerator generator(config);
    generator.generateTerrain();
    generator.printStatistics();
    generator.printPerformanceMetrics();
    generator.exportToFile("gpu_compute_terrain.vox");

    // Benchmark CPU vs GPU
    generator.benchmarkCPUvsGPU();

    std::cout << "\nGPU compute shader terrain generation complete!" << std::endl;
    std::cout << "Features:" << std::endl;
    std::cout << "- GPU compute shader simulation with parallel processing" << std::endl;
    std::cout << "- Work group-based execution with configurable group sizes" << std::endl;
    std::cout << "- Advanced noise computation with multiple octaves" << std::endl;
    std::cout << "- Memory bandwidth optimization and transfer simulation" << std::endl;
    std::cout << "- Performance benchmarking and metrics collection" << std::endl;
    std::cout << "- Environmental property calculation (temperature, humidity, pressure)" << std::endl;

    return 0;
}

#else
#include <iostream>
int main() {
    std::cout << "GPU compute shader terrain demo requires graphics support (ENABLE_GRAPHICS=ON)" << std::endl;
    return 1;
}
#endif
