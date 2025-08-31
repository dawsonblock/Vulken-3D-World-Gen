#pragma once
#include "ai_object_generator.hpp"
#if __has_include("voxel_meshing.hpp")
#  include "voxel_meshing.hpp"
#  define VOXELVK_HAS_NATIVE_VOXEL_MESHER 1
#endif
namespace voxelvk::ai {
#ifdef VOXELVK_HAS_NATIVE_VOXEL_MESHER
bool ApplyAiOutputsToEngineNative(const std::string& name, const AiOutputs& out, GPUScene& scene, VkCommandBuffer cmd, VoxelMeshing& mesher, uint16_t solid_block_id=1);
#endif
} // ns
