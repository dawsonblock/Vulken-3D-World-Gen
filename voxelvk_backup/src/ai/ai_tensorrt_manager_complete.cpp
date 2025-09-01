#include "ai_tensorrt_manager.hpp"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <NvOnnxParser.h>
#include <NvInferPlugin.h>

namespace voxelvk::ai {

// Complete TensorRT implementation with real model inference

// TensorRTEngine Complete Implementation
bool TensorRTEngine::buildFromOnnx(const std::string& onnx_path, const std::string& cache_path) {
    std::cout << "Building TensorRT engine from ONNX: " << onnx_path << std::endl;
    
    // Initialize TensorRT plugins
    initLibNvinferPlugins(&nvinfer1::getLogger(), "");
    
    // Create builder
    auto builder = std::unique_ptr<nvinfer1::IBuilder>(nvinfer1::createInferBuilder(nvinfer1::getLogger()));
    if (!builder) {
        std::cerr << "Failed to create TensorRT builder" << std::endl;
        return false;
    }
    
    // Create network
    const auto explicit_batch = 1U << static_cast<uint32_t>(nvinfer1::NetworkDefinitionCreationFlag::kEXPLICIT_BATCH);
    auto network = std::unique_ptr<nvinfer1::INetworkDefinition>(builder->createNetworkV2(explicit_batch));
    if (!network) {
        std::cerr << "Failed to create TensorRT network" << std::endl;
        return false;
    }
    
    // Create ONNX parser
    auto parser = std::unique_ptr<nvonnxparser::IParser>(nvonnxparser::createParser(*network, nvinfer1::getLogger()));
    if (!parser) {
        std::cerr << "Failed to create ONNX parser" << std::endl;
        return false;
    }
    
    // Parse ONNX model
    if (!parser->parseFromFile(onnx_path.c_str(), static_cast<int>(nvinfer1::ILogger::Severity::kWARNING))) {
        std::cerr << "Failed to parse ONNX file: " << onnx_path << std::endl;
        for (int i = 0; i < parser->getNbErrors(); ++i) {
            std::cerr << "Parser error " << i << ": " << parser->getError(i)->desc() << std::endl;
        }
        return false;
    }
    
    // Create builder config
    auto config = std::unique_ptr<nvinfer1::IBuilderConfig>(builder->createBuilderConfig());
    if (!config) {
        std::cerr << "Failed to create builder config" << std::endl;
        return false;
    }
    
    // Set memory pool size (2GB)
    config->setMemoryPoolLimit(nvinfer1::MemoryPoolType::kWORKSPACE, 2ULL << 30);
    
    // Enable FP16 precision if supported
    if (builder->platformHasFastFp16()) {
        config->setFlag(nvinfer1::BuilderFlag::kFP16);
        std::cout << "Enabled FP16 precision" << std::endl;
    }
    
    // Enable INT8 precision if calibration is available
    // config->setFlag(nvinfer1::BuilderFlag::kINT8);
    
    // Set optimization profiles for dynamic shapes
    auto profile = builder->createOptimizationProfile();
    if (profile) {
        // Configure input shapes (example for structure generation)
        profile->setDimensions("input", nvinfer1::OptProfileSelector::kMIN, nvinfer1::Dims4{1, 256, 1, 1});
        profile->setDimensions("input", nvinfer1::OptProfileSelector::kOPT, nvinfer1::Dims4{4, 256, 1, 1});
        profile->setDimensions("input", nvinfer1::OptProfileSelector::kMAX, nvinfer1::Dims4{16, 256, 1, 1});
        config->addOptimizationProfile(profile);
    }
    
    // Build engine
    std::cout << "Building TensorRT engine... (this may take several minutes)" << std::endl;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    engine_ = std::unique_ptr<nvinfer1::ICudaEngine>(builder->buildEngineWithConfig(*network, *config));
    if (!engine_) {
        std::cerr << "Failed to build TensorRT engine" << std::endl;
        return false;
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto build_time = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
    std::cout << "Engine built successfully in " << build_time.count() << " seconds" << std::endl;
    
    // Create execution context
    context_ = std::unique_ptr<nvinfer1::IExecutionContext>(engine_->createExecutionContext());
    if (!context_) {
        std::cerr << "Failed to create execution context" << std::endl;
        return false;
    }
    
    // Setup bindings
    if (!setupBindings()) {
        std::cerr << "Failed to setup bindings" << std::endl;
        return false;
    }
    
    // Save engine to cache if path provided
    if (!cache_path.empty()) {
        saveEngineToFile(cache_path);
    }
    
    std::cout << "TensorRT engine initialization complete" << std::endl;
    return true;
}

bool TensorRTEngine::saveEngineToFile(const std::string& file_path) {
    if (!engine_) {
        std::cerr << "No engine to save" << std::endl;
        return false;
    }
    
    // Serialize engine
    auto serialized_engine = std::unique_ptr<nvinfer1::IHostMemory>(engine_->serialize());
    if (!serialized_engine) {
        std::cerr << "Failed to serialize engine" << std::endl;
        return false;
    }
    
    // Write to file
    std::ofstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for writing: " << file_path << std::endl;
        return false;
    }
    
    file.write(static_cast<const char*>(serialized_engine->data()), serialized_engine->size());
    file.close();
    
    std::cout << "Engine saved to: " << file_path << std::endl;
    return true;
}

// Enhanced TensorRTMultiModelManager with actual AI inference
bool TensorRTMultiModelManager::generateStructureImpl(
    const StructurePrompt& prompt, const EnvironmentalContext& context, StructureBlueprint& result) {
    
    auto& engine = engines_[ContentType::Structure];
    if (!engine || !engine->isReady()) {
        setError("Structure generation engine not ready");
        return false;
    }
    
    // Prepare input data with proper encoding
    std::vector<float> input_data;
    if (!preprocessStructureInput(prompt, input_data)) {
        setError("Failed to preprocess structure input");
        return false;
    }
    
    // Set input data
    std::string input_name = "input";
    if (!engine->setInputData(input_name, input_data.data(), input_data.size() * sizeof(float))) {
        setError("Failed to set input data for structure generation");
        return false;
    }
    
    // Execute inference
    auto start_time = std::chrono::high_resolution_clock::now();
    if (!engine->executeInference()) {
        setError("Structure generation inference failed");
        return false;
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    
    // Get output data
    std::string output_name = "output";
    size_t output_size = engine->getOutputSize(output_name);
    std::vector<float> output_data(output_size / sizeof(float));
    
    if (!engine->getOutputData(output_name, output_data.data(), output_size)) {
        setError("Failed to get structure generation output");
        return false;
    }
    
    // Post-process output
    if (!postprocessStructureOutput(output_data, result)) {
        setError("Failed to postprocess structure output");
        return false;
    }
    
    // Update performance stats
    float inference_time = std::chrono::duration<float, std::milli>(end_time - start_time).count();
    updatePerformanceStats(inference_time, true);
    
    clearError();
    return true;
}

bool TensorRTMultiModelManager::preprocessStructureInput(
    const StructurePrompt& prompt, std::vector<float>& input_data) {
    
    // Advanced preprocessing for structure generation
    input_data.clear();
    input_data.reserve(256); // Model-specific input size
    
    // Encode position (normalized to [-1, 1])
    input_data.push_back(std::tanh(prompt.position.x / 1000.0f));
    input_data.push_back(std::tanh(prompt.position.y / 128.0f));
    input_data.push_back(std::tanh(prompt.position.z / 1000.0f));
    
    // Encode size bounds (normalized)
    input_data.push_back(prompt.size_bounds.x / 128.0f);
    input_data.push_back(prompt.size_bounds.y / 128.0f);
    input_data.push_back(prompt.size_bounds.z / 128.0f);
    
    // Encode complexity
    input_data.push_back(prompt.complexity);
    
    // Encode architectural style (one-hot encoding)
    std::vector<float> style_encoding(8, 0.0f);
    int style_index = static_cast<int>(prompt.style);
    if (style_index >= 0 && style_index < 8) {
        style_encoding[style_index] = 1.0f;
    }
    input_data.insert(input_data.end(), style_encoding.begin(), style_encoding.end());
    
    // Encode description using simple text features
    std::vector<float> text_features = encodeTextDescription(prompt.description);
    input_data.insert(input_data.end(), text_features.begin(), text_features.end());
    
    // Encode contextual information
    if (prompt.biome_context != "plains") {
        // Biome-specific encoding
        std::vector<float> biome_features = encodeBiomeContext(prompt.biome_context);
        input_data.insert(input_data.end(), biome_features.begin(), biome_features.end());
    }
    
    // Encode required features
    std::vector<float> feature_encoding = encodeRequiredFeatures(prompt.required_features);
    input_data.insert(input_data.end(), feature_encoding.begin(), feature_encoding.end());
    
    // Pad or truncate to expected input size
    if (input_data.size() < 256) {
        input_data.resize(256, 0.0f);
    } else if (input_data.size() > 256) {
        input_data.resize(256);
    }
    
    return true;
}

bool TensorRTMultiModelManager::postprocessStructureOutput(
    const std::vector<float>& output_data, StructureBlueprint& result) {
    
    if (output_data.empty()) {
        setError("Empty output from structure generation model");
        return false;
    }
    
    // Advanced postprocessing for structure generation
    // Assuming output format: [voxel_data, material_data, connectivity_data, metadata]
    
    size_t offset = 0;
    
    // Extract voxel structure (assuming 32x32x32 output)
    int structure_size = 32;
    size_t voxel_count = structure_size * structure_size * structure_size;
    
    if (output_data.size() < voxel_count) {
        setError("Insufficient output data for voxel structure");
        return false;
    }
    
    result.structure.W = result.structure.H = result.structure.D = structure_size;
    result.structure.occupancy.resize(voxel_count);
    
    // Convert float probabilities to binary occupancy
    for (size_t i = 0; i < voxel_count && offset < output_data.size(); ++i, ++offset) {
        result.structure.occupancy[i] = (output_data[offset] > 0.5f) ? 1 : 0;
    }
    
    // Extract material assignments (if available)
    if (offset + voxel_count <= output_data.size()) {
        for (size_t i = 0; i < voxel_count && offset < output_data.size(); ++i, ++offset) {
            // Map continuous output to discrete material IDs
            int material_id = static_cast<int>(output_data[offset] * 10.0f) % 10;
            
            // Store in material mapping
            if (result.structure.occupancy[i] > 0) {
                result.material_map.block_to_material[i] = "material_" + std::to_string(material_id);
            }
        }
    }
    
    // Extract connectivity information (simplified)
    if (offset + 64 <= output_data.size()) {
        // Process connectivity graph data
        for (int i = 0; i < 8 && offset < output_data.size(); ++i, offset += 8) {
            if (output_data[offset] > 0.7f) {
                ConnectivityGraph::Connection connection;
                connection.zone_a = static_cast<int>(output_data[offset + 1] * 10);
                connection.zone_b = static_cast<int>(output_data[offset + 2] * 10);
                connection.connection_type = (output_data[offset + 3] > 0.5f) ? "door" : "corridor";
                
                // Generate path points
                for (int j = 0; j < 3; ++j) {
                    glm::vec3 point;
                    point.x = output_data[offset + 4 + j] * structure_size;
                    point.y = output_data[offset + 5 + j] * structure_size;
                    point.z = output_data[offset + 6 + j] * structure_size;
                    connection.path.push_back(point);
                }
                
                result.connections.connections.push_back(connection);
            }
        }
    }
    
    // Extract functional zones
    generateFunctionalZones(result);
    
    // Set metadata
    result.structure_type = "ai_generated_building";
    result.style = ArchitecturalStyle::Medieval; // Default, could be inferred from output
    result.estimated_complexity = calculateComplexity(result.structure);
    result.bounding_box = {structure_size, structure_size, structure_size};
    
    // Generate anchor points (key structural points)
    generateAnchorPoints(result);
    
    return true;
}

bool TensorRTMultiModelManager::generateTextureImpl(const MaterialRequest& request, TextureData& result) {
    auto& engine = engines_[ContentType::Texture];
    if (!engine || !engine->isReady()) {
        // Fallback to procedural generation
        return generateProceduralTexture(request, result);
    }
    
    // Prepare input for texture generation
    std::vector<float> input_data;
    if (!preprocessTextureInput(request, input_data)) {
        setError("Failed to preprocess texture input");
        return false;
    }
    
    // Execute inference
    if (!engine->setInputData("input", input_data.data(), input_data.size() * sizeof(float))) {
        setError("Failed to set texture input data");
        return false;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    if (!engine->executeInference()) {
        setError("Texture generation inference failed");
        return false;
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    
    // Get texture output
    size_t output_size = engine->getOutputSize("output");
    std::vector<float> output_data(output_size / sizeof(float));
    
    if (!engine->getOutputData("output", output_data.data(), output_size)) {
        setError("Failed to get texture output");
        return false;
    }
    
    // Post-process texture output
    if (!postprocessTextureOutput(output_data, result)) {
        setError("Failed to postprocess texture output");
        return false;
    }
    
    float inference_time = std::chrono::duration<float, std::milli>(end_time - start_time).count();
    updatePerformanceStats(inference_time, true);
    
    clearError();
    return true;
}

bool TensorRTMultiModelManager::preprocessTextureInput(const MaterialRequest& request, std::vector<float>& input_data) {
    input_data.clear();
    input_data.reserve(128);
    
    // Encode material type
    std::vector<float> material_encoding = encodeMaterialType(request.material_type);
    input_data.insert(input_data.end(), material_encoding.begin(), material_encoding.end());
    
    // Encode style description
    std::vector<float> style_encoding = encodeTextDescription(request.style_description);
    input_data.insert(input_data.end(), style_encoding.begin(), style_encoding.end());
    
    // Encode resolution (normalized)
    input_data.push_back(static_cast<float>(request.resolution) / 1024.0f);
    
    // Encode flags
    input_data.push_back(request.generate_normal_map ? 1.0f : 0.0f);
    input_data.push_back(request.generate_roughness_map ? 1.0f : 0.0f);
    input_data.push_back(request.seamless ? 1.0f : 0.0f);
    
    // Pad to expected size
    input_data.resize(128, 0.0f);
    
    return true;
}

bool TensorRTMultiModelManager::postprocessTextureOutput(const std::vector<float>& output_data, TextureData& result) {
    if (output_data.empty()) {
        return false;
    }
    
    // Assuming model outputs normalized RGB values
    int resolution = 256; // Default resolution
    size_t pixel_count = resolution * resolution;
    
    if (output_data.size() < pixel_count * 3) {
        return false;
    }
    
    result.width = result.height = resolution;
    result.diffuse_map.resize(pixel_count * 3);
    
    // Convert float values to uint8
    for (size_t i = 0; i < pixel_count * 3; ++i) {
        float normalized_value = std::clamp(output_data[i], 0.0f, 1.0f);
        result.diffuse_map[i] = static_cast<uint8_t>(normalized_value * 255.0f);
    }
    
    // Generate additional maps if requested
    if (output_data.size() >= pixel_count * 6) { // RGB + Normal
        result.normal_map.resize(pixel_count * 3);
        for (size_t i = 0; i < pixel_count * 3; ++i) {
            float normal_value = std::clamp(output_data[pixel_count * 3 + i], 0.0f, 1.0f);
            result.normal_map[i] = static_cast<uint8_t>(normal_value * 255.0f);
        }
    }
    
    if (output_data.size() >= pixel_count * 7) { // RGB + Normal + Roughness
        result.roughness_map.resize(pixel_count);
        for (size_t i = 0; i < pixel_count; ++i) {
            float roughness_value = std::clamp(output_data[pixel_count * 6 + i], 0.0f, 1.0f);
            result.roughness_map[i] = static_cast<uint8_t>(roughness_value * 255.0f);
        }
    }
    
    result.material_name = "ai_generated_texture";
    result.tiling_factor = 1.0f;
    
    return true;
}

bool TensorRTMultiModelManager::generateProceduralTexture(const MaterialRequest& request, TextureData& result) {
    // Fallback procedural texture generation
    result.width = result.height = request.resolution;
    size_t pixel_count = request.resolution * request.resolution;
    
    result.diffuse_map.resize(pixel_count * 3);
    
    // Generate simple procedural texture based on material type
    std::random_device rd;
    std::mt19937 gen(rd());
    
    glm::vec3 base_color = {0.5f, 0.5f, 0.5f};
    
    if (request.material_type == "stone") {
        base_color = {0.6f, 0.6f, 0.6f};
    } else if (request.material_type == "wood") {
        base_color = {0.6f, 0.4f, 0.2f};
    } else if (request.material_type == "metal") {
        base_color = {0.7f, 0.7f, 0.7f};
    }
    
    // Generate noise-based texture
    for (int y = 0; y < request.resolution; ++y) {
        for (int x = 0; x < request.resolution; ++x) {
            size_t idx = (y * request.resolution + x) * 3;
            
            float noise = std::sin(x * 0.1f) * std::cos(y * 0.1f) * 0.1f + 
                         std::sin(x * 0.05f) * std::cos(y * 0.05f) * 0.05f;
            
            result.diffuse_map[idx + 0] = static_cast<uint8_t>((base_color.r + noise) * 255.0f);
            result.diffuse_map[idx + 1] = static_cast<uint8_t>((base_color.g + noise) * 255.0f);
            result.diffuse_map[idx + 2] = static_cast<uint8_t>((base_color.b + noise) * 255.0f);
        }
    }
    
    // Generate normal map if requested
    if (request.generate_normal_map) {
        result.normal_map.resize(pixel_count * 3);
        for (size_t i = 0; i < pixel_count; ++i) {
            result.normal_map[i * 3 + 0] = 128; // X
            result.normal_map[i * 3 + 1] = 128; // Y
            result.normal_map[i * 3 + 2] = 255; // Z (pointing up)
        }
    }
    
    result.material_name = "procedural_" + request.material_type;
    result.tiling_factor = 1.0f;
    
    return true;
}

// Helper functions for encoding
std::vector<float> TensorRTMultiModelManager::encodeTextDescription(const std::string& description) {
    // Simplified text encoding - in practice would use proper NLP embeddings
    std::vector<float> encoding(64, 0.0f);
    
    // Simple keyword-based encoding
    std::vector<std::pair<std::string, float>> keywords = {
        {"medieval", 0.9f}, {"modern", 0.8f}, {"ancient", 0.7f}, {"fantasy", 0.6f},
        {"stone", 0.5f}, {"wood", 0.4f}, {"metal", 0.3f}, {"crystal", 0.2f},
        {"weathered", -0.3f}, {"polished", 0.3f}, {"rough", -0.2f}, {"smooth", 0.2f}
    };
    
    std::string lower_desc = description;
    std::transform(lower_desc.begin(), lower_desc.end(), lower_desc.begin(), ::tolower);
    
    for (size_t i = 0; i < keywords.size() && i < encoding.size(); ++i) {
        if (lower_desc.find(keywords[i].first) != std::string::npos) {
            encoding[i] = keywords[i].second;
        }
    }
    
    return encoding;
}

std::vector<float> TensorRTMultiModelManager::encodeBiomeContext(const std::string& biome) {
    std::vector<float> encoding(16, 0.0f);
    
    // Simple biome encoding
    if (biome == "desert") {
        encoding[0] = 1.0f;
    } else if (biome == "forest") {
        encoding[1] = 1.0f;
    } else if (biome == "mountain") {
        encoding[2] = 1.0f;
    } else if (biome == "ocean") {
        encoding[3] = 1.0f;
    } else {
        encoding[4] = 1.0f; // Default/unknown
    }
    
    return encoding;
}

std::vector<float> TensorRTMultiModelManager::encodeRequiredFeatures(const std::vector<std::string>& features) {
    std::vector<float> encoding(32, 0.0f);
    
    std::vector<std::string> known_features = {
        "door", "window", "roof", "stairs", "chimney", "balcony", "tower", "wall",
        "foundation", "entrance", "courtyard", "garden", "fence", "bridge", "tunnel", "arch"
    };
    
    for (size_t i = 0; i < known_features.size() && i < encoding.size(); ++i) {
        for (const auto& feature : features) {
            if (feature == known_features[i]) {
                encoding[i] = 1.0f;
                break;
            }
        }
    }
    
    return encoding;
}

std::vector<float> TensorRTMultiModelManager::encodeMaterialType(const std::string& material_type) {
    std::vector<float> encoding(16, 0.0f);
    
    if (material_type == "stone") {
        encoding[0] = 1.0f;
    } else if (material_type == "wood") {
        encoding[1] = 1.0f;
    } else if (material_type == "metal") {
        encoding[2] = 1.0f;
    } else if (material_type == "fabric") {
        encoding[3] = 1.0f;
    } else if (material_type == "crystal") {
        encoding[4] = 1.0f;
    } else {
        encoding[5] = 1.0f; // Unknown/other
    }
    
    return encoding;
}

void TensorRTMultiModelManager::generateFunctionalZones(StructureBlueprint& result) {
    // Analyze generated structure and identify functional zones
    const auto& occupancy = result.structure.occupancy;
    int size = result.structure.W;
    
    // Simple zone detection based on empty spaces
    std::vector<std::vector<std::vector<bool>>> visited(size, 
        std::vector<std::vector<bool>>(size, std::vector<bool>(size, false)));
    
    int zone_id = 0;
    for (int z = 0; z < size; ++z) {
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                int idx = z * size * size + y * size + x;
                
                if (!visited[x][y][z] && occupancy[idx] == 0) {
                    // Found unvisited empty space - create zone
                    FunctionalZone zone;
                    zone.type = inferZoneType(x, y, z, size);
                    zone.center = {x, y, z};
                    
                    // Flood fill to find zone extent
                    floodFillZone(occupancy, visited, x, y, z, size, zone);
                    
                    if (zone.zone_blocks.occupancy.size() > 8) { // Minimum zone size
                        result.zones.push_back(zone);
                        zone_id++;
                    }
                }
            }
        }
    }
}

std::string TensorRTMultiModelManager::inferZoneType(int x, int y, int z, int size) {
    // Simple heuristics for zone type inference
    float height_ratio = static_cast<float>(y) / size;
    float center_distance = std::sqrt((x - size/2) * (x - size/2) + (z - size/2) * (z - size/2)) / (size/2);
    
    if (height_ratio < 0.2f) {
        return "basement";
    } else if (height_ratio > 0.8f) {
        return "attic";
    } else if (center_distance < 0.3f) {
        return "main_hall";
    } else if (center_distance > 0.7f) {
        return "balcony";
    } else {
        return "room";
    }
}

void TensorRTMultiModelManager::floodFillZone(const std::vector<uint8_t>& occupancy,
                                            std::vector<std::vector<std::vector<bool>>>& visited,
                                            int start_x, int start_y, int start_z, int size,
                                            FunctionalZone& zone) {
    std::queue<glm::ivec3> queue;
    queue.push({start_x, start_y, start_z});
    visited[start_x][start_y][start_z] = true;
    
    glm::ivec3 min_bounds = {size, size, size};
    glm::ivec3 max_bounds = {-1, -1, -1};
    
    while (!queue.empty()) {
        auto pos = queue.front();
        queue.pop();
        
        // Update bounds
        min_bounds.x = std::min(min_bounds.x, pos.x);
        min_bounds.y = std::min(min_bounds.y, pos.y);
        min_bounds.z = std::min(min_bounds.z, pos.z);
        max_bounds.x = std::max(max_bounds.x, pos.x);
        max_bounds.y = std::max(max_bounds.y, pos.y);
        max_bounds.z = std::max(max_bounds.z, pos.z);
        
        // Check 6 neighbors
        std::vector<glm::ivec3> neighbors = {
            {pos.x + 1, pos.y, pos.z}, {pos.x - 1, pos.y, pos.z},
            {pos.x, pos.y + 1, pos.z}, {pos.x, pos.y - 1, pos.z},
            {pos.x, pos.y, pos.z + 1}, {pos.x, pos.y, pos.z - 1}
        };
        
        for (const auto& neighbor : neighbors) {
            if (neighbor.x >= 0 && neighbor.x < size &&
                neighbor.y >= 0 && neighbor.y < size &&
                neighbor.z >= 0 && neighbor.z < size &&
                !visited[neighbor.x][neighbor.y][neighbor.z]) {
                
                int idx = neighbor.z * size * size + neighbor.y * size + neighbor.x;
                if (occupancy[idx] == 0) { // Empty space
                    visited[neighbor.x][neighbor.y][neighbor.z] = true;
                    queue.push(neighbor);
                }
            }
        }
    }
    
    // Create zone voxel grid
    glm::ivec3 zone_size = max_bounds - min_bounds + glm::ivec3(1);
    zone.zone_blocks.W = zone_size.x;
    zone.zone_blocks.H = zone_size.y;
    zone.zone_blocks.D = zone_size.z;
    zone.zone_blocks.occupancy.resize(zone_size.x * zone_size.y * zone_size.z, 0);
    
    // Mark zone boundaries
    for (int z = min_bounds.z; z <= max_bounds.z; ++z) {
        for (int y = min_bounds.y; y <= max_bounds.y; ++y) {
            for (int x = min_bounds.x; x <= max_bounds.x; ++x) {
                if (visited[x][y][z]) {
                    int local_x = x - min_bounds.x;
                    int local_y = y - min_bounds.y;
                    int local_z = z - min_bounds.z;
                    int local_idx = local_z * zone_size.y * zone_size.x + local_y * zone_size.x + local_x;
                    zone.zone_blocks.occupancy[local_idx] = 1;
                }
            }
        }
    }
}

void TensorRTMultiModelManager::generateAnchorPoints(StructureBlueprint& result) {
    // Generate key structural anchor points
    const auto& occupancy = result.structure.occupancy;
    int size = result.structure.W;
    
    // Find corners and key structural points
    for (int z = 0; z < size; z += size - 1) { // Only corners in Z
        for (int y = 0; y < size; y += 4) { // Every 4 blocks in Y
            for (int x = 0; x < size; x += size - 1) { // Only corners in X
                int idx = z * size * size + y * size + x;
                if (occupancy[idx] > 0) {
                    result.anchor_points.push_back({x, y, z});
                }
            }
        }
    }
    
    // Find foundation points (bottom level)
    for (int z = 0; z < size; ++z) {
        for (int x = 0; x < size; ++x) {
            for (int y = 0; y < size; ++y) {
                int idx = z * size * size + y * size + x;
                if (occupancy[idx] > 0) {
                    result.anchor_points.push_back({x, y, z});
                    break; // Only the bottom-most solid block
                }
            }
        }
    }
}

float TensorRTMultiModelManager::calculateComplexity(const VoxelGrid& structure) {
    if (structure.occupancy.empty()) return 0.0f;
    
    int solid_blocks = 0;
    int surface_blocks = 0;
    int size = structure.W;
    
    for (int z = 0; z < size; ++z) {
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                int idx = z * size * size + y * size + x;
                
                if (structure.occupancy[idx] > 0) {
                    solid_blocks++;
                    
                    // Check if this is a surface block (has at least one air neighbor)
                    bool is_surface = false;
                    std::vector<glm::ivec3> neighbors = {
                        {x + 1, y, z}, {x - 1, y, z},
                        {x, y + 1, z}, {x, y - 1, z},
                        {x, y, z + 1}, {x, y, z - 1}
                    };
                    
                    for (const auto& neighbor : neighbors) {
                        if (neighbor.x < 0 || neighbor.x >= size ||
                            neighbor.y < 0 || neighbor.y >= size ||
                            neighbor.z < 0 || neighbor.z >= size) {
                            is_surface = true;
                            break;
                        }
                        
                        int neighbor_idx = neighbor.z * size * size + neighbor.y * size + neighbor.x;
                        if (structure.occupancy[neighbor_idx] == 0) {
                            is_surface = true;
                            break;
                        }
                    }
                    
                    if (is_surface) surface_blocks++;
                }
            }
        }
    }
    
    if (solid_blocks == 0) return 0.0f;
    
    // Complexity based on surface-to-volume ratio and total volume
    float volume_ratio = static_cast<float>(solid_blocks) / (size * size * size);
    float surface_ratio = static_cast<float>(surface_blocks) / solid_blocks;
    
    return volume_ratio * surface_ratio;
}

} // namespace voxelvk::ai