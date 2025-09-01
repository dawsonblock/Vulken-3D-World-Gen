#include "ai_runtime_commands.hpp"
#include "ai_tensorrt_manager.hpp"
#include "rag_runtime_bridge.hpp"
#include <cstdio>

namespace voxelvk::ai {

bool AIRuntimeCommandSystem::initialize(TensorRTMultiModelManager* trt_manager,
                                        const EnhancedPaletteConfig& palette_config) {
    trt_manager_ = trt_manager;
    generation_config = EnhancedAiGenConfig{};
    // Default RAG wiring via bridge to update manager
    SetRagConfigHandler([this](bool enabled, int topk){
        if(!trt_manager_) return;
        generation_config.enable_rag = enabled;
        generation_config.rag_top_k = topk;
        trt_manager_->updateConfig(generation_config);
        std::printf("[AIRuntime] RAG updated: enable=%d top_k=%d\n", (int)enabled, topk);
    });
    return trt_manager_ != nullptr;
}

uint64_t AIRuntimeCommandSystem::generateStructureAt(const glm::vec3& position,
                                                     const std::string& description,
                                                     ArchitecturalStyle style,
                                                     float complexity,
                                                     bool use_rag) {
    if (!trt_manager_) return 0;
    generation_config.enable_rag = use_rag;
    trt_manager_->updateConfig(generation_config);
    // ... rest of the implementation would enqueue a request and return an id
    return 1; // placeholder id
}

} // ns