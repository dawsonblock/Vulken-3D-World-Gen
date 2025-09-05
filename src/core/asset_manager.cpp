#include "asset_manager.hpp"
#include <algorithm>
#include <cctype>

namespace voxelvk {
    std::unique_ptr<AssetManager> AssetManager::instance_;
}