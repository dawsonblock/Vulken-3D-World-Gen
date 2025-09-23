#ifdef ENABLE_GRAPHICS
#include <iostream>
#include <vector>
#include <random>
#include <cmath>
#include <fstream>
#include <chrono>
#include <functional>

// GPU-accelerated terrain generation system
class GPUTerrainGenerator {
public:
    struct TerrainConfig {
        int width = 512;
        int height = 256;
        int depth = 512;
        float noiseScale = 0.002f;
        int seed = 12345;
        bool useGPU = true;
        int chunkSize = 64;
    };

    struct ChunkData {
        int x, y, z;
        int size;
        std::vector<float> heightData;
        std::vector<uint8_t> typeData;
        bool isGenerated;

        ChunkData(int px, int py, int pz, int s)
            : x(px), y(py), z(pz), size(s), isGenerated(false) {
            heightData.resize(static_cast<size_t>(s * s));
            typeData.resize(static_cast<size_t>(s * s));
        }
    };

    struct PerformanceMetrics {
        float cpuTime;
        float gpuTime;
        int chunksGenerated;
        int voxelsProcessed;
        float memoryUsage;
    };

private:
    TerrainConfig config;
    std::vector<ChunkData> chunks;
    std::mt19937 rng;
    PerformanceMetrics metrics;

    // CPU-based noise generation (fallback)
    float cpuNoise(float x, float y, float z) {
        float n = 0.0f;
        float frequency = 1.0f;
        float amplitude = 1.0f;

        for (int i = 0; i < 6; i++) {
            n += amplitude * std::sin(x * frequency * config.noiseScale) *
                        std::cos(y * frequency * config.noiseScale) *
                        std::sin(z * frequency * config.noiseScale);
            frequency *= 2.0f;
            amplitude *= 0.5f;
        }

        return n;
    }

    // Simulate GPU-accelerated noise generation
    std::vector<float> simulateGPUNoise(const std::vector<std::tuple<float, float, float>>& positions) {
        std::vector<float> results(positions.size());

        // Simulate parallel processing
        #pragma omp parallel for
        for (size_t i = 0; i < positions.size(); i++) {
            float x, y, z;
            std::tie(x, y, z) = positions[i];

            // Simulate GPU shader-like computation
            float n = 0.0f;
            float frequency = 1.0f;
            float amplitude = 1.0f;

            for (int j = 0; j < 8; j++) { // More octaves for GPU
                float nx = x * frequency * config.noiseScale;
                float ny = y * frequency * config.noiseScale;
                float nz = z * frequency * config.noiseScale;

                n += amplitude * std::sin(nx) * std::cos(ny) * std::sin(nz);
                frequency *= 2.0f;
                amplitude *= 0.5f;
            }

            results[i] = n;
        }

        return results;
    }

