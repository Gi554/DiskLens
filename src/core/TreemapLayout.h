#pragma once

#include "core/Scanner.h"

namespace disklens {
struct MapItem { NodeId id; std::uintmax_t bytes; };
struct MapRect { NodeId id; double x, y, width, height; };
std::vector<MapRect> layoutTreemap(std::vector<MapItem> items, double width, double height);
} // namespace disklens
