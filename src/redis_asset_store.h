#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>

// Forward declaration
namespace sw::redis {
    class Redis;
    class Subscriber;
}

struct MeshBlob {
    std::string id;
    std::vector<uint8_t> data;
};

struct VoxelBlob {
    std::string id;
    std::vector<uint8_t> data;
};

class RedisAssetStore {
public:
    RedisAssetStore(const std::string& config_path);
    ~RedisAssetStore();

    bool fetch_mesh(const std::string& mesh_id, MeshBlob& blob);
    bool fetch_voxel_chunk(const std::string& chunk_id, VoxelBlob& blob);

    using ReloadCallback = std::function<void(const std::string& asset_id)>;
    void set_reload_callback(ReloadCallback cb);

private:
    void pubsub_thread_func();

    struct Impl;
    std::unique_ptr<Impl> pimpl;
};
