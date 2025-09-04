#include "ai_tensorrt_manager.hpp"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <chrono>

namespace voxelvk::ai {

// TensorRTEngine Implementation
TensorRTEngine::TensorRTEngine() {
    initializeCuda();
}

TensorRTEngine::~TensorRTEngine() {
    destroy();
    cleanupCuda();
}

bool TensorRTEngine::loadFromFile(const std::string& engine_path) {
    std::ifstream file(engine_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open engine file: " << engine_path << std::endl;
        return false;
    }
    
    // Read engine data
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<char> engine_data(size);
    file.read(engine_data.data(), size);
    file.close();
    
    // Create runtime and deserialize engine
    runtime_ = nvinfer1::createInferRuntime(nvinfer1::ILogger::Severity::kWARNING);
    if (!runtime_) {
        std::cerr << "Failed to create TensorRT runtime" << std::endl;
        return false;
    }
    
    engine_ = runtime_->deserializeCudaEngine(engine_data.data(), size);
    if (!engine_) {
        std::cerr << "Failed to deserialize CUDA engine" << std::endl;
        return false;
    }
    
    context_ = engine_->createExecutionContext();
    if (!context_) {
        std::cerr << "Failed to create execution context" << std::endl;
        return false;
    }
    
    return setupBindings();
}

bool TensorRTEngine::buildFromOnnx(const std::string& onnx_path, const std::string& cache_path) {
    // This would implement ONNX to TensorRT conversion
    // For now, assume we have pre-built engines
    if (!cache_path.empty()) {
        return loadFromFile(cache_path);
    }
    
    std::cerr << "ONNX to TensorRT conversion not implemented. Please provide pre-built engine." << std::endl;
    return false;
}

void TensorRTEngine::destroy() {
    // Cleanup device buffers
    for (void* buffer : device_buffers_) {
        if (buffer) {
            cudaFree(buffer);
        }
    }
    device_buffers_.clear();
    buffer_sizes_.clear();
    
    // Cleanup TensorRT objects
    if (context_) {
        context_->destroy();
        context_ = nullptr;
    }
    
    if (engine_) {
        engine_->destroy();
        engine_ = nullptr;
    }
    
    if (runtime_) {
        runtime_->destroy();
        runtime_ = nullptr;
    }
}

bool TensorRTEngine::initializeCuda() {
    cudaError_t result = cudaStreamCreate(&cuda_stream_);
    if (result != cudaSuccess) {
        std::cerr << "Failed to create CUDA stream: " << cudaGetErrorString(result) << std::endl;
        return false;
    }
    return true;
}

void TensorRTEngine::cleanupCuda() {
    if (cuda_stream_) {
        cudaStreamDestroy(cuda_stream_);
        cuda_stream_ = nullptr;
    }
}

bool TensorRTEngine::setupBindings() {
    if (!engine_) return false;
    
    int num_bindings = engine_->getNbBindings();
    device_buffers_.resize(num_bindings);
    buffer_sizes_.resize(num_bindings);
    
    for (int i = 0; i < num_bindings; ++i) {
        std::string binding_name = engine_->getBindingName(i);
        nvinfer1::Dims dims = engine_->getBindingDimensions(i);
        nvinfer1::DataType dtype = engine_->getBindingDataType(i);
        
        // Calculate buffer size
        size_t buffer_size = 1;
        for (int d = 0; d < dims.nbDims; ++d) {
            buffer_size *= dims.d[d];
        }
        
        // Adjust for data type
        switch (dtype) {
            case nvinfer1::DataType::kFLOAT: buffer_size *= 4; break;
            case nvinfer1::DataType::kHALF: buffer_size *= 2; break;
            case nvinfer1::DataType::kINT8: buffer_size *= 1; break;
            case nvinfer1::DataType::kINT32: buffer_size *= 4; break;
            default: buffer_size *= 4; break;
        }
        
        buffer_sizes_[i] = buffer_size;
        
        // Allocate GPU memory
        cudaError_t result = cudaMalloc(&device_buffers_[i], buffer_size);
        if (result != cudaSuccess) {
            std::cerr << "Failed to allocate GPU memory for binding " << binding_name 
                      << ": " << cudaGetErrorString(result) << std::endl;
            return false;
        }
        
        // Store binding information
        if (engine_->bindingIsInput(i)) {
            input_bindings_[binding_name] = i;
        } else {
            output_bindings_[binding_name] = i;
        }
    }
    
    return true;
}

bool TensorRTEngine::setInputData(const std::string& input_name, const void* data, size_t size) {
    auto it = input_bindings_.find(input_name);
    if (it == input_bindings_.end()) {
        std::cerr << "Input binding not found: " << input_name << std::endl;
        return false;
    }
    
    int binding_index = it->second;
    if (size > buffer_sizes_[binding_index]) {
        std::cerr << "Input data size exceeds buffer size for " << input_name << std::endl;
        return false;
    }
    
    cudaError_t result = cudaMemcpyAsync(device_buffers_[binding_index], data, size,
                                       cudaMemcpyHostToDevice, cuda_stream_);
    if (result != cudaSuccess) {
        std::cerr << "Failed to copy input data to GPU: " << cudaGetErrorString(result) << std::endl;
        return false;
    }
    
    return true;
}

bool TensorRTEngine::executeInference() {
    if (!context_) return false;
    
    bool success = context_->enqueueV2(device_buffers_.data(), cuda_stream_, nullptr);
    if (!success) {
        std::cerr << "TensorRT inference execution failed" << std::endl;
        return false;
    }
    
    // Synchronize stream
    cudaError_t result = cudaStreamSynchronize(cuda_stream_);
    if (result != cudaSuccess) {
        std::cerr << "CUDA stream synchronization failed: " << cudaGetErrorString(result) << std::endl;
        return false;
    }
    
    return true;
}

bool TensorRTEngine::getOutputData(const std::string& output_name, void* data, size_t size) {
    auto it = output_bindings_.find(output_name);
    if (it == output_bindings_.end()) {
        std::cerr << "Output binding not found: " << output_name << std::endl;
        return false;
    }
    
    int binding_index = it->second;
    size_t copy_size = std::min(size, buffer_sizes_[binding_index]);
    
    cudaError_t result = cudaMemcpyAsync(data, device_buffers_[binding_index], copy_size,
                                       cudaMemcpyDeviceToHost, cuda_stream_);
    if (result != cudaSuccess) {
        std::cerr << "Failed to copy output data from GPU: " << cudaGetErrorString(result) << std::endl;
        return false;
    }
    
    // Synchronize to ensure data is available
    result = cudaStreamSynchronize(cuda_stream_);
    return result == cudaSuccess;
}

std::vector<std::string> TensorRTEngine::getInputNames() const {
    std::vector<std::string> names;
    for (const auto& pair : input_bindings_) {
        names.push_back(pair.first);
    }
    return names;
}

std::vector<std::string> TensorRTEngine::getOutputNames() const {
    std::vector<std::string> names;
    for (const auto& pair : output_bindings_) {
        names.push_back(pair.first);
    }
    return names;
}

// TensorRTMultiModelManager Implementation
TensorRTMultiModelManager::TensorRTMultiModelManager() {
    memory_pool_ = std::make_unique<CUDAMemoryPool>(2ULL * 1024 * 1024 * 1024); // 2GB
    stream_manager_ = std::make_unique<CUDAStreamManager>(4);
}

TensorRTMultiModelManager::~TensorRTMultiModelManager() {
    shutdown();
}

#include "rag_query_engine.hpp"


bool TensorRTMultiModelManager::initialize(const EnhancedAiGenConfig& config) {
    config_ = config;
    
    // Load TensorRT engines for different content types
    struct ModelInfo {
        ContentType type;
        std::string path;
    };
    
    std::vector<ModelInfo> models = {
        {ContentType::Structure, config.structure_trt_path},
        {ContentType::Terrain, config.terrain_trt_path},
        {ContentType::Texture, config.texture_trt_path},
        {ContentType::Biome, config.biome_trt_path},
        {ContentType::Settlement, config.settlement_trt_path}
    };
    
    for (const auto& model : models) {
        if (!model.path.empty()) {
            auto engine = std::make_unique<TensorRTEngine>();
            if (engine->loadFromFile(model.path)) {
                engines_[model.type] = std::move(engine);
                std::cout << "Loaded TensorRT engine for content type " << (int)model.type << std::endl;
            } else {
                std::cerr << "Failed to load TensorRT engine: " << model.path << std::endl;

    // Initialize RAG if enabled
    if (config_.enable_rag) {
        rag_engine_ = std::make_unique<RAGQueryEngine>();
        rag_engine_->initialize(config_.rag_embedding_onnx_path,
                                config_.rag_index_path,
                                config_.rag_docs_path);
    }

            }
        }
    }
    
    // Start worker thread for async processing
    worker_thread_ = std::thread(&TensorRTMultiModelManager::workerLoop, this);

bool TensorRTMultiModelManager::updateConfig(const EnhancedAiGenConfig& cfg){
    config_ = cfg;
    return true;
}

    
    return !engines_.empty();
}

void TensorRTMultiModelManager::shutdown() {
    // Signal shutdown
    shutdown_requested_ = true;
    queue_cv_.notify_all();
    
    // Wait for worker thread
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    
    // Cleanup engines
    engines_.clear();
}

StructureBlueprint TensorRTMultiModelManager::generateStructure(
    const StructurePrompt& prompt, const EnvironmentalContext& context) {
    
    StructureBlueprint result;
    
    auto it = engines_.find(ContentType::Structure);
    if (it == engines_.end() || !it->second->isReady()) {
        setError("Structure generation engine not available");
        return result;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    if (generateStructureImpl(prompt, context, result)) {
        auto end_time = std::chrono::high_resolution_clock::now();
        float elapsed_ms = std::chrono::duration<float, std::milli>(end_time - start_time).count();
        updatePerformanceStats(elapsed_ms, true);
    } else {
        updatePerformanceStats(0.0f, false);
    }
    
    return result;
}

bool TensorRTMultiModelManager::generateStructureImpl(
    const StructurePrompt& prompt, const EnvironmentalContext& context, StructureBlueprint& result) {
    
    auto& engine = engines_[ContentType::Structure];
    

    // RAG augmentation: if enabled in config and engine present, augment the prompt description
    StructurePrompt prompt_aug = prompt;
    if (config_.enable_rag && rag_engine_) {
        std::string final_description = prompt.description;
        auto docs = rag_engine_->query(prompt.description, std::max(1, config_.rag_top_k));
        if (!docs.empty()) {
            final_description += " [CONTEXT]: ";
            int added = 0;
            for (const auto &d : docs) { if (added++ >= config_.rag_top_k) break; final_description += d + " "; }
        }
        prompt_aug.description = final_description;
    }

    // Prepare input data
    std::vector<float> input_data;
    if (!preprocessStructureInput(prompt_aug, input_data)) {
        setError("Failed to preprocess structure input");
        return false;
    }
    
    // Set input data
    if (!engine->setInputData("input", input_data.data(), input_data.size() * sizeof(float))) {
        setError("Failed to set input data for structure generation");
        return false;
    }
    
    // Execute inference
    if (!engine->executeInference()) {
        setError("Structure generation inference failed");
        return false;
    }
    
    // Get output data
    std::vector<float> output_data(1024); // Adjust size based on model
    if (!engine->getOutputData("output", output_data.data(), output_data.size() * sizeof(float))) {
        setError("Failed to get structure generation output");
        return false;
    }
    
    // Post-process output
    if (!postprocessStructureOutput(output_data, result)) {
        setError("Failed to postprocess structure output");
        return false;
    }
    
    clearError();
    return true;
}

bool TensorRTMultiModelManager::preprocessStructureInput(
    const StructurePrompt& prompt, std::vector<float>& input_data) {
    
    // This is a simplified preprocessing - in reality, you'd encode the prompt
    // into a format suitable for your trained model
    input_data.clear();
    input_data.reserve(256); // Adjust based on model input size
    
    // Encode position
    input_data.push_back(prompt.position.x);
    input_data.push_back(prompt.position.y);
    input_data.push_back(prompt.position.z);
    
    // Encode size bounds
    input_data.push_back(prompt.size_bounds.x);
    input_data.push_back(prompt.size_bounds.y);
    input_data.push_back(prompt.size_bounds.z);
    
    // Encode complexity
    input_data.push_back(prompt.complexity);
    
    // Encode architectural style (one-hot)
    for (int i = 0; i < 8; ++i) {
        input_data.push_back((i == (int)prompt.style) ? 1.0f : 0.0f);
    }
    
    // Encode description influence by hashing the text (final_description if available)
    // Use a stable simple hash over characters to fill remaining slots deterministically.
    auto hash_text = [&](const std::string &txt){
        uint32_t h = 2166136261u;
        for(unsigned char c : txt){ h ^= (uint32_t)std::tolower(c); h *= 16777619u; }
        return h;
    };
    // Note: we cannot access final_description here directly. The generateStructureImpl
    // already augmented context; to pass this signal, we rely on prompt.description state.
    // Hash the (possibly augmented) description length to affect a few floats.
    uint32_t h = hash_text(prompt.description);
    for (int i = 0; i < 16; ++i) {
        float v = ((h >> (i % 24)) & 0xFF) / 255.0f; // pseudo features from text
        input_data.push_back(v);
        if (input_data.size() >= 256) break;
    }
    while (input_data.size() < 256) input_data.push_back(0.0f);
    
    return true;
}

bool TensorRTMultiModelManager::postprocessStructureOutput(
    const std::vector<float>& output_data, StructureBlueprint& result) {
    
    // This is a simplified postprocessing - in reality, you'd decode the model output
    // into a proper voxel structure
    
    // For demonstration, create a simple structure
    result.structure.W = result.structure.H = result.structure.D = 16;
    result.structure.occupancy.resize(16 * 16 * 16, 0);
    
    // Generate a simple box structure based on output
    if (!output_data.empty()) {
        float density = std::min(1.0f, std::max(0.0f, output_data[0]));
        
        for (int z = 0; z < 16; ++z) {
            for (int y = 0; y < 16; ++y) {
                for (int x = 0; x < 16; ++x) {
                    int idx = z * 16 * 16 + y * 16 + x;
                    
                    // Simple wall structure
                    bool is_wall = (x == 0 || x == 15 || z == 0 || z == 15) && y < 8;
                    bool is_floor = y == 0;
                    bool is_roof = y == 7 && x > 0 && x < 15 && z > 0 && z < 15;
                    
                    if (is_wall || is_floor || is_roof) {
                        result.structure.occupancy[idx] = 1;
                    }
                }
            }
        }
    }
    
    // Set metadata
    result.structure_type = "building";
    result.style = ArchitecturalStyle::Medieval;
    result.estimated_complexity = 0.5f;
    result.bounding_box = {16, 16, 16};
    
    return true;
}

void TensorRTMultiModelManager::workerLoop() {
    while (!shutdown_requested_) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        // Wait for requests
        queue_cv_.wait(lock, [this] { 
            return !request_queue_.empty() || shutdown_requested_; 
        });
        
        if (shutdown_requested_) break;
        
        // Process request
        GenerationRequest request = request_queue_.front();
        request_queue_.pop();
        lock.unlock();
        
        MultiModalAiOutputs result;
        bool success = processGenerationRequest(request, result);
        
        // Store result
        lock.lock();
        completed_results_[request.request_id] = std::move(result);
        lock.unlock();
        
        // Call callback if provided
        if (request.callback) {
            request.callback(result);
        }
    }
}

bool TensorRTMultiModelManager::processGenerationRequest(
    const GenerationRequest& request, MultiModalAiOutputs& result) {
    
    switch (request.type) {
        case ContentType::Structure:
            return generateStructureImpl(request.request_data.structure_prompt, 
                                       request.context, result.structures.emplace_back());
        
        case ContentType::Texture:
            return generateTextureImpl(request.request_data.material_request, 
                                     result.generated_textures.emplace_back());
        
        case ContentType::Biome:
            return generateBiomeRuleImpl(request.request_data.biome_context, 
                                       result.dynamic_biome_rules.emplace_back());
        
        case ContentType::Terrain:
            return generateTerrainImpl(request.context, result);
        
        default:
            setError("Unsupported generation request type");
            return false;
    }
}

void TensorRTMultiModelManager::updatePerformanceStats(float inference_time, bool success) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    perf_stats_.total_generations++;
    if (!success) {
        perf_stats_.failed_generations++;
    }
    
