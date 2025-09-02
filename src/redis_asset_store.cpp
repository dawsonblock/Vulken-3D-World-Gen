#include "redis_asset_store.h"
#include <sw/redis++/redis++.h>
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <thread>
#include <iostream>

struct RedisAssetStore::Impl {
    sw::redis::Redis redis;
    std::unique_ptr<sw::redis::Subscriber> sub;
    std::thread pubsub_thread;
    ReloadCallback reload_callback;
    std::vector<std::string> channels;
    std::string mesh_namespace;
    std::string voxel_namespace;

    Impl(const std::string& uri, int pool_size, int connect_timeout, int socket_timeout)
        : redis(uri) {}
};

RedisAssetStore::RedisAssetStore(const std::string& config_path) {
    YAML::Node config = YAML::LoadFile(config_path);
    auto redis_config = config["redis"];
    std::string uri = redis_config["uri"].as<std::string>();
    int pool_size = redis_config["pool_size"].as<int>();
    int connect_timeout = redis_config["connect_timeout_ms"].as<int>();
    int socket_timeout = redis_config["socket_timeout_ms"].as<int>();

    pimpl = std::make_unique<Impl>(uri, pool_size, connect_timeout, socket_timeout);

    if (redis_config["enable_pubsub"].as<bool>()) {
        pimpl->sub = std::make_unique<sw::redis::Subscriber>(pimpl->redis.subscriber());
        for (const auto& channel : redis_config["channels"]) {
            pimpl->channels.push_back(channel.as<std::string>());
            pimpl->sub->subscribe(pimpl->channels.back());
        }
        pimpl->pubsub_thread = std::thread(&RedisAssetStore::pubsub_thread_func, this);
    }

    auto ns = config["namespaces"];
    pimpl->mesh_namespace = ns["mesh"].as<std::string>();
    pimpl->voxel_namespace = ns["voxel"].as<std::string>();
}

RedisAssetStore::~RedisAssetStore() {
    if (pimpl->sub) {
        pimpl->sub->unsubscribe();
        if (pimpl->pubsub_thread.joinable()) {
            pimpl->pubsub_thread.join();
        }
    }
}

bool RedisAssetStore::fetch_mesh(const std::string& mesh_id, MeshBlob& blob) {
    auto val = pimpl->redis.get(pimpl->mesh_namespace + mesh_id);
    if (val) {
        blob.id = mesh_id;
        blob.data.assign(val->begin(), val->end());
        return true;
    }
    return false;
}

bool RedisAssetStore::fetch_voxel_chunk(const std::string& chunk_id, VoxelBlob& blob) {
    auto val = pimpl->redis.get(pimpl->voxel_namespace + chunk_id);
    if (val) {
        blob.id = chunk_id;
        blob.data.assign(val->begin(), val->end());
        return true;
    }
    return false;
}

void RedisAssetStore::set_reload_callback(ReloadCallback cb) {
    pimpl->reload_callback = cb;
}

void RedisAssetStore::pubsub_thread_func() {
    pimpl->sub->on_message([this](std::string channel, std::string msg) {
        if (pimpl->reload_callback) {
            pimpl->reload_callback(msg);
        }
    });

    while (true) {
        try {
            pimpl->sub->consume();
        } catch (const sw::redis::Error &err) {
            std::cerr << "Redis pub/sub error: " << err.what() << std::endl;
        }
    }
}
