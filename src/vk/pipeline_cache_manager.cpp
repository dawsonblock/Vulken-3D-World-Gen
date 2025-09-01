#include "pipeline_cache_manager.hpp"
#include "error_handling.hpp"
#include "../core/logger.hpp"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <cstring>

namespace voxelvk {

static Logger g_cacheLogger("PipelineCache");

std::string PipelineCacheManager::s_buildUuid = "";

uint64_t PipelineSpec::getHash() const {
    // Simple hash combining key fields
    uint64_t hash = 0;
    
    // Hash SPIR-V
    for (uint32_t code : vertexSpirv) hash ^= code + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    for (uint32_t code : fragmentSpirv) hash ^= code + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    for (uint32_t code : computeSpirv) hash ^= code + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    
    // Hash render state
    hash ^= reinterpret_cast<uintptr_t>(renderPass) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= subpass + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= static_cast<uint64_t>(polygonMode) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= static_cast<uint64_t>(cullMode) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= static_cast<uint64_t>(depthCompareOp) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    
    return hash;
}

uint64_t ComputePipelineSpec::getHash() const {
    uint64_t hash = 0;
    for (uint32_t code : computeSpirv) {
        hash ^= code + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    hash ^= reinterpret_cast<uintptr_t>(layout) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    return hash;
}

bool PipelineCacheHeader::isValid(uint32_t currentBuildHash, const char* currentUuid) const {
    if (magic != MAGIC || version != VERSION) {
        return false;
    }
    
    if (buildHash != currentBuildHash) {
        return false;
    }
    
    if (std::memcmp(buildUuid, currentUuid, 16) != 0) {
        return false;
    }
    
    return true;
}

PipelineCacheManager::PipelineCacheManager(VkDevice device, VkPhysicalDevice physicalDevice)
    : device_(device), physicalDevice_(physicalDevice) {
    
    vkGetPhysicalDeviceProperties(physicalDevice_, &deviceProperties_);
    
    g_cacheLogger.Info("PipelineCacheManager initialized for device: {}", deviceProperties_.deviceName);
}

PipelineCacheManager::~PipelineCacheManager() {
    shutdown();
}

bool PipelineCacheManager::initialize(const std::string& cacheDirectory) {
    cacheDirectory_ = cacheDirectory;
    cacheFilePath_ = getCacheFilePath();
    
    if (!createCacheDirectory()) {
        g_cacheLogger.Error("Failed to create cache directory: {}", cacheDirectory_);
        return false;
    }
    
    // Create Vulkan pipeline cache
    VkPipelineCacheCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    
    // Try to load existing cache
    std::vector<char> cacheData;
    if (loadFromDisk()) {
        // Cache loaded successfully
        createInfo.initialDataSize = cacheData.size();
        createInfo.pInitialData = cacheData.data();
    }
    
    VkResult result = vkCreatePipelineCache(device_, &createInfo, nullptr, &vkCache_);
    if (result != VK_SUCCESS) {
        CHECK_VK_OBJECT(result, VkErrorCategory::PIPELINE_CREATION, "pipeline_cache");
        return false;
    }
    
    VK_OBJECT_NAME(device_, vkCache_, VK_OBJECT_TYPE_PIPELINE_CACHE, "MainPipelineCache");
    
    g_cacheLogger.Info("Pipeline cache initialized: {}", cacheFilePath_);
    return true;
}

void PipelineCacheManager::shutdown() {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    
    // Save cache to disk
    saveToDisk();
    
    // Destroy pipelines
    for (auto& [name, pipeline] : graphicsPipelines_) {
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device_, pipeline, nullptr);
        }
    }
    graphicsPipelines_.clear();
    
    for (auto& [name, pipeline] : computePipelines_) {
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device_, pipeline, nullptr);
        }
    }
    computePipelines_.clear();
    
    pipelineHashMap_.clear();
    
    // Destroy cache
    if (vkCache_ != VK_NULL_HANDLE) {
        vkDestroyPipelineCache(device_, vkCache_, nullptr);
        vkCache_ = VK_NULL_HANDLE;
    }
    
    g_cacheLogger.Info("Pipeline cache shutdown complete");
}

