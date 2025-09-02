#include "ai_palette_config_io.hpp"
#include <fstream>
#include <sstream>

namespace voxelvk::ai {

bool PaletteRuntime::load_from_file(const std::string& p){
  path = p; std::ifstream in(p); if(!in.is_open()) return false; std::stringstream ss; ss<<in.rdbuf(); in.close();
  return parse_ini_to_palette(ss.str(), cfg);
}

bool PaletteRuntime::save_to_file(const std::string& p) const{
  std::ofstream out(p); if(!out.is_open()) return false; out<<to_string(cfg); out.close(); return true;
}

bool PaletteRuntime::tick_hot_reload(){
  using namespace std::filesystem; std::error_code ec{}; if(!exists(path,ec)) return false;
  auto t = last_write_time(path, ec); if(ec) return false; if(t==last_write) return false; last_write = t; return load_from_file(path);
}

static void write_ai_section(std::ostringstream& o, const PaletteConfig& c){
  o<<"\n[ai_generation]\n";
  o<<"enable_ai_structures="<<(c.ai_generation.enable_ai_structures?"true":"false")<<"\n";
  o<<"enable_ai_textures="<<(c.ai_generation.enable_ai_textures?"true":"false")<<"\n";
  o<<"enable_dynamic_biomes="<<(c.ai_generation.enable_dynamic_biomes?"true":"false")<<"\n";
  o<<"structure_density="<<c.ai_generation.structure_density<<"\n";
  o<<"terrain_variation="<<c.ai_generation.terrain_variation<<"\n";
  o<<"ai_generation_quality="<<c.ai_generation.ai_generation_quality<<"\n";
  o<<"enable_rag="<<(c.ai_generation.enable_rag?"true":"false")<<"\n";
  o<<"rag_top_k="<<c.ai_generation.rag_top_k<<"\n";
}

std::string to_string(const PaletteConfig& c){
  std::ostringstream o;
  o<<"[biome]"<<"\n";
  o<<"sea_level="<<c.sea_level<<"\n";
  o<<"shoreline_margin="<<c.shoreline_margin<<"\n";
  o<<"beach_depth="<<c.beach_depth<<"\n";
  o<<"dirt_depth="<<c.dirt_depth<<"\n";
  o<<"snow_line="<<c.snow_line<<"\n";
  o<<"cliff_slope_threshold="<<c.cliff_slope_threshold<<"\n";
  write_ai_section(o,c);
  return o.str();
}

static bool parse_bool(const std::string& v){ return v=="1"||v=="true"||v=="True"||v=="TRUE"; }

bool parse_ini_to_palette(const std::string& text, PaletteConfig& out){
  std::istringstream in(text); std::string line; std::string section;
  while(std::getline(in,line)){
  if(line.empty()) continue;
  if(line[0]=='#'||line[0]==';') continue;
    if(line.front()=='[' && line.back()==']'){ section=line.substr(1,line.size()-2); continue; }
    auto eq = line.find('='); if(eq==std::string::npos) continue; std::string key=line.substr(0,eq), val=line.substr(eq+1);
    if(section=="biome"){
      if(key=="sea_level") out.sea_level=std::stoi(val);
      else if(key=="shoreline_margin") out.shoreline_margin=std::stoi(val);
      else if(key=="beach_depth") out.beach_depth=std::stoi(val);
      else if(key=="dirt_depth") out.dirt_depth=std::stoi(val);
      else if(key=="snow_line") out.snow_line=std::stoi(val);
      else if(key=="cliff_slope_threshold") out.cliff_slope_threshold=std::stof(val);
    } else if(section=="ai_generation"){
      if(key=="enable_ai_structures") out.ai_generation.enable_ai_structures = parse_bool(val);
      else if(key=="enable_ai_textures") out.ai_generation.enable_ai_textures = parse_bool(val);
      else if(key=="enable_dynamic_biomes") out.ai_generation.enable_dynamic_biomes = parse_bool(val);
      else if(key=="structure_density") out.ai_generation.structure_density = std::stof(val);
      else if(key=="terrain_variation") out.ai_generation.terrain_variation = std::stof(val);
      else if(key=="ai_generation_quality") out.ai_generation.ai_generation_quality = std::stof(val);
      else if(key=="enable_rag") out.ai_generation.enable_rag = parse_bool(val);
      else if(key=="rag_top_k") out.ai_generation.rag_top_k = std::stoi(val);
    }
  }
  return true;
}

} // ns