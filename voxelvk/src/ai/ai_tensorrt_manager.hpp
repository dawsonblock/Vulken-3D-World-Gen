#pragma once
#include "ai_enhanced_generator.hpp"
#include <NvInfer.h>
#include <NvInferRuntime.h>
#include <cuda_runtime.h>
#include <memory>
#include <unordered_map>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace voxelvk::ai {

// TensorRT engine wrapper for individual models
class TensorRTEngine {
public:
    TensorRTEngine();
    ~TensorRTEngine();
    
    // Engine management
    bool loadFromFile(const std::string& engine_path);
    bool buildFromOnnx(const std::string& onnx_path, const std::string& cache_path = "");
    void destroy();
    
    // Inference
    bool setInputData(const std::string& input_name, const void* data, size_t size);
    bool executeInference();
    bool getOutputData(const std::string& output_name, void* data, size_t size);
    
    // Utilities
    bool isReady() const { return engine_ != nullptr && context_ != nullptr; }
    size_t getInputSize(const std::string& input_name) const;
    size_t getOutputSize(const std::string& output_name) const;
    std::vector<std::string> getInputNames() const;
    std::vector<std::string> getOutputNames() const;
    
private:
    nvinfer1::IRuntime* runtime_ = nullptr;
    nvinfer1::ICudaEngine* engine_ = nullptr;
    nvinfer1::IExecutionContext* context_ = nullptr;
    
    std::unordered_map<std::string, int> input_bindings_;
    std::unordered_map<std::string, int> output_bindings_;
    std::vector<void*> device_buffers_;
    std::vector<size_t> buffer_sizes_;
    
    // CUDA resources
    cudaStream_t cuda_stream_ = nullptr;
    
    bool initializeCuda();
    void cleanupCuda();
    bool setupBindings();
};

// Memory pool for efficient GPU memory management
class CUDAMemoryPool {
public:
    CUDAMemoryPool(size_t pool_size_bytes);
    ~CUDAMemoryPool();
    
    void* allocate(size_t size);
    void deallocate(void* ptr);
    void reset(); // Reset all allocations
    
    size_t getTotalSize() const { return pool_size_; }
    size_t getUsedSize() const { return used_size_; }
    size_t getAvailableSize() const { return pool_size_ - used_size_; }
    
private:
    struct MemoryBlock {
        void* ptr;
        size_t size;
        bool in_use;
    };
    
    void* pool_memory_ = nullptr;
    size_t pool_size_;
    size_t used_size_ = 0;
    std::vector<MemoryBlock> blocks_;
    std::mutex pool_mutex_;
};

// CUDA stream manager for parallel operations
class CUDAStreamManager {
public:
    CUDAStreamManager(int num_streams = 4);
    ~CUDAStreamManager();
    
    cudaStream_t getAvailableStream();
    void returnStream(cudaStream_t stream);
    void synchronizeAll();
    
private:
    std::vector<cudaStream_t> streams_;
    std::queue<cudaStream_t> available_streams_;
    std::mutex stream_mutex_;
    std::condition_variable stream_cv_;
};

// Async generation request
struct GenerationRequest {
    ContentType type;
    std::string prompt;
    EnvironmentalContext context;
    std::function<void(const MultiModalAiOutputs&)> callback;
    bool high_priority = false;
    
    // Request-specific data
    union {
        StructurePrompt structure_prompt;
        MaterialRequest material_request;
        BiomeContext biome_context;
    } request_data;
    
    // Generation ID for tracking
    uint64_t request_id;
    std::chrono::steady_clock::time_point submitted_time;
};

// Main TensorRT multi-model manager
class TensorRTMultiModelManager {
public:
    TensorRTMultiModelManager();
    ~TensorRTMultiModelManager();
    
    // Initialization
    bool initialize(const EnhancedAiGenConfig& config);
    void shutdown();
    
    // Synchronous generation methods
    StructureBlueprint generateStructure(const StructurePrompt& prompt, 
                                       const EnvironmentalContext& context = {});
    std::vector<TextureData> generateTextures(const std::vector<MaterialRequest>& requests);
    BiomeRule generateBiomeRule(const BiomeContext& context);
    MultiModalAiOutputs generateTerrain(const EnvironmentalContext& context);
    
    // Batch processing for performance
    std::vector<StructureBlueprint> generateStructures(const std::vector<StructurePrompt>& prompts);
    std::vector<BiomeRule> generateBiomeRules(const std::vector<BiomeContext>& contexts);
    
