#pragma once
#include "engine/meshing/IMeshGenerator.h"

namespace vulken::meshing {

class MeshGenerator final : public IMeshGenerator {
public:
    std::optional<core::Mesh> generate(const core::VoxelChunk& chunk) override;

    // Future: strategy switches (MC, DC), isovalue, material rules
    void setIsoValue(float iso) { isoValue_ = iso; }

private:
    float isoValue_ = 0.0f; // surface threshold
    // Future: lookup tables, dual contouring QEF, etc.
};

} // namespace vulken::meshing
