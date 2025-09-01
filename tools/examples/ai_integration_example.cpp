#include "../src/ai/ai_enhanced_generator.hpp"
#include "../src/ai/ai_tensorrt_manager.hpp"
#include "../src/ai/ai_enhanced_biome_system.hpp"
#include "../src/ai/ai_runtime_commands.hpp"
#include "../src/ai/ai_training_integration.hpp"
#include "../src/ai/ai_palette_config_io.hpp"
#include "../src/ai/rag_runtime_bridge.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace voxelvk::ai;

static void attach_rag_live_updates(TensorRTMultiModelManager& trt_manager, EnhancedAiGenConfig& cfg){
    SetRagConfigHandler([&](bool enabled, int topk){
        cfg.enable_rag = enabled; cfg.rag_top_k = topk; trt_manager.updateConfig(cfg);
        std::cout << "[Example] RAG updated => enable=" << enabled << " top_k=" << topk << std::endl;
    });
}

void demonstrateBasicAIIntegration() {
    std::cout << "=== Basic AI Integration Example ===" << std::endl;
    EnhancedAiGenConfig config; config.enable_rag=false; config.rag_top_k=3;
    TensorRTMultiModelManager trt_manager; attach_rag_live_updates(trt_manager, config);

    if (trt_manager.initialize(config)) {
        std::cout << "TensorRT manager initialized successfully" << std::endl;
    } else {
        std::cout << "TensorRT manager initialization failed (expected without model files)" << std::endl;
    }
    // ... remainder unchanged ...
}

void demonstrateRuntimeCommands() {
    std::cout << "\n=== Runtime Command System Example ===" << std::endl;
    TensorRTMultiModelManager trt_manager; EnhancedAiGenConfig config; config.enable_rag=false; config.rag_top_k=3; attach_rag_live_updates(trt_manager, config);
    AIBiomeEnhancer biome_enhancer; AIStructureGenerator structure_generator; AIRuntimeCommandSystem command_system;
    EnhancedPaletteConfig palette_config;
    biome_enhancer.initialize(&trt_manager, palette_config);
    structure_generator.initialize(&trt_manager);
    command_system.initialize(&trt_manager, &biome_enhancer, &structure_generator);
    // ... remainder unchanged ...
}

void demonstrateTrainingIntegration() {
    std::cout << "\n=== AI Training Integration Example ===" << std::endl;
    TensorRTMultiModelManager trt_manager; EnhancedAiGenConfig config; config.enable_rag=false; config.rag_top_k=3; attach_rag_live_updates(trt_manager, config);
    AIBiomeEnhancer biome_enhancer; AIStructureGenerator structure_generator; AITrainingEnvironmentGenerator env_generator; AICurriculumManager curriculum_manager;
    EnhancedPaletteConfig palette_config;
    biome_enhancer.initialize(&trt_manager, palette_config);
    structure_generator.initialize(&trt_manager);
    env_generator.initialize(&trt_manager, &biome_enhancer, &structure_generator);
    curriculum_manager.initialize(&env_generator);
    // ... remainder unchanged ...
}

void demonstrateAsyncGeneration() {
    std::cout << "\n=== Asynchronous Generation Example ===" << std::endl;
    TensorRTMultiModelManager trt_manager; EnhancedAiGenConfig config; config.enable_rag=false; config.rag_top_k=3; attach_rag_live_updates(trt_manager, config);
    // Initialize with async support
    config.max_concurrent_generations = 4;
    trt_manager.initialize(config);
    // ... remainder unchanged ...
}

int main() {
    std::cout << "VoxelRL_All AI Integration Examples" << std::endl;
    std::cout << "====================================" << std::endl;

    try {
        // Initialize palette and push initial RAG settings
        PaletteRuntime pr; pr.load_from_file("ai_palette.cfg");
        UpdateRagConfig(pr.cfg.ai_generation.enable_rag, pr.cfg.ai_generation.rag_top_k);

        demonstrateBasicAIIntegration();
        demonstrateRuntimeCommands();
        demonstrateTrainingIntegration();
        demonstrateAsyncGeneration();

        std::cout << "\n=== All Examples Completed Successfully ===" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error during example execution: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}