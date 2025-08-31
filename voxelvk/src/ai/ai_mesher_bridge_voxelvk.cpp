#include "ai_mesher_bridge_voxelvk.hpp"
#include <cstdio>
namespace voxelvk::ai {
#ifdef VOXELVK_HAS_NATIVE_VOXEL_MESHER
VoxelVolume MakeVoxelVolume(const VoxelGrid& vox, uint16_t solid_block_id){
    VoxelVolume vol{}; vol.sizeX=vox.W; vol.sizeY=vox.H; vol.sizeZ=vox.D;
    vol.blocks.resize((size_t)vox.W*vox.H*vox.D);
    for(size_t i=0;i<vol.blocks.size();++i) vol.blocks[i]=vox.occupancy[i]?solid_block_id:0;
    return vol;
}
bool EngineSubmitVoxelsToMesher(const VoxelGrid& vox, GPUScene& scene, VkCommandBuffer cmd, VoxelMeshing& mesher, uint16_t solid_block_id){
    if(vox.W*vox.H*vox.D==0){ std::fputs("[AI] empty VoxelGrid\n", stderr); return false; }
    VoxelVolume vol = MakeVoxelVolume(vox, solid_block_id);
    mesher.mesh_volume(vol, scene, cmd);
    return true;
}
#endif
} // ns