VkPipeline PipelineCacheManager::getOrCreateGraphicsPipeline(const std::string& name, const PipelineSpec& spec) {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    
    // Check name-based cache first
    auto nameIt = graphicsPipelines_.find(name);
    if (nameIt != graphicsPipelines_.end()) {
        stats_.cacheHits++;
        return nameIt->second;
    }
    
    // Check hash-based cache
    uint64_t hash = spec.getHash();
    auto hashIt = pipelineHashMap_.find(hash);
    if (hashIt != pipelineHashMap_.end()) {
        stats_.cacheHits++;
        graphicsPipelines_[name] = hashIt->second;
        return hashIt->second;
    }
    
    // Create new pipeline
    auto startTime = std::chrono::high_resolution_clock::now();
    
    VkPipeline pipeline = createGraphicsPipeline(spec);
    if (pipeline == VK_NULL_HANDLE) {
        stats_.compilationErrors++;
        g_cacheLogger.Error("Failed to create graphics pipeline: {}", name);
        return VK_NULL_HANDLE;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    double compileTime = std::chrono::duration<double>(endTime - startTime).count();
    stats_.totalCompileTime += compileTime;
    stats_.cacheMisses++;
    
    // Store in caches
    graphicsPipelines_[name] = pipeline;
    pipelineHashMap_[hash] = pipeline;
    
    VK_OBJECT_NAME(device_, pipeline, VK_OBJECT_TYPE_PIPELINE, name.c_str());
    
    g_cacheLogger.Info("Created graphics pipeline '{}' in {:.3f}ms", name, compileTime * 1000.0);
    return pipeline;
}

VkPipeline PipelineCacheManager::getOrCreateComputePipeline(const std::string& name, const ComputePipelineSpec& spec) {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    
    // Check name-based cache first
    auto nameIt = computePipelines_.find(name);
    if (nameIt != computePipelines_.end()) {
        stats_.cacheHits++;
        return nameIt->second;
    }
    
    // Check hash-based cache
    uint64_t hash = spec.getHash();
    auto hashIt = pipelineHashMap_.find(hash);
    if (hashIt != pipelineHashMap_.end()) {
        stats_.cacheHits++;
        computePipelines_[name] = hashIt->second;
        return hashIt->second;
    }
    
    // Create new pipeline
    auto startTime = std::chrono::high_resolution_clock::now();
    
    VkPipeline pipeline = createComputePipeline(spec);
    if (pipeline == VK_NULL_HANDLE) {
        stats_.compilationErrors++;
        g_cacheLogger.Error("Failed to create compute pipeline: {}", name);
        return VK_NULL_HANDLE;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    double compileTime = std::chrono::duration<double>(endTime - startTime).count();
    stats_.totalCompileTime += compileTime;
    stats_.cacheMisses++;
    
    // Store in caches
    computePipelines_[name] = pipeline;
    pipelineHashMap_[hash] = pipeline;
    
    VK_OBJECT_NAME(device_, pipeline, VK_OBJECT_TYPE_COMPUTE_PIPELINE, name.c_str());
    
    g_cacheLogger.Info("Created compute pipeline '{}' in {:.3f}ms", name, compileTime * 1000.0);
    return pipeline;
}

bool PipelineCacheManager::loadFromDisk() {
    if (!std::filesystem::exists(cacheFilePath_)) {
        g_cacheLogger.Info("No existing cache file found: {}", cacheFilePath_);
        return false;
    }
    
    std::ifstream file(cacheFilePath_, std::ios::binary);
    if (!file.is_open()) {
        g_cacheLogger.Warn("Failed to open cache file: {}", cacheFilePath_);
        return false;
    }
    
    // Read and validate header
    PipelineCacheHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    
    if (!file.good()) {
        g_cacheLogger.Warn("Failed to read cache header");
        return false;
    }
    
    uint32_t currentBuildHash = calculateBuildHash();
    if (!header.isValid(currentBuildHash, s_buildUuid.c_str())) {
        g_cacheLogger.Info("Cache header validation failed, invalidating cache");
        std::filesystem::remove(cacheFilePath_);
        return false;
    }
    
    // Read cache data
    file.seekg(0, std::ios::end);
    size_t totalSize = file.tellg();
    size_t cacheDataSize = totalSize - sizeof(header);
    
    if (cacheDataSize == 0) {
        g_cacheLogger.Info("Cache file is empty");
        return false;
    }
    
    file.seekg(sizeof(header));
    std::vector<char> cacheData(cacheDataSize);
    file.read(cacheData.data(), cacheDataSize);
    
    if (!file.good()) {
        g_cacheLogger.Warn("Failed to read cache data");
        return false;
    }
    
    g_cacheLogger.Info("Loaded pipeline cache: {} bytes", cacheDataSize);
    return true;
}

bool PipelineCacheManager::saveToDisk() {
    if (vkCache_ == VK_NULL_HANDLE) {
        return false;
    }
    
    // Get cache data from Vulkan
    size_t cacheSize = 0;
    VkResult result = vkGetPipelineCacheData(device_, vkCache_, &cacheSize, nullptr);
    if (result != VK_SUCCESS || cacheSize == 0) {
        g_cacheLogger.Warn("No cache data to save");
        return false;
    }
    
    std::vector<char> cacheData(cacheSize);
    result = vkGetPipelineCacheData(device_, vkCache_, &cacheSize, cacheData.data());
    if (result != VK_SUCCESS) {
        g_cacheLogger.Error("Failed to retrieve cache data");
        return false;
    }
    
    // Write to file with header
    std::ofstream file(cacheFilePath_, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        g_cacheLogger.Error("Failed to open cache file for writing: {}", cacheFilePath_);
        return false;
    }
    
    // Write header
    PipelineCacheHeader header;
    header.buildHash = calculateBuildHash();
    header.driverVersion = deviceProperties_.driverVersion;
    header.vendorID = deviceProperties_.vendorID;
    header.deviceID = deviceProperties_.deviceID;
    std::memcpy(header.buildUuid, s_buildUuid.c_str(), std::min(s_buildUuid.size(), size_t(16)));
    
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file.write(cacheData.data(), cacheSize);
    
    if (!file.good()) {
        g_cacheLogger.Error("Failed to write cache data");
        return false;
    }
    
    g_cacheLogger.Info("Saved pipeline cache: {} bytes", cacheSize);
    return true;
}

void PipelineCacheManager::invalidateCache() {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    
    g_cacheLogger.Info("Invalidating pipeline cache");
    
    if (std::filesystem::exists(cacheFilePath_)) {
        std::filesystem::remove(cacheFilePath_);
    }
    
    // Clear runtime caches
    graphicsPipelines_.clear();
    computePipelines_.clear();
    pipelineHashMap_.clear();
}

size_t PipelineCacheManager::getCacheSize() const {
    if (vkCache_ == VK_NULL_HANDLE) return 0;
    
    size_t cacheSize = 0;
    vkGetPipelineCacheData(device_, vkCache_, &cacheSize, nullptr);
    return cacheSize;
}

VkPipeline PipelineCacheManager::createGraphicsPipeline(const PipelineSpec& spec) {
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    std::vector<VkShaderModule> shaderModules;
    
    // Create shader modules
    if (!spec.vertexSpirv.empty()) {
        VkShaderModule module = createShaderModule(spec.vertexSpirv, "vertex");
        if (module != VK_NULL_HANDLE) {
            shaderModules.push_back(module);
            VkPipelineShaderStageCreateInfo stageInfo{};
            stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
            stageInfo.module = module;
            stageInfo.pName = "main";
            shaderStages.push_back(stageInfo);
        }
    }
    
    if (!spec.fragmentSpirv.empty()) {
        VkShaderModule module = createShaderModule(spec.fragmentSpirv, "fragment");
        if (module != VK_NULL_HANDLE) {
            shaderModules.push_back(module);
            VkPipelineShaderStageCreateInfo stageInfo{};
            stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            stageInfo.module = module;
            stageInfo.pName = "main";
            shaderStages.push_back(stageInfo);
        }
    }
    
    if (shaderStages.empty()) {
        g_cacheLogger.Error("No valid shader stages for graphics pipeline");
        return VK_NULL_HANDLE;
    }
    
    // Vertex input state
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(spec.vertexBindings.size());
    vertexInputInfo.pVertexBindingDescriptions = spec.vertexBindings.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(spec.vertexAttributes.size());
    vertexInputInfo.pVertexAttributeDescriptions = spec.vertexAttributes.data();
    
    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;
    
    // Viewport state
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = spec.viewportCount;
    viewportState.scissorCount = spec.scissorCount;
    
    // Rasterization state
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = spec.polygonMode;
    rasterizer.lineWidth = spec.lineWidth;
    rasterizer.cullMode = spec.cullMode;
    rasterizer.frontFace = spec.frontFace;
    rasterizer.depthBiasEnable = spec.depthBiasEnable;
    
    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = spec.sampleShadingEnable;
    multisampling.rasterizationSamples = spec.rasterizationSamples;
    
    // Depth stencil
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = spec.depthTestEnable;
    depthStencil.depthWriteEnable = spec.depthWriteEnable;
    depthStencil.depthCompareOp = spec.depthCompareOp;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = spec.stencilTestEnable;
    
    // Color blending
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = static_cast<uint32_t>(spec.colorBlendAttachments.size());
    colorBlending.pAttachments = spec.colorBlendAttachments.data();
    
    // Dynamic state
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(spec.dynamicStates.size());
    dynamicState.pDynamicStates = spec.dynamicStates.data();
    
    // Create pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = spec.dynamicStates.empty() ? nullptr : &dynamicState;
    pipelineInfo.layout = spec.layout;
    pipelineInfo.renderPass = spec.renderPass;
    pipelineInfo.subpass = spec.subpass;
    
    VkPipeline pipeline;
    VkResult result = vkCreateGraphicsPipelines(device_, vkCache_, 1, &pipelineInfo, nullptr, &pipeline);
    
    // Clean up shader modules
    for (VkShaderModule module : shaderModules) {
        destroyShaderModule(module);
    }
    
    if (result != VK_SUCCESS) {
        CHECK_VK_OBJECT(result, VkErrorCategory::PIPELINE_CREATION, "graphics_pipeline");
        return VK_NULL_HANDLE;
    }
    
    return pipeline;
}

VkPipeline PipelineCacheManager::createComputePipeline(const ComputePipelineSpec& spec) {
    if (spec.computeSpirv.empty()) {
        g_cacheLogger.Error("No compute shader SPIR-V provided");
        return VK_NULL_HANDLE;
    }
    
    VkShaderModule computeModule = createShaderModule(spec.computeSpirv, "compute");
    if (computeModule == VK_NULL_HANDLE) {
        return VK_NULL_HANDLE;
    }
    
    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = computeModule;
    stageInfo.pName = "main";
    
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = stageInfo;
    pipelineInfo.layout = spec.layout;
    
    VkPipeline pipeline;
    VkResult result = vkCreateComputePipelines(device_, vkCache_, 1, &pipelineInfo, nullptr, &pipeline);
    
    destroyShaderModule(computeModule);
    
    if (result != VK_SUCCESS) {
        CHECK_VK_OBJECT(result, VkErrorCategory::PIPELINE_CREATION, "compute_pipeline");
        return VK_NULL_HANDLE;
    }
    
    return pipeline;
}

VkShaderModule PipelineCacheManager::createShaderModule(const std::vector<uint32_t>& spirv, const std::string& debugName) {
    if (spirv.empty()) {
        g_cacheLogger.Error("Empty SPIR-V for shader: {}", debugName);
        return VK_NULL_HANDLE;
    }
    
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirv.size() * sizeof(uint32_t);
    createInfo.pCode = spirv.data();
    
    VkShaderModule module;
    VkResult result = vkCreateShaderModule(device_, &createInfo, nullptr, &module);
    
    if (result != VK_SUCCESS) {
        CHECK_VK_OBJECT(result, VkErrorCategory::SHADER_COMPILATION, debugName.c_str());
        return VK_NULL_HANDLE;
    }
    
    VK_OBJECT_NAME(device_, module, VK_OBJECT_TYPE_SHADER_MODULE, debugName.c_str());
    return module;
}

void PipelineCacheManager::destroyShaderModule(VkShaderModule module) {
    if (module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device_, module, nullptr);
    }
}

std::string PipelineCacheManager::getCacheFilePath() const {
    return cacheDirectory_ + "/pipeline_cache_" + s_buildUuid + ".vkpcache";
}

bool PipelineCacheManager::createCacheDirectory() {
    try {
        std::filesystem::create_directories(cacheDirectory_);
        return true;
    } catch (const std::exception& e) {
        g_cacheLogger.Error("Failed to create cache directory: {}", e.what());
        return false;
    }
}

uint32_t PipelineCacheManager::calculateBuildHash() const {
    // Simple hash of key build parameters
    uint32_t hash = 0;
    hash ^= deviceProperties_.vendorID;
    hash ^= deviceProperties_.deviceID;
    hash ^= deviceProperties_.driverVersion;
    
    // Include build UUID in hash
    for (char c : s_buildUuid) {
        hash = hash * 31 + static_cast<uint32_t>(c);
    }
    
    return hash;
}

std::string PipelineCacheManager::generateBuildUuid() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%08x", static_cast<uint32_t>(time));
    
    return std::string(buffer);
}

