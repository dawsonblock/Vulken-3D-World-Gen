#version 450
#extension GL_EXT_samplerless_texture_functions : enable
layout (local_size_x = 8, local_size_y = 8, local_size_z = 4) in;
layout (set = 0, binding = 0, r8ui)   uniform readonly  uimage3D uOccupancy;
layout (set = 0, binding = 1, r32f)   uniform readonly  image2D  uHeight;
layout (set = 0, binding = 2, r8ui)   uniform readonly  uimage2D uBiome;
layout (set = 0, binding = 3, r16ui)  uniform writeonly uimage3D uBlocks;
layout(push_constant) uniform PC {
    int sea_level, shoreline_margin, beach_depth, dirt_depth, snow_line;
    float cliff_slope_threshold;
    uint idAir, idWater, idSand, idGrass, idDirt, idStone, idSnow, idWood, idLeaves, idClay;
} pc;
layout(constant_id = 0) const uint BIOME_SURFACE_0 = 3u;
layout(constant_id = 1) const uint BIOME_SURFACE_1 = 2u;
layout(constant_id = 2) const uint BIOME_SURFACE_2 = 3u;
layout(constant_id = 3) const uint BIOME_SURFACE_3 = 6u;
layout(constant_id = 4) const uint BIOME_SURFACE_4 = 3u;
layout(constant_id = 5) const uint BIOME_SURFACE_5 = 5u;
layout(constant_id = 6) const uint BIOME_SURFACE_6 = 3u;
layout(constant_id = 7) const uint BIOME_SURFACE_7 = 3u;
float H(int x,int z,ivec2 d){ x=clamp(x,0,d.x-1); z=clamp(z,0,d.y-1); return imageLoad(uHeight, ivec2(x,z)).r; }
float slope(int x,int z,ivec2 d){ float dx=0.5*(H(x+1,z,d)-H(x-1,z,d)); float dz=0.5*(H(x,z+1,d)-H(x,z-1,d)); return sqrt(dx*dx+dz*dz); }
uint mapBiome(uint b){
    switch(int(b)){ case 0:return BIOME_SURFACE_0; case 1:return BIOME_SURFACE_1; case 2:return BIOME_SURFACE_2;
    case 3:return BIOME_SURFACE_3; case 4:return BIOME_SURFACE_4; case 5:return BIOME_SURFACE_5;
    case 6:return BIOME_SURFACE_6; case 7:return BIOME_SURFACE_7; default:return 3u; }
}
void main(){
    ivec3 gid=ivec3(gl_GlobalInvocationID); ivec3 s=imageSize(uOccupancy); if(any(greaterThanEqual(gid,s))) return;
    int x=gid.x,y=gid.y,z=gid.z; ivec2 hm=imageSize(uHeight), bm=imageSize(uBiome);
    uint occ=imageLoad(uOccupancy,gid).r; int sy=int(imageLoad(uHeight, clamp(ivec2(x,z), ivec2(0), hm-1)).r);
    uint biome=imageLoad(uBiome, clamp(ivec2(x,z), ivec2(0), bm-1)).r;
    bool nearShore=abs(sy-pc.sea_level)<=pc.shoreline_margin, aboveSnow=sy>=pc.snow_line;
    bool cliff=slope(x,z,hm)>=pc.cliff_slope_threshold;
    uint top=mapBiome(biome); if(nearShore) top=pc.idSand; else if(aboveSnow) top=pc.idSnow; else if(cliff) top=pc.idStone;
    uint outId=pc.idAir;
    if(occ!=0u){
        if(y==sy) outId=top;
        else if(y<sy){
            if(top==pc.idSand) outId = (y>=sy-pc.beach_depth)?pc.idSand:pc.idStone;
            else if(top==pc.idSnow) outId = (y>=sy-pc.dirt_depth)?pc.idDirt:pc.idStone;
            else if(cliff) outId=pc.idStone;
            else outId=(y>=sy-pc.dirt_depth)?pc.idDirt:pc.idStone;
        } else outId=pc.idStone;
    } else outId=(y<=pc.sea_level)?pc.idWater:pc.idAir;
    imageStore(uBlocks, gid, uvec4(outId,0,0,0));
}
