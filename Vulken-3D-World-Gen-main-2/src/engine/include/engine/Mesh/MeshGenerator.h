#pragma once
#include <memory>
#include "../engine_export.h"
#include "../VoxelTypes.h"

namespace engine {

struct MeshGenParams {
    uint8_t isoLevel{128};
    // ...existing code...
};

class ENGINE_API IMeshGenerator {
public:
    virtual ~IMeshGenerator() = default;
    virtual Mesh generate(const VoxelChunk& chunk, const MeshGenParams& params) = 0;
};

using MeshGeneratorPtr = std::shared_ptr<IMeshGenerator>;

} // namespace engine
