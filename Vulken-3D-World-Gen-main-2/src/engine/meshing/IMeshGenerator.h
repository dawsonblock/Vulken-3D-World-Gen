#pragma once
#include <optional>
#include "engine/core/VoxelTypes.h"

namespace vulken::meshing {

class IMeshGenerator {
public:
    virtual ~IMeshGenerator() = default;
    virtual std::optional<core::Mesh> generate(const core::VoxelChunk& chunk) = 0;
};

} // namespace vulken::meshing
