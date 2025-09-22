#pragma once

#include <array>

namespace engine {

// Complete marching cubes triangle table (256 cases)
extern const std::array<std::array<int, 16>, 256> triTable;

} // namespace engine
