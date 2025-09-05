#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <fstream>
#include <filesystem>
#include <cstdlib>

namespace voxelvk {
    
    struct AssetConfig {
        std::string assetRoot = "assets";
        std::string cacheLocation = "cache/assets";
        bool hotReload = false;
        bool validateAssets = true;
        size_t maxCacheSizeMB = 512;
        
        void loadFromEnvironment() {
            if (const char* assetPath = std::getenv("ASSET_PATH")) {
                assetRoot = assetPath;
            }
        }
    };
    
    enum class AssetType {
        TEXTURE,
        MESH,
        VOXEL_DATA,
        AUDIO,
        SHADER,
        CONFIG,
        UNKNOWN
    };
    
    class Asset {
    public:
        virtual ~Asset() = default;
        virtual AssetType getType() const = 0;
        virtual const std::string& getName() const = 0;
        virtual size_t getMemoryUsage() const = 0;
        virtual bool isLoaded() const = 0;
    };
    
    class TextureAsset : public Asset {
    private:
        std::string name_;
        std::vector<uint8_t> data_;
        uint32_t width_ = 0;
        uint32_t height_ = 0;
        uint32_t channels_ = 0;
        bool loaded_ = false;
        
    public:
        TextureAsset(const std::string& name) : name_(name) {}
        
        AssetType getType() const override { return AssetType::TEXTURE; }
        const std::string& getName() const override { return name_; }
        size_t getMemoryUsage() const override { return data_.size(); }
        bool isLoaded() const override { return loaded_; }
        
        bool loadFromFile(const std::string& path) {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file) return false;
            
            auto size = file.tellg();
            data_.resize(size);
            file.seekg(0);
            file.read(reinterpret_cast<char*>(data_.data()), size);
            
            // Mock texture parsing - in real implementation would use stb_image
            width_ = 256;
            height_ = 256;
            channels_ = 4;
            loaded_ = true;
            
            return true;
        }
        
