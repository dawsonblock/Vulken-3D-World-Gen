#pragma once
#include "ai_biome_palette.hpp"
#if __has_include("voxel_meshing.hpp")
#  include "voxel_meshing.hpp"
#  define VOXELVK_HAS_NATIVE_VOXEL_MESHER 1
#endif
namespace voxelvk::ai {
#ifdef VOXELVK_HAS_NATIVE_VOXEL_MESHER
bool ApplyAiWorldWithPalette(const AiOutputs& out, const PaletteConfig& cfg, GPUScene& scene, VkCommandBuffer cmd, VoxelMeshing& mesher);
#endif
} // ns
