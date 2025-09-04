#pragma once
#include <string>
#include <filesystem>
#include "ai_biome_palette.hpp"
namespace voxelvk::ai {
struct PaletteRuntime{
  PaletteConfig cfg{}; std::string path="ai_palette.cfg"; std::filesystem::file_time_type last_write{};
  bool load_from_file(const std::string& p);
  bool save_to_file(const std::string& p) const;
  bool tick_hot_reload();
};
std::string to_string(const PaletteConfig& c);
bool parse_ini_to_palette(const std::string& text, PaletteConfig& out);
std::string default_palette_ini();
} // ns
