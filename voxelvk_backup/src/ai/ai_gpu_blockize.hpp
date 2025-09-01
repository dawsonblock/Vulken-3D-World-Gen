#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
namespace voxelvk::ai {
struct GpuBlockizeThresholds{
  int32_t sea_level=72, shoreline_margin=3, beach_depth=2, dirt_depth=4, snow_line=140;
  float cliff_slope_threshold=0.8f;
  uint32_t idAir=0, idWater=1, idSand=2, idGrass=3, idDirt=4, idStone=5, idSnow=6, idWood=7, idLeaves=8, idClay=9;
};
class AIGpuBlockizer{
public:
  bool init(VkDevice device, VkPipelineCache cache=VK_NULL_HANDLE);
  void destroy(VkDevice device);
  void dispatch(VkCommandBuffer cmd, VkImageView occ3D_r8ui, VkImageView height2D_r32f, VkImageView biome2D_r8ui, VkImageView out3D_r16ui, uint32_t SX,uint32_t SY,uint32_t SZ, const GpuBlockizeThresholds& pc);
private:
  VkDevice m_device=VK_NULL_HANDLE; VkDescriptorSetLayout m_dset_layout=VK_NULL_HANDLE; VkPipelineLayout m_pipe_layout=VK_NULL_HANDLE; VkPipeline m_pipeline=VK_NULL_HANDLE; VkDescriptorPool m_pool=VK_NULL_HANDLE;
};
} // ns
