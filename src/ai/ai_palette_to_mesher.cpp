#include "ai_palette_to_mesher.hpp"
namespace voxelvk::ai {
#ifdef VOXELVK_HAS_NATIVE_VOXEL_MESHER
bool ApplyAiWorldWithPalette(const AiOutputs& out, const PaletteConfig& cfg, GPUScene& scene, VkCommandBuffer cmd, VoxelMeshing& mesher){
  VoxelVolume vol = BuildVoxelVolumeWithPalette(out, cfg);
  mesher.mesh_volume(vol, scene, cmd);
  return true;
}
#endif
} // ns
