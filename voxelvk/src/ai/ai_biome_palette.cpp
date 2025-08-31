#include "ai_biome_palette.hpp"
#include <algorithm>
#include <cmath>
namespace voxelvk::ai {
#ifdef VOXELVK_HAS_NATIVE_VOXEL_MESHER
static inline int clampi(int v,int lo,int hi){ return std::max(lo,std::min(hi,v)); }
float EstimateSlope(const std::vector<float>& hm,int N,int x,int z){
  auto H=[&](int ix,int iz){ ix=clampi(ix,0,N-1); iz=clampi(iz,0,N-1); return hm[(size_t)iz*N+ix]; };
  float ddx=0.5f*(H(x+1,z)-H(x-1,z)), ddz=0.5f*(H(x,z+1)-H(x,z-1));
  return std::sqrt(ddx*ddx+ddz*ddz);
}
static inline uint16_t surf(uint32_t idx,const PaletteConfig& cfg){
  uint16_t id=cfg.biome_surface_map[idx%cfg.biome_surface_map.size()];
  const auto&p=cfg.ids; switch(id){default:case 3:return p.Grass; case 2:return p.Sand; case 6:return p.Snow; case 5:return p.Stone; case 1:return p.Water; case 4:return p.Dirt;}
}
VoxelVolume BuildVoxelVolumeWithPalette(const AiOutputs& out, const PaletteConfig& cfg){
  const int W=out.vox.W,H=out.vox.H,D=out.vox.D; const int N=(int)std::sqrt((double)out.heightmap.size());
  VoxelVolume vol; vol.sizeX=W; vol.sizeY=H; vol.sizeZ=D; vol.blocks.resize((size_t)W*H*D,cfg.ids.Air);
  auto idx3=[&](int x,int y,int z){ return (size_t)z*H*W + (size_t)y*W + x; };
  auto Hxz=[&](int x,int z){ if(N*N!=(int)out.heightmap.size()) return 0; x=clampi(x,0,N-1); z=clampi(z,0,N-1); return (int)out.heightmap[(size_t)z*N+x]; };
  auto Bxz=[&](int x,int z){ if(out.biome_map.empty() || (int)out.biome_map.size()!=N*N) return 0; x=clampi(x,0,N-1); z=clampi(z,0,N-1); return out.biome_map[(size_t)z*N+x]; };
  for(int z=0; z<D; ++z){
    for(int x=0; x<W; ++x){
      const int sy=std::clamp(Hxz(x,z),0,H-1); const float s=(N*N==(int)out.heightmap.size())?EstimateSlope(out.heightmap,N,x,z):0.f;
      uint16_t top = surf((uint32_t)Bxz(x,z), cfg);
      const bool near=std::abs(sy-cfg.sea_level)<=cfg.shoreline_margin, snow=sy>=cfg.snow_line, cliff=s>=cfg.cliff_slope_threshold;
      if(near) top=cfg.ids.Sand; else if(snow) top=cfg.ids.Snow; else if(cliff) top=cfg.ids.Stone;
      for(int y=0;y<H;++y){
        const bool solid = out.vox.occupancy[idx3(x,y,z)]!=0;
        uint16_t id=cfg.ids.Air;
        if(solid){
          if(y==sy) id=top;
          else if(y<sy){
            if(top==cfg.ids.Sand) id=(y>=sy-cfg.beach_depth)?cfg.ids.Sand:cfg.ids.Stone;
            else if(top==cfg.ids.Snow) id=(y>=sy-cfg.dirt_depth)?cfg.ids.Dirt:cfg.ids.Stone;
            else if(cliff) id=cfg.ids.Stone;
            else id=(y>=sy-cfg.dirt_depth)?cfg.ids.Dirt:cfg.ids.Stone;
          } else id=cfg.ids.Stone;
        } else {
          id=(y<=cfg.sea_level)?cfg.ids.Water:cfg.ids.Air;
        }
        vol.blocks[idx3(x,y,z)]=id;
      }
    }
  }
  return vol;
}
#endif
} // ns