        uint32_t getWidth() const { return width_; }
        uint32_t getHeight() const { return height_; }
        uint32_t getChannels() const { return channels_; }
        const std::vector<uint8_t>& getData() const { return data_; }
    };
    
    class MeshAsset : public Asset {
    private:
        std::string name_;
        std::vector<float> vertices_;
        std::vector<uint32_t> indices_;
        bool loaded_ = false;
        
    public:
        MeshAsset(const std::string& name) : name_(name) {}
        
        AssetType getType() const override { return AssetType::MESH; }
        const std::string& getName() const override { return name_; }
        size_t getMemoryUsage() const override { 
            return vertices_.size() * sizeof(float) + indices_.size() * sizeof(uint32_t);
        }
        bool isLoaded() const override { return loaded_; }
        
        bool loadFromFile(const std::string& path) {
            // Mock OBJ loading - in real implementation would use proper parser
            std::ifstream file(path);
            if (!file) return false;
            
            // Generate simple cube mesh
            vertices_ = {
                // Positions (x, y, z), Normals (nx, ny, nz), UVs (u, v)
                -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
                 1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
                 1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
                -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f
            };
            
            indices_ = {
                0, 1, 2, 2, 3, 0
            };
            
            loaded_ = true;
            return true;
        }
        
        const std::vector<float>& getVertices() const { return vertices_; }
        const std::vector<uint32_t>& getIndices() const { return indices_; }
    };
    
    class VoxelDataAsset : public Asset {
    private:
        std::string name_;
        std::vector<uint16_t> voxelData_;
        uint32_t chunkSize_ = 32;
        bool loaded_ = false;
        
    public:
        VoxelDataAsset(const std::string& name) : name_(name) {}
        
        AssetType getType() const override { return AssetType::VOXEL_DATA; }
        const std::string& getName() const override { return name_; }
        size_t getMemoryUsage() const override { return voxelData_.size() * sizeof(uint16_t); }
        bool isLoaded() const override { return loaded_; }
        
        bool loadFromFile(const std::string& path) {
            std::ifstream file(path, std::ios::binary);
            if (!file) return false;
            
            // Mock voxel data - in real implementation would parse VXL format
            size_t totalVoxels = chunkSize_ * chunkSize_ * chunkSize_;
            voxelData_.resize(totalVoxels);
            
            // Generate test pattern
            for (uint32_t z = 0; z < chunkSize_; ++z) {
                for (uint32_t y = 0; y < chunkSize_; ++y) {
                    for (uint32_t x = 0; x < chunkSize_; ++x) {
                        size_t index = x + y * chunkSize_ + z * chunkSize_ * chunkSize_;
                        
                        // Create a simple pattern
                        if (y < chunkSize_ / 2) {
                            voxelData_[index] = 1; // Solid bottom
                        } else if (x == 0 || x == chunkSize_ - 1 || 
                                 z == 0 || z == chunkSize_ - 1 ||
                                 y == chunkSize_ - 1) {
                            voxelData_[index] = 2; // Walls
                        } else {
                            voxelData_[index] = 0; // Air
                        }
                    }
                }
            }
            
            loaded_ = true;
            return true;
        }
        
        const std::vector<uint16_t>& getVoxelData() const { return voxelData_; }
        uint32_t getChunkSize() const { return chunkSize_; }
    };
    
    class AssetManager {
    private:
        static std::unique_ptr<AssetManager> instance_;
        std::unordered_map<std::string, std::shared_ptr<Asset>> assets_;
        AssetConfig config_;
        
        AssetManager() {
            config_.loadFromEnvironment();
        }
        
        AssetType getAssetTypeFromExtension(const std::string& path) const {
            std::string ext = std::filesystem::path(path).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".dds") {
                return AssetType::TEXTURE;
            } else if (ext == ".obj" || ext == ".gltf" || ext == ".glb") {
                return AssetType::MESH;
            } else if (ext == ".vxl" || ext == ".vox") {
                return AssetType::VOXEL_DATA;
            } else if (ext == ".wav" || ext == ".ogg" || ext == ".mp3") {
                return AssetType::AUDIO;
            } else if (ext == ".glsl" || ext == ".hlsl" || ext == ".spv") {
                return AssetType::SHADER;
            } else if (ext == ".yaml" || ext == ".json" || ext == ".cfg") {
                return AssetType::CONFIG;
            }
            
            return AssetType::UNKNOWN;
        }
        
        std::string resolveAssetPath(const std::string& relativePath) const {
            std::filesystem::path fullPath = config_.assetRoot;
            fullPath /= relativePath;
            return fullPath.string();
        }
        
    public:
        static AssetManager& getInstance() {
            if (!instance_) {
                instance_ = std::unique_ptr<AssetManager>(new AssetManager());
            }
            return *instance_;
        }
        
        template<typename T>
        std::shared_ptr<T> loadAsset(const std::string& path) {
            // Check if already loaded
            auto it = assets_.find(path);
            if (it != assets_.end()) {
                return std::dynamic_pointer_cast<T>(it->second);
            }
            
            // Create new asset
            auto asset = std::make_shared<T>(path);
            
            // Load from file
            std::string fullPath = resolveAssetPath(path);
            if (!asset->loadFromFile(fullPath)) {
                return nullptr;
            }
            
            // Store in cache
            assets_[path] = asset;
            return asset;
        }
        
        std::shared_ptr<TextureAsset> loadTexture(const std::string& path) {
            return loadAsset<TextureAsset>(path);
        }
        
        std::shared_ptr<MeshAsset> loadMesh(const std::string& path) {
            return loadAsset<MeshAsset>(path);
        }
        
        std::shared_ptr<VoxelDataAsset> loadVoxelData(const std::string& path) {
            return loadAsset<VoxelDataAsset>(path);
        }
        
        void unloadAsset(const std::string& path) {
            assets_.erase(path);
        }
        
        void unloadAll() {
            assets_.clear();
        }
        
        size_t getTotalMemoryUsage() const {
            size_t total = 0;
            for (const auto& [path, asset] : assets_) {
                total += asset->getMemoryUsage();
            }
            return total;
        }
        
        std::vector<std::string> getLoadedAssets() const {
            std::vector<std::string> paths;
            paths.reserve(assets_.size());
            for (const auto& [path, asset] : assets_) {
                paths.push_back(path);
            }
            return paths;
        }
        
        bool isAssetLoaded(const std::string& path) const {
            auto it = assets_.find(path);
            return it != assets_.end() && it->second->isLoaded();
        }
        
        const AssetConfig& getConfig() const { return config_; }
        
        void setConfig(const AssetConfig& config) { config_ = config; }
    };
    
} // namespace voxelvk