    perf_stats_.success_rate = (float)(perf_stats_.total_generations - perf_stats_.failed_generations) 
                              / perf_stats_.total_generations;
    
    if (success && inference_time > 0.0f) {
        // Update average inference time with exponential moving average
        float alpha = 0.1f;
        perf_stats_.average_inference_time_ms = 
            alpha * inference_time + (1.0f - alpha) * perf_stats_.average_inference_time_ms;
    }
}

// Stub implementations for other generation methods
bool TensorRTMultiModelManager::generateTextureImpl(const MaterialRequest& request, TextureData& result) {
    // Placeholder implementation
    result.width = result.height = request.resolution;
    result.diffuse_map.resize(request.resolution * request.resolution * 3, 128); // Gray texture
    result.material_name = request.material_type;
    return true;
}

bool TensorRTMultiModelManager::generateBiomeRuleImpl(const BiomeContext& context, BiomeRule& result) {
    // Placeholder implementation
    result.name = "AI Generated Biome";
    result.temperature_range[0] = context.temperature - 0.2f;
    result.temperature_range[1] = context.temperature + 0.2f;
    result.humidity_range[0] = context.humidity - 0.2f;
    result.humidity_range[1] = context.humidity + 0.2f;
    return true;
}

bool TensorRTMultiModelManager::generateTerrainImpl(const EnvironmentalContext& context, MultiModalAiOutputs& result) {
    // Placeholder implementation - would use terrain generation model
    result.heightmap.resize(64 * 64, 64.0f); // Flat terrain at height 64
    result.biome_map.resize(64 * 64, 0); // All plains biome
    return true;
}