    // Generate chunk data using CPU
    void generateChunkCPU(ChunkData& chunk) {
        auto start = std::chrono::high_resolution_clock::now();

        for (int z = 0; z < chunk.size; z++) {
            for (int x = 0; x < chunk.size; x++) {
                float worldX = chunk.x + x;
                float worldZ = chunk.z + z;

                float height = cpuNoise(worldX, 0, worldZ) * 50.0f + config.height * 0.3f;
                height = std::max(0.0f, std::min(height, static_cast<float>(config.height - 1)));

                chunk.heightData[z * chunk.size + x] = height;

                // Determine terrain type
                uint8_t type = 0; // Air
                if (height < config.height * 0.1f) {
                    type = 3; // Water
                } else if (height < config.height * 0.3f) {
                    type = 2; // Grass
                } else if (height < config.height * 0.7f) {
                    type = 1; // Stone
                } else {
                    type = 4; // Mountain
                }

                chunk.typeData[z * chunk.size + x] = type;
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        metrics.cpuTime += duration.count() / 1000.0f; // Convert to milliseconds
    }

    // Generate chunk data using simulated GPU
    void generateChunkGPU(ChunkData& chunk) {
        auto start = std::chrono::high_resolution_clock::now();

        // Prepare positions for GPU processing
        std::vector<std::tuple<float, float, float>> positions;
        positions.reserve(static_cast<size_t>(chunk.size * chunk.size));

        for (int z = 0; z < chunk.size; z++) {
            for (int x = 0; x < chunk.size; x++) {
                float worldX = chunk.x + x;
                float worldZ = chunk.z + z;
                positions.emplace_back(worldX, 0, worldZ);
            }
        }

        // Simulate GPU noise generation
        auto noiseResults = simulateGPUNoise(positions);

        // Process results
        for (int z = 0; z < chunk.size; z++) {
            for (int x = 0; x < chunk.size; x++) {
                int idx = z * chunk.size + x;

                float height = noiseResults[idx] * 50.0f + config.height * 0.3f;
                height = std::max(0.0f, std::min(height, static_cast<float>(config.height - 1)));

                chunk.heightData[idx] = height;

                // Determine terrain type
                uint8_t type = 0; // Air
                if (height < config.height * 0.1f) {
                    type = 3; // Water
                } else if (height < config.height * 0.3f) {
                    type = 2; // Grass
                } else if (height < config.height * 0.7f) {
                    type = 1; // Stone
                } else {
                    type = 4; // Mountain
                }

                chunk.typeData[idx] = type;
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        metrics.gpuTime += duration.count() / 1000.0f; // Convert to milliseconds
    }

    // Initialize chunks
    void initializeChunks() {
        std::cout << "Initializing terrain chunks..." << std::endl;

        int chunksX = (config.width + config.chunkSize - 1) / config.chunkSize;
        int chunksZ = (config.depth + config.chunkSize - 1) / config.chunkSize;

        chunks.reserve(static_cast<size_t>(chunksX * chunksZ));

        for (int z = 0; z < chunksZ; z++) {
            for (int x = 0; x < chunksX; x++) {
                chunks.emplace_back(x * config.chunkSize, 0, z * config.chunkSize, config.chunkSize);
            }
        }

        std::cout << "Created " << chunks.size() << " chunks (" << chunksX << "x" << chunksZ << ")" << std::endl;
    }

public:
    GPUTerrainGenerator(const TerrainConfig& cfg) : config(cfg), rng(static_cast<unsigned int>(config.seed)) {
        metrics = {0.0f, 0.0f, 0, 0, 0.0f};
        initializeChunks();
    }

    void generateTerrain() {
        std::cout << "Generating GPU-accelerated terrain..." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();

        for (auto& chunk : chunks) {
            if (config.useGPU) {
                generateChunkGPU(chunk);
            } else {
                generateChunkCPU(chunk);
            }
            chunk.isGenerated = true;
            metrics.chunksGenerated++;
            metrics.voxelsProcessed += chunk.size * chunk.size;
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        metrics.memoryUsage = static_cast<float>(chunks.size() * config.chunkSize * config.chunkSize * sizeof(float)) / (1024.0f * 1024.0f); // MB

        std::cout << "Terrain generation completed in " << totalDuration.count() << "ms" << std::endl;
    }

    void printPerformanceMetrics() {
        std::cout << "\nGPU Terrain Generation Performance:" << std::endl;
        std::cout << "====================================" << std::endl;
        std::cout << "Chunks generated: " << metrics.chunksGenerated << std::endl;
        std::cout << "Voxels processed: " << metrics.voxelsProcessed << std::endl;
        std::cout << "Memory usage: " << metrics.memoryUsage << " MB" << std::endl;

        if (config.useGPU) {
            std::cout << "GPU processing time: " << metrics.gpuTime << " ms" << std::endl;
            std::cout << "Average time per chunk: " << (metrics.gpuTime / metrics.chunksGenerated) << " ms" << std::endl;
            std::cout << "Voxels per second: " << (metrics.voxelsProcessed / (metrics.gpuTime / 1000.0f)) << std::endl;
        } else {
            std::cout << "CPU processing time: " << metrics.cpuTime << " ms" << std::endl;
            std::cout << "Average time per chunk: " << (metrics.cpuTime / metrics.chunksGenerated) << " ms" << std::endl;
            std::cout << "Voxels per second: " << (metrics.voxelsProcessed / (metrics.cpuTime / 1000.0f)) << std::endl;
        }
    }

    void printStatistics() {
        std::vector<int> typeCounts(6, 0);
        int totalVoxels = 0;

        for (const auto& chunk : chunks) {
            if (chunk.isGenerated) {
                for (size_t i = 0; i < chunk.typeData.size(); i++) {
                    typeCounts[chunk.typeData[i]]++;
                    totalVoxels++;
                }
            }
        }

        std::cout << "\nGPU Terrain Statistics:" << std::endl;
        std::cout << "=======================" << std::endl;
        std::cout << "Dimensions: " << config.width << "x" << config.height << "x" << config.depth << std::endl;
        std::cout << "Chunk size: " << config.chunkSize << "x" << config.chunkSize << std::endl;
        std::cout << "Total voxels: " << totalVoxels << std::endl;
        std::cout << "Air: " << typeCounts[0] << " voxels" << std::endl;
        std::cout << "Stone: " << typeCounts[1] << " voxels" << std::endl;
        std::cout << "Grass: " << typeCounts[2] << " voxels" << std::endl;
        std::cout << "Water: " << typeCounts[3] << " voxels" << std::endl;
        std::cout << "Mountain: " << typeCounts[4] << " voxels" << std::endl;
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
        file.write(reinterpret_cast<const char*>(&config.chunkSize), sizeof(config.chunkSize));

        // Write chunk data
        for (const auto& chunk : chunks) {
            if (chunk.isGenerated) {
                file.write(reinterpret_cast<const char*>(&chunk.x), sizeof(chunk.x));
                file.write(reinterpret_cast<const char*>(&chunk.y), sizeof(chunk.y));
                file.write(reinterpret_cast<const char*>(&chunk.z), sizeof(chunk.z));
                file.write(reinterpret_cast<const char*>(&chunk.size), sizeof(chunk.size));
                file.write(reinterpret_cast<const char*>(chunk.heightData.data()),
                          static_cast<std::streamsize>(chunk.heightData.size() * sizeof(float)));
                file.write(reinterpret_cast<const char*>(chunk.typeData.data()),
                          static_cast<std::streamsize>(chunk.typeData.size()));
            }
        }

        std::cout << "GPU terrain exported to: " << filename << std::endl;
    }

    void benchmarkCPUvsGPU() {
        std::cout << "\nBenchmarking CPU vs GPU performance..." << std::endl;
        std::cout << "=====================================" << std::endl;

        // Reset metrics
        metrics = {0.0f, 0.0f, 0, 0, 0.0f};

        // Test CPU performance
        std::cout << "Testing CPU performance..." << std::endl;
        config.useGPU = false;
        auto cpuStart = std::chrono::high_resolution_clock::now();

        for (auto& chunk : chunks) {
            generateChunkCPU(chunk);
            metrics.chunksGenerated++;
            metrics.voxelsProcessed += chunk.size * chunk.size;
        }

        auto cpuEnd = std::chrono::high_resolution_clock::now();
        auto cpuDuration = std::chrono::duration_cast<std::chrono::milliseconds>(cpuEnd - cpuStart);

        float cpuTime = cpuDuration.count();
        float cpuVoxelsPerSecond = metrics.voxelsProcessed / (cpuTime / 1000.0f);

        // Reset for GPU test
        metrics = {0.0f, 0.0f, 0, 0, 0.0f};

        // Test GPU performance
        std::cout << "Testing GPU performance..." << std::endl;
        config.useGPU = true;
        auto gpuStart = std::chrono::high_resolution_clock::now();

        for (auto& chunk : chunks) {
            generateChunkGPU(chunk);
            metrics.chunksGenerated++;
            metrics.voxelsProcessed += chunk.size * chunk.size;
        }

        auto gpuEnd = std::chrono::high_resolution_clock::now();
        auto gpuDuration = std::chrono::duration_cast<std::chrono::milliseconds>(gpuEnd - gpuStart);

        float gpuTime = gpuDuration.count();
        float gpuVoxelsPerSecond = metrics.voxelsProcessed / (gpuTime / 1000.0f);

        // Print comparison
        std::cout << "\nPerformance Comparison:" << std::endl;
        std::cout << "CPU Time: " << cpuTime << " ms" << std::endl;
        std::cout << "GPU Time: " << gpuTime << " ms" << std::endl;
        std::cout << "Speedup: " << (cpuTime / gpuTime) << "x" << std::endl;
        std::cout << "CPU Voxels/sec: " << cpuVoxelsPerSecond << std::endl;
        std::cout << "GPU Voxels/sec: " << gpuVoxelsPerSecond << std::endl;
    }
};

int main() {
    std::cout << "GPU-Accelerated Terrain Generator Demo" << std::endl;
    std::cout << "======================================" << std::endl;

    // Create terrain configuration
    GPUTerrainGenerator::TerrainConfig config;
    config.width = 256;
    config.height = 128;
    config.depth = 256;
    config.noiseScale = 0.005f;
    config.seed = 42;
    config.useGPU = true;
    config.chunkSize = 32;

    // Generate terrain
    GPUTerrainGenerator generator(config);
    generator.generateTerrain();
    generator.printStatistics();
    generator.printPerformanceMetrics();
    generator.exportToFile("gpu_terrain.vox");

    // Benchmark CPU vs GPU
    generator.benchmarkCPUvsGPU();

    std::cout << "\nGPU terrain generation complete!" << std::endl;
    std::cout << "Features:" << std::endl;
    std::cout << "- GPU-accelerated noise generation" << std::endl;
    std::cout << "- Chunk-based terrain processing" << std::endl;
    std::cout << "- Parallel computation simulation" << std::endl;
    std::cout << "- Performance benchmarking" << std::endl;
    std::cout << "- Memory-efficient data structures" << std::endl;

    return 0;
}

#else
#include <iostream>
int main() {
    std::cout << "GPU terrain demo requires graphics support (ENABLE_GRAPHICS=ON)" << std::endl;
    return 1;
}
#endif
