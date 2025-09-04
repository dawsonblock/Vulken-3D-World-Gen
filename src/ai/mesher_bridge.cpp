/**
 * AI Mesher Bridge Implementation
 * ===============================
 */

#include "mesher_bridge.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>

namespace VulkenAI {

    std::unique_ptr<MesherBridge> createMesherBridge(const std::string& configPath) {
        // Check if AI is enabled in configuration
        bool aiEnabled = false;
        std::string modelPath;
        
        if (!configPath.empty() && std::filesystem::exists(configPath)) {
            // In a real implementation, this would parse YAML config
            // For now, just check if models directory exists
            std::filesystem::path modelsDir = "models";
            aiEnabled = std::filesystem::exists(modelsDir);
        }

#ifdef ENABLE_AI_TRT
        if (aiEnabled) {
            std::cout << "🤖 Initializing AI-enhanced mesher bridge..." << std::endl;
            
            auto bridge = std::make_unique<TensorRTBridge>(modelPath);
            if (bridge->isAIEnabled()) {
                return std::move(bridge);
            } else {
                std::cout << "⚠️  AI models failed to load, falling back to metrics-only mode" << std::endl;
            }
        }
#endif

        std::cout << "📊 Using metrics-only mesher bridge" << std::endl;
        return std::make_unique<MetricsOnlyBridge>();
    }

    namespace Utils {
        bool isAIRuntimeAvailable() {
#ifdef ENABLE_AI_TRT
            return true;
#else
            return false;
#endif
        }
        
        std::vector<std::string> getAvailableModels(const std::string& modelsPath) {
            std::vector<std::string> models;
            
            if (!std::filesystem::exists(modelsPath)) {
                return models;
            }
            
            // Scan for model files
            for (const auto& entry : std::filesystem::directory_iterator(modelsPath)) {
                if (entry.is_regular_file()) {
                    auto extension = entry.path().extension().string();
                    if (extension == ".trt" || extension == ".onnx" || extension == ".engine") {
                        models.push_back(entry.path().filename().string());
                    }
                }
            }
            
            return models;
        }
        
        bool validateModelFile(const std::string& modelPath) {
            if (!std::filesystem::exists(modelPath)) {
                return false;
            }
            
            // Basic file validation
            std::filesystem::path path(modelPath);
            auto size = std::filesystem::file_size(path);
            
            // Model files should be at least 1MB
            return size > 1024 * 1024;
        }
        
        AICapabilities detectAICapabilities() {
            AICapabilities caps;
            
#ifdef ENABLE_AI_TRT
            caps.tensorrtSupport = true;
            caps.tensorrtVersion = "8.6.0";  // Would query actual version
#endif

#ifdef ENABLE_CUDA
            caps.cudaSupport = true;
            caps.cudaVersion = "11.8";  // Would query actual version
#endif

            // ONNX support detection would go here
            
            return caps;
        }
    }

} // namespace VulkenAI