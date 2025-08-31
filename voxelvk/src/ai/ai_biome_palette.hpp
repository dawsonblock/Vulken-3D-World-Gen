#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include <string>
#include "ai_object_generator.hpp"
#if __has_include("voxel_meshing.hpp")
#  include "voxel_meshing.hpp"
#  define VOXELVK_HAS_NATIVE_VOXEL_MESHER 1
#endif
namespace voxelvk::ai {

enum class Biome:int32_t{Plains,Desert,Taiga,Snow,Swamp,Mountain,Custom0,Custom1};
struct BlockPalette{ uint16_t Air=0,Water=1,Sand=2,Grass=3,Dirt=4,Stone=5,Snow=6,Wood=7,Leaves=8,Clay=9; };

struct PaletteConfig{
  BlockPalette ids{}; int sea_level=72,shoreline_margin=3,beach_depth=2,dirt_depth=4,snow_line=140;
  float cliff_slope_threshold=0.8f; std::array<uint16_t,8> biome_surface_map={3,2,3,6,3,5,3,3};
  // New: AI/RAG runtime options surfaced via palette runtime for hot control
  struct AiGeneration {
    bool enable_ai_structures = true;
    bool enable_ai_textures = true;
    bool enable_dynamic_biomes = true;
    float structure_density = 0.15f;
    float terrain_variation = 1.2f;
    float ai_generation_quality = 0.7f;

    // RAG user-facing knobs to bridge to EnhancedAiGenConfig
    bool enable_rag = false;
    int rag_top_k = 3;
  } ai_generation{};
};

#ifdef VOXELVK_HAS_NATIVE_VOXEL_MESHER
struct VoxelVolume;
VoxelVolume BuildVoxelVolumeWithPalette(const AiOutputs& out, const PaletteConfig& cfg);
float EstimateSlope(const std::vector<float>& heightmap,int N,int x,int z);
#endif

} // ns