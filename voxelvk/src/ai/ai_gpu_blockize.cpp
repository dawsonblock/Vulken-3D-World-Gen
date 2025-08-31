#include "ai_gpu_blockize.hpp"
#include <vector>
#include <cstdio>
static std::vector<uint32_t> load_spirv_file(const char* path){
  std::vector<uint32_t> data; FILE* f=fopen(path,"rb"); if(!f){ std::fprintf(stderr,"[AI] Could not open %s\n", path); return data; }
  fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET); data.resize((size_t)sz/4); fread(data.data(),1,(size_t)sz,f); fclose(f); return data;
}
namespace voxelvk::ai {
bool AIGpuBlockizer::init(VkDevice device, VkPipelineCache cache){
  m_device=device;
  VkDescriptorSetLayoutBinding b[4] = {}; for(int i=0;i<4;i++){ b[i].binding=i; b[i].descriptorCount=1; b[i].stageFlags=VK_SHADER_STAGE_COMPUTE_BIT; b[i].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; }
  VkDescriptorSetLayoutCreateInfo dsl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO}; dsl.bindingCount=4; dsl.pBindings=b; vkCreateDescriptorSetLayout(device,&dsl,nullptr,&m_dset_layout);
  VkPushConstantRange pcr{}; pcr.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT; pcr.offset=0; pcr.size=sizeof(GpuBlockizeThresholds);
  VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO}; plci.setLayoutCount=1; plci.pSetLayouts=&m_dset_layout; plci.pushConstantRangeCount=1; plci.pPushConstantRanges=&pcr; vkCreatePipelineLayout(device,&plci,nullptr,&m_pipe_layout);
  auto spirv=load_spirv_file("spv/ai/blockize.comp.spv"); if(spirv.empty()) return False;
  VkShaderModuleCreateInfo smci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO}; smci.codeSize=spirv.size()*4; smci.pCode=spirv.data(); VkShaderModule sm; vkCreateShaderModule(device,&smci,nullptr,&sm);
  VkComputePipelineCreateInfo cpci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO}; cpci.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}; cpci.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT; cpci.stage.module=sm; cpci.stage.pName="main"; cpci.layout=m_pipe_layout;
  vkCreateComputePipelines(device, cache, 1, &cpci, nullptr, &m_pipeline); vkDestroyShaderModule(device,sm,nullptr);
  VkDescriptorPoolSize ps{}; ps.type=VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; ps.descriptorCount=4;
  VkDescriptorPoolCreateInfo dpci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO}; dpci.maxSets=1; dpci.poolSizeCount=1; dpci.pPoolSizes=&ps; vkCreateDescriptorPool(device,&dpci,nullptr,&m_pool);
  return true;
}
void AIGpuBlockizer::destroy(VkDevice device){
  if(m_pool) vkDestroyDescriptorPool(device,m_pool,nullptr);
  if(m_pipeline) vkDestroyPipeline(device,m_pipeline,nullptr);
  if(m_pipe_layout) vkDestroyPipelineLayout(device,m_pipe_layout,nullptr);
  if(m_dset_layout) vkDestroyDescriptorSetLayout(device,m_dset_layout,nullptr);
  m_pool=m_pipeline=m_pipe_layout=m_dset_layout=VK_NULL_HANDLE;
}
void AIGpuBlockizer::dispatch(VkCommandBuffer cmd, VkImageView occ3D_r8ui, VkImageView height2D_r32f, VkImageView biome2D_r8ui, VkImageView out3D_r16ui, uint32_t SX,uint32_t SY,uint32_t SZ, const GpuBlockizeThresholds& pc){
  VkDescriptorSetAllocateInfo dsai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO}; dsai.descriptorPool=m_pool; dsai.descriptorSetCount=1; dsai.pSetLayouts=&m_dset_layout; VkDescriptorSet ds; vkAllocateDescriptorSets(m_device,&dsai,&ds);
  auto storage_image=[](VkImageView v){ VkDescriptorImageInfo ii{}; ii.imageView=v; ii.imageLayout=VK_IMAGE_LAYOUT_GENERAL; return ii; };
  VkDescriptorImageInfo i0=storage_image(occ3D_r8ui), i1=storage_image(height2D_r32f), i2=storage_image(biome2D_r8ui), i3=storage_image(out3D_r16ui);
  VkWriteDescriptorSet w[4]{}; for(int i=0;i<4;i++){ w[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; w[i].dstSet=ds; w[i].dstBinding=i; w[i].descriptorCount=1; w[i].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; }
  w[0].pImageInfo=&i0; w[1].pImageInfo=&i1; w[2].pImageInfo=&i2; w[3].pImageInfo=&i3; vkUpdateDescriptorSets(m_device,4,w,0,nullptr);
  vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,m_pipeline);
  vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,m_pipe_layout,0,1,&ds,0,nullptr);
  vkCmdPushConstants(cmd,m_pipe_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(GpuBlockizeThresholds),&pc);
  uint32_t gx=(SX+7)/8, gy=(SY+7)/8, gz=(SZ+3)/4; vkCmdDispatch(cmd,gx,gy,gz);
}
} // ns
