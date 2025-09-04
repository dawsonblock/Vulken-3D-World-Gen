#pragma once
#include "MeshGenerator.h"

namespace engine {

class ENGINE_API MarchingCubesGenerator : public IMeshGenerator {
public:
    Mesh generate(const VoxelChunk& chunk, const MeshGenParams& params) override;
};

} // namespace engine