// SPIR-V Loader implementation
std::string SPIRVLoader::s_lastValidationError = "";

std::vector<uint32_t> SPIRVLoader::loadFromFile(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        s_lastValidationError = "Failed to open file: " + filePath;
        return {};
    }
    
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    if (fileSize % sizeof(uint32_t) != 0) {
        s_lastValidationError = "Invalid SPIR-V file size (not multiple of 4): " + filePath;
        return {};
    }
    
    std::vector<uint32_t> spirv(fileSize / sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(spirv.data()), fileSize);
    
    if (!file.good()) {
        s_lastValidationError = "Failed to read SPIR-V file: " + filePath;
        return {};
    }
    
    return spirv;
}

std::vector<uint32_t> SPIRVLoader::loadFromMemory(const void* data, size_t size) {
    if (size % sizeof(uint32_t) != 0) {
        s_lastValidationError = "Invalid SPIR-V data size (not multiple of 4)";
        return {};
    }
    
    const uint32_t* codePtr = static_cast<const uint32_t*>(data);
    return std::vector<uint32_t>(codePtr, codePtr + size / sizeof(uint32_t));
}

bool SPIRVLoader::validate(const std::vector<uint32_t>& spirv) {
    if (spirv.empty()) {
        s_lastValidationError = "Empty SPIR-V";
        return false;
    }
    
    if (spirv.size() < 5) {
        s_lastValidationError = "SPIR-V too small (minimum 5 words)";
        return false;
    }
    
    // Check magic number
    if (spirv[0] != 0x07230203) {
        s_lastValidationError = "Invalid SPIR-V magic number";
        return false;
    }
    
    s_lastValidationError.clear();
    return true;
}

std::string SPIRVLoader::getValidationError() {
    return s_lastValidationError;
}

void SPIRVLoader::reflectShader(const std::vector<uint32_t>& spirv, const std::string& debugName) {
    if (!validate(spirv)) {
        g_cacheLogger.Warn("Cannot reflect invalid SPIR-V: {}", debugName);
        return;
    }
    
    // Basic reflection - version, instruction count
    uint32_t version = spirv[1];
    uint32_t majorVersion = (version >> 16) & 0xFF;
    uint32_t minorVersion = (version >> 8) & 0xFF;
    
    g_cacheLogger.Debug("SPIR-V {} - Version: {}.{}, Size: {} words", 
        debugName, majorVersion, minorVersion, spirv.size());
}

} // namespace voxelvk