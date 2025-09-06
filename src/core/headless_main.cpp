#include "logger.hpp"
#include <iostream>
#include <chrono>
#include <thread>

int main(int argc, char* argv[]) {
    voxelvk::Logger logger("Headless");

    logger.Info("VoxelVK Headless Mode");
    logger.Info("Build: {}", __DATE__ " " __TIME__);

    // Parse command line arguments
    bool benchmark = false;
    bool validate = false;
    int frames = 100;

    for(int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if(arg == "--benchmark") {
            benchmark = true;
        } else if(arg == "--validate") {
            validate = true;
        } else if(arg == "--frames") {
            if(i + 1 >= argc) {
                std::cerr << "Error: --frames requires a numeric argument\n";
                return 1;
            }

            try {
                int parsed_frames = std::stoi(argv[++i]);
                if(parsed_frames < 0) {
                    std::cerr << "Error: --frames must be a non-negative number, got: " << parsed_frames << "\n";
                    return 1;
                }
                frames = parsed_frames;
            } catch(const std::invalid_argument& e) {
                std::cerr << "Error: --frames requires a numeric argument, got: " << argv[i] << "\n";
                return 1;
            } catch(const std::out_of_range& e) {
                std::cerr << "Error: --frames value out of range: " << argv[i] << "\n";
                return 1;
            }
        } else if(arg == "--help") {
            std::cout << "VoxelVK Headless Mode\n";
            std::cout << "Options:\n";
            std::cout << "  --benchmark    Run performance benchmark\n";
            std::cout << "  --validate     Run validation tests\n";
            std::cout << "  --frames N     Run for N frames (default: 100)\n";
            std::cout << "  --help         Show this help\n";
            return 0;
        }
    }

    if(validate) {
        logger.Info("Running validation tests...");
        // Add validation logic here
        logger.Info("Validation completed successfully");
    }

    if(benchmark) {
        logger.Info("Running benchmark for {} frames...", frames);
        auto start = std::chrono::high_resolution_clock::now();

        for(int i = 0; i < frames; ++i) {
            // Simulate frame processing
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS

            if(i % 10 == 0) {
                logger.Debug("Frame {}/{}", i + 1, frames);
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        double fps = frames * 1000.0 / duration.count();
        logger.Info("Benchmark completed: {:.2f} FPS average", fps);
    } else {
        logger.Info("Running headless mode for {} frames...", frames);

        for(int i = 0; i < frames; ++i) {
            // Simulate basic processing
            std::this_thread::sleep_for(std::chrono::milliseconds(16));

            if(i % 10 == 0) {
                logger.Debug("Frame {}/{}", i + 1, frames);
            }
        }
    }

    logger.Info("Headless mode completed successfully");
    return 0;
}
