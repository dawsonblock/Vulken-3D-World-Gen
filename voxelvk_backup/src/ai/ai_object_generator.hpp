#pragma once
#include <cstdint>
#include <vector>
#include <string>
namespace voxelvk { namespace ai {
struct VoxelGrid { int W=0, H=0, D=0; std::vector<uint8_t> occupancy; };
struct TriMesh   { std::vector<float> positions,normals,uvs; std::vector<uint32_t> indices; };
struct AiOutputs { VoxelGrid vox; std::vector<float> heightmap; std::vector<int32_t> biome_map; TriMesh mesh; };
struct AiGenConfig { std::string trt_engine_path; int latent_size=1024; int sdf_size=192; float iso_level=0.0f; bool use_gpu=true; };
}} // ns