    // Asynchronous generation
    uint64_t generateAsync(const GenerationRequest& request);
    bool isGenerationComplete(uint64_t request_id);
    bool getGenerationResult(uint64_t request_id, MultiModalAiOutputs& result);
    void cancelGeneration(uint64_t request_id);
    
    // Performance monitoring
    struct PerformanceStats {
        float average_inference_time_ms = 0.0f;
        float gpu_utilization_percent = 0.0f;
        size_t memory_usage_bytes = 0;
        uint64_t total_generations = 0;
        uint64_t failed_generations = 0;
        float success_rate = 1.0f;
    };
    
    PerformanceStats getPerformanceStats() const { return perf_stats_; }
    void resetPerformanceStats();
    
    // Configuration management
    bool updateConfig(const EnhancedAiGenConfig& config);
    const EnhancedAiGenConfig& getConfig() const { return config_; }
    

    // RAG configuration helpers
    void enableRAG(std::unique_ptr<RAGQueryEngine> engine) { rag_engine_ = std::move(engine); }

    // Model management
    bool loadModel(ContentType type, const std::string& model_path);
    bool unloadModel(ContentType type);
    bool isModelLoaded(ContentType type) const;
    std::vector<ContentType> getLoadedModels() const;
    
    // Error handling
    std::string getLastError() const { return last_error_; }
    bool hasError() const { return !last_error_.empty(); }
    
private:

    // RAG engine
    class RAGQueryEngine; // fwd decl
    std::unique_ptr<RAGQueryEngine> rag_engine_;

    EnhancedAiGenConfig config_;
    
    // TensorRT engines for different content types
    std::unordered_map<ContentType, std::unique_ptr<TensorRTEngine>> engines_;
    
    // CUDA resources
    std::unique_ptr<CUDAMemoryPool> memory_pool_;
    std::unique_ptr<CUDAStreamManager> stream_manager_;
    
    // Async processing
    std::thread worker_thread_;
    std::queue<GenerationRequest> request_queue_;
    std::unordered_map<uint64_t, MultiModalAiOutputs> completed_results_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    bool shutdown_requested_ = false;
    uint64_t next_request_id_ = 1;
    
    // Performance tracking
    mutable PerformanceStats perf_stats_;
    mutable std::mutex stats_mutex_;
    
    // Error handling
    std::string last_error_;
    
    // Internal methods
    void workerLoop();
    bool processGenerationRequest(const GenerationRequest& request, MultiModalAiOutputs& result);
    
    // Generation implementations
    bool generateStructureImpl(const StructurePrompt& prompt, 
                              const EnvironmentalContext& context,
                              StructureBlueprint& result);
    bool generateTextureImpl(const MaterialRequest& request, TextureData& result);
    bool generateBiomeRuleImpl(const BiomeContext& context, BiomeRule& result);
    bool generateTerrainImpl(const EnvironmentalContext& context, MultiModalAiOutputs& result);
    
    // Utilities
    void updatePerformanceStats(float inference_time, bool success);
    bool validateInputs(const GenerationRequest& request);
    void setError(const std::string& error_message);
    void clearError();
    
    // Model-specific preprocessing/postprocessing
    bool preprocessStructureInput(const StructurePrompt& prompt, std::vector<float>& input_data);
    bool postprocessStructureOutput(const std::vector<float>& output_data, StructureBlueprint& result);
    bool preprocessTextureInput(const MaterialRequest& request, std::vector<float>& input_data);
    bool postprocessTextureOutput(const std::vector<float>& output_data, TextureData& result);
    bool preprocessBiomeInput(const BiomeContext& context, std::vector<float>& input_data);
    bool postprocessBiomeOutput(const std::vector<float>& output_data, BiomeRule& result);
};

// Utility functions for TensorRT integration
namespace tensorrt_utils {
    // Model conversion utilities
    bool convertOnnxToTensorRT(const std::string& onnx_path, 
                              const std::string& trt_path,
                              bool use_fp16 = true,
                              int max_batch_size = 1);
    
    // Performance optimization
    bool optimizeEngineForHardware(const std::string& engine_path);
    
    // Model validation
    bool validateModelCompatibility(const std::string& model_path, ContentType expected_type);
    
    // Hardware detection
    struct HardwareInfo {
        std::string gpu_name;
        int compute_capability_major;
        int compute_capability_minor;
        size_t total_memory_bytes;
        size_t free_memory_bytes;
        bool supports_fp16;
        bool supports_int8;
    };
    
    HardwareInfo detectHardware();
    bool isHardwareCompatible(const HardwareInfo& info);
}

} // namespace voxelvk::ai