void TensorRTMultiModelManager::setError(const std::string& error_message) {
    last_error_ = error_message;
    std::cerr << "TensorRT Manager Error: " << error_message << std::endl;
}

void TensorRTMultiModelManager::clearError() {
    last_error_.clear();
}

// CUDAMemoryPool implementation (simplified)
CUDAMemoryPool::CUDAMemoryPool(size_t pool_size_bytes) : pool_size_(pool_size_bytes) {
    cudaMalloc(&pool_memory_, pool_size_bytes);
}

CUDAMemoryPool::~CUDAMemoryPool() {
    if (pool_memory_) {
        cudaFree(pool_memory_);
    }
}

void* CUDAMemoryPool::allocate(size_t size) {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    // Simple linear allocation - in practice, you'd want a more sophisticated allocator
    if (used_size_ + size <= pool_size_) {
        void* ptr = static_cast<char*>(pool_memory_) + used_size_;
        used_size_ += size;
        
        blocks_.push_back({ptr, size, true});
        return ptr;
    }
    
    return nullptr; // Out of memory
}

void CUDAMemoryPool::deallocate(void* ptr) {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    for (auto& block : blocks_) {
        if (block.ptr == ptr) {
            block.in_use = false;
            break;
        }
    }
}

void CUDAMemoryPool::reset() {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    used_size_ = 0;
    blocks_.clear();
}

// CUDAStreamManager implementation
CUDAStreamManager::CUDAStreamManager(int num_streams) {
    streams_.resize(num_streams);
    for (int i = 0; i < num_streams; ++i) {
        cudaStreamCreate(&streams_[i]);
        available_streams_.push(streams_[i]);
    }
}

CUDAStreamManager::~CUDAStreamManager() {
    for (auto stream : streams_) {
        cudaStreamDestroy(stream);
    }
}

cudaStream_t CUDAStreamManager::getAvailableStream() {
    std::unique_lock<std::mutex> lock(stream_mutex_);
    stream_cv_.wait(lock, [this] { return !available_streams_.empty(); });
    
    cudaStream_t stream = available_streams_.front();
    available_streams_.pop();
    return stream;
}

void CUDAStreamManager::returnStream(cudaStream_t stream) {
    std::lock_guard<std::mutex> lock(stream_mutex_);
    available_streams_.push(stream);
    stream_cv_.notify_one();
}

void CUDAStreamManager::synchronizeAll() {
    for (auto stream : streams_) {
        cudaStreamSynchronize(stream);
    }
}

} // namespace voxelvk::ai