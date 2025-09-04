#pragma once
#include <unordered_map>
#include <list>
#include "IChunkCache.h"

namespace engine {

class ENGINE_API SimpleLRUCache : public IChunkCache {
public:
    explicit SimpleLRUCache(size_t capacity) : capacity_(capacity) {}

    VoxelChunkPtr get(const ChunkCoord& c) override {
        auto it = map_.find(c);
        if (it == map_.end()) return nullptr;
        touch(it);
        return it->second.value;
    }

    void put(const ChunkCoord& c, VoxelChunkPtr chunk) override {
        auto it = map_.find(c);
        if (it != map_.end()) {
            it->second.value = std::move(chunk);
            touch(it);
            return;
        }
        if (list_.size() >= capacity_ && !list_.empty()) {
            auto old = list_.back();
            list_.pop_back();
            map_.erase(old);
        }
        list_.push_front(c);
        map_.emplace(c, Node{list_.begin(), std::move(chunk)});
    }

    void erase(const ChunkCoord& c) override {
        auto it = map_.find(c);
        if (it == map_.end()) return;
        list_.erase(it->second.it);
        map_.erase(it);
    }

    void clear() override {
        map_.clear();
        list_.clear();
    }

    size_t size() const override { return map_.size(); }
    size_t capacity() const override { return capacity_; }

private:
    struct Node {
        std::list<ChunkCoord>::iterator it;
        VoxelChunkPtr value;
    };

    void touch(typename std::unordered_map<ChunkCoord, Node, ChunkCoordHash>::iterator it) {
        list_.erase(it->second.it);
        list_.push_front(it->first);
        it->second.it = list_.begin();
    }

    size_t capacity_;
    std::list<ChunkCoord> list_;
    std::unordered_map<ChunkCoord, Node, ChunkCoordHash> map_;
};

} // namespace engine
