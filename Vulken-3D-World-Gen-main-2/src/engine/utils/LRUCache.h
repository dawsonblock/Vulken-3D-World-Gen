#pragma once
#include <list>
#include <unordered_map>
#include <utility>
#include <cstddef>
#include <optional>

namespace vulken::utils {

template <class K, class V>
class LRUCache {
public:
    explicit LRUCache(size_t capacity) : capacity_(capacity) {}

    void setCapacity(size_t c) {
        capacity_ = c;
        evictIfNeeded();
    }

    bool get(const K& k, V& out) {
        auto it = map_.find(k);
        if (it == map_.end()) return false;
        touch(it);
        out = it->second.first;
        return true;
    }

    void put(const K& k, const V& v) {
        auto it = map_.find(k);
        if (it != map_.end()) {
            it->second.first = v;
            touch(it);
        } else {
            order_.push_front(k);
            map_[k] = {v, order_.begin()};
            evictIfNeeded();
        }
    }

    bool erase(const K& k) {
        auto it = map_.find(k);
        if (it == map_.end()) return false;
        order_.erase(it->second.second);
        map_.erase(it);
        return true;
    }

    size_t size() const { return map_.size(); }

private:
    using ListIt = typename std::list<K>::iterator;
    size_t capacity_;
    std::list<K> order_;
    std::unordered_map<K, std::pair<V, ListIt>> map_;

    void touch(typename std::unordered_map<K, std::pair<V, ListIt>>::iterator it) {
        order_.erase(it->second.second);
        order_.push_front(it->first);
        it->second.second = order_.begin();
    }

    void evictIfNeeded() {
        while (capacity_ > 0 && map_.size() > capacity_) {
            const K& k = order_.back();
            map_.erase(k);
            order_.pop_back();
        }
    }
};

} // namespace vulken::utils
