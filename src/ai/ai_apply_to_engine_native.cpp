#include "ai_apply_to_engine_native.hpp"
#include "ai_mesher_bridge_voxelvk.hpp"
#include <cstdio>
namespace voxelvk::ai {
#ifdef VOXELVK_HAS_NATIVE_VOXEL_MESHER
bool ApplyAiOutputsToEngineNative(const std::string& name, const AiOutputs& out, GPUScene& scene, VkCommandBuffer cmd, VoxelMeshing& mesher, uint16_t solid_block_id){
  bool any=false;
  if(!out.vox.occupancy.empty())
    any |= EngineSubmitVoxelsToMesher(out.vox, scene, cmd, mesher, solid_block_id);
  if(!any) std::fprintf(stderr, "[AI] ApplyAiOutputsToEngineNative: nothing applied for '%s'\n", name.c_str());
  return any;
}
#endif
} // ns
