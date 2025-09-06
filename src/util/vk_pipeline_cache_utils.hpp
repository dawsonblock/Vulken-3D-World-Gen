
#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

namespace voxelvk { namespace util {

// env helper
inline std::string env_or(const char* key, const std::string& defv){
    const char* v = std::getenv(key);
    return (v && *v) ? std::string(v) : defv;
}

inline bool read_file(const std::string& path, std::vector<char>& out){
    std::ifstream f(path, std::ios::binary);
    if(!f.good()) return false;
    f.seekg(0, std::ios::end);
    std::streamoff tellgResult = f.tellg();
    if(tellgResult < 0) return false;
    std::streampos sz = tellgResult;
    if(sz <= 0) return false;
    out.resize(static_cast<size_t>(sz));
    f.seekg(0, std::ios::beg);
    f.read(out.data(), static_cast<std::streamsize>(out.size()));
    return f.good();
}

inline bool write_file(const std::string& path, const std::vector<char>& data){
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if(!f.good()) return false;
    f.write(data.data(), static_cast<std::streamsize>(data.size()));
    return f.good();
}

/// Create a pipeline cache using bytes from VK_PIPELINE_CACHE_PATH if present.
/// Returns VK_NULL_HANDLE on failure, but never throws.
inline VkPipelineCache create_pipeline_cache_from_env(VkDevice device, const char* env_key="VK_PIPELINE_CACHE_PATH"){
    std::string path = env_or(env_key, "");
    VkPipelineCache cache = VK_NULL_HANDLE;

    std::vector<char> bytes;
    VkPipelineCacheCreateInfo ci{}; ci.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    if(!path.empty() && read_file(path, bytes) && !bytes.empty()){
        ci.initialDataSize = bytes.size();
        ci.pInitialData = bytes.data();
        std::fprintf(stderr, "[vkpcache] Loading %zu bytes from %s\n", bytes.size(), path.c_str());
    } else if(!path.empty()){
        std::fprintf(stderr, "[vkpcache] No existing cache at %s; starting fresh.\n", path.c_str());
    }
    if(vkCreatePipelineCache(device, &ci, nullptr, &cache) != VK_SUCCESS){
        std::fprintf(stderr, "[vkpcache] vkCreatePipelineCache failed.\n");
        return VK_NULL_HANDLE;
    }
    return cache;
}

/// Save a pipeline cache back to VK_PIPELINE_CACHE_PATH (best-effort).
inline void save_pipeline_cache_to_env(VkDevice device, VkPipelineCache cache, const char* env_key="VK_PIPELINE_CACHE_PATH"){
    if(cache == VK_NULL_HANDLE) return;
    std::string path = env_or(env_key, "");
    if(path.empty()) return;

    size_t sz = 0;
    if(vkGetPipelineCacheData(device, cache, &sz, nullptr) != VK_SUCCESS || sz == 0){
        std::fprintf(stderr, "[vkpcache] vkGetPipelineCacheData probe failed or empty.\n");
        return;
    }
    std::vector<char> data(sz);
    if(vkGetPipelineCacheData(device, cache, &sz, data.data()) != VK_SUCCESS){
        std::fprintf(stderr, "[vkpcache] vkGetPipelineCacheData fetch failed.\n");
        return;
    }
    if(!write_file(path, data)){
        std::fprintf(stderr, "[vkpcache] Failed to write %zu bytes to %s\n", data.size(), path.c_str());
    } else {
        std::fprintf(stderr, "[vkpcache] Wrote %zu bytes to %s\n", data.size(), path.c_str());
    }
}

}} // namespace voxelvk::util
